/* @(#) $Id$ */

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * GwyHRuler is based on GtkHRuler (instead of subclassing) since GtkHRuler
 * can be subject of removal from Gtk+ in some unspecified point in the future.
 */

#ifndef __GWY_HRULER_H__
#define __GWY_HRULER_H__


#include <gdk/gdk.h>
#include <libgwydgets/gwyruler.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GWY_TYPE_HRULER            (gwy_hruler_get_type())
#define GWY_HRULER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_HRULER, GwyHRuler))
#define GWY_HRULER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_HRULER, GwyHRulerClass))
#define GWY_IS_HRULER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_HRULER))
#define GWY_IS_HRULER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_HRULER))
#define GWY_HRULER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_HRULER, GwyHRulerClass))


typedef struct _GwyHRuler       GwyHRuler;
typedef struct _GwyHRulerClass  GwyHRulerClass;

struct _GwyHRuler
{
    GwyRuler ruler;
};

struct _GwyHRulerClass
{
    GwyRulerClass parent_class;
};


GType      gwy_hruler_get_type (void) G_GNUC_CONST;
GtkWidget* gwy_hruler_new      (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GWY_HRULER_H__ */
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
