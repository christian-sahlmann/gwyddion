/**
 * @(#) $Id$
 * A very simple Gwyddion plug-in example in C++.
 * Written by Yeti <yeti@gwyddion.net>.  Public domain.
 **/
#include <dump.hh>
#include <plugin-helper.hh>
#include <cfloat>
#include <iostream>

using namespace std;

static bool action_register(char *args[]);
static bool action_run     (char *args[]);

static PluginAction actions[] = {
    { string("register"), 0, &action_register },
    { string("run"),      2, &action_run      },
};

#define NACTIONS sizeof(actions)/sizeof(actions[0])

int
main(int argc, char *argv[])
{
    return !run_action(NACTIONS, actions, argc, argv);
}

static bool
action_register(char *args[])
{
    cout << "invert_cpp" << endl;
    cout << "/_Test/Value Invert (C++)" << endl;
    cout << "noninteractive with_defaults" << endl;
    return true;
}

static bool
action_run(char *args[])
{
    if (string("noninteractive") != args[0]
        && string("with_defaults") != args[0])
        return false;

    Dump dump;
    dump.read(args[1]);

    map<string,DataField>::iterator iter = dump.data.find(string("/0/data"));
    double min = DBL_MAX;
    double max = -DBL_MAX;
    unsigned long int n = iter->second.xres * iter->second.yres;
    double *a = iter->second.data;
    unsigned long int i;
    for (i = 0; i < n; i++) {
        if (a[i] < min)
            min = a[i];
        if (a[i] > max)
            max = a[i];
    }
    double mirror = min + max;
    for (i = 0; i < n; i++)
        a[i] = mirror - a[i];

    dump.write(args[1]);
    return true;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
