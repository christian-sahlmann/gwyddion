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

#ifndef __GWY_SCI_TEXT_H__
#define __GWY_SCI_TEXT_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define GWY_TYPE_SCI_TEXT            (gwy_sci_text_get_type())
#define GWY_SCI_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SCI_TEXT, GwySciText))
#define GWY_SCI_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SCI_TEXT, GwySciTextClass))
#define GWY_IS_SCI_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SCI_TEXT))
#define GWY_IS_SCI_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SCI_TEXT))
#define GWY_SCI_TEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SCI_TEXT, GwySciTextClass))

typedef struct _GwySciText      GwySciText;
typedef struct _GwySciTextClass GwySciTextClass;

struct _GwySciText {
    GtkVBox vbox;

    gboolean has_preview;

    GtkWidget *entry;
    GtkWidget *frame;
    GtkWidget *preview;
    GtkWidget *symbols;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwySciTextClass {
    GtkVBoxClass parent_class;

    /* Signals */
    void (*edited)(GwySciText *sci_text);

    gpointer reserved1;
    gpointer reserved2;
};


GType      gwy_sci_text_get_type       (void) G_GNUC_CONST;
GtkWidget* gwy_sci_text_new            (void);
gchar*     gwy_sci_text_get_text       (GwySciText *sci_text);
void       gwy_sci_text_set_text       (GwySciText *sci_text,
                                        const gchar *new_text);
gboolean   gwy_sci_text_get_has_preview(GwySciText *sci_text);
void       gwy_sci_text_set_has_preview(GwySciText *sci_text,
                                        gboolean has_preview);
GtkWidget* gwy_sci_text_get_entry      (GwySciText *sci_text);

G_END_DECLS

#endif /*__GWY_SCI_TEXT_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
