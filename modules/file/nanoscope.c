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

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} NanoscopeData;

typedef struct {
    GtkWidget *data_view;
    GwyContainer *data;
    GList *list;
} NanoscopeDialogControls;

static gboolean       module_register     (const gchar *name);
static gint           nanoscope_detect    (const gchar *filename,
                                           gboolean only_name);
static GwyContainer*  nanoscope_load      (const gchar *filename);
static GwyDataField*  hash_to_data_field  (GHashTable *hash,
                                           NanoscopeFileType file_type,
                                           gsize bufsize,
                                           gchar *buffer,
                                           gdouble zscale,
                                           gdouble curscale,
                                           gchar **p);
static NanoscopeData* select_which_data   (GList *list);
static gboolean       read_ascii_data     (gint n,
                                           gdouble *data,
                                           gchar **buffer,
                                           gint bpp);
static gboolean       read_binary_data    (gint n,
                                           gdouble *data,
                                           gchar *buffer,
                                           gint bpp);
static GHashTable*    read_hash           (gchar **buffer);
static gchar*         next_line           (gchar **buffer);

static gchar*         get_data_name        (GHashTable *hash);
static void           get_value_scales    (GHashTable *hash,
                                           gdouble *zscale,
                                           gdouble *curscale);
static void           fill_metadata       (GwyContainer *data,
                                           GHashTable *hash,
                                           GList *list);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "nanoscope",
    "Load Nanoscope data.",
    "Yeti <yeti@gwyddion.net>",
    "0.6",
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
        "Nanoscope files",
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
    gsize size = 0;
    NanoscopeFileType file_type;
    NanoscopeData *ndata;
    GHashTable *hash;
    GList *l, *list = NULL;
    /* FIXME defaults */
    gdouble zscale = 9.583688e-9;
    gdouble curscale = 10.0e-9;
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
            get_value_scales(hash, &zscale, &curscale);
            continue;
        }
        if (strcmp(g_hash_table_lookup(hash, "#self"), "Ciao image list"))
            continue;

        ndata->data_field = hash_to_data_field(hash, file_type,
                                               size, buffer,
                                               zscale, curscale, &p);
        ok = ok && ndata->data_field;
    }

    /* select (let user select) which data to load */
    ndata = ok ? select_which_data(list) : NULL;
    if (ndata) {
        object = gwy_container_new();
        gwy_container_set_object_by_name(GWY_CONTAINER(object), "/0/data",
                                         G_OBJECT(ndata->data_field));
        if ((p = get_data_name(ndata->hash)))
            gwy_container_set_string_by_name(GWY_CONTAINER(object),
                                             "/filename/title", p);
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

static gchar*
get_data_name(GHashTable *hash)
{
    gchar *name, *p1, *p2;

    name = g_hash_table_lookup(hash, "@2:Image Data");
    if (!name)
        return NULL;

    if ((p1 = strchr(name, '[')) && (p2 = strchr(p1+1, ']')))
        return g_strndup(p1 + 1, p2 - p1 - 1);

    if ((p1 = strchr(name, '"')) && (p2 = strchr(p1+1, '"')))
        return g_strndup(p1 + 1, p2 - p1 - 1);

    return NULL;
}

static void
get_value_scales(GHashTable *hash,
                 gdouble *zscale,
                 gdouble *curscale)
{
    gchar *s, *end, un[6];

    /* z sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Zscan"))) {
        g_warning("`@Sens. Zscan' not found");
        return;
    }
    if (s[0] != 'V' || s[1] != ' ') {
        g_warning("Cannot parse `@Sens. Zscan': <%s>", s+2);
        return;
    }
    *zscale = g_ascii_strtod(s+2, &end);
    if (errno || *end != ' ' || sscanf(end+1, "%5s", un) != 1) {
        g_warning("Cannot parse `@Sens. Zscan': <%s>", s+2);
        return;
    }
    if (strcmp(un, "nm/V") == 0)
        *zscale /= 1e9;
    else if (strcmp(un, "~m/V") == 0 || strcmp(un, "um/V") == 0)
        *zscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *zscale /= 1e9;
    }

    /* current sensitivity */
    if (!(s = g_hash_table_lookup(hash, "@Sens. Current"))) {
        g_warning("`@Sens. Current' not found");
         return;
    }
    if (s[0] != 'V' || s[1] != ' ') {
        g_warning("Cannot parse `@Sens. Current': <%s>", s+2);
        return;
    }
    *curscale = g_ascii_strtod(s+2, &end);
    if (errno || *end != ' ' || sscanf(end+1, "%5s", un) != 1) {
        g_warning("Cannot parse `@Sens. Current': <%s>", s+2);
        return;
    }
    if (strcmp(un, "pA/V") == 0)
        *curscale /= 1e12;
    if (strcmp(un, "nA/V") == 0)
        *curscale /= 1e9;
    else if (strcmp(un, "~A/V") == 0 || strcmp(un, "uA/V") == 0)
        *curscale /= 1e6;
    else {
        g_warning("Cannot understand z units: <%s>", un);
        *curscale /= 1e9;
    }
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    static gchar buffer[256];
    gchar *key = (gchar*)hkey;
    gchar *value = (gchar*)hvalue;
    gchar *s;

    if (!strcmp(key, "#self") || key[0] == '@' || !value[0])
        return;

    /* FIXME: naughty /-avoiding trick */
    s = gwy_strreplace(hkey, "/", "∕", (gsize)-1);
    g_snprintf(buffer, sizeof(buffer), "/meta/%s", s);
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                     buffer, g_strdup(value));
    g_free(s);
}

