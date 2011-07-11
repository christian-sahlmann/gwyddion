/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_PROCESS_ENUMS_H__
#define __GWY_PROCESS_ENUMS_H__

#include <glib/gmacros.h>
#include <libgwyddion/gwyenum.h>

G_BEGIN_DECLS

typedef enum {
    GWY_MERGE_UNION,
    GWY_MERGE_INTERSECTION
} GwyMergeType;

typedef enum {
    GWY_MASK_EXCLUDE = 0,
    GWY_MASK_INCLUDE = 1,
    GWY_MASK_IGNORE  = 2,
} GwyMaskingType;

typedef enum {
    GWY_WATERSHED_STATE_INIT,
    GWY_WATERSHED_STATE_LOCATE,
    GWY_WATERSHED_STATE_MIN,
    GWY_WATERSHED_STATE_WATERSHED,
    GWY_WATERSHED_STATE_MARK,
    GWY_WATERSHED_STATE_FINISHED
} GwyWatershedStateType;

typedef enum {
    GWY_SYMMETRY_AUTO,
    GWY_SYMMETRY_PARALLEL,
    GWY_SYMMETRY_TRIANGULAR,
    GWY_SYMMETRY_SQUARE,
    GWY_SYMMETRY_RHOMBIC,
    GWY_SYMMETRY_HEXAGONAL,
    GWY_SYMMETRY_LAST
} GwyPlaneSymmetry;

typedef enum { /*< lowercase_name=gwy_2d_cwt_wavelet_type >*/
    GWY_2DCWT_GAUSS,
    GWY_2DCWT_HAT
} Gwy2DCWTWaveletType;

typedef enum {
    GWY_ORIENTATION_HORIZONTAL,
    GWY_ORIENTATION_VERTICAL
} GwyOrientation;

typedef enum {
    GWY_TRANSFORM_DIRECTION_BACKWARD = -1,
    GWY_TRANSFORM_DIRECTION_FORWARD = 1
} GwyTransformDirection;

typedef enum {
    GWY_DATA_FIELD_CACHE_MIN = 0,
    GWY_DATA_FIELD_CACHE_MAX,
    GWY_DATA_FIELD_CACHE_SUM,
    GWY_DATA_FIELD_CACHE_RMS,
    GWY_DATA_FIELD_CACHE_MED,
    GWY_DATA_FIELD_CACHE_ARF,
    GWY_DATA_FIELD_CACHE_ART,
    GWY_DATA_FIELD_CACHE_ARE,
    GWY_DATA_FIELD_CACHE_SIZE = 30
} GwyDataFieldCached;

typedef enum {
    GWY_COMPUTATION_STATE_INIT,
    GWY_COMPUTATION_STATE_ITERATE,
    GWY_COMPUTATION_STATE_FINISHED
} GwyComputationStateType;

typedef enum {
    GWY_DWT_HAAR,
    GWY_DWT_DAUB4,
    GWY_DWT_DAUB6,
    GWY_DWT_DAUB8,
    GWY_DWT_DAUB12,
    GWY_DWT_DAUB20
} GwyDWTType;

typedef enum {
    GWY_DWT_DENOISE_UNIVERSAL,
    GWY_DWT_DENOISE_SCALE_ADAPTIVE,
    GWY_DWT_DENOISE_SPACE_ADAPTIVE
} GwyDWTDenoiseType;

typedef enum {
    GWY_INTERPOLATION_NONE      = 0,
    GWY_INTERPOLATION_ROUND     = 1,
    GWY_INTERPOLATION_LINEAR    = 2,
    GWY_INTERPOLATION_BILINEAR  = GWY_INTERPOLATION_LINEAR,
    GWY_INTERPOLATION_KEY       = 3,
    GWY_INTERPOLATION_BSPLINE   = 4,
    GWY_INTERPOLATION_OMOMS     = 5,
    GWY_INTERPOLATION_NNA       = 6,
    GWY_INTERPOLATION_SCHAUM    = 7
} GwyInterpolationType;

typedef enum {
    GWY_PLANE_FIT_A = 1,
    GWY_PLANE_FIT_BX,
    GWY_PLANE_FIT_BY,
    GWY_PLANE_FIT_ANGLE,
    GWY_PLANE_FIT_SLOPE,
    GWY_PLANE_FIT_S0,
    GWY_PLANE_FIT_S0_REDUCED
} GwyPlaneFitQuantity;

typedef enum {
    GWY_WINDOWING_NONE       = 0,
    GWY_WINDOWING_HANN       = 1,
    GWY_WINDOWING_HAMMING    = 2,
    GWY_WINDOWING_BLACKMANN  = 3,
    GWY_WINDOWING_LANCZOS    = 4,
    GWY_WINDOWING_WELCH      = 5,
    GWY_WINDOWING_RECT       = 6,
    GWY_WINDOWING_NUTTALL    = 7,
    GWY_WINDOWING_FLAT_TOP   = 8,
    GWY_WINDOWING_KAISER25   = 9
} GwyWindowingType;

