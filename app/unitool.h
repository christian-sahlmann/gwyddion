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

#ifndef __GWY_APP_UNITOOL_H__
#define __GWY_APP_UNITOOL_H__

#include <gtk/gtkwidget.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwymodule/gwymodule-tool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_UNITOOL_RESPONSE_UNSELECT 255

typedef struct _GwyUnitoolSlots GwyUnitoolSlots;

typedef struct {
    gdouble mag;
    gint precision;
    gchar *units;
} GwyUnitoolUnits;

typedef struct {
    gpointer user_data;

    GwyUnitoolSlots *func_slots;
    GwyDataWindow *data_window;
    GwyVectorLayer *layer;
    gboolean is_visible;  /* GTK_WIDGET_VISIBLE() returns BS... */

    GtkWidget *windowname;
    GtkWidget *dialog;

    GwyUnitoolUnits coord_units;

    /*< private >*/
    gulong layer_updated_id;
    gulong data_updated_id;
    gulong response_id;
    gulong windowname_id;
} GwyUnitoolState;

struct _GwyUnitoolSlots {
    GType layer_type;
    void (*layer_setup)(GwyUnitoolState *state);
    GtkWidget* (*dialog_create)(GwyUnitoolState *state);
    void (*dialog_update)(GwyUnitoolState *state);
    void (*dialog_abandon)(GwyUnitoolState *state);
    void (*apply)(GwyUnitoolState *state);
    void (*response)(GwyUnitoolState *state, gint response);
};

gboolean     gwy_unitool_use                     (GwyUnitoolState *state,
                                                  GwyDataWindow *data_window,
                                                  GwyToolSwitchEvent reason);
/* helpers */
GtkWidget*   gwy_unitool_windowname_frame_create (GwyUnitoolState *state);
gdouble      gwy_unitool_get_z_average           (GwyDataField *dfield,
                                                  gdouble xreal,
                                                  gdouble yreal,
                                                  gint radius);
void         gwy_unitool_update_label            (GwyUnitoolUnits *units,
                                                  GtkWidget *label,
                                                  gdouble value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_APP_UNITOOL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */


