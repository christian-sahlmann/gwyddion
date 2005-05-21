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

#ifndef __GWY_EXPR_H__
#define __GWY_EXPR_H__

#include <glib/gerror.h>

G_BEGIN_DECLS

#define GWY_EXPR_ERROR gwy_expr_error_quark()

typedef enum {
    GWY_EXPR_ERROR_CLOSING_PARENTHESIS,
    GWY_EXPR_ERROR_EMPTY,
    GWY_EXPR_ERROR_EMPTY_PARENTHESES,
    GWY_EXPR_ERROR_GARBAGE,
    GWY_EXPR_ERROR_INVALID_ARGUMENT,
    GWY_EXPR_ERROR_INVALID_TOKEN,
    GWY_EXPR_ERROR_MISSING_ARGUMENT,
    GWY_EXPR_ERROR_NOT_EXECUTABLE,
    GWY_EXPR_ERROR_OPENING_PARENTHESIS,
    GWY_EXPR_ERROR_STRAY_COMMA,
    GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS,
    GWY_EXPR_ERROR_CONSTANT_NAME
} GwyExprError;

typedef struct _GwyExpr GwyExpr;

GQuark    gwy_expr_error_quark       (void);

GwyExpr*  gwy_expr_new               (void);
void      gwy_expr_free              (GwyExpr *expr);
gboolean  gwy_expr_evaluate          (GwyExpr *expr,
                                      const gchar *text,
                                      gdouble *result,
                                      GError **err);
gboolean  gwy_expr_compile           (GwyExpr *expr,
                                      const gchar *text,
                                      GError **err);
guint     gwy_expr_resolve_variables (GwyExpr *expr,
                                      guint n,
                                      gchar * const *names,
                                      guint *indices);
gint      gwy_expr_get_variables     (GwyExpr *expr,
                                      gchar ***names);
gdouble   gwy_expr_execute           (GwyExpr *expr,
                                      const gdouble *values);
void      gwy_expr_vector_execute    (GwyExpr *expr,
                                      guint n,
                                      gdouble **data,
                                      gdouble *result);
gboolean  gwy_expr_define_constant   (GwyExpr *expr,
                                      const gchar *name,
                                      gdouble value,
                                      GError **err);
gboolean  gwy_expr_undefine_constant (GwyExpr *expr,
                                      const gchar *name);

G_END_DECLS

#endif /* __GWY_EXPR_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

