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
#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyentities.h>

static const GwyTextEntity ENTITIES[] = {
    { "Alpha", "\xce\x91" },
    { "Beta", "\xce\x92" },
    { "Chi", "\xce\xa7" },
    { "Dagger", "\xe2\x80\xa1" },
    { "Delta", "\xce\x94" },
    { "Epsilon", "\xce\x95" },
    { "Eta", "\xce\x97" },
    { "Gamma", "\xce\x93" },
    { "Iota", "\xce\x99" },
    { "Kappa", "\xce\x9a" },
    { "Lambda", "\xce\x9b" },
    { "Mu", "\xce\x9c" },
    { "Nu", "\xce\x9d" },
    { "Omega", "\xce\xa9" },
    { "Omicron", "\xce\x9f" },
    { "Phi", "\xce\xa6" },
    { "Pi", "\xce\xa0" },
    { "Psi", "\xce\xa8" },
    { "Rho", "\xce\xa1" },
    { "Sigma", "\xce\xa3" },
    { "Tau", "\xce\xa4" },
    { "Theta", "\xce\x98" },
    { "Upsilon", "\xce\xa5" },
    { "Xi", "\xce\x9e" },
    { "Zeta", "\xce\x96" },
    { "alefsym", "\xe2\x84\xb5" },
    { "alpha", "\xce\xb1" },
    { "and", "\xe2\x88\xa7" },
    { "ang", "\xe2\x88\xa0" },
    { "asymp", "\xe2\x89\x88" },
    { "beta", "\xce\xb2" },
    { "bull", "\xe2\x80\xa2" },
    { "cap", "\xe2\x88\xa9" },
    { "cent", "\xc2\xa2" },
    { "chi", "\xcf\x87" },
    { "circ", "\xcb\x86" },
    { "cong", "\xe2\x89\x85" },
    { "copy", "\xc2\xa9" },
    { "cup", "\xe2\x88\xaa" },
    { "curren", "\xc2\xa4" },
    { "dagger", "\xe2\x80\xa0" },
    { "deg", "\xc2\xb0" },
    { "delta", "\xce\xb4" },
    { "divide", "\xc3\xb7" },
    { "empty", "\xe2\x88\x85" },
    { "epsilon", "\xce\xb5" },
    { "equiv", "\xe2\x89\xa1" },
    { "eta", "\xce\xb7" },
    { "euro", "\xe2\x82\xac" },
    { "exist", "\xe2\x88\x83" },
    { "fnof", "\xc6\x92" },
    { "forall", "\xe2\x88\x80" },
    { "gamma", "\xce\xb3" },
    { "ge", "\xe2\x89\xa5" },
    { "hellip", "\xe2\x80\xa6" },
    { "image", "\xe2\x84\x91" },
    { "infin", "\xe2\x88\x9e" },
    { "int", "\xe2\x88\xab" },
    { "iota", "\xce\xb9" },
    { "isin", "\xe2\x88\x88" },
    { "kappa", "\xce\xba" },
    { "lArr", "\xe2\x87\x90" },
    { "lambda", "\xce\xbb" },
    { "lang", "\xe2\x8c\xa9" },
    { "larr", "\xe2\x86\x90" },
    { "le", "\xe2\x89\xa4" },
    { "lfloor", "\xe2\x8c\x8a" },
    { "micro", "\xc2\xb5" },
    { "middot", "\xc2\xb7" },
    { "minus", "\xe2\x88\x92" },
    { "mu", "\xce\xbc" },
    { "nabla", "\xe2\x88\x87" },
    { "ne", "\xe2\x89\xa0" },
    { "ni", "\xe2\x88\x8b" },
    { "not", "\xc2\xac" },
    { "notin", "\xe2\x88\x89" },
    { "nsub", "\xe2\x8a\x84" },
    { "nu", "\xce\xbd" },
    { "omega", "\xcf\x89" },
    { "omicron", "\xce\xbf" },
    { "oplus", "\xe2\x8a\x95" },
    { "or", "\xe2\x88\xa8" },
    { "otimes", "\xe2\x8a\x97" },
    { "part", "\xe2\x88\x82" },
    { "permil", "\xe2\x80\xb0" },
    { "perp", "\xe2\x8a\xa5" },
    { "phi", "\xcf\x86" },
    { "pi", "\xcf\x80" },
    { "piv", "\xcf\x96" },
    { "plusmn", "\xc2\xb1" },
    { "pound", "\xc2\xa3" },
    { "prod", "\xe2\x88\x8f" },
    { "prop", "\xe2\x88\x9d" },
    { "psi", "\xcf\x88" },
    { "rArr", "\xe2\x87\x92" },
    { "radic", "\xe2\x88\x9a" },
    { "rang", "\xe2\x8c\xaa" },
    { "rarr", "\xe2\x86\x92" },
    { "rceil", "\xe2\x8c\x89" },
    { "real", "\xe2\x84\x9c" },
    { "reg", "\xc2\xae" },
    { "rfloor", "\xe2\x8c\x8b" },
    { "rho", "\xcf\x81" },
    { "sdot", "\xe2\x8b\x85" },
    { "sect", "\xc2\xa7" },
    { "sigma", "\xcf\x83" },
    { "sigmaf", "\xcf\x82" },
    { "sim", "\xe2\x88\xbc" },
    { "sub", "\xe2\x8a\x82" },
    { "sube", "\xe2\x8a\x86" },
    { "sum", "\xe2\x88\x91" },
    { "sup", "\xe2\x8a\x83" },
    { "tau", "\xcf\x84" },
    { "there4", "\xe2\x88\xb4" },
    { "theta", "\xce\xb8" },
    { "thetasym", "\xcf\x91" },
    { "times", "\xc3\x97" },
    { "trade", "\xe2\x84\xa2" },
    { "upsih", "\xcf\x92" },
    { "upsilon", "\xcf\x85" },
    { "weierp", "\xe2\x84\x98" },
    { "xi", "\xce\xbe" },
    { "yen", "\xc2\xa5" },
    { "zeta", "\xce\xb6" },
    { NULL, NULL }
};

