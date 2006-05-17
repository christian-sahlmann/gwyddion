/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-tool.h>
#include <libprocess/elliptic.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define GWY_TYPE_TOOL_MASK_EDITOR            (gwy_tool_mask_editor_get_type())
#define GWY_TOOL_MASK_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TOOL_MASK_EDITOR, GwyToolMaskEditor))
#define GWY_IS_TOOL_MASK_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TOOL_MASK_EDITOR))
#define GWY_TOOL_MASK_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TOOL_MASK_EDITOR, GwyToolMaskEditorClass))

enum {
    SENS_DATA = 1 << 0,
    SENS_MASK = 1 << 1,
};

typedef enum {
    MASK_EDIT_SET       = 0,
    MASK_EDIT_ADD       = 1,
    MASK_EDIT_REMOVE    = 2,
    MASK_EDIT_INTERSECT = 3
} MaskEditMode;

typedef enum {
    MASK_SHAPE_RECTANGLE = 0,
    MASK_SHAPE_ELLIPSE   = 1
} MaskEditShape;

typedef struct _GwyToolMaskEditor      GwyToolMaskEditor;
typedef struct _GwyToolMaskEditorClass GwyToolMaskEditorClass;

typedef void (*FieldFillFunc)(GwyDataField*, gint, gint, gint, gint, gdouble);

typedef struct {
    MaskEditMode mode;
    MaskEditShape shape;
    gint32 gsamount;
    gboolean from_border;
} ToolArgs;

struct _GwyToolMaskEditor {
    GwyPlainTool parent_instance;

    ToolArgs args;

    GwySensitivityGroup *sensgroup;
    GSList *mode;
    GSList *shape;

    GtkObject *gsamount;
    GtkWidget *from_border;

    /* potential class data */
    GType layer_type_rect;
    GType layer_type_ell;
};

struct _GwyToolMaskEditorClass {
    GwyPlainToolClass parent_class;
};

static gboolean module_register(void);

