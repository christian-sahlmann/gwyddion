/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymacros.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>

#define MAGIC_BIN "\\*File list\r\n"
#define MAGIC_TXT "?*File list\r\n"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

typedef enum {
    NANOSCOPE_FILE_TYPE_NONE = 0,
    NANOSCOPE_FILE_TYPE_BIN,
    NANOSCOPE_FILE_TYPE_TXT
} NanoscopeFileType;

typedef enum {
    NANOSCOPE_VALUE_OLD = 0,
    NANOSCOPE_VALUE_VALUE,
    NANOSCOPE_VALUE_SCALE,
    NANOSCOPE_VALUE_SELECT
} NanoscopeValueType;

/*
 * Old-style record is
 * \Foo: HardValue
 * New-style record is
 * \@Bar: V [SoftScale] (HardScale) HardValue
 * where SoftScale and HardScale are optional.
 */
typedef struct {
    NanoscopeValueType type;
    const gchar *soft_scale;
    gdouble hard_scale;
    const gchar *hard_scale_units;
    gdouble hard_value;
    const gchar *hard_value_str;
    const gchar *hard_value_units;
} NanoscopeValue;

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} NanoscopeData;

typedef struct {
    GtkWidget *data_view;
    GwyContainer *data;
    GList *list;
} NanoscopeDialogControls;

static gboolean        module_register     (const gchar *name);
static gint            nanoscope_detect    (const gchar *filename,
                                            gboolean only_name);
static GwyContainer*   nanoscope_load      (const gchar *filename);
static GwyDataField*   hash_to_data_field  (GHashTable *hash,
                                            GHashTable *scannerlist,
                                            GHashTable *scanlist,
                                            NanoscopeFileType file_type,
                                            guint bufsize,
                                            gchar *buffer,
                                            gint gxres,
                                            gint gyres,
                                            gchar **p);
static NanoscopeData*  select_which_data   (GList *list);
static gboolean        read_ascii_data     (gint n,
                                            gdouble *data,
                                            gchar **buffer,
                                            gint bpp);
static gboolean        read_binary_data    (gint n,
                                            gdouble *data,
                                            gchar *buffer,
                                            gint bpp);
static GHashTable*     read_hash           (gchar **buffer);

static void            get_scan_list_res   (GHashTable *hash,
                                            gint *xres,
                                            gint *yres);
static GwySIUnit*      get_physical_scale  (GHashTable *hash,
                                            GHashTable *scannerlist,
                                            GHashTable *scanlist,
                                            gdouble *scale);
static void            fill_metadata       (GwyContainer *data,
                                            GHashTable *hash,
                                            GList *list);
static NanoscopeValue* parse_value         (const gchar *key,
                                            gchar *line);
static gboolean        require_keys        (GHashTable *hash,
                                            ...);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Veeco Nanoscope data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.9.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo nanoscope_func_info = {
        "nanoscope",
        N_("Nanoscope files"),
        (GwyFileDetectFunc)&nanoscope_detect,
        (GwyFileLoadFunc)&nanoscope_load,
        NULL,
    };

    gwy_file_func_register(name, &nanoscope_func_info);

    return TRUE;
}