typedef enum {
    GWY_TIP_PYRAMIDE       = 0,
    GWY_TIP_CONTACT        = 1,
    GWY_TIP_NONCONTACT     = 2,
    GWY_TIP_DELTA          = 3
} GwyTipType;

typedef enum {
    GWY_CORRELATION_NORMAL  = 0,
    GWY_CORRELATION_FFT     = 1,
    GWY_CORRELATION_POC     = 2
} GwyCorrelationType;

/* The values should not grow over 62 (to be usable as bit positions) */
typedef enum {
    GWY_GRAIN_VALUE_PROJECTED_AREA       = 0,
    GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE    = 1,
    GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS    = 2,
    GWY_GRAIN_VALUE_SURFACE_AREA         = 3,
    GWY_GRAIN_VALUE_MAXIMUM              = 4,
    GWY_GRAIN_VALUE_MINIMUM              = 5,
    GWY_GRAIN_VALUE_MEAN                 = 6,
    GWY_GRAIN_VALUE_MEDIAN               = 7,
    GWY_GRAIN_VALUE_HALF_HEIGHT_AREA     = 9,
    GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH = 10,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE   = 12,
    GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE  = 13,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE   = 14,
    GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE  = 15,
    GWY_GRAIN_VALUE_CENTER_X             = 16,
    GWY_GRAIN_VALUE_CENTER_Y             = 17,
    GWY_GRAIN_VALUE_VOLUME_0             = 18,
    GWY_GRAIN_VALUE_VOLUME_MIN           = 19,
    GWY_GRAIN_VALUE_VOLUME_LAPLACE       = 20,
    GWY_GRAIN_VALUE_SLOPE_THETA          = 23,
    GWY_GRAIN_VALUE_SLOPE_PHI            = 24,
    GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM     = 25,
    GWY_GRAIN_VALUE_BOUNDARY_MINIMUM     = 26,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_X   = 27,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Y   = 28,
    GWY_GRAIN_VALUE_CURVATURE_CENTER_Z   = 29,
    GWY_GRAIN_VALUE_CURVATURE1           = 30,
    GWY_GRAIN_VALUE_CURVATURE2           = 31,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE1     = 32,
    GWY_GRAIN_VALUE_CURVATURE_ANGLE2     = 33,
} GwyGrainQuantity;

typedef enum {
    GWY_DATA_COMPATIBILITY_RES     = 1 << 0,
    GWY_DATA_COMPATIBILITY_REAL    = 1 << 1,
    GWY_DATA_COMPATIBILITY_MEASURE = 1 << 2,
    GWY_DATA_COMPATIBILITY_LATERAL = 1 << 3,
    GWY_DATA_COMPATIBILITY_VALUE   = 1 << 4,
    GWY_DATA_COMPATIBILITY_ALL     = 0x001f
} GwyDataCompatibilityFlags;

typedef enum {
    GWY_LINE_STAT_MEAN,
    GWY_LINE_STAT_MEDIAN,
    GWY_LINE_STAT_MINIMUM,
    GWY_LINE_STAT_MAXIMUM,
    GWY_LINE_STAT_RMS,
    GWY_LINE_STAT_LENGTH,
    GWY_LINE_STAT_SLOPE,
    GWY_LINE_STAT_TAN_BETA0,
    GWY_LINE_STAT_RA,
    GWY_LINE_STAT_RZ,
    GWY_LINE_STAT_RT,
    GWY_LINE_STAT_SKEW,
    GWY_LINE_STAT_KURTOSIS
} GwyLineStatQuantity;

typedef enum {
    GWY_EXTERIOR_UNDEFINED,
    GWY_EXTERIOR_BORDER_EXTEND,
    GWY_EXTERIOR_MIRROR_EXTEND,
    GWY_EXTERIOR_PERIODIC,
    GWY_EXTERIOR_FIXED_VALUE
} GwyExteriorType;

const GwyEnum* gwy_merge_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_masking_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_plane_symmetry_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_2d_cwt_wavelet_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_orientation_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_dwt_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_dwt_denoise_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_interpolation_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_windowing_type_get_enum(void) G_GNUC_CONST;
const GwyEnum* gwy_correlation_type_get_enum(void) G_GNUC_CONST;

/* Well, this is not exactly an enum... */
typedef struct {
    guint state;
    gdouble fraction;
} GwyComputationState;

G_END_DECLS

#endif /* __GWY_PROCESS_ENUMS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
