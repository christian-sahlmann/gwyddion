/*
 *  $Id$
 *  Copyright (C) 2009,2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-mapvue">
 *   <comment>MapVue data file</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x57\x04\x02\x00"/>
 *   </magic>
 *   <glob pattern="*.map"/>
 *   <glob pattern="*.MAP"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * MapVue
 * .map
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

/* This is actually image type (dec 1111 for phase or 2222 for intensity)
 * + the first reference tag (dec 2) */
#define MAGIC1 "\x57\x04\x02\x00"
#define MAGIC2 "\xae\x08\x02\x00"
#define MAGIC_SIZE (sizeof(MAGIC1)-1)

#define EXTENSION ".map"

enum {
    TAG_MARKER2 = 0xccaa,
    TAG_MARKER4 = 0xccbb,
    END_OF_HEADER = 301
};

typedef guint (*MapVueReadGroup)(const guchar *p,
                                 gsize size,
                                 gpointer group,
                                 GError **error);

/* Obsolete dimensions */
typedef struct {
    gint reftag;
    gint column_start;
    gint row_start;
    gint n_columns;
    gint n_rows;
} MapVueGroup2;

/* Dimensions */
typedef struct {
    gint reftag;
    gint column_start;
    gint row_start;
    gint n_columns;
    gint n_rows;
} MapVueGroup3;

/* Info fields */
typedef struct {
    gint reftag;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
    gchar time[9];
    gchar date[9];
} MapVueGroup51;

/* Labelling info, date/time */
typedef struct {
    gint reftag;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
    gchar field5[256];
    gchar time[9];
    gchar date[9];
} MapVueGroup52;

/* Labelling info fields 6&7 */
typedef struct {
    gint reftag;
    gint tagmark;
    gint tagsize;
    gchar field6[256];
    gchar field7[256];
} MapVueGroup53;

/* Wedge/wavelength */
typedef struct {
    gint reftag;
    gdouble wedge;
    gdouble testwedge;
    gdouble wavelength;
    gdouble new_wavelength;
} MapVueGroup101;

/* FIDs */
typedef struct {
    guint n;
    gboolean auto_positioning;
    /* The docs say maximum of four.  If there are more, ignore them. */
    gdouble x_coordinates[4];
    gdouble y_coordinates[4];
} MapVueFids;

typedef struct {
    gint reftag;
    MapVueFids int_fids;
    MapVueFids test_fids;
    MapVueFids dim_fids;
    MapVueFids A_fids;
    MapVueFids B_fids;
    MapVueFids C_fids;
    MapVueFids D_fids;
    MapVueFids IT_fids;  /* coordinates in mm */
    gdouble dim_distance;
    gchar dim_label[256];
    guint IT_fiducial_index;
    guint center_fiducial_index;
    gdouble x_center_offset;  /* in mm */
    gdouble y_center_offset;  /* in mm */
    gdouble physical_radius;  /* in mm */
} MapVueGroup155;

/* Data scale factor */
typedef struct {
    gint reftag;
    gdouble data_scale_factor;
} MapVueGroup201;

/* Number of segments */
typedef struct {
    gint reftag;
    gint n_segments;
} MapVueGroup451;

/* Data type */
typedef struct {
    gint reftag;
    gint data_type;
} MapVueGroup501;

/* Aspect ratio */
typedef struct {
    gint reftag;
    gdouble x_frame_scale;  /* This seems to be (yres/xres)/(xreal/yreal) */
} MapVueGroup550;

/* Maginification/aspect ratio */
typedef struct {
    gint reftag;
    gdouble magnification;
    gdouble x_frame_scale;  /* This seems to be (yres/xres)/(xreal/yreal) */
    gdouble y_optical_scale;
} MapVueGroup551;

/* Display information */
typedef struct {
    gint reftag;
    gint maximum_frames;
    gint display_rows;
    gint total_rows;
    gint total_columns;
} MapVueGroup651;

/* System information */
typedef struct {
    gint reftag;
    gint system_information;
} MapVueGroup801;

/* XXX: Format documentation seems to mix 851 and 852 together, but the files
 * split them similarly to 52 and 53 */
/* Fields names */
typedef struct {
    gint reftag;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
    gchar field5[256];
} MapVueGroup851;

/* Fields names 6&7 */
typedef struct {
    gint reftag;
    gchar field6[256];
    gchar field7[256];
} MapVueGroup852;

/* Another fields names */
typedef struct {
    gint reftag;
    gchar field1[256];
    gchar field2[256];
    gchar field3[256];
    gchar field4[256];
} MapVueGroup853;

