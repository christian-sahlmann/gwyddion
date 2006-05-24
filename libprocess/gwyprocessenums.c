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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libprocess/gwyprocessenums.h>

/**
 * GwyMergeType:
 * @GWY_MERGE_UNION: Union (logical or) merging.
 * @GWY_MERGE_INTERSECTION: Intersection (logical and) merging.
 *
 * Mask merge type (namely used in grain processing).
 **/
/**
 * gwy_merge_type_get_enum:
 *
 * Returns #GwyEnum for #GwyMergeType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_merge_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Union"),            GWY_MERGE_UNION,        },
        { N_("Intersection"),     GWY_MERGE_INTERSECTION, },
        { NULL,                   0,                      },
    };
    return entries;
}

/**
 * GwyPlaneSymmetry:
 * @GWY_SYMMETRY_AUTO: Automatic symmetry selection.
 * @GWY_SYMMETRY_PARALLEL: Parallel symmetry, there is one prevalent direction
 *                         (bilateral).
 * @GWY_SYMMETRY_TRIANGULAR: Triangular symmetry, there are three prevalent
 *                           directions (unilateral) by 120 degrees.
 * @GWY_SYMMETRY_SQUARE: Square symmetry, two prevalent directions (bilateral)
 *                       oriented approximately along image sides.
 * @GWY_SYMMETRY_RHOMBIC: Rhombic symmetry, two prevalent directions
 *                        (bilateral) oriented approximately along diagonals.
 * @GWY_SYMMETRY_HEXAGONAL: Hexagonal symmetry, three prevalent directions
 *                          (bilateral) by 120 degrees.
 * @GWY_SYMMETRY_LAST: The number of symmetries.
 *
 * Plane symmetry types for rotation correction.
 **/
/**
 * gwy_plane_symmetry_get_enum:
 *
 * Returns #GwyEnum for #GwyPlaneSymmetry enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_plane_symmetry_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Detected"),   GWY_SYMMETRY_AUTO       },
        { N_("Parallel"),   GWY_SYMMETRY_PARALLEL   },
        { N_("Triangular"), GWY_SYMMETRY_TRIANGULAR },
        { N_("Square"),     GWY_SYMMETRY_SQUARE     },
        { N_("Rhombic"),    GWY_SYMMETRY_RHOMBIC    },
        { N_("Hexagonal"),  GWY_SYMMETRY_HEXAGONAL  },
        { NULL,             0,                      },
    };
    return entries;
};

/**
 * gwy_2d_cwt_wavelet_type_get_enum:
 *
 * Returns #GwyEnum for #Gwy2DCWTWaveletType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_2d_cwt_wavelet_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Gaussian"),          GWY_2DCWT_GAUSS      },
        { N_("Hat"),               GWY_2DCWT_HAT        },
        { NULL,                    0,                   },
    };
    return entries;
}

/**
 * GwyOrientation:
 * @GWY_ORIENTATION_HORIZONTAL: Horizontal orientation.
 * @GWY_ORIENTATION_VERTICAL: Vertical orientation.
 *
 * Orientation type.
 **/
/**
 * gwy_orientation_get_enum:
 *
 * Returns #GwyEnum for #GwyOrientation enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_orientation_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Horizontal"),  GWY_ORIENTATION_HORIZONTAL,  },
        { N_("Vertical"),    GWY_ORIENTATION_VERTICAL,    },
        { NULL,              0,                           },
    };
    return entries;
}

/**
 * gwy_dwt_type_get_enum:
 *
 * Returns #GwyEnum for #GwyDWTType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_dwt_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Haar"),          GWY_DWT_HAAR    },
        { N_("Daubechies 4"),  GWY_DWT_DAUB4   },
        { N_("Daubechies 6"),  GWY_DWT_DAUB6   },
        { N_("Daubechies 8"),  GWY_DWT_DAUB8   },
        { N_("Daubechies 12"), GWY_DWT_DAUB12  },
        { N_("Daubechies 20"), GWY_DWT_DAUB20  },
        { NULL,                0,              },
     };
    return entries;
}

/**
 * gwy_dwt_denoise_type_get_enum:
 *
 * Returns #GwyEnum for #GwyDWTDenoiseType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_dwt_denoise_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Universal"),                 GWY_DWT_DENOISE_UNIVERSAL,       },
        { N_("Scale adaptive"),            GWY_DWT_DENOISE_SCALE_ADAPTIVE,  },
        { N_("Scale and space adaptive"),  GWY_DWT_DENOISE_SPACE_ADAPTIVE,  },
        { NULL,                            0,                               },
    };
    return entries;
}

/**
 * GwyInterpolationType:
 * @GWY_INTERPOLATION_NONE: No interpolation at all, resulting values are not
 *                          meaningful and should not be used (use for
 *                          resize operations discarding original data).
 * @GWY_INTERPOLATION_ROUND: Round interpolation (nearest neighbour
 *                           interpolation).
 * @GWY_INTERPOLATION_BILINEAR: Bilinear interpolation.
 * @GWY_INTERPOLATION_KEY: Key interpolation.
 * @GWY_INTERPOLATION_BSPLINE: B-spline interpolation.
 * @GWY_INTERPOLATION_OMOMS: Omoms interpolation
 * @GWY_INTERPOLATION_NNA: Nearest neighbour approximation.
 *
 * Interpolation types.
 **/
