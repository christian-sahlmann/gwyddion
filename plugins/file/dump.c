/**
 * @(#) $Id$
 * A Gwyddion plug-in providing the `dump' format by simply copying what
 * it gets from plug-in proxy.
 * Written by Yeti <yeti@gwyddion.net>.  Public domain.
 **/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
#define bool int
enum { false = 0, true = 1 };
#endif

typedef struct {
    const char *name;              /* action name: register, load, run, ... */
    int arg_num;                   /* number of arguments NOT including
                                      program name, nor action name */
    bool (*action)(char *args[]);  /* action itself, will get array of
                                      arg_num program argument, NOT including
                                      program name, nor action name */
} PluginAction;

static bool run_action    (unsigned int nactions,
                           PluginAction *actions,
                           int argc,
                           char *argv[]);
static bool copy_file     (const char *src_file,
                           const char *dest_file);

/* Plug-in helper data.
 * Each action (register, run) is represented by a PluginAction struct with
 * one function actually performing the action. */
static bool action_register(char *args[]);
static bool action_load    (char *args[]);
static bool action_save    (char *args[]);

static PluginAction plugin_actions[] = {
    /* name       arguments action */
    { "register", 0,        &action_register },
    { "load",     2,        &action_load     },
    { "save",     2,        &action_save     },
};

#define NACTIONS sizeof(plugin_actions)/sizeof(plugin_actions[0])

int
main(int argc, char *argv[])
{
    return !run_action(NACTIONS, plugin_actions, argc, argv);
}

/* "register" action: print registration information to standard output */
static bool
action_register(char *args[])
{
    puts("dump\n"
         "Plug-in proxy dump (.dump)\n"
         "*.dump\n"
         "load save");
    return true;
}

/* "load" action: load files, i.e. copy them with no conversion to dump files */
static bool
action_load(char *args[])
{
    /* Copy second file to first (dump) file */
    return copy_file(args[1], args[0]);
}

/* "save" action: save files, i.e. copy dump files with no conversion to them */
static bool
action_save(char *args[])
{
    /* Copy first (dump) file to second file */
    return copy_file(args[0], args[1]);
}


/* copy src_file to dest_file */
static bool
copy_file(const char *src_file,
          const char *dest_file)
{
    enum { BUF_SIZE = 4096 };
    char *buffer;
    FILE *fhr, *fhw;
    unsigned long int n;

    if (!(fhr = fopen(src_file, "rb"))) {
        fprintf(stderr, "Cannot open `%s' for reading.", src_file);
        return false;
    }
    if (!(fhw = fopen(dest_file, "wb"))) {
        fprintf(stderr, "Cannot open `%s' for writing.", dest_file);
        fclose(fhr);
        return false;
    }
    buffer = (char*)malloc(BUF_SIZE);

    do {
        n = fread(buffer, 1, BUF_SIZE, fhr);
        if (n && fwrite(buffer, 1, n, fhw) != n) {
            fprintf(stderr, "Cannot write to `%s'.", dest_file);
            break;
        }
    } while (n == BUF_SIZE);

    fclose(fhr);
    fclose(fhw);

    return true;
}

/************ plug-in helper ***************/
/* Find action and run it */
static bool
run_action(unsigned int nactions, PluginAction *actions,
           int argc, char *argv[])
{
    unsigned int i;

    for (i = 0; i < nactions; i++) {
        if (argc == actions[i].arg_num + 2
            && strcmp(actions[i].name, argv[1]) == 0) {
            actions[i].action(argv + 2);
            return true;
        }
    }
    fprintf(stderr, "Plug-in has to be called from Gwyddion plugin-proxy.\n");
    return false;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