static GType gwy_tool_mask_editor_get_type           (void) G_GNUC_CONST;
static void  gwy_tool_mask_editor_finalize           (GObject *object);
static void  gwy_tool_mask_editor_init_dialog        (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_data_switched      (GwyTool *gwytool,
                                                      GwyDataView *data_view);
static void  gwy_tool_mask_editor_mask_changed       (GwyPlainTool *plain_tool);
static void  gwy_tool_mask_editor_mode_changed       (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_shape_changed      (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_gsamount_changed   (GtkAdjustment *adj,
                                                      GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_from_border_changed(GtkToggleButton *toggle,
                                                      GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_invert             (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_remove             (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_fill               (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_grow               (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_shrink             (GwyToolMaskEditor *tool);
static void  gwy_tool_mask_editor_selection_finished (GwyPlainTool *plain_tool);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Mask editor tool, allows to interactively add or remove parts "
       "of mask."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

static const gchar mode_key[]        = "/module/maskeditor/mode";
static const gchar shape_key[]       = "/module/maskeditor/shape";
static const gchar gsamount_key[]    = "/module/maskeditor/gsamount";
static const gchar from_border_key[] = "/module/maskeditor/from_border";

static const ToolArgs default_args = {
    MASK_EDIT_SET,
    MASK_SHAPE_RECTANGLE,
    1,
    FALSE,
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwyToolMaskEditor, gwy_tool_mask_editor, GWY_TYPE_PLAIN_TOOL)

static gboolean
module_register(void)
{
    gwy_tool_func_register(GWY_TYPE_TOOL_MASK_EDITOR);

    return TRUE;
}

static void
gwy_tool_mask_editor_class_init(GwyToolMaskEditorClass *klass)
{
    GwyPlainToolClass *ptool_class = GWY_PLAIN_TOOL_CLASS(klass);
    GwyToolClass *tool_class = GWY_TOOL_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_tool_mask_editor_finalize;

    tool_class->stock_id = GWY_STOCK_MASK_EDITOR;
    tool_class->title = _("Mask Editor");
    tool_class->tooltip = _("Edit mask");
    tool_class->prefix = "/module/maskeditor";
    tool_class->data_switched = gwy_tool_mask_editor_data_switched;

    ptool_class->mask_changed = gwy_tool_mask_editor_mask_changed;
    ptool_class->selection_finished = gwy_tool_mask_editor_selection_finished;
}

static void
gwy_tool_mask_editor_finalize(GObject *object)
{
    GwyToolMaskEditor *tool;
    GwyContainer *settings;

    tool = GWY_TOOL_MASK_EDITOR(object);

    settings = gwy_app_settings_get();
    gwy_container_set_enum_by_name(settings, mode_key,
                                   tool->args.mode);
    gwy_container_set_enum_by_name(settings, shape_key,
                                   tool->args.shape);
    gwy_container_set_int32_by_name(settings, gsamount_key,
                                    tool->args.gsamount);
    gwy_container_set_boolean_by_name(settings, from_border_key,
                                      tool->args.from_border);

    G_OBJECT_CLASS(gwy_tool_mask_editor_parent_class)->finalize(object);
}

static void
gwy_tool_mask_editor_init(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GwyContainer *settings;

    plain_tool = GWY_PLAIN_TOOL(tool);
    tool->layer_type_rect = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerRectangle");
    if (!tool->layer_type_rect)
        return;

    tool->layer_type_ell = gwy_plain_tool_check_layer_type(plain_tool,
                                                           "GwyLayerEllipse");
    if (!tool->layer_type_ell)
        return;

    settings = gwy_app_settings_get();
    tool->args = default_args;
    gwy_container_gis_enum_by_name(settings, mode_key,
                                   &tool->args.mode);
    gwy_container_gis_enum_by_name(settings, shape_key,
                                   &tool->args.shape);
    gwy_container_gis_int32_by_name(settings, gsamount_key,
                                    &tool->args.gsamount);
    gwy_container_gis_boolean_by_name(settings, from_border_key,
                                      &tool->args.from_border);

    switch (tool->args.shape) {
        case MASK_SHAPE_RECTANGLE:
        gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                         "rectangle");
        break;

        case MASK_SHAPE_ELLIPSE:
        gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_ell,
                                         "ellipse");
        break;

        default:
        g_return_if_reached();
        break;
    }

    gwy_tool_mask_editor_init_dialog(tool);
}

static void
gwy_tool_mask_editor_init_dialog(GwyToolMaskEditor *tool)
{
    static struct {
        guint type;
        const gchar *stock_id;
        const gchar *text;
    }
    const modes[] = {
        {
            MASK_EDIT_SET,
            GWY_STOCK_MASK,
            N_("Set mask to selection"),
        },
        {
            MASK_EDIT_ADD,
            GWY_STOCK_MASK_ADD,
            N_("Add selection to mask"),
        },
        {
            MASK_EDIT_REMOVE,
            GWY_STOCK_MASK_SUBTRACT,
            N_("Subtract selection from mask"),
        },
        {
            MASK_EDIT_INTERSECT,
            GWY_STOCK_MASK_INTERSECT,
            N_("Intersect selection with mask"),
        },
    },
    shapes[] = {
        {
            MASK_SHAPE_RECTANGLE,
            GWY_STOCK_MASK,
            N_("Rectangular shapes"),
        },
        {
            MASK_SHAPE_ELLIPSE,
            GWY_STOCK_MASK_CIRCLE,
            N_("Elliptic shapes"),
        },
    };

    GtkRadioButton *group;
    GtkSizeGroup *sizegroup;
    GtkTooltips *tips;
    GtkDialog *dialog;
    GtkTable *table;
    GtkWidget *image, *label, *button;
    GtkBox *hbox;
    gint i, row;

    dialog = GTK_DIALOG(GWY_TOOL(tool)->dialog);
    tips = gwy_app_get_tooltips();
    sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    tool->sensgroup = gwy_sensitivity_group_new();

    table = GTK_TABLE(gtk_table_new(9, 4, FALSE));
    gtk_table_set_col_spacings(table, 6);
    gtk_table_set_row_spacings(table, 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), GTK_WIDGET(table),
                       FALSE, FALSE, 0);
    row = 0;

    /* Editor */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Editor</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* Mode */
    label = gtk_label_new(_("Mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_table_attach(table, GTK_WIDGET(hbox), 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(modes); i++) {
        button = gtk_radio_button_new_from_widget(group);
        g_object_set(button, "draw-indicator", FALSE, NULL);
        image = gtk_image_new_from_stock(modes[i].stock_id,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_container_add(GTK_CONTAINER(button), image);
        g_object_set_data(G_OBJECT(button), "select-mode",
                          GUINT_TO_POINTER(modes[i].type));
        gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
        gtk_tooltips_set_tip(tips, button, gettext(modes[i].text), NULL);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_tool_mask_editor_mode_changed),
                                 tool);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    tool->mode = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(tool->mode, "select-mode", tool->args.mode);
    row++;

    /* Shape */
    label = gtk_label_new(_("Shape:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

    hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_table_attach(table, GTK_WIDGET(hbox), 1, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    group = NULL;
    for (i = 0; i < G_N_ELEMENTS(shapes); i++) {
        button = gtk_radio_button_new_from_widget(group);
        g_object_set(button, "draw-indicator", FALSE, NULL);
        image = gtk_image_new_from_stock(shapes[i].stock_id,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_container_add(GTK_CONTAINER(button), image);
        g_object_set_data(G_OBJECT(button), "shape-type",
                          GUINT_TO_POINTER(shapes[i].type));
        gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
        gtk_tooltips_set_tip(tips, button, gettext(shapes[i].text), NULL);
        g_signal_connect_swapped(button, "clicked",
                                 G_CALLBACK(gwy_tool_mask_editor_shape_changed),
                                 tool);
        if (!group)
            group = GTK_RADIO_BUTTON(button);
    }
    tool->shape = gtk_radio_button_get_group(group);
    gwy_radio_buttons_set_current(tool->shape, "shape-type", tool->args.shape);
    gtk_table_set_row_spacing(table, row, 12);
    row++;

    /* Actions */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Actions</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_table_attach(table, GTK_WIDGET(hbox), 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gwy_stock_like_button_new(_("_Invert"), GWY_STOCK_MASK_INVERT);
    gtk_size_group_add_widget(sizegroup, button);
    gwy_sensitivity_group_add_widget(tool->sensgroup, button,
                                     SENS_DATA | SENS_MASK);
    gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_tool_mask_editor_invert), tool);

    button = gwy_stock_like_button_new(_("_Remove"), GWY_STOCK_MASK_REMOVE);
    gtk_size_group_add_widget(sizegroup, button);
    gwy_sensitivity_group_add_widget(tool->sensgroup, button,
                                     SENS_DATA | SENS_MASK);
    gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_tool_mask_editor_remove), tool);

    button = gwy_stock_like_button_new(_("_Fill"), GWY_STOCK_MASK);
    gtk_size_group_add_widget(sizegroup, button);
    gwy_sensitivity_group_add_widget(tool->sensgroup, button, SENS_DATA);
    gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_tool_mask_editor_fill), tool);

    label = gtk_label_new(NULL);
    gtk_box_pack_start(hbox, label, TRUE, TRUE, 0);
    gtk_table_set_row_spacing(table, row, 12);
    row++;

    /* Grow/Shrink */
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Grow/Shrink</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    /* Buttons */
    hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_table_attach(table, GTK_WIDGET(hbox), 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gwy_stock_like_button_new(_("_Grow"), GWY_STOCK_MASK_GROW);
    gtk_size_group_add_widget(sizegroup, button);
    gwy_sensitivity_group_add_widget(tool->sensgroup, button,
                                     SENS_DATA | SENS_MASK);
    gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_tool_mask_editor_grow), tool);

    button = gwy_stock_like_button_new(_("Shrin_k"), GWY_STOCK_MASK_SHRINK);
    gtk_size_group_add_widget(sizegroup, button);
    gwy_sensitivity_group_add_widget(tool->sensgroup, button,
                                     SENS_DATA | SENS_MASK);
    gtk_box_pack_start(hbox, button, FALSE, FALSE, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_tool_mask_editor_shrink), tool);

    label = gtk_label_new(NULL);
    gtk_box_pack_start(hbox, label, TRUE, TRUE, 0);
    row++;

    /* Options */
    tool->gsamount = gtk_adjustment_new(tool->args.gsamount, 1.0, 256.0,
                                        1.0, 10.0, 0.0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row, _("_Amount:"), "px",
                            tool->gsamount, 0);
    g_signal_connect(tool->gsamount, "value-changed",
                     G_CALLBACK(gwy_tool_mask_editor_gsamount_changed), tool);
    row++;

    tool->from_border
        = gtk_check_button_new_with_mnemonic(_("Shrink from _border"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tool->from_border),
                                 tool->args.from_border);
    gtk_table_attach(table, tool->from_border, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(tool->from_border, "toggled",
                     G_CALLBACK(gwy_tool_mask_editor_from_border_changed),
                     tool);

    gwy_tool_add_hide_button(GWY_TOOL(tool), TRUE);

    g_object_unref(sizegroup);
    g_object_unref(tool->sensgroup);
    gtk_widget_show_all(dialog->vbox);
}

static void
gwy_tool_mask_editor_data_switched(GwyTool *gwytool,
                                   GwyDataView *data_view)
{
    GwyPlainTool *plain_tool;
    GwyToolMaskEditor *tool;

    GWY_TOOL_CLASS(gwy_tool_mask_editor_parent_class)->data_switched(gwytool,
                                                                     data_view);
    plain_tool = GWY_PLAIN_TOOL(gwytool);
    if (plain_tool->init_failed)
        return;

    tool = GWY_TOOL_MASK_EDITOR(gwytool);
    if (data_view) {
        g_object_set(plain_tool->layer, "draw-reflection", FALSE, NULL);
        if (tool->args.shape == MASK_SHAPE_RECTANGLE)
            g_object_set(plain_tool->layer, "is-crop", FALSE, NULL);
        gwy_selection_set_max_objects(plain_tool->selection, 1);
    }

    gwy_sensitivity_group_set_state(tool->sensgroup, SENS_DATA,
                                    data_view ? SENS_DATA : 0);
    gwy_tool_mask_editor_mask_changed(plain_tool);
}

static void
gwy_tool_mask_editor_mask_changed(GwyPlainTool *plain_tool)
{
    GwyToolMaskEditor *tool;
    guint state = 0;

    tool = GWY_TOOL_MASK_EDITOR(plain_tool);
    if (plain_tool->mask_field) {
        gwy_debug("mask field exists");
        if (gwy_data_field_get_max(plain_tool->mask_field) > 0) {
            gwy_debug("mask field is nonempty");
            state = SENS_MASK;
        }
    }

    gwy_sensitivity_group_set_state(tool->sensgroup, SENS_MASK, state);
}

static void
gwy_tool_mask_editor_mode_changed(GwyToolMaskEditor *tool)
{
    tool->args.mode = gwy_radio_buttons_get_current(tool->mode, "select-mode");
    if (tool->args.mode == -1)
        g_warning("Mode set to -1!");
}

static void
gwy_tool_mask_editor_shape_changed(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    MaskEditShape shape;

    shape = gwy_radio_buttons_get_current(tool->shape, "shape-type");
    if (shape == tool->args.shape)
        return;
    tool->args.shape = shape;

    plain_tool = GWY_PLAIN_TOOL(tool);
    switch (tool->args.shape) {
        case MASK_SHAPE_RECTANGLE:
        gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_rect,
                                         "rectangle");
        break;

        case MASK_SHAPE_ELLIPSE:
        gwy_plain_tool_connect_selection(plain_tool, tool->layer_type_ell,
                                         "ellipse");
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
gwy_tool_mask_editor_gsamount_changed(GtkAdjustment *adj,
                                      GwyToolMaskEditor *tool)
{
    tool->args.gsamount = gwy_adjustment_get_int(adj);
}

static void
gwy_tool_mask_editor_from_border_changed(GtkToggleButton *toggle,
                                         GwyToolMaskEditor *tool)
{
    tool->args.from_border = gtk_toggle_button_get_active(toggle);
}

static GwyDataField*
gwy_tool_mask_editor_maybe_add_mask(GwyPlainTool *plain_tool,
                                    GQuark quark)
{
    GwyDataField *mfield;

    if (!(mfield = plain_tool->mask_field)) {
        mfield = gwy_data_field_new_alike(plain_tool->data_field, TRUE);
        gwy_container_set_object(plain_tool->container, quark, mfield);
        g_object_unref(mfield);
    }
    return mfield;
}

static void
gwy_tool_mask_editor_invert(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GQuark quark;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->mask_field);

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    gwy_data_field_multiply(plain_tool->mask_field, -1.0);
    gwy_data_field_add(plain_tool->mask_field, 1.0);
    gwy_data_field_data_changed(plain_tool->mask_field);
}

static void
gwy_tool_mask_editor_remove(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GQuark quark;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->mask_field);

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    gwy_container_remove(plain_tool->container, quark);
}

static void
gwy_tool_mask_editor_fill(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GwyDataField *mfield;
    GQuark quark;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->data_field);

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
    mfield = gwy_tool_mask_editor_maybe_add_mask(plain_tool, quark);
    gwy_data_field_fill(mfield, 1.0);
    gwy_data_field_data_changed(mfield);
}

static void
gwy_tool_mask_editor_grow(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GQuark quark;
    gdouble *data, *buffer, *prow;
    gdouble min, q1, q2;
    gint xres, yres, rowstride;
    gint i, j, iter;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->mask_field);

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);

    xres = gwy_data_field_get_xres(plain_tool->mask_field);
    yres = gwy_data_field_get_yres(plain_tool->mask_field);
    data = gwy_data_field_get_data(plain_tool->mask_field);

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (iter = 0; iter < tool->args.gsamount; iter++) {
        rowstride = xres;
        min = G_MAXDOUBLE;
        for (j = 0; j < xres; j++)
            prow[j] = -G_MAXDOUBLE;
        memcpy(buffer, data, xres*sizeof(gdouble));
        for (i = 0; i < yres; i++) {
            gdouble *row = data + i*xres;

            if (i == yres-1)
                rowstride = 0;

            j = 0;
            q2 = MAX(buffer[j], buffer[j+1]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);
            for (j = 1; j < xres-1; j++) {
                q1 = MAX(prow[j], buffer[j-1]);
                q2 = MAX(buffer[j], buffer[j+1]);
                q2 = MAX(q2, row[j+rowstride]);
                row[j] = MAX(q1, q2);
                min = MIN(min, row[j]);
            }
            j = xres-1;
            q2 = MAX(buffer[j-1], buffer[j]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);

            GWY_SWAP(gdouble*, prow, buffer);
            if (i < yres-1)
                memcpy(buffer, data + (i+1)*xres, xres*sizeof(gdouble));
        }
        if (min >= 1.0)
            break;
    }
    g_free(buffer);
    g_free(prow);

    gwy_data_field_data_changed(plain_tool->mask_field);
}

static void
gwy_tool_mask_editor_shrink(GwyToolMaskEditor *tool)
{
    GwyPlainTool *plain_tool;
    GQuark quark;
    gdouble *data, *buffer, *prow;
    gdouble q1, q2, max;
    gint xres, yres, rowstride;
    gint i, j, iter;

    plain_tool = GWY_PLAIN_TOOL(tool);
    g_return_if_fail(plain_tool->mask_field);

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);

    xres = gwy_data_field_get_xres(plain_tool->mask_field);
    yres = gwy_data_field_get_yres(plain_tool->mask_field);
    data = gwy_data_field_get_data(plain_tool->mask_field);

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (iter = 0; iter < tool->args.gsamount; iter++) {
        rowstride = xres;
        max = -G_MAXDOUBLE;
        for (j = 0; j < xres; j++)
            prow[j] = G_MAXDOUBLE;
        memcpy(buffer, data, xres*sizeof(gdouble));
        for (i = 0; i < yres; i++) {
            gdouble *row = data + i*xres;

            if (i == yres-1)
                rowstride = 0;

            j = 0;
            q2 = MIN(buffer[j], buffer[j+1]);
            q1 = MIN(prow[j], row[j+rowstride]);
            row[j] = MIN(q1, q2);
            max = MAX(max, row[j]);
            for (j = 1; j < xres-1; j++) {
                q1 = MIN(prow[j], buffer[j-1]);
                q2 = MIN(buffer[j], buffer[j+1]);
                q2 = MIN(q2, row[j+rowstride]);
                row[j] = MIN(q1, q2);
                max = MAX(max, row[j]);
            }
            j = xres-1;
            q2 = MIN(buffer[j-1], buffer[j]);
            q1 = MIN(prow[j], row[j+rowstride]);
            row[j] = MIN(q1, q2);
            max = MAX(max, row[j]);

            GWY_SWAP(gdouble*, prow, buffer);
            if (i < yres-1)
                memcpy(buffer, data + (i+1)*xres, xres*sizeof(gdouble));
        }

        /* To shrink from borders we only have to clear boundary pixels in
         * the first iteration, then it goes on itself */
        if (iter == 0 && tool->args.from_border) {
            gwy_data_field_area_clear(plain_tool->mask_field,
                                      0, 0, xres, 1);
            gwy_data_field_area_clear(plain_tool->mask_field,
                                      0, 0, 1, yres);
            gwy_data_field_area_clear(plain_tool->mask_field,
                                      xres-1, 0, 1, yres);
            gwy_data_field_area_clear(plain_tool->mask_field,
                                      0, yres-1, xres, 1);
        }

        if (max <= 0.0)
            break;
    }
    g_free(buffer);
    g_free(prow);

    gwy_data_field_data_changed(plain_tool->mask_field);
}