/* Comment fields */
typedef struct {
    gint reftag;
    gchar comment[256];
} MapVueGroup901;

/* Center and radius */
typedef struct {
    gint reftag;
    gdouble x_center_coord;
    gdouble y_center_coord;
    gdouble radius;
    gboolean radius_defined;
} MapVueGroup951;

/* Power */
typedef struct {
    gint reftag;
    gdouble power;
} MapVueGroup1001;

/* Mapping dynamics */
typedef struct {
    gint reftag;
    gdouble map_velocity;
    gdouble map_acceleration;
    gdouble map_runout;
} MapVueGroup1052;

/* Shifts */
typedef struct {
    gint reftag;
    gdouble x_shift;
    gdouble y_shift;
} MapVueGroup1102;

/* Map stage coordinates */
typedef struct {
    gint reftag;
    gdouble stage_x;
    gdouble stage_y;
} MapVueGroup1151;

/* Map stage coordinates */
typedef struct {
    gint reftag;
    gdouble stage_x;
    gdouble stage_y;
    gdouble stage_z;
} MapVueGroup1152;

/* This is not what the files contain at all. */
/* Dissimilar material properties */
typedef struct {
    gint reftag;
    gint64 propagation_factor;
    gint64 index_of_refraction;
    gint64 wavelength;
    gchar material[256];
    gchar name[256];
    gdouble x_coordinates;
    gdouble y_coordinates;
} MapVueGroup1560;

/* Initial phase shift information */
typedef struct {
    gint reftag;
    guint actsft;
    guint numfrms;
    guint offset1;
    guint phssft1;
    guint offset2;
    guint phssft2;
} MapVueGroup1201;

/* Measured phase shift information */
typedef struct {
    gint reftag;
    gdouble sftmean;
    gdouble sftdev;
} MapVueGroup1251;

/* Map scale */
typedef struct {
    gint reftag;
    gdouble phase_shift;
    gdouble map_scale;
} MapVueGroup2002;

/* I do not have any description of the contents, but anyway, these are known */
/* Recipe */
typedef struct {
    gint reftag;
} MapVueGroup2051;

/* Calibration */
typedef struct {
    gint reftag;
} MapVueGroup2101;

/* Reference image */
typedef struct {
    gint reftag;
} MapVueGroup2201;

/* Stitch quality */
typedef struct {
    gint reftag;
    gdouble stitch_quality;
} MapVueGroup2251;

/* Film surface analysis */
typedef struct {
    gint reftag;
    gdouble film_thickness_approx;
    gint film_surface;
} MapVueGroup2351;

typedef struct {
    gint id;
    MapVueGroup2 group2;
    MapVueGroup3 group3;
    MapVueGroup51 group51;
    MapVueGroup52 group52;
    MapVueGroup53 group53;
    MapVueGroup101 group101;
    MapVueGroup155 group155;
    MapVueGroup201 group201;
    MapVueGroup451 group451;
    MapVueGroup501 group501;
    MapVueGroup550 group550;
    MapVueGroup551 group551;
    MapVueGroup651 group651;
    MapVueGroup801 group801;
    MapVueGroup851 group851;
    MapVueGroup852 group852;
    MapVueGroup853 group853;
    MapVueGroup901 group901;
    MapVueGroup951 group951;
    MapVueGroup1001 group1001;
    MapVueGroup1052 group1052;
    MapVueGroup1102 group1102;
    MapVueGroup1151 group1151;
    MapVueGroup1152 group1152;
    MapVueGroup1201 group1201;
    MapVueGroup1251 group1251;
    MapVueGroup1560 group1560;
    MapVueGroup2002 group2002;
    MapVueGroup2051 group2051;
    MapVueGroup2101 group2101;
    MapVueGroup2201 group2201;
    MapVueGroup2251 group2251;
    MapVueGroup2351 group2351;
} MapVueFile;

static gboolean      module_register           (void);
static gint          mapvue_detect             (const GwyFileDetectInfo *fileinfo,
                                                gboolean only_name);
static GwyContainer* mapvue_load               (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static guint         mapvue_read_header        (const guchar *buffer,
                                                gsize size,
                                                MapVueFile *mapvuevile,
                                                GError **error);
static guint         mapvue_skip_group         (const guchar *p,
                                                gsize size,
                                                guint reftag,
                                                GError **error);
static GwyDataField* read_data_field           (gconstpointer p,
                                                gint xres,
                                                gint yres,
                                                guint bpp,
                                                GwyDataField **maskfield);
static gboolean      calculate_scales_from_fids(const MapVueGroup155 *group155,
                                                gdouble aspratio,
                                                gdouble *xscale,
                                                gdouble *yscale);

#define MAPVUE_GROUP_READER(x) \
    static guint mapvue_read_group##x(const guchar *p, gsize size, \
                                      gpointer grpdata, GError **error)

