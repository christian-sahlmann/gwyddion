/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/* Color picker button for GNOME
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Backported to Gtk+-2.2 and GLib-2.2 by Yeti in Feb 2004.
 *
 * _GtkColorButtonPrivate made a normal structure member and moved to the
 * header file as there's no support for private in GLib-2.2.
 * Renamed to GwyColorButton to avoid name clash with Gtk+-2.4.
 *
 * In May 2004 all the dialog functionality was removed as we need much
 * more control.  DnD did not fit then and was removed too.
 */

#ifndef __GWY_COLOR_BUTTON_H__
#define __GWY_COLOR_BUTTON_H__

#include <gtk/gtkbutton.h>
#include <libdraw/gwyrgba.h>

G_BEGIN_DECLS

#define GWY_TYPE_COLOR_BUTTON             (gwy_color_button_get_type ())
#define GWY_COLOR_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_COLOR_BUTTON, GwyColorButton))
#define GWY_COLOR_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_COLOR_BUTTON, GwyColorButtonClass))
#define GWY_IS_COLOR_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_COLOR_BUTTON))
#define GWY_IS_COLOR_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_COLOR_BUTTON))
#define GWY_COLOR_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_COLOR_BUTTON, GwyColorButtonClass))

typedef struct _GwyColorButton          GwyColorButton;
typedef struct _GwyColorButtonClass     GwyColorButtonClass;

struct _GwyColorButton {
  GtkButton button;

  /*< private >*/
  GdkPixbuf *pixbuf;    /* Pixbuf for rendering sample */
  GdkGC *gc;            /* GC for drawing */
  GtkWidget *drawing_area;/* Drawing area for color sample */
  GwyRGBA color;
  guint use_alpha : 1;  /* Use alpha or not */
};

struct _GwyColorButtonClass {
  GtkButtonClass parent_class;

  /* Padding for future expansion */
  void (*reserved1)(void);
  void (*reserved2)(void);
};


GType      gwy_color_button_get_type       (void) G_GNUC_CONST;
GtkWidget *gwy_color_button_new            (void);
GtkWidget *gwy_color_button_new_with_color (GwyRGBA        *color);
void       gwy_color_button_set_color      (GwyColorButton *color_button,
                                            GwyRGBA        *color);
void       gwy_color_button_get_color      (GwyColorButton *color_button,
                                            GwyRGBA        *color);
void       gwy_color_button_set_use_alpha  (GwyColorButton *color_button,
                                            gboolean        use_alpha);
gboolean   gwy_color_button_get_use_alpha  (GwyColorButton *color_button);


G_END_DECLS

#endif  /* __GWY_COLOR_BUTTON_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
