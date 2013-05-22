/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#ifndef PKG_THREAD_POOL_H__
#define PKG_THREAD_POOL_H__


#include <pthread.h>
#include <semaphore.h>

#include "pkgdts.h"


#if !defined(THREAD_NUM)
#   define THREAD_NUM 10
#endif /* */

/* --------------------------------- */

typedef struct msg_node_    msg_node;
typedef struct nq_thread_pool_  nq_thread_pool;


typedef enum {
    EXIT = 1,
    EXEC
}POOL_CMD;

struct msg_node_ {
    long mtype;
    POOL_CMD cmd; /* EXEC or EXIT */
};

struct nq_thread_pool_ {

    sem_t   pdt_sem;
    int     msgid;

    int     threads;

    pthread_mutex_t   pdt_mutex;
    pkg_node * pdt_head;
    pkg_node * pdt_rear;

    pthread_mutex_t   raw_mutex;
    pkg_node * raw_head;
    pkg_node * raw_rear;

    pthread_t * ptid;
};

/* --------------------------------- */



nq_thread_pool * nq_thread_pool_create (void);
int              nq_thread_pool_destroy (nq_thread_pool *);
void             nq_thread_pool_add_raw_material(nq_thread_pool * , pkg_node *);
pkg_node *       nq_thread_pool_get_product (nq_thread_pool *);

#endif /* PKG_THREAD_POOL_H__  */