MAPVUE_GROUP_READER(2);
MAPVUE_GROUP_READER(3);
MAPVUE_GROUP_READER(51);
MAPVUE_GROUP_READER(52);
MAPVUE_GROUP_READER(53);
MAPVUE_GROUP_READER(101);
MAPVUE_GROUP_READER(155);
MAPVUE_GROUP_READER(201);
MAPVUE_GROUP_READER(451);
MAPVUE_GROUP_READER(501);
MAPVUE_GROUP_READER(550);
MAPVUE_GROUP_READER(551);
MAPVUE_GROUP_READER(651);
MAPVUE_GROUP_READER(801);
MAPVUE_GROUP_READER(851);
MAPVUE_GROUP_READER(852);
MAPVUE_GROUP_READER(853);
MAPVUE_GROUP_READER(901);
MAPVUE_GROUP_READER(951);
MAPVUE_GROUP_READER(1001);
MAPVUE_GROUP_READER(1052);
MAPVUE_GROUP_READER(1102);
MAPVUE_GROUP_READER(1151);
MAPVUE_GROUP_READER(1152);
MAPVUE_GROUP_READER(1201);
MAPVUE_GROUP_READER(1251);
#if 0
MAPVUE_GROUP_READER(1560);
#endif
MAPVUE_GROUP_READER(2002);
MAPVUE_GROUP_READER(2051);
MAPVUE_GROUP_READER(2101);
MAPVUE_GROUP_READER(2201);
MAPVUE_GROUP_READER(2251);
MAPVUE_GROUP_READER(2351);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports MapVue data files (.map)."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("mapvue",
                           N_("MapVue files (.map)"),
                           (GwyFileDetectFunc)&mapvue_detect,
                           (GwyFileLoadFunc)&mapvue_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mapvue_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION)
               ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0))
        score = 100;

    return score;
}

