/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#if !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif 

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "pkgtraversal.h"
#include "pkgcontainer.h"
#include "pkgthreadpool.h"
#include "nqcommon.h"


struct _nqstack 
{
    unsigned int top; /* Alway point to the first free item */
    unsigned int size; 
    unsigned int stack[PKG_NUM];
};

typedef struct _nqstack nqstack;

static void nq_traversal_dep_recursion_check(pkg_dts * pkgtab_p, unsigned int root_pkg_off, nqstack * pkgstack);

static int nqstack_push (nqstack * pkgstack, pkg_dts * pkgtab_p, unsigned int pkg_off);
static int nqstack_pop (nqstack * pkgstack, pkg_dts * pkgtab_p);
static void nqstack_show (nqstack * pkgstack_p, pkg_dts * pkgtab_p);


static int (* get_depend_func) (char *, raw_node **);

static int nq_traversal_raw_add_to_raw_node_list(char * name, unsigned int con_offset, 
        unsigned int level, pkg_node ** target_list, pkg_node ** free_node_list);


static int nq_traversal_reclaim_pkg_node(pkg_node *, pkg_node **);






void nq_traversal_set_get_depend_func(int (* func)(char *, raw_node **))
{
    get_depend_func = func;
}


int nq_traversal_all_dependence(char *pkg_name, container_dts **r_con_dts) 
{
    assert(pkg_name);

    r_con_dts = r_con_dts;

    int r_val;

    int raw_count = 0; /* 对原料 （求依赖的包） 进行计数，为 0 时，作为下边 while 循环结束条件 */
    
    pkg_node * free_node_list = NULL;

    raw_node * child_raw_p;
    raw_node * child_raw_tmp_p;

    pkg_node * raw_list = NULL;

    pkg_node * pkg_node_p;
    pkg_node * pkg_node_tmp_p;

    nq_thread_pool * thread_pool = nq_thread_pool_create();
    
    if (!thread_pool)
    {
        nq_errmsg("Can't create threads pool!");
        return -1;
    }

    container_dts * con_dts_p = nq_container_dts_alloc(0);
            /* Don't worried about 0-length, it will adjust automatically */

    if (!con_dts_p)
    {
        nq_errmsg("Can't create packages container!");
        return -1;
    }

    
    /* OK, now, All data structures have been created */

    nq_traversal_raw_add_to_raw_node_list(pkg_name, 0, 
            0, &raw_list, &free_node_list);

    nq_thread_pool_add_raw_material(thread_pool, raw_list);
    raw_list = NULL;

    if ((r_val = (nq_container_dts_insert (pkg_name, con_dts_p, 0))) < 0)
    {
        nq_container_dts_destroy(con_dts_p);
        nq_errmsg_exit("Critical: Can't handle any more! Now exit");
    }

    ++raw_count;

    while(1)
    {
        /*
        pkg_node_p = raw_list;
        raw_list = NULL;
        */

        pkg_node_p = nq_thread_pool_get_product(thread_pool);
        --raw_count;

        while (pkg_node_p)
        {
            /* nq_traversal_get_pkg_depend(pkg_node_p); */
            child_raw_p = pkg_node_p->child;

            while(child_raw_p)
            {
                if ((r_val = (nq_container_dts_insert (child_raw_p->name, con_dts_p, 
                                    pkg_node_p->con_offset))) < 0)
                {
                    nq_container_dts_destroy(con_dts_p);
                    nq_errmsg_exit("Critical: Can't handle any more! Now exit");
                }

                else if (r_val > 0)
                {
                    nq_traversal_raw_add_to_raw_node_list(child_raw_p->name, r_val, 
                            pkg_node_p->level + 1, &raw_list, &free_node_list);

                    nq_thread_pool_add_raw_material (thread_pool, raw_list);
                    raw_list = NULL;

                    ++raw_count;
                }

                else 
                {
                    /* if r_val == 0; it must discard, since it has been traversaled once */
                    free(child_raw_p->name);
                }

                child_raw_tmp_p = child_raw_p;
                child_raw_p     = child_raw_p->next;
                free(child_raw_tmp_p);
            }

            pkg_node_tmp_p = pkg_node_p;
            pkg_node_p     = pkg_node_p->next;
            nq_traversal_reclaim_pkg_node(pkg_node_tmp_p, &free_node_list);
            pkg_node_tmp_p = NULL;

        }

        if (raw_count == 0)
        {
            while (free_node_list)
            {
                pkg_node_tmp_p = free_node_list;
                free_node_list = free_node_list->next;
                free(pkg_node_tmp_p->name);
                free(pkg_node_tmp_p);
            }

            break; 
        }
    }

    if (!r_con_dts)
    {
        nq_traversal_get_level_dep (con_dts_p);
        nq_traversal_get_cricle_dep (con_dts_p);
        nq_container_dts_destroy(con_dts_p);
    }
    else
        *r_con_dts = con_dts_p;

    nq_thread_pool_destroy(thread_pool);

    return 0;
}

int nq_traversal_get_level_dep (container_dts * con_dts_p)
{
    nq_container_second_adjust_level(con_dts_p);

    int i, j; 
    j = 0;
    int level = con_dts_p->cur_level;

    for(j = 0; j <= level; j++)
    {
        printf("level: %4d -> ", j);
        for(i = 0; i < con_dts_p->cur_top; i++)
        {
            if (con_dts_p->pkgtab[i].level == j)
                printf("%s ", con_dts_p->pkgtab[i].pkgname);
        }
        printf("\n");
    }
    return 0;
}

