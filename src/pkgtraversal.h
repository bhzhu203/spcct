/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */


#ifndef PKG_TRAVERSAL_H__
#define PKG_TRAVERSAL_H__

#include "pkgcontainer.h"
#include "pkgdts.h"


/*
 * proto type of get_depend_func:
 *  int get_depend_func(char * pkgname, pkg_dts_raw ** dep_list);
 *     @pkgname:  PKG's name, 
 *     @dep_list: The list of pkgname's dependencies(I will free dep_list)
 *  return :
 *      -1: Error
 *       0: Success
 *  
 *  Note: This function must be reentrant, since multi-threads...
 */

void nq_traversal_set_get_depend_func(int (* func)(char *, raw_node **));

int nq_traversal_all_dependence(char *pkg_name, container_dts **con_dts_p);

int nq_traversal_get_cricle_dep (container_dts * con_dts_p);

int nq_traversal_get_level_dep (container_dts * con_dts_p);

int nq_traversal_get_pkg_depend(pkg_node * parent_node);


#endif /* PKG_TRAVERSAL_H__ */