static GwyContainer*
mapvue_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    MapVueFile mapvuefile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize header_size, expected_size, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL, *mfield;
    gint xres, yres;
    guint bpp;
    gdouble xreal, yreal;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE || (memcmp(buffer, MAGIC1, MAGIC_SIZE) != 0
                              && memcmp(buffer, MAGIC2, MAGIC_SIZE) != 0)) {
        err_FILE_TYPE(error, "MapVue");
        goto fail;
    }

    p = buffer;
    mapvuefile.id = gwy_get_guint16_le(&p);
    gwy_debug("id: %u", mapvuefile.id);
    if (!(header_size = mapvue_read_header(p, size - 2, &mapvuefile, error)))
        goto fail;
    p += header_size;

    /* Try the better tag first even though the presence of both is unlikely. */
    if (mapvuefile.group3.reftag == 3) {
        gwy_debug("obtaining pixel dimensions from tag 3");
        xres = mapvuefile.group3.n_columns;
        yres = mapvuefile.group3.n_rows;
        bpp = 4;
    }
    else if (mapvuefile.group2.reftag == 2) {
        gwy_debug("obtaining pixel dimensions from tag 2");
        xres = mapvuefile.group2.n_columns;
        yres = mapvuefile.group2.n_rows;
        bpp = 2;
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Required tag %u or %u was not found."), 2, 3);
        goto fail;
    }
    expected_size = xres * yres * bpp;

    if (mapvuefile.group551.reftag == 551) {
        gwy_debug("obtaining physical dimensions from tag 551");
        /* FIXME: Just guessing. */
        yreal = 1e-3*yres*mapvuefile.group551.y_optical_scale;
        xreal = yreal*mapvuefile.group551.x_frame_scale*xres/yres;
    }
    else if (mapvuefile.group550.reftag == 550) {
        gdouble aspratio = mapvuefile.group550.x_frame_scale;

        gwy_debug("obtaining physical dimensions from tag 550");
        if (mapvuefile.group155.reftag == 155
            && calculate_scales_from_fids(&mapvuefile.group155,
                                          aspratio, &xreal, &yreal)) {
            xreal *= xres;
            yreal *= yres;
        }
        else {
            g_warning("Cannot figure out dimensions from fiducials, "
                      "setting y size to 1m.");
            yreal = 1.0;
            xreal = yreal*aspratio*xres/yres;
        }
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Required tag %u or %u was not found."), 550, 551);
        goto fail;
    }


    /* XXX: Does not catch premature end of the tag reading cycle! */
    if (err_SIZE_MISMATCH(error, expected_size, size - (p - buffer), FALSE))
        goto fail;

    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    dfield = read_data_field((gconstpointer)p, xres, yres, bpp, &mfield);
    gwy_data_field_set_xreal(dfield, xreal);
    gwy_data_field_set_yreal(dfield, yreal);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

    if (mfield) {
        gwy_container_set_object_by_name(container, "/0/mask", mfield);
        g_object_unref(mfield);
    }

    /*
    meta = mapvue_get_metadata(&ufile);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);
    */

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static guint
mapvue_read_header(const guchar *buffer, gsize size, MapVueFile *mapvuefile,
                   GError **error)
{
    const guchar *p = buffer;

    gwy_clear(mapvuefile, 1);
    while ((gsize)(p - buffer) + 2 < size) {
        MapVueReadGroup readgroup;
        gpointer groupdata;
        guint tagsize, reftag;

        reftag = gwy_get_guint16_le(&p);
        gwy_debug("Found tag %u at pos %04x", reftag, (guint)(p - buffer) - 2);
        /* The groups should be ordered, but do not rely on that. */
        if (reftag == 2) {
            readgroup = &mapvue_read_group2;
            groupdata = &mapvuefile->group2;
        }
        else if (reftag == 3) {
            readgroup = &mapvue_read_group3;
            groupdata = &mapvuefile->group3;
        }
        else if (reftag == 51) {
            readgroup = &mapvue_read_group51;
            groupdata = &mapvuefile->group51;
        }
        else if (reftag == 52) {
            readgroup = &mapvue_read_group52;
            groupdata = &mapvuefile->group52;
        }
        else if (reftag == 53) {
            readgroup = &mapvue_read_group53;
            groupdata = &mapvuefile->group53;
        }
        else if (reftag == 101) {
            readgroup = &mapvue_read_group101;
            groupdata = &mapvuefile->group101;
        }
        else if (reftag == 155) {
            readgroup = &mapvue_read_group155;
            groupdata = &mapvuefile->group155;
        }
        else if (reftag == 201) {
            readgroup = &mapvue_read_group201;
            groupdata = &mapvuefile->group201;
        }
        else if (reftag == 451) {
            readgroup = &mapvue_read_group451;
            groupdata = &mapvuefile->group451;
        }
        else if (reftag == 501) {
            readgroup = &mapvue_read_group501;
            groupdata = &mapvuefile->group501;
        }
        else if (reftag == 550) {
            readgroup = &mapvue_read_group550;
            groupdata = &mapvuefile->group550;
        }
        else if (reftag == 551) {
            readgroup = &mapvue_read_group551;
            groupdata = &mapvuefile->group551;
        }
        else if (reftag == 651) {
            readgroup = &mapvue_read_group651;
            groupdata = &mapvuefile->group651;
        }
        else if (reftag == 801) {
            readgroup = &mapvue_read_group801;
            groupdata = &mapvuefile->group801;
        }
        else if (reftag == 851) {
            readgroup = &mapvue_read_group851;
            groupdata = &mapvuefile->group851;
        }
        else if (reftag == 852) {
            readgroup = &mapvue_read_group852;
            groupdata = &mapvuefile->group852;
        }
        else if (reftag == 853) {
            readgroup = &mapvue_read_group853;
            groupdata = &mapvuefile->group853;
        }
        else if (reftag == 901) {
            readgroup = &mapvue_read_group901;
            groupdata = &mapvuefile->group901;
        }
        else if (reftag == 951) {
            readgroup = &mapvue_read_group951;
            groupdata = &mapvuefile->group951;
        }
        else if (reftag == 1001) {
            readgroup = &mapvue_read_group1001;
            groupdata = &mapvuefile->group1001;
        }
        else if (reftag == 1052) {
            readgroup = &mapvue_read_group1052;
            groupdata = &mapvuefile->group1052;
        }
        else if (reftag == 1102) {
            readgroup = &mapvue_read_group1102;
            groupdata = &mapvuefile->group1102;
        }
        else if (reftag == 1151) {
            readgroup = &mapvue_read_group1151;
            groupdata = &mapvuefile->group1151;
        }
        else if (reftag == 1152) {
            readgroup = &mapvue_read_group1152;
            groupdata = &mapvuefile->group1152;
        }
#if 0
        else if (reftag == 1560) {
            readgroup = &mapvue_read_group1560;
            groupdata = &mapvuefile->group1560;
        }
#endif
        else if (reftag == 1201) {
            readgroup = &mapvue_read_group1201;
            groupdata = &mapvuefile->group1201;
        }
        else if (reftag == 1251) {
            readgroup = &mapvue_read_group1251;
            groupdata = &mapvuefile->group1251;
        }
        else if (reftag == 2002) {
            readgroup = &mapvue_read_group2002;
            groupdata = &mapvuefile->group2002;
        }
        else if (reftag == 2051) {
            readgroup = &mapvue_read_group2051;
            groupdata = &mapvuefile->group2051;
        }
        else if (reftag == 2101) {
            readgroup = &mapvue_read_group2101;
            groupdata = &mapvuefile->group2101;
        }
        else if (reftag == 2201) {
            readgroup = &mapvue_read_group2201;
            groupdata = &mapvuefile->group2201;
        }
        else if (reftag == 2251) {
            readgroup = &mapvue_read_group2251;
            groupdata = &mapvuefile->group2251;
        }
        else if (reftag == 2351) {
            readgroup = &mapvue_read_group2351;
            groupdata = &mapvuefile->group2351;
        }
        else if (reftag == END_OF_HEADER) {
            return (guint)(p - buffer);
        }
        else {
            gwy_debug("Unknown tag %u", reftag);
            tagsize = mapvue_skip_group(p, size - (p - buffer), reftag, error);
            p += tagsize;
            if (!tagsize)
                return 0;

            gwy_debug("Unknown group %u successfully skipped", reftag);
            continue;
        }

        *(gint*)groupdata = reftag;
        tagsize = readgroup(p, size - (p - buffer), groupdata, error);
        p += tagsize;
        if (!tagsize)
            return 0;

        gwy_debug("Read group %u of size %u", reftag, tagsize);
    }

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File end was reached while scanning tags."));

    return 0;
}

