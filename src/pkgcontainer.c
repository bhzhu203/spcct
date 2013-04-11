/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#if !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif 

#include <search.h>

#include <string.h>
#include <stdlib.h>

#include "pkgcontainer.h"
#include "nqcommon.h"


typedef enum {
    CHILD,
    PARENT
} OPTYPE ;

static int nq_container_pkg_add_rs (pkg_rs_node ** node, unsigned int index, OPTYPE optype);

static void nq_container_pkg_dts_free_rs(pkg_dts *node);

static int nq_container_add_rs_info(pkg_dts * pkgtab_p, unsigned int parent, 
        unsigned int child);

/*
static int nq_container_update_pkg_level (container_dts * con_dts_p, 
        unsigned int index, unsigned int parent);

static int nq_container_is_dep_mutual(pkg_dts * pkgtab_p, unsigned int parent, 
        unsigned int child);

static int nq_container_child_is_parent (pkg_dts * pkgtab_p, unsigned int parent, 
        unsigned int child);
*/

static int nq_container_parent_is_child (pkg_dts * pkgtab_p, unsigned int parent, 
        unsigned int child);

static int nq_container_first_adjust_level(pkg_dts * pkgtab_p, unsigned int new_parent, 
        unsigned int child);

/*
 * @len: Packages * (struct _pkg_dts)
 */
container_dts * nq_container_dts_alloc (unsigned long len)
{
    container_dts * con_dts_p;
    pkg_dts       * pkg_dts_p;
    struct hsearch_data * htab_p;

    con_dts_p = (container_dts *)malloc(sizeof(struct _container_dts));
    if (!con_dts_p)
    {
        nq_errmsg("malloc");
        return NULL;
    }


    /* 使用数组方式,可快速分配,避免逐个软件包进行 malloc, free ... */

    if (len <= sizeof(struct _pkg_dts) * PKG_NUM)
        len = PKG_CONTAINER_LEN;

    pkg_dts_p = (pkg_dts *)malloc(len);

    if (!pkg_dts_p)
    {
        nq_errmsg("malloc");
        free(con_dts_p);
        return NULL;
    }

    memset(pkg_dts_p, 0, len);
    memset(con_dts_p, 0, sizeof(struct _container_dts));
    htab_p = &con_dts_p->htab;

    /* hash table */

    if(hcreate_r(NEL_SIZE, htab_p) == 0)
    {
        nq_errmsg("hcreate_r");
        free(con_dts_p);
        free(pkg_dts_p);
        return NULL;
    }

    con_dts_p->pkgtab = pkg_dts_p;

    return con_dts_p;
}

int nq_container_dts_insert (char * pkg_name, container_dts * con_dts_p, unsigned int parent)
{
    struct hsearch_data *htab_p;
    pkg_dts * pkgtab_p;
    ENTRY htab_item;
    ENTRY * r_htab_item_p = NULL;
    unsigned int index;

    int r_val;

    if (!(pkg_name && con_dts_p))
    {
        nq_errmsg("Invalid PKG name and Container Pointer found");
        return -1;
    }

    htab_p   = &con_dts_p->htab;
    pkgtab_p = con_dts_p->pkgtab;

    htab_item.key = pkg_name;

    if (hsearch_r(htab_item, FIND, &r_htab_item_p, htab_p) != 0)
    {
        nq_debug("PKG: %s have beed traversed", pkg_name);

#if defined(__amd64__) || defined(__x86_64__)
        index = (unsigned int)((unsigned long)(r_htab_item_p->data) & 0xFFFFFFFF);
#else
        index = (unsigned int)(r_htab_item_p->data);
#endif
        nq_container_add_rs_info(pkgtab_p, parent, index);

        /*
        if (!((index == 0) || nq_container_parent_is_child (pkgtab_p, parent, index)))
            nq_container_first_adjust_level(pkgtab_p, parent, index);
        */

        return 0;
    }

    if(con_dts_p->cur_top == PKG_NUM)
    {
        /* Impossible at present! Just for robust */ 
        nq_errmsg("Critical: Maybe dependent relationship too deep to handle, results from now on have been unreliable!!");
        return -1;
    }

    index = con_dts_p->cur_top;
    con_dts_p->cur_top += 1;

    pkgtab_p[index].active = 1;
    pkgtab_p[index].pkgname = pkg_name;

    pkgtab_p[index].first_stack  = -1;
    pkgtab_p[index].second_stack = -1;

#if defined(__amd64__) || defined(__x86_64__)
    htab_item.data = (void *)((unsigned long)index & 0xFFFFFFFF); 
#else
    htab_item.data = (void *)index;
#endif
    r_val = hsearch_r (htab_item, ENTER, &r_htab_item_p, htab_p);
    if(r_val == 0)
    {
        nq_errmsg("Critical: Maybe dependent relationship too deep to handle, results from now on have been unreliable!!");
        return -1;
    }

    if (index != 0)
    {
        nq_container_add_rs_info(pkgtab_p, parent, index);
        pkgtab_p[index].level = pkgtab_p[parent].level + 1; /* 当前依赖层数, 0 ~ n */
    }

    return index;
}

