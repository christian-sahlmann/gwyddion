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

#include "config.h"
#include <libdraw/gwydraw.h>
#include "gwydgets.h"

static GdkGLConfig *glconfig = NULL;
static guint types_initialized = 0;

/************************** Initialization ****************************/

/**
 * gwy_widgets_type_init:
 *
 * Initializes libgwywidgets data types, making their deserialization safe.
 *
 * Eventually calls gwy_draw_type_init().
 **/
void
gwy_widgets_type_init(void)
{
    if (types_initialized)
        return;

    gwy_draw_type_init();

    types_initialized += gwy_graph_curve_model_get_type();
    types_initialized += gwy_graph_model_get_type();
    types_initialized += gwy_3d_label_get_type();
    /* not serializable (yet) */
    types_initialized += gwy_gl_material_get_type();
    types_initialized |= 1;
}

/**
 * gwy_widgets_gl_init:
 *
 * Configures an OpenGL-capable visual for 3D widgets.
 *
 * Use gwy_widgets_get_gl_config() to get the framebuffer configuration.
 *
 * This function must be called before OpenGL widgets can be used.
 *
 * Returns: %TRUE if an appropriate visual was found.
 **/
gboolean
gwy_widgets_gl_init(void)
{
    /* when called twice, fail but successfully :o) */
    g_return_val_if_fail(glconfig == NULL, TRUE);

    glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                         | GDK_GL_MODE_DEPTH
                                         | GDK_GL_MODE_DOUBLE);
    /* Try double-buffered visual */
    if (!glconfig) {
        g_warning("Cannot find a double-buffered OpenGL visual, "
                  "Trying single-buffered visual.");

        /* Try single-buffered visual */
        glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB
                                             | GDK_GL_MODE_DEPTH);
        if (!glconfig) {
            g_warning("No appropriate OpenGL-capable visual found.");
        }
    }

    return glconfig != NULL;
}

/**
 * gwy_widgets_get_gl_config:
 *
 * Returns OpenGL framebuffer configuration for 3D widgets.
 *
 * Call gwy_widgets_gl_init() first.
 *
 * Returns: The OpenGL framebuffer configuration.
 **/
GdkGLConfig*
gwy_widgets_get_gl_config(void)
{
    return glconfig;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
