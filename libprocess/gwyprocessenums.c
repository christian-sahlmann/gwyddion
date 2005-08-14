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
 * GwyOrientation:
 * @GWY_ORIENTATION_HORIZONTAL: Horizontal orientation.
 * @GWY_ORIENTATION_VERTICAL: Vertical orientation.
 *
 * Orientation type.
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

const GwyEnum*
gwy_windowing_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("None"),     GWY_WINDOWING_NONE      },
        { N_("Hann"),     GWY_WINDOWING_HANN      },
        { N_("Hamming"),  GWY_WINDOWING_HAMMING   },
        { N_("Blackman"), GWY_WINDOWING_BLACKMANN },
        { N_("Lanzcos"),  GWY_WINDOWING_LANCZOS   },
        { N_("Welch"),    GWY_WINDOWING_WELCH     },
        { N_("Rect"),     GWY_WINDOWING_RECT      },
        { NULL,           0,                      },
    };
    return entries;
}

/************************** Documentation ****************************/

/**
 * GwyTransformDirection:
 * @GWY_TRANSFORM_DIRECTION_BACKWARD: Backward (inverse) transform.
 * @GWY_TRANSFORM_DIRECTION_FORWARD: Forward (direct) transform.
 *
 * Transform (namely integral transform) direction.
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
 * Quantity that can be requested from gwy_data_field_area_fit_local_planes()
 * et al.
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
 * There should be little need to this enum directly.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