static inline gboolean
err_TAG_SIZE(GError **error, guint reftag, guint expected, guint real)
{
    if (expected < real)
        return FALSE;

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Expected tag %u size is %u bytes, "
                  "but the actual size is %u bytes."),
                reftag, expected, real);
    return TRUE;
}

/* A group with marker and tagsize -- which exlcudes reftag, marker and self. */
static guint
mapvue_group_size(const guchar **p, gsize size, guint reftag, GError **error)
{
    guint marker, sizesize, tagsize;

    if (size < 2) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag %u size is %u bytes, "
                      "which is not enough to hold the tag marker."),
                    reftag, (guint)size);
        return 0;
    }

    marker = gwy_get_guint16_le(p);
    if (marker == TAG_MARKER2) {
        if (size < 4) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Tag %u size is %u bytes, "
                          "which is not enough to hold "
                          "the tag marker and size."),
                        reftag, (guint)size);
            return 0;
        }
        tagsize = gwy_get_guint16_le(p);
        sizesize = 2;
    }
    else if (marker == TAG_MARKER4) {
        if (size < 6) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Tag %u size is %u bytes, "
                          "which is not enough to hold "
                          "the tag marker and size."),
                        reftag, (guint)size);
            return 0;
        }
        tagsize = gwy_get_guint32_le(p);
        sizesize = 4;
    }
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag marker is missing on an unknown tag %u."),
                    reftag);
        return 0;
    }

    if (err_TAG_SIZE(error, reftag, tagsize + 2 + sizesize, size))
        return 0;

    return tagsize + 2 + sizesize;
}

/* Read pascal-like string with size checking. */
static guint
mapvue_read_string(const guchar **p, gsize size, gchar *str, GError **error)
{
    guint strsize;

    if (size < 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Character array does not fit into the file."));
        return 0;
    }

    strsize = **p;
    /* gwy_debug("strsize = %u", strsize); */
    (*p)++;

    if (strsize+1 > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Character array does not fit into the file."));
        return 0;
    }

    memcpy(str, *p, strsize);
    str[strsize] = '\0';
    *p += strsize;
    return strsize + 1;
}

static guint
mapvue_read_fids(const guchar **p, gsize size, MapVueFids *fids, GError **error)
{
    guint i;

    if (size < 2) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Fiducial record does not fit into the file."));
        return 0;
    }
    size -= 2;
    fids->n = gwy_get_guint16_le(p);
    gwy_debug("nfids: %u", fids->n);
    if (!fids->n)
        return 2;

    if (size < 2) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Fiducial record does not fit into the file."));
        return 0;
    }
    size -= 2;
    fids->auto_positioning = gwy_get_guint16_le(p);
    if (fids->n > 4)
        g_warning("More than 4 fids.");

    if (size < 2*4*fids->n) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Fiducial record does not fit into the file."));
        return 0;
    }

    for (i = 0; i < MIN(fids->n, 4); i++) {
        fids->x_coordinates[i] = gwy_get_gfloat_le(p);
        gwy_debug("fid x[%u]: %g", i, fids->x_coordinates[i]);
    }
    for (i = 0; i < MIN(fids->n, 4); i++) {
        fids->y_coordinates[i] = gwy_get_gfloat_le(p);
        gwy_debug("fid y[%u]: %g", i, fids->y_coordinates[i]);
    }

    return 4 + 2*4*fids->n;
}