static int nq_container_add_rs_info(pkg_dts * pkgtab_p, unsigned int parent, 
        unsigned int child)
{
    int r_val;

    if ((r_val = nq_container_pkg_add_rs(&pkgtab_p[child].ancestor, parent, PARENT)) < 0)
        nq_warnmsg("Add RelationShip: \"%s\" will not add \"%s\" to its Ancestors list!", 
                pkgtab_p[child].pkgname, pkgtab_p[parent].pkgname);

    if ((r_val = nq_container_pkg_add_rs(&pkgtab_p[parent].child, child, CHILD)) < 0)
        nq_warnmsg("Add RelationShip: \"%s\" will not add \"%s\" to its Children list!", 
                pkgtab_p[parent].pkgname, pkgtab_p[child].pkgname);

    return r_val;
}

/*

static int nq_container_update_pkg_level (container_dts * con_dts_p, 
        unsigned int index, unsigned int parent)
{

    pkg_dts * me;
    pkg_dts * new_parent_node; 
    pkg_dts * pkgtab_p;

    pkg_rs_node * child;

    unsigned int level;
    unsigned int new_level;
    unsigned int level_dis;

    pkgtab_p = con_dts_p->pkgtab;

    new_parent_node = &pkgtab_p[parent];
    me              = &pkgtab_p[index];

    level            = me->level;
    new_level = new_parent_node->level + 1;


    // 父跟子没有相互依赖  并且  新级别比旧级别要往下 
    if (!nq_container_is_dep_mutual(pkgtab_p, parent, index) && (new_level > level))
    {
        level_dis = new_level - level;
        me->level = new_level;

        child = me->child;

        while (child)
        {
            pkgtab_p[child->data].level += level_dis;
            child = child->next;
        }
    }

    // 父中添加 子 
    // 子中添加 祖先 

    if (nq_container_pkg_add_rs (&me->ancestor, parent, PARENT) < 0)
    {   
        nq_warnmsg("Add RelationShip: \"%s\" will not add \"%s\" to its Ancestors list!", 
                me->pkgname, new_parent_node->pkgname);
    }   

    if(nq_container_pkg_add_rs (&new_parent_node->child, index, CHILD) < 0)
    {   
        nq_warnmsg("Add RelationShip: \"%s\" will not add \"%s\" to its Children list!", 
                new_parent_node->pkgname, me->pkgname);
    }   

    return 0;
}
*/

/*
 * 检测父子是否相互依赖 
 * 1 是,
 * 0 否
 *
 ***   可在本函数中扩展: 维护一份相互依赖的包的信息
 */