/**
 * gwy_interpolation_type_get_enum:
 *
 * Returns #GwyEnum for #GwyInterpolationType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_interpolation_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Round"),    GWY_INTERPOLATION_ROUND,    },
        { N_("Bilinear"), GWY_INTERPOLATION_BILINEAR, },
        { N_("Key"),      GWY_INTERPOLATION_KEY,      },
        { N_("BSpline"),  GWY_INTERPOLATION_BSPLINE,  },
        { N_("OMOMS"),    GWY_INTERPOLATION_OMOMS,    },
        { N_("NNA"),      GWY_INTERPOLATION_NNA,      },
        { NULL,           0,                          },
    };
    return entries;
}

/**
 * GwyWindowingType:
 * @GWY_WINDOWING_NONE: No windowing is applied.
 * @GWY_WINDOWING_HANN: Hann window.
 * @GWY_WINDOWING_HAMMING: Hamming window.
 * @GWY_WINDOWING_BLACKMANN: Blackmann window.
 * @GWY_WINDOWING_LANCZOS: Lanczos window.
 * @GWY_WINDOWING_WELCH: Welch window.
 * @GWY_WINDOWING_RECT: Rectangular window.
 *
 * Frequency windowing type.
 **/
/**
 * gwy_windowing_type_get_enum:
 *
 * Returns #GwyEnum for #GwyWindowingType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_windowing_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("None"),     GWY_WINDOWING_NONE,      },
        { N_("Hann"),     GWY_WINDOWING_HANN,      },
        { N_("Hamming"),  GWY_WINDOWING_HAMMING,   },
        { N_("Blackman"), GWY_WINDOWING_BLACKMANN, },
        { N_("Lanzcos"),  GWY_WINDOWING_LANCZOS,   },
        { N_("Welch"),    GWY_WINDOWING_WELCH,     },
        { N_("Rect"),     GWY_WINDOWING_RECT,      },
        { NULL,           0,                       },
    };
    return entries;
}

const GwyEnum*
gwy_correlation_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Normal"),      GWY_CORRELATION_NORMAL, },
        { N_("FFT"),         GWY_CORRELATION_FFT,    },
        { N_("Phase only"),  GWY_CORRELATION_POC,    },
        { NULL,           0,                       },
    };
    return entries;
}


/************************** Documentation ****************************/

/**
 * SECTION:gwyprocessenums
 * @title: gwyprocessenums
 * @short_description: Common enumerations
 **/

/**
 * GwyTransformDirection:
 * @GWY_TRANSFORM_DIRECTION_BACKWARD: Backward (inverse) transform.
 * @GWY_TRANSFORM_DIRECTION_FORWARD: Forward (direct) transform.
 *
 * Transform (namely integral transform) direction.
 *
 * In FFT, it is equal to sign of the exponent, that is backward transform
 * uses -1, forward transform +1.
 **/

/**
 * GwyPlaneFitQuantity:
 * @GWY_PLANE_FIT_A: Constant coefficient (mean value).
 * @GWY_PLANE_FIT_BX: Linear coefficient in x, if x in in pixel coordinates.
 * @GWY_PLANE_FIT_BY: Linear coefficient in y, if y is in pixel coordinates.
 * @GWY_PLANE_FIT_ANGLE: Slope orientation in (x,y) plane (in radians).
 * @GWY_PLANE_FIT_SLOPE: Absolute slope value (that is sqrt(bx*bx + by*by)).
 * @GWY_PLANE_FIT_S0: Residual sum of squares.
 * @GWY_PLANE_FIT_S0_REDUCED: Slope-reduced residual sum of squares.
 *
 * Local plane fitting quantity to request from
 * gwy_data_field_area_fit_local_planes() and similar functions.
 **/

/**
 * GwyGrainQuantity:
 * @GWY_GRAIN_VALUE_PROJECTED_AREA: Projected (flat) grain area.
 * @GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE: Side of a square with the same area
 *                                     as the grain.
 * @GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS: Radius of a disc with the same area
 *                                     as the grain.
 * @GWY_GRAIN_VALUE_SURFACE_AREA: Surface area.
 * @GWY_GRAIN_VALUE_MAXIMUM: Minimum value.
 * @GWY_GRAIN_VALUE_MINIMUM: Maximum value.
 * @GWY_GRAIN_VALUE_MEAN: Mean value.
 * @GWY_GRAIN_VALUE_MEDIAN: Median value.
 * @GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH: Length of projected grain boundary.
 *                                        Note for grains not topologically
 *                                        equivalent to disc, only the length
 *                                        of the outer boundary is calculated.
 * @GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE: Minimum lateral bounding size, in other
 *                                      words the minimum length of grain
 *                                      projection to any line in the
 *                                      horizontal plane.
 * @GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE: Direction of the minimum lateral
 *                                       bounding size (arbitrary one if the
 *                                       minimum is not unique).
 * @GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE: Minimum lateral bounding size, in other
 *                                      words the maximum length of grain
 *                                      projection to any line in the
 *                                      horizontal plane.
 * @GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE: Direction of the maximum lateral
 *                                       bounding size (arbitrary one if the
 *                                       maximum is not unique).
 *
 * Grain quantity to request from gwy_data_field_grains_get_distribution()
 * and similar functions.
 **/

/**
 * GwyDataFieldCached:
 * @GWY_DATA_FIELD_CACHE_MIN: Overall minimum.
 * @GWY_DATA_FIELD_CACHE_MAX: Overall maximum.
 * @GWY_DATA_FIELD_CACHE_SUM: Sum of all values.
 * @GWY_DATA_FIELD_CACHE_RMS: Root mean square.
 * @GWY_DATA_FIELD_CACHE_MED: Median.
 * @GWY_DATA_FIELD_CACHE_ARF: Auto-range from.
 * @GWY_DATA_FIELD_CACHE_ART: Auto-range to.
 * @GWY_DATA_FIELD_CACHE_ARE: Surface area.
 * @GWY_DATA_FIELD_CACHE_SIZE: The size of statistics cache.
 *
 * Cached data field quantity types.
 *
 * There should be little need to this enum directly except in libgwyprocess
 * methods.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