static guint
mapvue_skip_group(const guchar *p, gsize size, guint reftag, GError **error)
{
    guint tagsize = mapvue_group_size(&p, size, reftag, error);

    if (!tagsize)
        return 0;

    return tagsize;
}

static guint
mapvue_read_group2(const guchar *p, gsize size, gpointer grpdata,
                   GError **error)
{
    enum { SIZE = 4*2 };
    MapVueGroup2 *group = (MapVueGroup2*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->column_start = gwy_get_guint16_le(&p);
    group->row_start = gwy_get_guint16_le(&p);
    group->n_columns = gwy_get_guint16_le(&p);
    group->n_rows = gwy_get_guint16_le(&p);
    return SIZE;
}

static guint
mapvue_read_group3(const guchar *p, gsize size, gpointer grpdata,
                   GError **error)
{
    enum { SIZE = 4*4 };
    MapVueGroup3 *group = (MapVueGroup3*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    group->column_start = gwy_get_guint32_le(&p);
    group->row_start = gwy_get_guint32_le(&p);
    group->n_columns = gwy_get_guint32_le(&p);
    group->n_rows = gwy_get_guint32_le(&p);
    return size;
}

static guint
mapvue_read_group51(const guchar *p, gsize size, gpointer grpdata,
                    GError **error)
{
    enum { SIZE = 2*8 };
    guint len, tagsize = 0;
    MapVueGroup51 *group = (MapVueGroup51*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size - tagsize))
        return 0;

    memcpy(group->time, p, 8);
    p += 8;
    memcpy(group->date, p, 8);
    p += 8;

    return tagsize + SIZE;
}

static guint
mapvue_read_group52(const guchar *p, gsize size, gpointer grpdata,
                    GError **error)
{
    enum { SIZE = 2*8 };
    guint len, tagsize = 0;
    MapVueGroup52 *group = (MapVueGroup52*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field5, error)))
        return 0;
    tagsize += len;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size - tagsize))
        return 0;

    memcpy(group->time, p, 8);
    p += 8;
    memcpy(group->date, p, 8);
    p += 8;

    return tagsize + SIZE;
}

static guint
mapvue_read_group53(const guchar *p, gsize size, gpointer grpdata,
                    GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup53 *group = (MapVueGroup53*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field6, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field7, error)))
        return 0;
    tagsize += len;

    return size;
}

static guint
mapvue_read_group101(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 16 };
    MapVueGroup101 *group = (MapVueGroup101*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->wedge = gwy_get_gfloat_le(&p);
    group->testwedge = gwy_get_gfloat_le(&p);
    group->wavelength = gwy_get_gfloat_le(&p);
    group->new_wavelength = gwy_get_gfloat_le(&p);
    return SIZE;
}

static guint
mapvue_read_group155(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2*2 + 3*4 };
    MapVueGroup155 *group = (MapVueGroup155*)grpdata;
    guint len, tagsize = 0;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->int_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->test_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->dim_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->A_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->B_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->C_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->D_fids, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_fids(&p, size - tagsize, &group->IT_fids, error)))
        return 0;
    tagsize += len;

    if (size < tagsize + 4) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag %u size is %u which is not sufficient to hold "
                      "its content."),
                    155, (guint)size);
        return 0;
    }
    group->dim_distance = gwy_get_gfloat_le(&p);
    tagsize += 4;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->dim_label, error)))
        return 0;
    tagsize += len;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size - tagsize))
        return 0;

    group->IT_fiducial_index = gwy_get_guint16_le(&p);
    group->center_fiducial_index = gwy_get_guint16_le(&p);
    group->x_center_offset = gwy_get_gfloat_le(&p);
    group->y_center_offset = gwy_get_gfloat_le(&p);
    group->physical_radius = gwy_get_gfloat_le(&p);

    return SIZE + tagsize;
}

static guint
mapvue_read_group201(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 4 };
    MapVueGroup201 *group = (MapVueGroup201*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->data_scale_factor = gwy_get_gfloat_le(&p);
    return SIZE;
}

static guint
mapvue_read_group451(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2 };
    MapVueGroup451 *group = (MapVueGroup451*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->n_segments = gwy_get_gint16_le(&p);
    return SIZE;
}

