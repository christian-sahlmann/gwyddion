/*
 *  @(#) $Id: pygwy-console.c 16948 2015-02-22 07:22:30Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  Description: This file contains pygwy console module.
 */
#ifndef __PYGWY_H__
#define __PYGWY_H__

#define pygwy_plugin_dir_name "pygwy"

PyObject* pygwy_create_environment (const gchar *filename,
                                    gboolean show_errors);
void      pygwy_initialize         (void);
void      pygwy_run_string         (const char *cmd,
                                    int type,
                                    PyObject *g,
                                    PyObject *l);

#endif
