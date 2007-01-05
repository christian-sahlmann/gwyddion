/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <libprocess/datafield.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

/**
 * gwy_app_channel_check_nonsquare:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Sets `realsquare' for a channel highly non-square pixels.
 *
 * The threshold for highly non-square is somewhat arbitrary.
 * Fortunately, most files encoutered in practice have the measure ratio either
 * very close to 1, larger or equal than 2.
 *
 * Returns: %TRUE if the channel was found to have highly non-square pixels
 *          and `realsquare' was set (otherwise it was unset).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_check_nonsquare(GwyContainer *data,
                                gint id)
{
    GwyDataField *dfield;
    gdouble xmeasure, ymeasure, q;
    gboolean nonsquare;
    GQuark quark;
    const gchar *key;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    xmeasure = gwy_data_field_get_xmeasure(dfield);
    ymeasure = gwy_data_field_get_ymeasure(dfield);
    q = xmeasure/ymeasure;

    nonsquare = (q > G_SQRT2 || q < 1.0/G_SQRT2);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/realsquare", NULL);
    if (nonsquare)
        gwy_container_set_boolean_by_name(data, s, TRUE);
    else
        gwy_container_remove_by_name(data, s);
    g_free(s);

    return nonsquare;
}

/**
 * guess_channel_type:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Adds a channel title based on data field units.
 *
 * The guess is very simple, but probably better than `Unknown channel' in
 * most cases.  If there already is a title it is left intact, making use of
 * this function as a fall-back easier.
 *
 * Returns: %TRUE if the title was set (either by this function or before).
 *
 * Since: 2.3
 **/
gboolean
gwy_app_channel_title_fall_back(GwyContainer *data,
                                gint id)
{
    static const struct {
        const gchar *unit;
        const gchar *title;
    }
    map[] = {
        { "m",   "Topography", },
        { "A",   "Current",    },
        { "deg", "Phase",      },
        { "V",   "Voltage",    },
        { "N",   "Force",      },
    };

    GwySIUnit *siunit, *test;
    GwyDataField *dfield;
    const gchar *key, *title;
    GQuark quark;
    guint i;
    gchar *s;

    quark = gwy_app_get_data_key_for_id(id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(data, quark));
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);

    key = g_quark_to_string(quark);
    s = g_strconcat(key, "/title", NULL);
    quark = g_quark_from_string(s);
    g_free(s);
    if (gwy_container_contains(data, quark))
        return TRUE;

    siunit = gwy_data_field_get_si_unit_z(dfield);
    test = gwy_si_unit_new(NULL);
    title = NULL;

    for (title = NULL, i = 0; i < G_N_ELEMENTS(map) && !title; i++) {
        gwy_si_unit_set_from_string(test, map[i].unit);
        if (gwy_si_unit_equal(siunit, test))
            title = map[i].title;
    }

    g_object_unref(test);

    if (title) {
        gwy_container_set_string(data, quark, g_strdup(title));
        return TRUE;
    }

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