static guint
mapvue_read_group501(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2 };
    MapVueGroup501 *group = (MapVueGroup501*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->data_type = gwy_get_gint16_le(&p);
    return SIZE;
}

static guint
mapvue_read_group550(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 4 };
    MapVueGroup550 *group = (MapVueGroup550*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->x_frame_scale = gwy_get_gfloat_le(&p);
    gwy_debug("x_frame_scale: %g", group->x_frame_scale);
    return size;
}

static guint
mapvue_read_group551(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 3*4 };
    MapVueGroup551 *group = (MapVueGroup551*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->magnification = gwy_get_gfloat_le(&p);
    group->x_frame_scale = gwy_get_gfloat_le(&p);
    group->y_optical_scale = gwy_get_gfloat_le(&p);
    gwy_debug("x_frame_scale: %g, y_optical_scale: %g",
              group->x_frame_scale, group->y_optical_scale);
    return SIZE;
}

static guint
mapvue_read_group651(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 8 };
    MapVueGroup651 *group = (MapVueGroup651*)grpdata;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->maximum_frames = gwy_get_guint16_le(&p);
    group->display_rows = gwy_get_guint16_le(&p);
    group->total_rows = gwy_get_guint16_le(&p);
    group->total_columns = gwy_get_guint16_le(&p);
    return SIZE;
}

static guint
mapvue_read_group801(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 2 };
    MapVueGroup801 *group = (MapVueGroup801*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->system_information = gwy_get_guint16_le(&p);

    return size;
}

static guint
mapvue_read_group851(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup851 *group = (MapVueGroup851*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field5, error)))
        return 0;
    tagsize += len;

    return size;
}

static guint
mapvue_read_group852(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup852 *group = (MapVueGroup852*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field6, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field7, error)))
        return 0;
    tagsize += len;

    return size;
}

static guint
mapvue_read_group853(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup853 *group = (MapVueGroup853*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field1, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field2, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field3, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->field4, error)))
        return 0;
    tagsize += len;

    return size;
}

static guint
mapvue_read_group901(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    guint len, tagsize = 0;
    MapVueGroup901 *group = (MapVueGroup901*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->comment, error)))
        return 0;
    tagsize += len;

    return size;
}

