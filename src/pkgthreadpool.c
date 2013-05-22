
/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "pkgthreadpool.h"
#include "nqcommon.h"
#include "pkgtraversal.h"


static void * pkg_thread_get_rdeps (void *);
static int nq_thread_pool_check_value(nq_thread_pool *);

nq_thread_pool   * nq_thread_pool_create ()
{
    nq_debug("Initial thread pool");
    nq_thread_pool * r_pool = NULL;
    int loop_cnt;
    void * r_mem_ptr;

    r_mem_ptr = malloc(sizeof(nq_thread_pool) + sizeof(pthread_t) * THREAD_NUM);
    memset(r_mem_ptr, 0, sizeof(nq_thread_pool) + sizeof(pthread_t) * THREAD_NUM);

    if (!r_mem_ptr)
        return NULL;

    r_pool = (nq_thread_pool *)r_mem_ptr;
    r_pool->ptid = (pthread_t *)(r_pool+1); /* A trick, for simplifying code */

    pthread_mutex_init(&r_pool->pdt_mutex, NULL);
    pthread_mutex_init(&r_pool->raw_mutex, NULL);

    sem_init(&r_pool->pdt_sem, 0, 0); 

    if((r_pool->msgid = msgget(IPC_PRIVATE, 0600 | IPC_CREAT | IPC_EXCL)) < 0) 
    {
        nq_errmsg("msgget: %s", strerror(errno));
        r_pool = NULL;
        goto ERROR_RET_1;
    }

    nq_debug("Create threads..");

    int pid_index = 0;

    for (loop_cnt = 0; loop_cnt < THREAD_NUM; loop_cnt++)
    {
        if (pthread_create(&r_pool->ptid[pid_index], NULL, pkg_thread_get_rdeps,  r_pool) != 0)
        {
            nq_errmsg("pthread_create");
        }
        else
        {
            ++pid_index;
        }
    }
    r_pool->threads = pid_index;

    goto FUNC_RET;

ERROR_RET_1:

    if (pthread_mutex_destroy(&r_pool->pdt_mutex) != 0)
        nq_errmsg("pthread_mutex_destroy");

    if (pthread_mutex_destroy(&r_pool->raw_mutex) != 0)
        nq_errmsg("pthread_mutex_destroy");

    if(sem_destroy(&r_pool->pdt_sem) < 0)
        nq_errmsg("sem_destroy: %s", strerror(errno));

    free(r_mem_ptr);

FUNC_RET:

    return r_pool;
}



static void * pkg_thread_get_rdeps (void * data)
{
    int r_val;
    msg_node r_msg;

    pkg_node * raw_node;
    nq_thread_pool * thread_pool;

    thread_pool = (nq_thread_pool *)data;

    while(1)
    {
        if(msgrcv(thread_pool->msgid, &r_msg, sizeof(POOL_CMD), 0, 0) < 0)
        {
            nq_errmsg("msgrcv: %s", strerror(errno));
            continue ;
        }

        if (r_msg.cmd == EXIT)
            break;

        nq_debug("received EXEC message");
        pthread_mutex_lock(&thread_pool->raw_mutex);

        if (thread_pool->raw_rear)
        {
            nq_debug("starting take one raw material");

            raw_node = thread_pool->raw_rear;

            if (thread_pool->raw_head == thread_pool->raw_rear)
            {
                thread_pool->raw_head = NULL;
                thread_pool->raw_rear = NULL;
            }
            else
            {

                thread_pool->raw_rear = raw_node->prev;
                thread_pool->raw_rear->next = NULL;
            }

            pthread_mutex_unlock(&thread_pool->raw_mutex);
        }
        else
        {
            nq_errmsg("Null raw node ? Something about thread synchronization error had occured!");
            pthread_mutex_unlock(&thread_pool->raw_mutex);
            continue ;
        }

        /* get dependencies */
        r_val = nq_traversal_get_pkg_depend(raw_node);

        if (r_val < 0)
        {
            nq_errmsg("get \"%s\" 's dependencies failed", raw_node->name); 
            raw_node->child = NULL; /* Even though failed, we have to let the main thread to free its memory */
        }

        pthread_mutex_lock(&thread_pool->pdt_mutex);

        /* Add to the list head. */
        raw_node->next = thread_pool->pdt_head;

        if (thread_pool->pdt_head)
        {
            thread_pool->pdt_head->prev = raw_node;
        }
        else
        { 
            thread_pool->pdt_rear = raw_node;
        }

        thread_pool->pdt_head = raw_node;

        pthread_mutex_unlock(&thread_pool->pdt_mutex);

        sem_post(&thread_pool->pdt_sem);
    }

    return NULL;
}


