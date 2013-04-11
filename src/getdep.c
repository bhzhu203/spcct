/*
 * This file is part of the spcct and licensed under the GNU General Public License.
 * http://www.gnu.org/licenses/gpl.txt
 */

#if !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif 

#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <regex.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>


#include "pkgtraversal.h"
#include "pkgcontainer.h"
#include "pkgdts.h"
#include "nqcommon.h"

/* #define DEPCMD "/path/to/get-dep.py" */

char * depcmd = NULL;


#if !defined(MYNAME)
#   define MYNAME "spcct"
#endif 


#define OPT_ERROR      0x01
#define OPT_CHKLEVEL   0x02
#define OPT_CHKCIRCLE  0x04
#define OPT_SHOWHELP   0x08
#define OPT_DEPCMD     0x10


#ifndef MAX_LINE
#   define MAX_LINE  1024
#endif 


static int get_depend_func(char *, raw_node **);

static int nq_regex_get_match(char * s, char * pattern, int cflags, int eflags, 
        size_t nmatch, regmatch_t * offset);


static int get_depend_func(char * name, raw_node ** list)
{
    int r_val;
    int n;

    FILE * fp;

    char * fstr;
    char * new_fstr;

    char * child_name;

    regmatch_t * m_offset;

    char getdep_cmd[MAX_LINE];

    raw_node * r_list = NULL;
    raw_node * raw_node_tmp;


    snprintf(getdep_cmd, MAX_LINE, "%s %s", depcmd, name);

    if (!(fp = popen(getdep_cmd, "r")))
    {
        nq_errmsg("open %s: %s", name, strerror(errno));
        return -1;
    }

  
    m_offset = (regmatch_t *)malloc(sizeof(regmatch_t));

    if(!m_offset)
    {
        nq_errmsg("malloc: failed");
        r_val = -1;
        goto RETURN_1;
    }

    n = 2;
    while(1)
    {
        rewind(fp);

        fstr = (char *)malloc(MAX_LINE * n);
        if (!fstr)
        {
            nq_errmsg("malloc: failed.  Can't get all dependencies");
            r_val = -1;
            goto RETURN_2;
        }
        memset(fstr, 0, MAX_LINE * n);

        if ((r_val = fread(fstr, 1, MAX_LINE * n, fp)) < MAX_LINE * n)
        {
            if (ferror(fp))
            {
                nq_errmsg("fread");
                r_val = -1;
                goto RETURN_3;
            }
            
            if (feof(fp))
            {
                break ;
            }
        }

        else if (!feof(fp))
        {
            clearerr(fp);
            free(fstr);
            ++n;
            continue ; /* Continue read pipe */
        }

        else
        { /* OK, if it can execute here, I don't know what happened */
            break;
        }

    }

    if (strlen(fstr) <= 0)
    {
        r_val = 0;
        goto RETURN_3;
    }

    new_fstr = fstr;

    do 
    {
        char tmp_char;

        if(nq_regex_get_match(new_fstr, "\\b[a-zA-Z0-9+._-]*\\b", 0, 0, 1, m_offset) != 0)
            break;

        child_name = (char *)malloc(m_offset[0].rm_eo - m_offset[0].rm_so + 1);
        if (!child_name)
        {
            goto OUTPUT_MSG;
        }

        snprintf(child_name, m_offset[0].rm_eo - m_offset[0].rm_so + 1, "%s", &new_fstr[m_offset[0].rm_so]); 

        raw_node_tmp = nq_raw_node_alloc(child_name);

        if (!raw_node_tmp)
        {
            free(child_name);
            goto OUTPUT_MSG;
        }

        raw_node_tmp->next = r_list;
        if (r_list)
        {
            r_list->prev = raw_node_tmp;
        }
        r_list = raw_node_tmp;

        goto NEXT_LOOP;

        
OUTPUT_MSG:
        tmp_char = new_fstr[m_offset[0].rm_eo];
        new_fstr[m_offset[0].rm_eo] = '\0';
        nq_warnmsg("%s: lost its child: %s ", name, &new_fstr[m_offset[0].rm_so]);
        new_fstr[m_offset[0].rm_eo] = tmp_char;
NEXT_LOOP:
        new_fstr += m_offset[0].rm_eo;

    } while(1);


    *list = r_list;

RETURN_3:
    free(fstr);
RETURN_2:
    free(m_offset);
RETURN_1:
    pclose(fp);

    return r_val;
}


