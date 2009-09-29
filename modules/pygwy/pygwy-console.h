#ifndef _PYGWY_CONSOLE_H
#define _PYGWY_CONSOLE_H
/*
 *  Copyright (C) 2008 Jan Horak
 *  E-mail: xhorak@gmail.com
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
 *
 *  Description: This file contains pygwy console module.
 */

#include <Python.h>
#include <gtk/gtk.h>
typedef struct {
   PyObject *std_err;
   PyObject *dictionary;
   GtkWidget *console_output;
   GtkWidget *console_file_content;
   gchar *script_filename;
} PygwyConsoleSetup;

void              pygwy_register_console             (void);
#endif