static int nq_traversal_raw_add_to_raw_node_list(char * name, unsigned int con_offset, 
        unsigned int level, pkg_node ** target_list, pkg_node ** free_node_list)
{
    pkg_node * r_val; 

    if (free_node_list && *free_node_list)
    {
        r_val = *free_node_list;
        *free_node_list = r_val->next;
    }
    else
    {
        r_val = (pkg_node *)malloc(sizeof(pkg_node));
        if (!r_val)
        {
            nq_errmsg("malloc failed");
            return -1;
        }
    }
        
    r_val->name = name;
    r_val->con_offset = con_offset;
    r_val->level = level;

    r_val->child = NULL;

    r_val->next = *target_list;
    if (target_list && *target_list)
        (*target_list)->prev = r_val->prev;

    *target_list = r_val;


    return 0;
}


int nq_traversal_get_pkg_depend(pkg_node * parent_node)
{
    if (!parent_node)
        return -1;

    return get_depend_func(parent_node->name, &parent_node->child);
}


static int nq_traversal_reclaim_pkg_node(pkg_node * del_node, pkg_node ** free_list)
{
    if (!del_node)
        return 0;

    memset(del_node, 0, sizeof(pkg_node));

    del_node->next = *free_list; 

    if(free_list && *free_list)
        (*free_list)->prev = del_node;

    *free_list = del_node;

    return 0;

}



int nq_traversal_get_cricle_dep (container_dts * con_dts_p)
{
    assert(con_dts_p);

    nqstack pkgstack;

    memset(&pkgstack, 0xFF, sizeof(nqstack));
    pkgstack.size = con_dts_p->cur_top + 1;
    pkgstack.top = 0;

    pkg_dts * pkgtab_p = con_dts_p->pkgtab;

    nq_traversal_dep_recursion_check(pkgtab_p, 0, &pkgstack);

    return 0;
}


static void nq_traversal_dep_recursion_check(pkg_dts * pkgtab_p, unsigned int root_pkg_off, struct _nqstack * pkgstack_p)
{
    pkg_rs_node * child;
    pkg_dts * root_pkg = &pkgtab_p[root_pkg_off];

    child = root_pkg->child;

    if (nqstack_push (pkgstack_p, pkgtab_p, root_pkg_off) == -1)
    {
        nqstack_show (pkgstack_p, pkgtab_p);
        nqstack_pop (pkgstack_p, pkgtab_p);
        return ;
    }

    while (child)
    {
        nq_traversal_dep_recursion_check(pkgtab_p, child->data, pkgstack_p);
        child = child->next;
    }

    nqstack_pop (pkgstack_p, pkgtab_p);
}

/*
 *  Return: 
 *      -2 : Push error.
 *      -1 : Push success, and is the 2rd time push.(So, it's a circle!)
 *      >=0 : Push success, return its position within the stack.
 */

static int nqstack_push(nqstack * pkgstack_p, pkg_dts * pkgtab_p, unsigned int pkg_off)
{
    int r_val = -2;

    if (pkgstack_p->top >= pkgstack_p->size)
    {
        nq_errmsg("Stack overflow");
        return r_val;
    }

    r_val = pkgstack_p->top;

    if (pkgtab_p[pkg_off].first_stack != -1)
    {
        pkgtab_p[pkg_off].second_stack = r_val; 
        r_val = -1;
    }
    else
    {
        pkgtab_p[pkg_off].first_stack = r_val;
    }

    pkgstack_p->stack[pkgstack_p->top] = pkg_off;
    pkgstack_p->top++;

    return r_val;
}

/*
 *  Return: 
 *      -1 : Pop error.
 *      >=0 : Pop success, return the pop position.
 */

static int nqstack_pop (nqstack * pkgstack_p, pkg_dts * pkgtab_p)
{
    int r_val = -1;
    unsigned int ipkgtab;

    pkgstack_p->top--;
    r_val = pkgstack_p->top;
    ipkgtab = pkgstack_p->stack[r_val];


    if (pkgstack_p->top < 0)
    {
        nq_errmsg("Stack underflow");
        return -1;
    }

    if (pkgtab_p[ipkgtab].second_stack != -1)
    {
        pkgtab_p[ipkgtab].second_stack = -1;
    }
    else
    {
        pkgtab_p[ipkgtab].first_stack = -1;
    }

    return r_val;
}


static void nqstack_show (nqstack * pkgstack_p, pkg_dts * pkgtab_p)
{
    unsigned int istack;
    unsigned int ipkgtab;
    unsigned int dup_item_off;
    int is_stdout;

    dup_item_off = pkgstack_p->stack[pkgstack_p->top - 1];
    is_stdout = isatty(1);

    for (istack = 0; istack < pkgstack_p->top - 1; istack++)
    {
        ipkgtab = pkgstack_p->stack[istack];
        if (is_stdout && (dup_item_off == ipkgtab))
            printf("\e[1;31m%s\e[0m <- ", pkgtab_p[ipkgtab].pkgname);
        else
            printf("%s <- ", pkgtab_p[ipkgtab].pkgname);
    }

    if (is_stdout)
        nq_normsg("\e[1;31m%s\e[0m", pkgtab_p[pkgstack_p->stack[istack]].pkgname);
    else
        nq_normsg("%s", pkgtab_p[pkgstack_p->stack[istack]].pkgname);
}


