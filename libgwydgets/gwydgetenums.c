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
#include <libgwydgets/gwydgetenums.h>

/**
 * GwyGraphCurveType:
 * @GWY_GRAPH_CURVE_HIDDEN: Curve is invisible.
 * @GWY_GRAPH_CURVE_POINTS: Curve data is plotted with symbols.
 * @GWY_GRAPH_CURVE_LINE: Curve data is plotted with a line.
 * @GWY_GRAPH_CURVE_LINE_POINTS: Curve data is plotted with symbols and a line.
 *
 * Graph curve plotting type.
 **/
/**
 * gwy_graph_curve_type_get_enum:
 *
 * Returns #GwyEnum for #GwyGraphCurveType enum type.
 *
 * Returns: %NULL-terminated #GwyEnum which must not be modified nor freed.
 **/
const GwyEnum*
gwy_graph_curve_type_get_enum(void)
{
    static const GwyEnum entries[] = {
        { N_("Hidden"),        GWY_GRAPH_CURVE_HIDDEN,      },
        { N_("Points"),        GWY_GRAPH_CURVE_POINTS,      },
        { N_("Line"),          GWY_GRAPH_CURVE_LINE,        },
        { N_("Line + points"), GWY_GRAPH_CURVE_LINE_POINTS, },
        { NULL,                0,                           },
    };
    return entries;
}

/**
 * GwyAxisScaleFormat:
 * @GWY_AXIS_SCALE_FORMAT_FLOAT: Floating point format.
 * @GWY_AXIS_SCALE_FORMAT_EXP: Exponential (`scienfitic') format.
 * @GWY_AXIS_SCALE_FORMAT_INT: Integer format.
 * @GWY_AXIS_SCALE_FORMAT_AUTO: Automatical format.
 *
 * Labeled axis tick mark format.
 **/

/**
 * Gwy3DMovement:
 * @GWY_3D_MOVEMENT_NONE: View cannot be changed by user.
 * @GWY_3D_MOVEMENT_ROTATION: View can be rotated.
 * @GWY_3D_MOVEMENT_SCALE: View can be scaled.
 * @GWY_3D_MOVEMENT_DEFORMATION: View can be scaled.
 * @GWY_3D_MOVEMENT_LIGHT: Light position can be changed.
 *
 * The type of 3D view change that happens when user drags it with mouse.
 */

/**
 * Gwy3DProjection:
 * @GWY_3D_PROJECTION_ORTHOGRAPHIC: Otrhographic projection.
 * @GWY_3D_PROJECTION_PERSPECTIVE: Perspective projection.
 *
 * 3D View projection type.
 **/

/**
 * Gwy3DVisualization:
 * @GWY_3D_VISUALIZATION_GRADIENT: Data is displayed with color corresponding
 *                                 to 2D view.
 * @GWY_3D_VISUALIZATION_LIGHTING: Data is displayed as an uniform material
 *                                 with some lighting.
 *
 * 3D View data visualization type.
 **/

/**
 * Gwy3DViewLabel:
 * @GWY_3D_VIEW_LABEL_X: X-axis label.
 * @GWY_3D_VIEW_LABEL_Y: Y-axis label.
 * @GWY_3D_VIEW_LABEL_MIN: Z-axis bottom label.
 * @GWY_3D_VIEW_LABEL_MAX: Z-axis top label.
 *
 * 3D View label type.
 **/

/**
 * GwyHScaleStyle:
 * @GWY_HSCALE_DEFAULT: Default label, hscale, spinbutton, and units widget
 *                      row.
 * @GWY_HSCALE_LOG: Hscale is logarithmic.
 * @GWY_HSCALE_SQRT: Hscale is square root.
 * @GWY_HSCALE_NO_SCALE: There is no hscale.
 * @GWY_HSCALE_WIDGET: An user-specified widget is used in place of hscale and
 *                     spinbutton.
 * @GWY_HSCALE_WIDGET_NO_EXPAND: An user-specified widget is used in place of
 *                               hscale and spinbutton, and it is left-aligned
 *                               instead of taking all the alloted space.
 * @GWY_HSCALE_CHECK: The label is actually a check button that controls
 *                    sensitivity of the row.
 *
 * Options controlling gwy_table_attach_hscale() behaviour.
 **/

/**
 * GwyLayerBasicRangeType:
 * @GWY_LAYER_BASIC_RANGE_FULL: Color gradient is uniformly mapped to range
 *                              from data minimum to maximum.
 * @GWY_LAYER_BASIC_RANGE_FIXED: Color gradient is uniformly mapped to a fixed
 *                               range, independent on data.
 * @GWY_LAYER_BASIC_RANGE_AUTO: Color gradient is uniformly mapped to a range
 *                              inside which most of data points lie, that is
 *                              height distribution tails are cut off.
 * @GWY_LAYER_BASIC_RANGE_ADAPT: Color range is mapped nonuniformly,
 *                               see gwy_pixbuf_draw_data_field_adaptive().
 *
 * Types of color gradient mapping.
 **/

/**
 * GwyCurveType:
 * @GWY_CURVE_TYPE_LINEAR: Linear interpolation.
 * @GWY_CURVE_TYPE_SPLINE: Spline interpolation.
 * @GWY_CURVE_TYPE_FREE: Free form curve.
 *
 * Curve drawing type.
 **/

/**
 * SECTION:gwydgetenums
 * @title: gwydgetenums
 * @short_description: Common enumerations
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