/**
 * gwy_entities_entity_to_utf8:
 * @entity: A single entity name, as a nul-delimited string.
 *
 * Converts a single named entity @entity to UTF-8 representation.
 *
 * The string passed to this function should be a bare entity name, i.e. it
 * should not contain the ampersand and semicolon.
 *
 * Returns: @entity if the name was not recognized, or a valid UTF-8 string.
 *          The returned string should be considered constant and never
 *          freed or modified (except when it's @entity again).
 **/
G_CONST_RETURN gchar*
gwy_entities_entity_to_utf8(const gchar *entity)
{
    gint min, max, i, j;

    g_return_val_if_fail(entity, entity);

    min = 0;
    max = G_N_ELEMENTS(ENTITIES) - 2;

    j = strcmp(entity, ENTITIES[min].entity);
    if (j < 0)
        return entity;
    if (!j)
        return ENTITIES[min].utf8;

    j = strcmp(entity, ENTITIES[max].entity);
    if (j > 0)
        return entity;
    if (!j)
        return ENTITIES[max].utf8;

    while (1) {
        i = (min + max)/2;
        if (i == min)
            return entity;
        j = strcmp(entity, ENTITIES[i].entity);
        if (!j)
            return ENTITIES[i].utf8;
        if (j > 0)
            min = i;
        else
            max = i;
    }
}

/**
 * gwy_entities_text_to_utf8:
 * @text: A nul-delimited string.
 *
 * Converts entities in a text to UTF-8.
 *
 * Returns: A newly allocated nul-delimited string containing the converted
 * text.
 **/
gchar*
gwy_entities_text_to_utf8(const gchar *text)
{
    gchar *result, *pos, *amp;
    const gchar *scln, *end, *ent;
    gint i;

    g_return_val_if_fail(text, NULL);

    i = strlen(text) + 1;
    end = text + i - 1;
    /* XXX: we hope no entity -> UTF-8 conversion can lead to a longer string */
    result = g_new(gchar, i);
    pos = result;

    /* if there's no semicolon, the text can't contain entities */
    while ((scln = strchr(text, ';'))) {
        /* find _corresponding_ ampersand.
         * this means the nearest preceeding ampersand.
         * if there's no ampersand, the text can't contain entities */
        i = scln - text;
        memcpy(pos, text, i + 1);
        pos += i;
        text += i + 1;
        for (amp = pos - 1; i; i--)
            if (*(amp--) == '&')
                break;
        amp++;
        if (*amp == '&') {
            /* now there's a possible entity starting at amp
             * and ending at pos */
            *pos = '\0';
            ent = gwy_entities_entity_to_utf8(amp + 1);
            if (ent == amp + 1) {
                /* no entity */
                *(pos++) = ';';
            }
            else {
                /* entity found */
                strcpy(amp, ent);
                pos = amp + strlen(ent);
            }
        }
        else
            pos++;
    }

    memcpy(pos, text, end - text + 1);
    return result;
}

/**
 * gwy_entities_get_entities:
 *
 * Returns an array of all available entities.
 *
 * The array is terminated by { %NULL, %NULL } item.  It must be treated as
 * constant and never modified or freed.
 *
 * Returns: The entities as a #GwyTextEntity array.
 **/
G_CONST_RETURN GwyTextEntity*
gwy_entities_get_entities(void)
{
    return ENTITIES;
}

/************************** Documentation ****************************/

/**
 * GwyTextEntity:
 * @entity: Bare entity name, without the leading ampersand and trailing
 *          semicolon.
 * @utf8: The corresponding character, in UTF-8.
 *
 * The type of text entity data.
 **/


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