/*
static int nq_container_is_dep_mutual(pkg_dts * pkgtab_p, unsigned int parent, unsigned int child)
{

    pkg_rs_node * parent_ancestor;
    pkg_rs_node * parent_child;

    pkg_rs_node * child_ancestor;
    pkg_rs_node * child_child;

    parent_ancestor = pkgtab_p[parent].ancestor;
    parent_child    = pkgtab_p[parent].child;

    child_ancestor = pkgtab_p[child].ancestor;
    child_child    = pkgtab_p[child].child;


    if (!(parent_child && parent_ancestor && child_child && child_ancestor))
        return 0;

    return 0;

}
*/

static int nq_container_pkg_add_rs (pkg_rs_node ** node, unsigned int index, OPTYPE optype)
{
    pkg_rs_node * lcl_node;

    lcl_node = (pkg_rs_node *)malloc(sizeof(pkg_rs_node));

    if (!lcl_node)
    {   
        nq_errmsg("malloc");
        return -1; 
    }   

    memset(lcl_node, 0, sizeof(pkg_rs_node));
    lcl_node->data = index;

    switch(optype)
    {
        case CHILD:
            lcl_node->next = *node;
            *node = lcl_node;
            break; 

        case PARENT:
            lcl_node->prev = *node;
            *node = lcl_node;
            break;

        default:
            nq_errmsg("Unknow OPTYPE: %d", optype);
            free(lcl_node);
            return -1;
    }
    return 0;
}


void nq_container_dts_destroy(container_dts * con_dts_p)
{
    unsigned int pkg_dts_num;
    unsigned int index;

    pkg_dts_num = con_dts_p->cur_top;

    index = 0;
    con_dts_p->pkgtab[0].pkgname = NULL; /* the PKG name, leave it(the Caller need to free it) */

    while(index < pkg_dts_num)
    {
        nq_container_pkg_dts_free_rs(&con_dts_p->pkgtab[index]);
        ++index;
    }

    free(con_dts_p->pkgtab);
    hdestroy_r(&con_dts_p->htab);
    free(con_dts_p);
}

static void nq_container_pkg_dts_free_rs(pkg_dts *node)
{
    pkg_rs_node *lcl_node;
    pkg_rs_node *lcl_node_f; /* forward */

    lcl_node = node->ancestor;

    while(lcl_node)
    {
        lcl_node_f = lcl_node->prev;
        free(lcl_node);
        lcl_node = lcl_node_f;
    }
    node->ancestor = NULL;

    lcl_node = node->child;
    while(lcl_node)
    {
        lcl_node_f = lcl_node->next;
        free(lcl_node);
        lcl_node = lcl_node_f;
    }
    node->child = NULL;
    
    if (node->pkgname)
        free(node->pkgname);
}

/* 
 * 检查 child 的 child 中是否包含有 parent 
 * Note: 测试中发现有些圆圈依赖会造成第二次结构内级别调整时无限循环的严重问题
 *       因此加强本函数的判断:
 *     从 child 的 child 出发,,一直递归到结束, 中途如果遇到圆圈依赖,
 *     则立即返回 (跟pkgtraversal.c:nq_traversal_dep_recursion_check()
 *     部分功能相似, 不过后者只验直接的那层关系, 本函数也验间接的)
 *  Return: 0: child 与 parent 不在同一圆圈中, 
 *          1: 从child 出发, 遇到了 parent
 */

static int nq_container_parent_is_child (pkg_dts * pkgtab_p, unsigned int parent_off, unsigned int child_off)
{
    pkg_rs_node * child = pkgtab_p[child_off].child;


    if (pkgtab_p[child_off].first_stack != -1)
    {
        pkgtab_p[child_off].first_stack = -1;
        return 0;
    }

    if ((child_off == parent_off) || (child && (child->data == parent_off)))
    {
        return 1;
    }

    pkgtab_p[child_off].first_stack = 0x5A;

    while(child)
    {
        if ((pkgtab_p[child->data].first_stack == -1 ) && nq_container_parent_is_child (pkgtab_p, parent_off, child->data))
        {
            pkgtab_p[child_off].first_stack = -1;
            return 1;
        }

        child = child->next;
    }

    pkgtab_p[child_off].first_stack = -1;

    return 0;
}

