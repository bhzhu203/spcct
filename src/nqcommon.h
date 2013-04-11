/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#ifndef NQ_COMMON_H__
#define NQ_COMMON_H__



#include <stdio.h>
#include <stdlib.h>



int nq_set_fd_nonblocking(int fd);



#ifdef NQDEBUG

#define nq_errmsg(fmt, args...) \
    do {\
        fprintf(stderr, "Error:  %s:%d " fmt "\n", __FILE__, __LINE__, ##args); \
    } while(0)

#define nq_errmsg_exit(fmt, args...) \
    do {\
        fprintf(stderr, "Error: %s:%d " fmt "\n", __FILE__, __LINE__, ##args); \
        exit(1); \
    } while(0)

#define nq_warnmsg(fmt, args...) \
    do {\
        fprintf(stderr, "Warning: %s:%d " fmt "\n", __FILE__, __LINE__, ##args); \
    } while(0)

#define nq_normsg(fmt, args...) \
    do {\
        fprintf(stdout, fmt "\n", ##args); \
    } while(0)

#define nq_debug(fmt, args...) \
    do {\
        fprintf(stdout, "Debug: %s:%d " fmt "\n", __FILE__, __LINE__, ##args); \
    } while(0)


#else

#include <stdarg.h>

void nq_errmsg_exit(char *fmt, ...);
void nq_warnmsg(char *fmt, ...);
void nq_errmsg(char *fmt, ...);
void nq_normsg(char *fmt, ...);

#define nq_debug(fmt, args...) \
    do {\
    } while(0)

#endif /* NQDEBUG */


off_t nq_get_file_length(char * path);

#endif /* NQ_COMMON_H__ */
