/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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

#include <libgwydgets/gwyscitext.h>

G_BEGIN_DECLS

#define GWY_TYPE_AXIS_DIALOG            (gwy_axis_dialog_get_type())
#define GWY_AXIS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialog))
#define GWY_AXIS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS_DIALOG, GwyAxisDialogClass))
#define GWY_IS_AXIS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS_DIALOG))
#define GWY_IS_AXIS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS_DIALOG))
#define GWY_AXIS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS_DIALOG, GwyAxisDialogClass))

typedef struct _GwyAxisDialog      GwyAxisDialog;
typedef struct _GwyAxisDialogClass GwyAxisDialogClass;

struct _GwyAxisDialog {
    GtkDialog dialog;

    GtkWidget *sci_text;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyAxisDialogClass {
    GtkDialogClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType       gwy_axis_dialog_get_type (void) G_GNUC_CONST;
GtkWidget*  gwy_axis_dialog_new      (void);


G_END_DECLS

#endif /* __GWY_GRADSPHERE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