/*
static int nq_container_parent_is_child (pkg_dts * pkgtab_p, unsigned int parent_off, unsigned int child_off)
{
    pkg_rs_node * child  = pkgtab_p[child_off].child;

    int r_val = 0;

    while(child)
    {
        if (child->data == parent_off)
        {
            r_val = 1;
            break;
        }
        child = child->next; 
    }

    return r_val;
}
*/

/* 
 * 检查 parent 的 ancestor 中是否包含有 child  ; "浅"检查, 只检查直接的部分, 间接的部分未检查
 */
/*
static int nq_container_child_is_parent (pkg_dts * pkgtab_p, unsigned int parent_off, unsigned int child_off)
{
    pkg_rs_node * parent = pkgtab_p[parent_off].ancestor;

    int r_val = 0;

    while(parent)
    {
        if (parent->data == child_off)
        {
            r_val = 1;
            break;
        }
        parent = parent->prev; 
    }

    return r_val;
}
*/


/*
 * 如果新级别比现在级别高(依赖层更往下), 则调整到新级别
 * Note: 本函数只要执行就是一定调整的, 因此要避免相互依赖
 *       的两个包使用本函数
 */

static int nq_container_first_adjust_level(pkg_dts * pkgtab_p, unsigned int new_parent_off, 
        unsigned int child_off)
{
    pkg_dts * parent = &pkgtab_p[new_parent_off];
    pkg_dts * child  = &pkgtab_p[child_off];
    
    if (child->level >= parent->level + 1)
        return 0;  /* 更靠底, 不调整 */

    child->level = parent->level + 1;
    return child->level;
}

/* 
 *  调整算法, 假设要扫描依赖的包是 A :
 *      1) 按层(级数)扫描, 从第1层开始(第0层是 A )
 *      2) 调整包 P 成为其所有 ancestors 中最底层那个 parent 的子. 即
 *             级别为该 parent 的级别 + 1
 *      3) 若 P 与其 parent 是(直接的或间接的)相互依赖之关系,则不执行 2 .
 *      4) 扫描过程中,随时比较所扫描到的包的 level, 留下大者.作为循环的结束条件
 */

void nq_container_second_adjust_level(container_dts * con_dts_p)
{
    pkg_dts * pkgtab_p;

    pkg_dts * pkg;
    pkg_rs_node * pkg_ancestor;


    unsigned int con_top;

    unsigned int cur_level = 1;
    unsigned int top_level;

    unsigned int pkg_off;

    con_top    = con_dts_p->cur_top;
    top_level  = 1;
    pkgtab_p   = con_dts_p->pkgtab;


    for (; cur_level <= top_level; cur_level++)
    {
        for (pkg_off = 1; pkg_off < con_top; pkg_off++)
        {
            pkg = &pkgtab_p[pkg_off];

            top_level = top_level > pkg->level ? top_level : pkg->level;

            if (pkg->level != cur_level)
                continue ;

            pkg_ancestor = pkg->ancestor;

            while (pkg_ancestor)
            {
                if ( (pkg->level < pkgtab_p[pkg_ancestor->data].level + 1) &&
                     (!nq_container_parent_is_child(pkgtab_p, pkg_ancestor->data, pkg_off)) )
                {
                    if (nq_container_first_adjust_level(pkgtab_p, pkg_ancestor->data, pkg_off))
                    {
                        top_level = top_level > pkg->level ? top_level : pkg->level; 
                            /* 需要及时获取新调整的级别, 可能遇到一个让当前级别实际的少一级的临界值 */
                        break ; 
                    }
                }
                pkg_ancestor = pkg_ancestor->prev;
            }
        }
    }
    
    con_dts_p->cur_level = top_level;
}


