/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */

#ifndef PKG_DATA_STRUCT_H__

#define PKG_DATA_STRUCT_H__

typedef struct _pkg_dts         pkg_dts;
typedef struct _raw_node        raw_node;
typedef struct _pkg_node        pkg_node;
typedef struct _pkg_list        pkg_list;
typedef struct _pkg_rs_node     pkg_rs_node;
typedef struct _pkg_stack       pkg_stack;

struct _pkg_dts {

    unsigned char active : 1; 
    char * pkgname;

    pkg_rs_node *ancestor;  
    pkg_rs_node *child;   /* 分别其为依赖关系上的祖宗及子孙, 只分 2 代! */

    unsigned int level;     /* 依赖层数， 搜索的包为 0, 其子依赖逐层加 1 */

    /* ------for Circular-dependencies checking-------- */

    int first_stack;
    int second_stack;
};
    
/* PKG RelationShip node */
struct _pkg_rs_node {
    unsigned int data;  /* Just hold the offset */
    union {
        struct _pkg_rs_node *next; /* for Children */
        struct _pkg_rs_node *prev; /* for Ancestors */
    };
};

/* ------------------------------------------------------------- */

struct _raw_node {
    char * name;
    struct _raw_node * prev;
    struct _raw_node * next;
};

struct _pkg_node {

    char * name;
    unsigned int con_offset;
    unsigned int level;
    struct _raw_node * child;

    struct _pkg_node * next;
    struct _pkg_node * prev;

};

struct _pkg_list {


    struct _pkg_node * raw_list;
    struct _pkg_node * raw_list_tail;

    struct _pkg_node * pdt_list;
    struct _pkg_node * pdt_list_tail;
};


raw_node * nq_raw_node_alloc(char * pkg_name);

pkg_node * nq_pkg_node_alloc(char * pkg_name);


#endif  /* PCK_DATA_STRUCT_H__ */


