/**
 * @(#) $Id$
 * Gwyddion plug-in helper.
 * Written by Yeti <yeti@gwyddion.net>.  Public domain.
 **/
#ifndef __GWYDDION_PLUGIN_HELPER_H__
#define __GWYDDION_PLUGIN_HELPER_H__

#include <string>
#include <iostream>

class PluginAction {
    public:
    std::string name;              /* action name: register, load, run, ... */
    int arg_num;                   /* number of arguments NOT including
                                      program name, nor action name */
    bool (*action)(char *args[]);  /* action itself, will get array of
                                      arg_num program argument, NOT including
                                      program name, nor action name */

    bool check(int argc, char *argv[]) { return argc == arg_num + 2
                                                && name.compare(argv[1]) == 0; }
};

/* Find action and run it */
static bool
run_action(int nactions, PluginAction *actions,
           int argc, char *argv[])
{
    unsigned int i;

    for (i = 0; i < nactions; i++) {
        if (actions[i].check(argc, argv)) {
            actions[i].action(argv + 2);
            return true;
        }
    }
    std::cerr << "Plug-in has to be called from Gwyddion plugin-proxy."
              << std::endl;
    return false;
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