static gint
nanoscope_detect(const gchar *filename,
                 gboolean only_name)
{
    FILE *fh;
    gint score;
    gchar magic[MAGIC_SIZE];

    if (only_name)
        return 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && (memcmp(magic, MAGIC_TXT, MAGIC_SIZE) == 0
            || memcmp(magic, MAGIC_BIN, MAGIC_SIZE) == 0))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename)
{
    GObject *object = NULL;
    GError *err = NULL;
    gchar *buffer = NULL;
    gchar *p;
    guint size = 0;
    NanoscopeFileType file_type;
    NanoscopeData *ndata;
    NanoscopeValue *val;
    GHashTable *hash, *scannerlist = NULL, *scanlist = NULL;
    GList *l, *list = NULL;
    gint xres = 0, yres = 0;
    gboolean ok;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    file_type = NANOSCOPE_FILE_TYPE_NONE;
    if (size > MAGIC_SIZE) {
        if (!memcmp(buffer, MAGIC_TXT, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
        else if (!memcmp(buffer, MAGIC_BIN, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_BIN;
    }
    if (!file_type) {
        g_warning("File %s doesn't seem to be a nanoscope file", filename);
        g_free(buffer);
        return NULL;
    }
    gwy_debug("File type: %d", file_type);
    /* as already know file_type, fix the first char for hash reading */
    *buffer = '\\';

    p = buffer;
    while ((hash = read_hash(&p))) {
        ndata = g_new0(NanoscopeData, 1);
        ndata->hash = hash;
        list = g_list_append(list, ndata);
    }
    ok = TRUE;
    for (l = list; ok && l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        hash = ndata->hash;
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Scanner list") == 0) {
            scannerlist = hash;
            continue;
        }
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Ciao scan list") == 0) {
            get_scan_list_res(hash, &xres, &yres);
            scanlist = hash;
        }
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Ciao image list"))
            continue;

        ndata->data_field = hash_to_data_field(hash, scannerlist, scanlist,
                                               file_type, size, buffer,
                                               xres, yres,
                                               &p);
        ok = ok && ndata->data_field;
    }

    /* select (let user select) which data to load */
    ndata = ok ? select_which_data(list) : NULL;
    if (ndata) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(ndata->data_field));
        if ((val = g_hash_table_lookup(ndata->hash, "@2:Image Data"))
            && val->soft_scale)
            gwy_container_set_string_by_name(GWY_CONTAINER(object),
                                             "/filename/title",
                                             g_strdup(val->soft_scale));
        fill_metadata(GWY_CONTAINER(object), ndata->hash, list);
    }

    /* unref all data fields, the container already keeps a reference to the
     * right one */
    g_free(buffer);
    for (l = list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        g_hash_table_destroy(ndata->hash);
        gwy_object_unref(ndata->data_field);
        g_free(ndata);
    }
    g_list_free(list);

    return (GwyContainer*)object;
}

static void
get_scan_list_res(GHashTable *hash,
                  gint *xres, gint *yres)
{
    NanoscopeValue *val;

    /* XXX: Some observed files contained correct dimensions only in
     * a global section, sizes in `image list' sections were bogus.
     * Version: 0x05300001 */
    if ((val = g_hash_table_lookup(hash, "Samps/line")))
        *xres = ROUND(val->hard_value);
    if ((val = g_hash_table_lookup(hash, "Lines")))
        *yres = ROUND(val->hard_value);
    gwy_debug("Global xres, yres = %d, %d", *xres, *yres);
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    static gchar buffer[256];
    gchar *key = (gchar*)hkey;
    NanoscopeValue *val = (NanoscopeValue*)hvalue;
    gchar *s, *v, *w;

    if (!strcmp(key, "#self")
        || !val->hard_value_str
        || !val->hard_value_str[0])
        return;

    if (key[0] == '@')
        key++;
    /* FIXME: naughty /-avoiding trick */
    s = gwy_strreplace(key, "/", "∕", (guint)-1);
    g_snprintf(buffer, sizeof(buffer), "/meta/%s", s);
    v = g_strdup(val->hard_value_str);
    if (strchr(v, '\272')) {
        w = gwy_strreplace(v, "\272", "deg", -1);
        g_free(v);
        v = w;
    }
    if (strchr(v, '~')) {
        w = gwy_strreplace(v, "~", "µ", -1);
        g_free(v);
        v = w;
    }
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data), buffer, v);
    g_free(s);
}

static void
fill_metadata(GwyContainer *data,
              GHashTable *hash,
              GList *list)
{
    static const gchar *hashes[] = {
        "File list", "Scanner list", "Equipment list", "Ciao scan list",
    };
    GList *l;
    guint i;

    for (l = list; l; l = g_list_next(l)) {
        GHashTable *h = ((NanoscopeData*)l->data)->hash;
        for (i = 0; i < G_N_ELEMENTS(hashes); i++) {
            if (strcmp(g_hash_table_lookup(h, "#self"), hashes[i]) == 0) {
                g_hash_table_foreach(h, add_metadata, data);
                break;
            }
        }
    }
    g_hash_table_foreach(hash, add_metadata, data);
}


