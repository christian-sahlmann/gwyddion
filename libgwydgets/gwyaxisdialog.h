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

#ifndef __GWY_AXIS_DIALOG_H__
#define __GWY_AXIS_DIALOG_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>

#include "gwyscitext.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_AXIS_DIALOG            (gwy_axis_dialog_get_type())
#define GWY_AXIS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialog))
#define GWY_AXIS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS_DIALOG, GwyAxisDialog))
#define GWY_IS_AXIS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS_DIALOG))
#define GWY_IS_AXIS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS_DIALOG))
#define GWY_AXIS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialogClass))

typedef struct {
   GtkDialog dialog;

   GwySciText *sci_text;
   
} GwyAxisDialog;

typedef struct {
   GtkDialogClass parent_class;
} GwyAxisDialogClass;

GtkWidget *gwy_axis_dialog_new();


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GRADSPHERE_H__ */

