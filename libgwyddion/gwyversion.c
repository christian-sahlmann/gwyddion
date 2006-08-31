/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwyversion.h>

/**
 * gwy_version_major:
 *
 * Gets the major version of Gwyddion.
 *
 * If the version is 1.99.7.20060604, this function returns 1.
 *
 * Returns: The major version.
 **/
gint
gwy_version_major(void)
{
    return GWY_VERSION_MAJOR;
}

/**
 * gwy_version_minor:
 *
 * Gets the minor version of Gwyddion.
 *
 * If the version is 1.99.7.20060604, this function returns 99.
 *
 * Returns: The minor version.
 **/
gint
gwy_version_minor(void)
{
    return GWY_VERSION_MINOR;
}

/**
 * gwy_version_string:
 *
 * Gets the full Gwyddion version as a string.
 *
 * If the version is 1.99.7.20060604, this function returns
 * <literal>"1.99.7.20060604"</literal>.
 *
 * This is the only method to get finer version information than major.minor.
 * However, only development versions use finer versioning than major.minor
 * therefore a module or app requiring such information is probably broken
 * anyway.  A meaningful use is to advertise the version of Gwyddion your app
 * runs with.
 *
 * Returns: The full version as a constant string.
 **/
const gchar*
gwy_version_string(void)
{
    return GWY_VERSION_STRING;
}

/************************** Documentation ****************************/
/**
 * SECTION:gwyversion
 * @title: gwyversion
 * @short_description: Version information
 *
 * Macros like %GWY_VERSION_MAJOR can be used for compile-time version checks,
 * that is they tell what version a module or app is being compiled or was
 * compiled with.
 *
 * On the other hand functions like gwy_version_major() can be used to run-time
 * version checks and they tell what version a module or app was linked or
 * is running with.
 **/

/**
 * GWY_VERSION_MAJOR:
 *
 * Expands to the major version of Gwyddion as a number.
 *
 * If the version is 1.99.7.20060604, this macro is defined as 1.
 **/

/**
 * GWY_VERSION_MINOR:
 *
 * Expands to the minor version of Gwyddion as a number.
 *
 * If the version is 1.99.7.20060604, this macro is defined as 99.
 **/

/**
 * GWY_VERSION_STRING:
 *
 * Expands to the full Gwyddion version as a string.
 *
 * If the version is 1.99.7.20060604, this macro is defined as
 * <literal>"1.99.7.20060604"</literal>.
 *
 * See gwy_version_string() for caveats.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
