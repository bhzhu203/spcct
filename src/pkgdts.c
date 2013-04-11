/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#include <stdlib.h>
#include <string.h>

#include "pkgdts.h"


raw_node * nq_raw_node_alloc(char * pkg_name)
{
    raw_node * r_val = (raw_node *)malloc(sizeof(raw_node));
    
    if(!r_val)
        return NULL;

    r_val->name = pkg_name;
    r_val->prev = NULL;
    r_val->next = NULL;

    return r_val;
}

pkg_node * nq_pkg_node_alloc(char * pkg_name)
{
    pkg_node * r_val = (pkg_node *)malloc(sizeof(pkg_node));
    
    if(!r_val)
        return NULL;

    memset(r_val, 0, sizeof(pkg_node));

    r_val->name = pkg_name;
    r_val->child = NULL;
    r_val->prev = NULL;
    r_val->next = NULL;

    return r_val;

}
