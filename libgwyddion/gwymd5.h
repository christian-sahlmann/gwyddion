/*
 * @(#) $Id$
 *
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * GWYified 2002 by Sven Neumann <sven@gimp.org>
 * Gwyddionized 2004 by Yeti <yeti@physics.muni.cz>
 */

/* parts of this file are :
 * Written March 1993 by Branko Lankester
 * Modified June 1993 by Colin Plumb for altered md5.c.
 * Modified October 1995 by Erik Troan for RPM
 */

#ifndef __GWY_MD5_H__
#define __GWY_MD5_H__

G_BEGIN_DECLS

/* For information look into the C source or the html documentation */

void gwy_md5_get_digest (const gchar *buffer,
                         gint         buffer_size,
                         guchar       digest[16]);


G_END_DECLS

#endif  /* __GWY_MD5_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