static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   GHashTable *scannerlist,
                   GHashTable *scanlist,
                   NanoscopeFileType file_type,
                   guint bufsize,
                   gchar *buffer,
                   gint gxres,
                   gint gyres,
                   gchar **p)
{
    NanoscopeValue *val;
    GwyDataField *dfield;
    GwySIUnit *unitz, *unitxy;
    const gchar *s;
    gchar *end;
    gchar un[5];
    gint xres, yres, bpp, offset, size, power10;
    gdouble xreal, yreal, q;
    gdouble *data;

    if (!require_keys(hash, "Samps/line", "Number of lines", "Bytes/pixel",
                      "Scan size", "Data offset", "Data length", NULL))
        return NULL;

    val = g_hash_table_lookup(hash, "Samps/line");
    xres = ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Number of lines");
    yres = ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Bytes/pixel");
    bpp = ROUND(val->hard_value);

    /* scan size */
    val = g_hash_table_lookup(hash, "Scan size");
    xreal = g_ascii_strtod(val->hard_value_str, &end);
    if (errno || *end != ' ') {
        g_warning("Cannot parse <Scan size>: <%s>", val->hard_value_str);
        return NULL;
    }
    s = end+1;
    yreal = g_ascii_strtod(s, &end);
    if (errno || *end != ' ') {
        g_warning("Cannot parse <Scan size>: <%s>", s);
        return NULL;
    }
    if (sscanf(end+1, "%4s", un) != 1) {
        g_warning("Cannot parse <Scan size>: <%s>", s);
        return NULL;
    }
    unitxy = GWY_SI_UNIT(gwy_si_unit_new_parse(un, &power10));
    q = exp(G_LN10*power10);
    xreal *= q;
    yreal *= q;

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        val = g_hash_table_lookup(hash, "Data offset");
        offset = ROUND(val->hard_value);

        val = g_hash_table_lookup(hash, "Data length");
        size = ROUND(val->hard_value);
        if (size != bpp*xres*yres) {
            /* Keep square pixels */
            if (gxres) {
                xreal *= (gdouble)gxres/xres;
                xres = gxres;
            }
            if (gyres) {
                yreal *= (gdouble)gyres/yres;
                yres = gyres;
            }
            if (size != bpp*xres*yres) {
               g_warning("Data size %d != %d bpp*xres*yres",
                         size, bpp*xres*yres);
                return NULL;
            }
        }

        if (offset + size > (gint)bufsize) {
            g_warning("Data don't fit to the file");
            return NULL;
        }
    }

    q = 1.0;
    unitz = get_physical_scale(hash, scannerlist, scanlist, &q);
    if (!unitz)
        return NULL;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                               FALSE));
    data = gwy_data_field_get_data(dfield);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_TXT:
        if (!read_ascii_data(xres*yres, data, p, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        case NANOSCOPE_FILE_TYPE_BIN:
        if (!read_binary_data(xres*yres, data, buffer + offset, bpp)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_multiply(dfield, q);
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);

    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    g_object_unref(unitxy);

    return dfield;
}

static GwySIUnit*
get_physical_scale(GHashTable *hash,
                   GHashTable *scannerlist,
                   GHashTable *scanlist,
                   gdouble *scale)
{
    GwySIUnit *siunit, *siunit2;
    NanoscopeValue *val, *sval;
    gchar *key;
    gint q;

    /* XXX: This is a damned heuristics.  For some value types we try to guess
     * a different quantity scale to look up. */
    if (!(val = g_hash_table_lookup(hash, "@2:Z scale"))) {
        g_warning("`@2:Z scale' not found");
        return NULL;
    }
    key = g_strdup_printf("@%s", val->soft_scale);

    if (!(sval = g_hash_table_lookup(scannerlist, key))
        && (!scanlist || !(sval = g_hash_table_lookup(scanlist, key)))) {
        g_warning("`%s' not found", key);
        g_free(key);
        /* XXX */
        *scale = val->hard_value;
        return GWY_SI_UNIT(gwy_si_unit_new(""));
    }

    *scale = val->hard_value*sval->hard_value;

    if (!sval->hard_value_units || !*sval->hard_value_units) {
        if (strcmp(val->soft_scale, "Sens. Phase") == 0)
            siunit = GWY_SI_UNIT(gwy_si_unit_new("deg"));
        else
            siunit = GWY_SI_UNIT(gwy_si_unit_new("V"));
    }
    else {
        siunit = GWY_SI_UNIT(gwy_si_unit_new_parse(sval->hard_value_units, &q));
        siunit2 = GWY_SI_UNIT(gwy_si_unit_new("V"));
        gwy_si_unit_multiply(siunit, siunit2, siunit);
        gwy_debug("Scale1 = %g V/LSB", val->hard_value);
        gwy_debug("Scale2 = %g %s", sval->hard_value, sval->hard_value_units);
        *scale *= exp(G_LN10*q);
        gwy_debug("Total scale = %g %s/LSB",
                  *scale, gwy_si_unit_get_unit_string(siunit));
        g_object_unref(siunit2);
    }
    g_free(key);

    return siunit;
}

static void
selection_changed(GtkWidget *button,
                  NanoscopeDialogControls *controls)
{
    guint i;
    GList *l;
    GwyDataField *dfield;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    gwy_debug("%u", i);
    g_assert(i != (guint)-1);
    l = g_list_nth(controls->list, i);
    dfield = ((NanoscopeData*)l->data)->data_field;
    gwy_container_set_object_by_name(controls->data, "/0/data",
                                     G_OBJECT(dfield));
    gwy_data_view_update(GWY_DATA_VIEW(controls->data_view));
}

static NanoscopeData*
select_which_data(GList *list)
{
    NanoscopeData *ndata, *ndata0;
    NanoscopeDialogControls controls;
    NanoscopeValue *val;
    GtkObject *layer;
    GwyDataField *dfield;
    GtkWidget *dialog, *label, *hbox, *vbox, *align;
    GwyEnum *choices;
    GList *l;
    GSList *radio, *rl;
    gint i, count, response;
    gint xres, yres;
    gdouble zoomval;

    count = 0;
    ndata0 = NULL;
    l = NULL;
    while (list) {
        ndata = (NanoscopeData*)list->data;
        if (ndata->data_field) {
            count++;
            l = g_list_append(l, ndata);
            if (!ndata0)
                ndata0 = ndata;
        }
        list = g_list_next(list);
    }
    controls.list = l;
    if (count == 0 || count == 1) {
        g_list_free(controls.list);
        return ndata0;
    }

    choices = g_new(GwyEnum, count);
    i = 0;
    for (l = controls.list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        val = g_hash_table_lookup(ndata->hash, "@2:Image Data");
        choices[i].name = val->hard_value_str;
        choices[i].value = i;
        i++;
    }

    dialog = gtk_dialog_new_with_buttons(_("Select Data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 20);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);

    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    vbox = gtk_vbox_new(TRUE, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);

    label = gtk_label_new(_("Data to load:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    radio = gwy_radio_buttons_create(choices, count, "data",
                                     G_CALLBACK(selection_changed), &controls,
                                     0);
    for (i = 0, rl = radio; rl; i++, rl = g_slist_next(rl))
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(rl->data), TRUE, TRUE, 0);

    /* preview */
    align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

    l = g_list_nth(controls.list, 0);
    dfield = ((NanoscopeData*)l->data)->data_field;
    controls.data = GWY_CONTAINER(gwy_container_new());
    gwy_container_set_object_by_name(controls.data, "/0/data",
                                     G_OBJECT(dfield));
    xres = gwy_data_field_get_xres(GWY_DATA_FIELD(dfield));
    yres = gwy_data_field_get_yres(GWY_DATA_FIELD(dfield));
    zoomval = 120.0/MAX(xres, yres);

    controls.data_view = gwy_data_view_new(controls.data);
    g_object_unref(controls.data);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.data_view), zoomval);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.data_view),
                                 GWY_PIXMAP_LAYER(layer));
    gtk_container_add(GTK_CONTAINER(align), controls.data_view);

    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_free(choices);
            g_list_free(list);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    response = GPOINTER_TO_INT(gwy_radio_buttons_get_current(radio, "data"));
    gtk_widget_destroy(dialog);

    l = g_list_nth(controls.list, response);
    ndata0 = (NanoscopeData*)l->data;

    g_free(choices);
    g_list_free(controls.list);

    return ndata0;
}