static int nq_regex_get_match(char * s, char * pattern, int cflags, int eflags, 
        size_t nmatch, regmatch_t * offset)
{
    int r_val;
    char errbuff[1024];
    regex_t regex;

    r_val = regcomp(&regex, pattern, cflags);
    if (r_val)
    {
        regerror(r_val, &regex, errbuff, 1024);
        nq_errmsg("regcomp: %s", errbuff);
        regfree(&regex);  /* I am not very sure whether must do it or not here */
        return -1;
    }

    r_val = regexec(&regex, s, nmatch, offset, eflags);
    if (r_val == REG_NOMATCH)
    {
        nq_debug("Not match"); 
        regfree(&regex); 
        return 1;
    }
    
    regfree(&regex); 
    return 0;
}


int main(int argc, char *argv[])
{
    int opt;
    container_dts *con_dts_p = NULL;

    unsigned int cmd = 0;

    struct option options[] = {
        { "circle", no_argument, NULL, 'c' },
        { "level", no_argument, NULL, 'l' },
        { "cmd", required_argument, NULL, 'C' },
        { "help", no_argument, NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    while (1)
    {
        opt = getopt_long(argc, argv, "C:clh", options, NULL);

        if (opt < 0)
            break ;

        switch (opt)
        {
            case 'c':
                cmd |= OPT_CHKCIRCLE;
                break ;

            case 'l':
                cmd |= OPT_CHKLEVEL;
                break;

            case 'C':
                cmd |= OPT_DEPCMD;
                if (access(optarg, X_OK) < 0)
                {
                    nq_errmsg("%s: %s", optarg, strerror(errno));
                    cmd |= OPT_ERROR;
                }
                else
                {
                    depcmd = optarg;
                }
                break;

            case 'h':
                cmd |= OPT_SHOWHELP;
                break;

            default:
                cmd |= OPT_ERROR;
                break;
        }
    }


    if (cmd & OPT_SHOWHELP)
    {
        nq_normsg("\n\
  "MYNAME"  A package dependencies checking tool for StartOS. \n\n\
  Usage\n\
          "MYNAME"  [options]  pkg1 [pkg2 ...]      \n\
          pkg1 [pkg2 ...] means the packages' name which need to be checked. \n\n\
  Options\n\
          -C, --cmd /path/to/depdendencies_command \n\
                   This option is required, and must also specify an argument, "MYNAME" uses the\n\
                   argument as the external tool to obtains pkg's dependencies. \n\
          -l, --level \n\
                   Checking the level of pkg's dependencies, and output. \n\
          -c, --circle \n\
                   Checking the circular in pkg's dependencies, and output. \n\
          -h, --help \n\
                   Show this message.\n\n\
  Copyright (C) 2012-2013 Dongguan Vali Network Technology Co.,Ltd.\n\
  License: GPL   \n\
  Tseesing <chen-qx@live.cn>\n\
  Bug report, suggestion, etc. to: osbug@ivali.com\n\n");
      
        return 0;
    }

    if ((cmd & OPT_ERROR) || (~cmd & OPT_DEPCMD))
    {
        fprintf(stderr, "\nUsage: " MYNAME " -C <depcmd> [-l] [-c] [-h] pkg1 [pkg2 ...]\n\n");
        return 1;
    }

    nq_traversal_set_get_depend_func(get_depend_func);

    cmd |= (cmd & OPT_CHKCIRCLE) ? cmd : OPT_CHKLEVEL;

    while (optind < argc)
    {
        /* nq_traversal_all_dependence(argv[optind], NULL); */

        if (nq_traversal_all_dependence(argv[optind], &con_dts_p) == 0)
        {
            if (cmd & OPT_CHKLEVEL)
                nq_traversal_get_level_dep(con_dts_p);

            if (cmd & OPT_CHKCIRCLE)
                nq_traversal_get_cricle_dep(con_dts_p);

            nq_container_dts_destroy(con_dts_p);
            con_dts_p = NULL;
        }
        else
        {
            nq_errmsg("%s: checking error!",argv[optind]);
        }

        optind++;
    }

    return 0;
}

