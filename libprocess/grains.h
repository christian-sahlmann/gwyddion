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

#ifndef __GWY_PROCESS_GRAINS_H__
#define __GWY_PROCESS_GRAINS_H__

G_BEGIN_DECLS

/* XXX: never used in libprocess itself, there should be probably some header
 * file with common enums... */
typedef enum {
    GWY_MERGE_UNION        = 0, /*union of all found grains*/
    GWY_MERGE_INTERSECTION = 1  /*intersection of grains found by different methods*/
} GwyMergeType;

typedef enum {
    GWY_WSHED_INIT         = 0, /*start initializations*/
    GWY_WSHED_LOCATE       = 1, /*locate steps*/
    GWY_WSHED_MIN          = 2, /*find minima*/
    GWY_WSHED_WSHED        = 3, /*watershed steps*/
    GWY_WSHED_MARK         = 4, /*mark grain boundaries*/
    GWY_WSHED_FINISHED     = 5
} GwyWatershedStateType;

typedef struct {
    GwyWatershedStateType state;
    gint internal_i;
    GwyDataField *min;
    GwyDataField *water;
    GwyDataField *mark_dfield;
    gint fraction;
    GString *description;
} GwyWatershedStatus;

void gwy_data_field_grains_mark_curvature(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gdouble threshval,
                                          gint below);

void gwy_data_field_grains_mark_watershed(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          gint locate_steps,
                                          gint locate_thresh,
                                          gdouble locate_dropsize,
                                          gint wshed_steps,
                                          gdouble wshed_dropsize,
                                          gboolean prefilter,
                                          gint dir);

#ifndef GWY_DISABLE_DEPRECATED
void gwy_data_field_grains_remove_manually(GwyDataField *grain_field,
                                           gint i);
#endif

gboolean gwy_data_field_grains_remove_grain(GwyDataField *grain_field,
                                            gint col,
                                            gint row);

void gwy_data_field_grains_remove_by_size(GwyDataField *grain_field,
                                          gint size);

void gwy_data_field_grains_remove_by_height(GwyDataField *data_field,
                                            GwyDataField *grain_field,
                                            gdouble threshval,
                                            gint direction);

void gwy_data_field_grains_watershed_iteration(GwyDataField *data_field,
                                               GwyDataField *grain_field,
                                               GwyWatershedStatus *status,
                                               gint locate_steps,
                                               gint locate_thresh,
                                               gdouble locate_dropsize,
                                               gint wshed_steps,
                                               gdouble wshed_dropsize,
                                               gboolean prefilter,
                                               gint dir);

void gwy_data_field_grains_mark_local_maxima(GwyDataField *data_field,
                                             GwyDataField *grain_field);

void gwy_data_field_grains_mark_height(GwyDataField *data_field,
                                       GwyDataField *grain_field,
                                       gdouble threshval,
                                       gint below);

void gwy_data_field_grains_mark_slope(GwyDataField *data_field,
                                      GwyDataField *grain_field,
                                      gdouble threshval,
                                      gint below);

gdouble gwy_data_field_grains_get_average(GwyDataField *grain_field);

void gwy_data_field_grains_get_distribution(GwyDataField *grain_field,
                                            GwyDataLine *distribution);

void gwy_data_field_grains_add(GwyDataField *grain_field,
                              GwyDataField *add_field);

void gwy_data_field_grains_intersect(GwyDataField *grain_field,
                                     GwyDataField *intersect_field);


G_END_DECLS

#endif /*__GWY_PROCESS_GRAINS__*/