static guint
mapvue_read_group951(const guchar *p, gsize size, gpointer grpdata,
                     GError **error)
{
    enum { SIZE = 3*4 + 1 };
    MapVueGroup951 *group = (MapVueGroup951*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->x_center_coord = gwy_get_gfloat_le(&p);
    group->y_center_coord = gwy_get_gfloat_le(&p);
    group->radius = gwy_get_gfloat_le(&p);
    group->radius_defined = *(p++);

    return size;
}

static guint
mapvue_read_group1001(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 4 };
    MapVueGroup1001 *group = (MapVueGroup1001*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->power = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group1052(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 3*4 };
    MapVueGroup1052 *group = (MapVueGroup1052*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->map_velocity = gwy_get_gfloat_le(&p);
    group->map_acceleration = gwy_get_gfloat_le(&p);
    group->map_runout = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group1102(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 2*4 };
    MapVueGroup1102 *group = (MapVueGroup1102*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->x_shift = gwy_get_gfloat_le(&p);
    group->y_shift = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group1151(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 2*4 };
    MapVueGroup1151 *group = (MapVueGroup1151*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->stage_x = gwy_get_gfloat_le(&p);
    group->stage_y = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group1152(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 3*4 };
    MapVueGroup1152 *group = (MapVueGroup1152*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->stage_x = gwy_get_gfloat_le(&p);
    group->stage_y = gwy_get_gfloat_le(&p);
    group->stage_z = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group1201(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 6*2 };
    MapVueGroup1201 *group = (MapVueGroup1201*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->actsft = gwy_get_gint16_le(&p);
    group->numfrms = gwy_get_gint16_le(&p);
    group->offset1 = gwy_get_gint16_le(&p);
    group->phssft1 = gwy_get_gint16_le(&p);
    group->offset2 = gwy_get_gint16_le(&p);
    group->phssft2 = gwy_get_gint16_le(&p);

    return size;
}

static guint
mapvue_read_group1251(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 2*4 };
    MapVueGroup1251 *group = (MapVueGroup1251*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->sftmean = gwy_get_gfloat_le(&p);
    group->sftdev = gwy_get_gfloat_le(&p);

    return size;
}

#if 0
static guint
mapvue_read_group1560(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 3*8 + 2 + 2*8 };
    MapVueGroup1560 *group = (MapVueGroup1560*)grpdata;
    guint len, tagsize = 3*8;

    /* XXX: What if group size > known data size */
    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->propagation_factor = gwy_get_gint64_le(&p);
    group->index_of_refraction = gwy_get_gint64_le(&p);
    group->wavelength = gwy_get_gint64_le(&p);

    if (!(len = mapvue_read_string(&p, size - tagsize, group->material, error)))
        return 0;
    tagsize += len;

    if (!(len = mapvue_read_string(&p, size - tagsize, group->name, error)))
        return 0;
    tagsize += len;

    if (err_TAG_SIZE(error, group->reftag, tagsize + 2*8, size))
        return 0;

    group->x_coordinates = gwy_get_gdouble_le(&p);
    group->y_coordinates = gwy_get_gdouble_le(&p);

    return size;
}
#endif

static guint
mapvue_read_group2002(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 8 };
    MapVueGroup2002 *group = (MapVueGroup2002*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->phase_shift = gwy_get_gfloat_le(&p);
    group->map_scale = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group2051(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 0 };
    MapVueGroup2051 *group = (MapVueGroup2051*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    return size;
}

static guint
mapvue_read_group2101(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 0 };
    MapVueGroup2101 *group = (MapVueGroup2101*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    return size;
}

static guint
mapvue_read_group2201(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 0 };
    MapVueGroup2201 *group = (MapVueGroup2201*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    return size;
}

static guint
mapvue_read_group2251(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 4 };
    MapVueGroup2251 *group = (MapVueGroup2251*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->stitch_quality = gwy_get_gfloat_le(&p);

    return size;
}

static guint
mapvue_read_group2351(const guchar *p, gsize size, gpointer grpdata,
                      GError **error)
{
    enum { SIZE = 12 };
    MapVueGroup2351 *group = (MapVueGroup2351*)grpdata;

    if (!(size = mapvue_group_size(&p, size, group->reftag, error)))
        return 0;

    if (err_TAG_SIZE(error, group->reftag, SIZE, size))
        return 0;

    group->film_thickness_approx = gwy_get_gdouble_le(&p);
    group->film_surface = gwy_get_gint16_le(&p);

    return size;
}

static GwyDataField*
read_data_field(gconstpointer p,
                gint xres, gint yres,
                guint bpp,
                GwyDataField **maskfield)
{
    GwyDataField *dfield, *mfield;
    GwySIUnit *unit;
    gdouble *data, *mdata;
    gint i, j, mcount;

    if (maskfield)
        *maskfield = NULL;

    dfield = gwy_data_field_new(xres, yres, 1.0, 1.0, FALSE);
    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);
    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_fill(mfield, 1.0);

    data = gwy_data_field_get_data(dfield);
    mdata = gwy_data_field_get_data(mfield);
    if (bpp == 4) {
        const guint32 *d32 = p;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint32 v = GINT32_FROM_LE(*d32);
                if (v != G_MAXINT32)
                    data[i*xres + j] = v/(gdouble)G_MAXINT32/5.625e4;
                else
                    mdata[i*xres + j] = 0.0;
                d32++;
            }
        }
    }
    else if (bpp == 2) {
        const guint16 *d16 = p;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint16 v = GINT16_FROM_LE(*d16);
                // FIXME: The scale factor 7.61 was just estimated from image
                // comparison!
                if (v != G_MAXINT16)
                    data[i*xres + j] = v/(gdouble)G_MAXINT16/5.625e4/7.61;
                else
                    mdata[i*xres + j] = 0.0;
                d16++;
            }
        }
    }

    mcount = gwy_app_channel_remove_bad_data(dfield, mfield);

    if (maskfield && mcount)
        *maskfield = mfield;
    else
        g_object_unref(mfield);

    return dfield;
}

static gboolean
calculate_scales_from_fids(const MapVueGroup155 *group155,
                           gdouble aspratio,
                           gdouble *xscale, gdouble *yscale)
{
    const MapVueFids *dimfids = &group155->dim_fids;
    gdouble h;

    gwy_debug("Trying to figure out dimension from fiducials.");
    if (dimfids->n != 2) {
        gwy_debug("The number of dim fids differs from 2, giving up.");
        return FALSE;
    }
    if (!group155->dim_distance) {
        gwy_debug("Dim distance is zero, that would lead to zero dimensions.");
        return FALSE;
    }

    h = hypot(aspratio*(dimfids->x_coordinates[1] - dimfids->x_coordinates[0]),
              dimfids->y_coordinates[1] - dimfids->y_coordinates[0]);
    if (!h) {
        gwy_debug("Pixel distance of fiducials is zero, that would lead to "
                  "infinite dimensions.");
        return FALSE;
    }

    *yscale = 1e-3*group155->dim_distance/h;
    *xscale = aspratio * (*yscale);
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
