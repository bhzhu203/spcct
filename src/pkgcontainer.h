/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#ifndef PKG_CONTAINER_H__
#define PKG_CONTAINER_H__


#if !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif 

#include <search.h>

#include "pkgdts.h"

typedef struct _container_dts container_dts;

struct _container_dts  {

    struct hsearch_data htab;

    unsigned int cur_top; /* Point to the PKG that last insert forever */
    unsigned int cur_level; /* 当前依赖嵌套层数, 0~ */
    pkg_dts * pkgtab;
};


/* 目前实际不到 1W 个包,不过占用空间不大 可预留 */
#if !defined(PKG_NUM)
#   define PKG_NUM 10000
#endif 

#if !defined(PKG_CONTAINER_LEN)
#   define PKG_CONTAINER_LEN PKG_NUM * sizeof(pkg_dts);
#endif 

#if !defined(NEL_SIZE)
#   define NEL_SIZE PKG_NUM
#endif 




container_dts * nq_container_dts_alloc (unsigned long int len);

/* 成功则返回其插入位置: 即 con_dts_p->pkgtab 内存区的偏移 */
int nq_container_dts_insert (char * pkg_name, container_dts * con_dts_p, unsigned int parent);

void nq_container_dts_destroy(container_dts * con_dts_p);

void nq_container_second_adjust_level(container_dts * con_dts_p);


#endif /* HASH_TABLE_H__ */





