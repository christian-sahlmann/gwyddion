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

#ifndef __GWY_SHADER_H__
#define __GWY_SHADER_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <libdraw/gwygradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_SHADER            (gwy_shader_get_type())
#define GWY_SHADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SHADER, GwyShader))
#define GWY_SHADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SHADER, GwyShaderClass))
#define GWY_IS_SHADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SHADER))
#define GWY_IS_SHADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SHADER))
#define GWY_SHADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SHADER, GwyShaderClass))

typedef struct _GwyShader      GwyShader;
typedef struct _GwyShaderClass GwyShaderClass;

struct _GwyShader {
    GtkWidget widget;

    GwyGradient *gradient;
    GtkUpdateType update_policy;
    guint8 button;
    gint radius;
    guint32 timer_id;
    gulong gradient_change_id;

    /* Current angles */
    gdouble theta;
    gdouble phi;

    /* Old adjustment values */
    gdouble old_theta;
    gdouble old_phi;

    /* The sphere */
    GdkPixbuf *pixbuf;

    gpointer reserved1;
};

struct _GwyShaderClass {
    GtkWidgetClass parent_class;

    void (*angle_changed)(GwyShader *shader);

    gpointer reserved1;
};

GType          gwy_shader_get_type             (void) G_GNUC_CONST;
GtkWidget*     gwy_shader_new                  (const gchar *gradient);
gdouble        gwy_shader_get_theta            (GwyShader *shader);
gdouble        gwy_shader_get_phi              (GwyShader *shader);
void           gwy_shader_set_theta            (GwyShader *shader,
                                                gdouble theta);
void           gwy_shader_set_phi              (GwyShader *shader,
                                                gdouble phi);
void           gwy_shader_set_angle            (GwyShader *shader,
                                                gdouble theta,
                                                gdouble phi);
const gchar*   gwy_shader_get_gradient         (GwyShader *shader);
void           gwy_shader_set_gradient         (GwyShader *shader,
                                                const gchar *gradient);
GtkUpdateType  gwy_shader_get_update_policy    (GwyShader *shader);
void           gwy_shader_set_update_policy    (GwyShader *shader,
                                                GtkUpdateType update_policy);

G_END_DECLS

#endif /* __GWY_SHADER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
