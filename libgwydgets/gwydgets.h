/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_GWYDGETS_H__
#define __GWY_GWYDGETS_H__

#include <libgwydgets/gwy3dlabels.h>
#include <libgwydgets/gwy3dview.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwyaxisdialog.h>
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwycolorbutton.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydataviewlayer.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwyglmaterial.h>
#include <libgwydgets/gwygradsphere.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygrapharea.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwyhruler.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwyoptionmenus.h>
#include <libgwydgets/gwypixmaplayer.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwyruler.h>
#include <libgwydgets/gwyscitext.h>
#include <libgwydgets/gwyshader.h>
#include <libgwydgets/gwyspherecoords.h>
#include <libgwydgets/gwystatusbar.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwytoolbox.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwyvectorshade.h>
#include <libgwydgets/gwyvmenubar.h>
#include <libgwydgets/gwyvruler.h>
#include <libgwydgets/gwyvalunit.h>

G_BEGIN_DECLS

#define gwy_adjustment_get_int(adj) \
    ((gint)(gtk_adjustment_get_value(GTK_ADJUSTMENT(adj)) + 0.5))

typedef enum {
    GWY_HSCALE_DEFAULT          = 0,
    GWY_HSCALE_LOG              = 1,
    GWY_HSCALE_SQRT             = 2,
    GWY_HSCALE_NO_SCALE         = 6,
    GWY_HSCALE_WIDGET           = 7,
    GWY_HSCALE_WIDGET_NO_EXPAND = 8,
    GWY_HSCALE_CHECK            = 1024
} GwyHScaleStyle;

void         gwy_widgets_type_init          (void);
gboolean     gwy_widgets_gl_init            (void);
GdkGLConfig* gwy_widgets_get_gl_config      (void);

GtkWidget*   gwy_table_attach_spinbutton    (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkObject *adj);
void         gwy_table_attach_row           (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkWidget *middle_widget);
GtkWidget*   gwy_table_attach_hscale        (GtkWidget *table,
                                             gint row,
                                             const gchar *name,
                                             const gchar *units,
                                             GtkObject *pivot,
                                             GwyHScaleStyle style);
void         gwy_table_hscale_set_sensitive (GtkObject *pivot,
                                             gboolean sensitive);
GtkWidget*   gwy_table_get_child_widget     (GtkWidget *table,
                                             gint row,
                                             gint col);
void         gwy_color_selector_for_mask    (const gchar *dialog_title,
                                             GwyDataView *data_view,
                                             GwyColorButton *color_button,
                                             GwyContainer *container,
                                             const gchar *prefix);
gboolean     gwy_dialog_prevent_delete_cb   (void);
GtkWidget*   gwy_stock_like_button_new      (const gchar *label_text,
                                             const gchar *stock_id);

G_END_DECLS

#endif /* __GWY_GWYDGETS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
