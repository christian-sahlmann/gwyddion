/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include "init.h"
#include "file.h"
#include "settings.h"
#include "app.h"

int
main(int argc, char *argv[])
{
    const gchar *module_dirs[] = {
        GWY_MODULE_DIR ,
        GWY_MODULE_DIR "/file",
        GWY_MODULE_DIR "/process",
        GWY_MODULE_DIR "/tool",
        GWY_MODULE_DIR "/graph",
        NULL
    };
    gchar *config_file;

    gtk_init(&argc, &argv);
    config_file = g_build_filename(g_get_home_dir(), ".gwyddion", "gwydrc",
                                   NULL);
    gwy_app_type_init();
    gwy_app_settings_load(config_file);
    gwy_app_settings_get();
    gwy_module_register_modules(module_dirs);
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
    int argc = 1;
    char* prgname = "STREAM.exe";
    char** argv=&prgname;

    return main(argc, argv);

}
#endif /* WIN32 */