static void
fill_metadata(GwyContainer *data,
              G_GNUC_UNUSED GHashTable *hash,
              GList *list)
{
    static const gchar *hashes[] = {
        "File list", "Scanner list", "Equipment list",
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
}


static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   NanoscopeFileType file_type,
                   gsize bufsize,
                   gchar *buffer,
                   gdouble zscale,
                   gdouble curscale,
                   gchar **p)
{
    GwyDataField *dfield;
    GObject *unit;
    const gchar *s;
    gchar *t, *end;
    gchar un[5];
    gint xres, yres, bpp, offset, size;
    gdouble xreal, yreal, q, zmagnify = 1.0, zscalesens = 1.0;
    gdouble *data;
    gboolean is_current;  /* assume height if not current, and ignore phase,
                             etc. FIXME: should try to interpret the units */

    if (!(s = g_hash_table_lookup(hash, "@2:Image Data"))) {
        g_warning("No `@2 Image Data' found");
        return NULL;
    }
    is_current = strstr(s, "[Current]") || strstr(s, "\"Current\"");
    gwy_debug("is current = %d", is_current);

    if (!(s = g_hash_table_lookup(hash, "Samps/line"))) {
        g_warning("`Samps/line' not found");
        return NULL;
    }
    xres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Number of lines"))) {
        g_warning("`Number of lines' not found");
        return NULL;
    }
    yres = atoi(s);

    if (!(s = g_hash_table_lookup(hash, "Bytes/pixel"))) {
        g_warning("`Bytes/pixel' not found");
        return NULL;
    }
    bpp = atoi(s);

    /* scan size */
    if (!(s = g_hash_table_lookup(hash, "Scan size"))) {
        g_warning("`Scan size' not found");
        return NULL;
    }
    xreal = g_ascii_strtod(s, &end);
    if (errno || *end != ' ') {
        g_warning("Cannot parse `Scan size': <%s>", s);
        return NULL;
    }
    t = end+1;
    yreal = g_ascii_strtod(t, &end);
    if (errno || *end != ' ') {
        g_warning("Cannot parse `Scan size': <%s>", s);
        return NULL;
    }
    if (sscanf(end+1, "%4s", un) != 1) {
        g_warning("Cannot parse `Scan size': <%s>", s);
        return NULL;
    }
    if (strcmp(un, "nm") == 0) {
        xreal /= 1e9;
        yreal /= 1e9;
    }
    else if (strcmp(un, "~m") == 0 || strcmp(un, "um") == 0) {
        xreal /= 1e6;
        yreal /= 1e6;
    }
    else {
        g_warning("Cannot understand size units: <%s>", un);
        xreal /= 1e9;
        yreal /= 1e9;
    }

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        if (!(s = g_hash_table_lookup(hash, "Data offset"))) {
            g_warning("`Data offset' not found");
            return NULL;
        }
        offset = atoi(s);

        if (!(s = g_hash_table_lookup(hash, "Data length"))) {
            g_warning("`Data length' not found");
            return NULL;
        }
        size = atoi(s);
        if (size != bpp*xres*yres) {
            g_warning("Data size %d != %d bpp*xres*yres", size, bpp*xres*yres);
            return NULL;
        }

        if (offset + size > (gint)bufsize) {
            g_warning("Data don't fit to the file");
            return NULL;
        }
    }

    /* XXX: now ignored */
    if (!(t = g_hash_table_lookup(hash, "@Z magnify")))
        g_warning("`@Z magnify' not found");
    else {
        if (!(s = strchr(t, ']')))
            g_warning("Cannot parse `@Z magnify': <%s>", t);
        else {
            zmagnify = g_ascii_strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@Z magnify' value: <%s>", s+1);
                zmagnify = 1.0;
            }
        }
    }

    if (!(t = g_hash_table_lookup(hash, "@2:Z scale")))
        g_warning("`@2:Z scale' not found");
    else {
        if (!(s = strchr(t, '(')))
            g_warning("Cannot parse `@2:Z scale': <%s>", t);
        else {
            zscalesens = g_ascii_strtod(s+1, &t);
            if (t == s+1) {
                g_warning("Cannot parse `@2:Z scale' value: <%s>", s+1);
                zscalesens = 1.0;
            }
        }
    }

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
    /*q = (is_current ? curscale : zscale) * zscalesens/zmagnify*10;*/
    q = (is_current ? curscale : zscale) * zscalesens*10;
    gwy_debug("curscale = %fe-9, zscale = %fe-9, zscalesens = %f, zmagnify = %f",
              curscale*1e9, zscale*1e9, zscalesens, zmagnify);
    gwy_data_field_multiply(dfield, q);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, GWY_SI_UNIT(unit));
    g_object_unref(unit);

    unit = gwy_si_unit_new(is_current ? "A" : "m");
    gwy_data_field_set_si_unit_z(dfield, GWY_SI_UNIT(unit));
    g_object_unref(unit);

    return dfield;
}