static gboolean
read_ascii_data(gint n, gdouble *data,
                gchar **buffer,
                gint bpp)
{
    gint i;
    gdouble q;
    gchar *end;
    long l, min, max;

    q = 1.0/(1 << (8*bpp));
    min = 10000000;
    max = -10000000;
    for (i = 0; i < n; i++) {
        /*data[i] = q*strtol(*buffer, &end, 10);*/
        l = strtol(*buffer, &end, 10);
        min = MIN(l, min);
        max = MAX(l, max);
        data[i] = q*l;
        if (end == *buffer) {
            g_warning("Garbage after data sample #%d", i);
            return FALSE;
        }
        *buffer = end;
    }
    gwy_debug("min = %ld, max = %ld", min, max);
    return TRUE;
}

/* FIXME: We hope Nanoscope files always use little endian, because we only
 * have seen them on Intel. */
static gboolean
read_binary_data(gint n, gdouble *data,
                 gchar *buffer,
                 gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 1:
        for (i = 0; i < n; i++)
            data[i] = q*buffer[i];
        break;

        case 2:
        {
            gint16 *p = (gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT16_FROM_LE(p[i]);
        }
        break;

        case 4:
        {
            gint32 *p = (gint32*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT32_FROM_LE(p[i]);
        }
        break;

        default:
        g_warning("bpp = %d unimplemented", bpp);
        return FALSE;
        break;
    }

    return TRUE;
}

static GHashTable*
read_hash(gchar **buffer)
{
    GHashTable *hash;
    NanoscopeValue *value;
    gchar *line, *colon;

    line = gwy_str_next_line(buffer);
    if (line[0] != '\\' || line[1] != '*')
        return NULL;
    if (!strcmp(line, "\\*File list end")) {
        gwy_debug("FILE LIST END");
        return NULL;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "#self", line + 2);    /* self */
    gwy_debug("hash table <%s>", line + 2);
    while ((*buffer)[0] == '\\' && (*buffer)[1] && (*buffer)[1] != '*') {
        line = gwy_str_next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2]) {
            g_warning("Truncated line <%s>", line ? line : "(null)");
            goto fail;
        }
        colon = strchr(line + 3, ':');
        if (!colon || !isspace(colon[1])) {
            g_warning("No colon in line <%s>", line);
            goto fail;
        }
        *colon = '\0';
        do {
            colon++;
        } while (g_ascii_isspace(*colon));
        value = parse_value(line, colon);
        if (value)
            g_hash_table_insert(hash, line, value);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

/* General parameter line parser */
static NanoscopeValue*
parse_value(const gchar *key, gchar *line)
{
    NanoscopeValue *val;
    gchar *p, *q;

    val = g_new0(NanoscopeValue, 1);

    /* old-style values */
    if (key[0] != '@') {
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ' && !strchr(p+1, ' ')) {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        return val;
    }

    /* type */
    switch (line[0]) {
        case 'V':
        val->type = NANOSCOPE_VALUE_VALUE;
        break;

        case 'S':
        val->type = NANOSCOPE_VALUE_SELECT;
        break;

        case 'C':
        val->type = NANOSCOPE_VALUE_SCALE;
        break;

        default:
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
        break;
    }

    line++;
    if (line[0] != ' ') {
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
    }
    do {
        line++;
    } while (g_ascii_isspace(*line));

    /* softscale */
    if (line[0] == '[') {
        if (!(p = strchr(line, ']'))) {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        if (p-line-1 > 0) {
            *p = '\0';
            val->soft_scale = line+1;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hardscale (probably useless) */
    if (line[0] == '(') {
        do {
            line++;
        } while (g_ascii_isspace(*line));
        if (!(p = strchr(line, ')'))) {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        val->hard_scale = g_ascii_strtod(line, &q);
        while (g_ascii_isspace(*q))
            q++;
        if (p-q > 0) {
            *p = '\0';
            val->hard_scale_units = q;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hard value (everything else) */
    switch (val->type) {
        case NANOSCOPE_VALUE_SELECT:
        val->hard_value_str = line;
        break;

        case NANOSCOPE_VALUE_SCALE:
        val->hard_value = g_ascii_strtod(line, &p);
        break;

        case NANOSCOPE_VALUE_VALUE:
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ' && !strchr(p+1, ' ')) {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return val;
}

static gboolean
require_keys(GHashTable *hash,
             ...)
{
    va_list ap;
    const gchar *key;

    va_start(ap, hash);
    while ((key = va_arg(ap, const gchar *))) {
        if (!g_hash_table_lookup(hash, key)) {
            g_warning("Parameter <%s> not found", key);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

