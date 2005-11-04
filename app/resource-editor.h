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

#ifndef __GWY_RESOURCE_EDITOR_H__
#define __GWY_RESOURCE_EDITOR_H__

#include <gtk/gtkwindow.h>
#include <libgwyddion/gwyresource.h>

G_BEGIN_DECLS

typedef enum {
    GWY_RESOURCE_EDITOR_BUTTON_EDIT,
    GWY_RESOURCE_EDITOR_BUTTON_NEW,
    GWY_RESOURCE_EDITOR_BUTTON_COPY,
    GWY_RESOURCE_EDITOR_BUTTON_DELETE,
    GWY_RESOURCE_EDITOR_BUTTON_SET_DEFAULT,
    GWY_RESOURCE_EDITOR_NBUTTONS
} GwyResourceEditorButton;

#define GWY_TYPE_RESOURCE_EDITOR             (gwy_resource_editor_get_type())
#define GWY_RESOURCE_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RESOURCE_EDITOR, GwyResourceEditor))
#define GWY_RESOURCE_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RESOURCE_EDITOR, GwyResourceEditorClass))
#define GWY_IS_RESOURCE_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RESOURCE_EDITOR))
#define GWY_IS_RESOURCE_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RESOURCE_EDITOR))
#define GWY_RESOURCE_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RESOURCE_EDITOR, GwyResourceEditorClass))

typedef struct _GwyResourceEditor      GwyResourceEditor;
typedef struct _GwyResourceEditorClass GwyResourceEditorClass;

struct _GwyResourceEditor {
    GtkWindow parent_instance;

    GtkWidget *vbox;
    GtkWidget *treeview;
    GtkWidget *buttons[GWY_RESOURCE_EDITOR_NBUTTONS];

    GtkWidget *edit_window;
    GString *edited_resource;
    guint commit_id;
};

struct _GwyResourceEditorClass {
    GtkWindowClass parent_class;

    GType resource_type;
    const gchar *base_resource;
    const gchar *window_title;
    const gchar *editor_title;

    GQuark current_key;
    GQuark width_key;
    GQuark height_key;

    GtkWidget* (*construct_treeview)(GCallback callback,
                                     gpointer cbdata,
                                     const gchar *active);
    void (*construct_editor)(GwyResourceEditor *editor);
    void (*apply_changes)(GwyResourceEditor *editor);
    void (*switch_resource)(GwyResourceEditor *editor);

    GwyResourceEditor *instance;
};

GType        gwy_resource_editor_get_type    (void) G_GNUC_CONST;
GwyResource* gwy_resource_editor_get_edited  (GwyResourceEditor *editor);
void         gwy_resource_editor_queue_commit(GwyResourceEditor *editor);
gboolean     gwy_resource_editor_commit      (GwyResourceEditor *editor);
void         gwy_resource_editor_setup       (GwyResourceEditor *editor);
void         gwy_resource_editor_class_setup (GwyResourceEditorClass *klass);

G_END_DECLS

#endif /*__GWY_RESOURCE_EDITOR_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