static void
selection_changed(GtkWidget *button,
                  NanoscopeDialogControls *controls)
{
    gsize i;
    GList *l;
    GwyDataField *dfield;

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    i = gwy_radio_buttons_get_current_from_widget(button, "data");
    gwy_debug("%u", i);
    g_assert(i != (gsize)-1);
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
        choices[i].name = g_hash_table_lookup(ndata->hash, "@2:Image Data");
        if (choices[i].name[0] && choices[i].name[1])
            choices[i].name += 2;
        choices[i].value = i;
        i++;
    }

    dialog = gtk_dialog_new_with_buttons(_("Select data"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

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
                data[i] = q*p[i];
        }
        break;

        case 4:
        {
            gint32 *p = (gint32*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*p[i];
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
    gchar *line, *colon;

    line = next_line(buffer);
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
        line = next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2])
            goto fail;
        colon = strchr(line + 3, ':');
        if (!colon || !isspace(colon[1]))
            goto fail;
        *colon = '\0';
        g_hash_table_insert(hash, line, colon + 2);
        gwy_debug("<%s>: <%s>", line, colon + 2);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}


/**
 * next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from @buffer.
 *
 * @buffer is updated to point after the end of the line and the "\n" 
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The line is not duplicated, the returned pointer points somewhere
 *          to @buffer.
 **/
static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