pkg_node * nq_thread_pool_get_product (nq_thread_pool * thread_pool)
{
    nq_debug("get product");
    /* Get product from list tail */
    pkg_node * r_pkg_node;

    sem_wait(&thread_pool->pdt_sem);
    pthread_mutex_lock(&thread_pool->pdt_mutex);

    r_pkg_node = thread_pool->pdt_rear;

    if (r_pkg_node)
    {
        if (thread_pool->pdt_head == thread_pool->pdt_rear)
        {
            thread_pool->pdt_head = NULL;
            thread_pool->pdt_rear = NULL;
        }
        else 
        {

            thread_pool->pdt_rear = r_pkg_node->prev;
            r_pkg_node->prev->next = NULL;
        }

        /*
         r_pkg_node->next = NULL;
         r_pkg_node->prev = NULL;
         */
    }
    else
    {
        nq_errmsg("Null product node ? Something thread synchronization error had occured!");
    }

    pthread_mutex_unlock(&thread_pool->pdt_mutex);

    return r_pkg_node;
}

void nq_thread_pool_add_raw_material(nq_thread_pool * thread_pool, pkg_node * raw_node)
{ 
    nq_debug("add one raw material");
    /* Add to the list head; */
    msg_node exec_cmd = {
        .mtype = 1,
        .cmd = EXEC
    };

    pthread_mutex_lock(&thread_pool->raw_mutex);

    raw_node->prev = NULL;
    raw_node->next = thread_pool->raw_head;

    if (raw_node->next)
        raw_node->next->prev = raw_node;
    thread_pool->raw_head = raw_node;

    if (!thread_pool->raw_rear)
        thread_pool->raw_rear = raw_node;

    pthread_mutex_unlock(&thread_pool->raw_mutex);

    /* 使用 SysV message queue 通知  */
    msgsnd(thread_pool->msgid, &exec_cmd, sizeof(POOL_CMD), 0);
}

int nq_thread_pool_destroy (nq_thread_pool * thread_pool)
{
    int loop_cnt;
    msg_node exit_cmd = {
        .mtype = 1,
        .cmd = EXIT
    };

    nq_debug("nqthreadpool destroy...");

    nq_thread_pool_check_value(thread_pool);

    for (loop_cnt = 0; loop_cnt < thread_pool->threads; loop_cnt++)
        msgsnd(thread_pool->msgid, &exit_cmd, sizeof(POOL_CMD), 0);

    for (loop_cnt = 0; loop_cnt < thread_pool->threads; loop_cnt++)
        pthread_join(thread_pool->ptid[loop_cnt], NULL);


    pthread_mutex_destroy(&thread_pool->pdt_mutex);
    pthread_mutex_destroy(&thread_pool->raw_mutex);
    sem_destroy(&thread_pool->pdt_sem);

    msgctl(thread_pool->msgid, IPC_RMID, NULL);

    free(thread_pool);
    thread_pool = NULL;

    return 0;
}

static int nq_thread_pool_check_value(nq_thread_pool * thread_pool)
{
    nq_debug("Check thread pool value ...");

    if(thread_pool->pdt_head || thread_pool->raw_head)
    {
        nq_debug("raw & pdt list not null");
        return 1;
    }

    if (pthread_mutex_trylock(&thread_pool->pdt_mutex))
    {
        nq_debug("There are products wait to be read.");
        pthread_mutex_unlock(&thread_pool->pdt_mutex);
        return 1;
    }
    pthread_mutex_unlock(&thread_pool->pdt_mutex);

    if (pthread_mutex_trylock(&thread_pool->raw_mutex))
    {
        nq_debug("There are raw materials wait to be processed.");
        pthread_mutex_unlock(&thread_pool->raw_mutex);
        return 1;
    }
    pthread_mutex_unlock(&thread_pool->raw_mutex);

    return 0;
}