static void
gwy_tool_mask_editor_selection_finished(GwyPlainTool *plain_tool)
{
    GwyToolMaskEditor *tool;
    GwyDataField *mfield = NULL;
    FieldFillFunc fill_func;
    GQuark quark;
    gdouble sel[4];
    gint isel[4];

    g_return_if_fail(plain_tool->data_field);

    tool = GWY_TOOL_MASK_EDITOR(plain_tool);
    if (!gwy_selection_get_object(plain_tool->selection, 0, sel))
        return;

    isel[0] = gwy_data_field_rtoj(plain_tool->data_field, sel[0]);
    isel[1] = gwy_data_field_rtoi(plain_tool->data_field, sel[1]);
    isel[2] = gwy_data_field_rtoj(plain_tool->data_field, sel[2]) + 1;
    isel[3] = gwy_data_field_rtoi(plain_tool->data_field, sel[3]) + 1;
    gwy_debug("(%d,%d) (%d,%d)", isel[0], isel[1], isel[2], isel[3]);
    isel[2] -= isel[0];
    isel[3] -= isel[1];

    switch (tool->args.shape) {
        case MASK_SHAPE_RECTANGLE:
        fill_func = &gwy_data_field_area_fill;
        break;

        case MASK_SHAPE_ELLIPSE:
        fill_func = (FieldFillFunc)&gwy_data_field_elliptic_area_fill;
        break;

        default:
        g_return_if_reached();
        break;
    }

    quark = gwy_app_get_mask_key_for_id(plain_tool->id);
    switch (tool->args.mode) {
        case MASK_EDIT_SET:
        gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
        mfield = gwy_tool_mask_editor_maybe_add_mask(plain_tool, quark);
        gwy_data_field_clear(mfield);
        fill_func(mfield, isel[0], isel[1], isel[2], isel[3], 1.0);
        break;

        case MASK_EDIT_ADD:
        gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
        mfield = gwy_tool_mask_editor_maybe_add_mask(plain_tool, quark);
        fill_func(mfield, isel[0], isel[1], isel[2], isel[3], 1.0);
        break;

        case MASK_EDIT_REMOVE:
        if (plain_tool->mask_field) {
            gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
            mfield = plain_tool->mask_field;
            fill_func(mfield, isel[0], isel[1], isel[2], isel[3], 0.0);
            if (!gwy_data_field_get_max(mfield) > 0.0) {
                gwy_container_remove(plain_tool->container, quark);
                mfield = NULL;
            }
        }
        break;

        case MASK_EDIT_INTERSECT:
        if (plain_tool->mask_field) {
            gwy_app_undo_qcheckpointv(plain_tool->container, 1, &quark);
            mfield = plain_tool->mask_field;
            gwy_data_field_clamp(mfield, 0.0, 1.0);
            if (tool->args.shape == MASK_SHAPE_ELLIPSE) {
                gdouble *data;
                gint n;

                n = gwy_data_field_get_elliptic_area_size(isel[2], isel[3]);
                data = g_new(gdouble, n);
                gwy_data_field_elliptic_area_extract(mfield,
                                                     isel[0], isel[1],
                                                     isel[2], isel[3],
                                                     data);
                while (n)
                    data[--n] += 1.0;
                gwy_data_field_elliptic_area_unextract(mfield,
                                                       isel[0], isel[1],
                                                       isel[2], isel[3],
                                                       data);
                g_free(data);
            }
            else {
                gwy_data_field_area_add(mfield,
                                        isel[0], isel[1], isel[2], isel[3],
                                        1.0);
            }
            gwy_data_field_add(mfield, -1.0);
            gwy_data_field_clamp(mfield, 0.0, 1.0);
            if (!gwy_data_field_get_max(mfield) > 0.0) {
                gwy_container_remove(plain_tool->container, quark);
                mfield = NULL;
            }
        }
        break;

        default:
        break;
    }
    gwy_selection_clear(plain_tool->selection);
    if (mfield)
        gwy_data_field_data_changed(mfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

