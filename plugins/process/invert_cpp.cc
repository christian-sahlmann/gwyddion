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

/* Plug-in helper data.
 * Each action (register, run) is represented by a PluginAction object with
 * one function actually performing the action. */
static bool action_register(char *args[]);
static bool action_run     (char *args[]);

static PluginAction actions[] = {
    /* name               arguments action */
    { string("register"), 0,        &action_register },
    { string("run"),      2,        &action_run      },
};

#define NACTIONS sizeof(actions)/sizeof(actions[0])

int
main(int argc, char *argv[])
{
    /* Just let plug-in helper decide what to do */
    return !run_action(NACTIONS, actions, argc, argv);
}

/* "register" action: print registration information to standard output */
static bool
action_register(char *args[])
{
    cout << "invert_cpp" << endl;
    cout << "/_Test/Value Invert (C++)" << endl;
    cout << "noninteractive with_defaults" << endl;
    return true;
}

/* "run" action: actually do something;
 * the first argument is run mode, the second one a dump file name to read
 * and overwrite with result */
static bool
action_run(char *args[])
{
    /* Run mode sanity check */
    if (string("noninteractive") != args[0]
        && string("with_defaults") != args[0])
        return false;

    /* Read the dump file */
    Dump dump;
    dump.read(args[1]);

    /* Get "/0/data" data field, i.e., the main data. */
    map<string,DataField>::iterator iter = dump.data.find(string("/0/data"));
    /* Find minimum and maximum to keep data range during value inversion */
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
    /* Invert */
    double mirror = min + max;
    for (i = 0; i < n; i++)
        a[i] = mirror - a[i];

    /* Write back the dump file */
    dump.write(args[1]);

    return true;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
