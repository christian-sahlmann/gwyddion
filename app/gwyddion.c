/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <libgwymodule/gwymodule.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwystock.h>
#include "app.h"
#include "settings.h"
#include "file.h"
#include "gwyddion.h"

int
main(int argc, char *argv[])
{
    gchar **module_dirs;
    gchar *config_file;

#ifdef G_OS_WIN32
    gwy_find_self_set_argv0(argv[0]);
#endif
    gtk_init(&argc, &argv);
    config_file = gwy_app_settings_get_config_filename();
    gwy_app_init();
    gwy_app_settings_load(config_file);
    gwy_app_settings_get();
    module_dirs = gwy_app_settings_get_module_dirs();

    gwy_app_splash_create();
    gwy_app_splash_set_message_prefix(_("Registering "));
    gwy_app_splash_set_message(_("stock items"));
    gwy_stock_register_stock_items();
    gwy_module_set_register_callback(gwy_app_splash_set_message);
    gwy_module_register_modules((const gchar**)module_dirs);
    gwy_module_set_register_callback(NULL);
    gwy_app_splash_set_message_prefix(NULL);
    gwy_app_splash_close();

    gwy_app_toolbox_create();
    gwy_app_file_open_initial(argv + 1, argc - 1);
    gtk_main();
    gwy_app_settings_save(config_file);
    gwy_app_settings_free();
    g_free(config_file);

    return 0;
}

#ifdef WIN32
#define _X86_
#include <windef.h>

int
APIENTRY WinMain(HINSTANCE hInstance,
                 HINSTANCE hPrevInstance,
                 LPSTR     lpCmdLine,
                 int       nCmdShow)
{
    /* FIXME: parse the command line
     * XXX: file_real_open() now expects filenames in system encoding, not
     * UTF-8, this is probably wrong on Windows */
    int argc = 1;
    char* prgname = "STREAM.exe";
    char** argv=&prgname;

    return main(argc, argv);

}
#endif /* WIN32 */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
