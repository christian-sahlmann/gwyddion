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

static const GwyTextEntity entities[] = {
    /* Uppercase Greek letters */
    { "Alpha", "\xce\x91" },
    { "Beta", "\xce\x92" },
    { "Gamma", "\xce\x93" },
    { "Delta", "\xce\x94" },
    { "Epsilon", "\xce\x95" },
    { "Zeta", "\xce\x96" },
    { "Eta", "\xce\x97" },
    { "Theta", "\xce\x98" },
    { "Iota", "\xce\x99" },
    { "Kappa", "\xce\x9a" },
    { "Lambda", "\xce\x9b" },
    { "Mu", "\xce\x9c" },
    { "Nu", "\xce\x9d" },
    { "Xi", "\xce\x9e" },
    { "Omicron", "\xce\x9f" },
    { "Pi", "\xce\xa0" },
    { "Rho", "\xce\xa1" },
    { "Sigma", "\xce\xa3" },
    { "Tau", "\xce\xa4" },
    { "Upsilon", "\xce\xa5" },
    { "Phi", "\xce\xa6" },
    { "Chi", "\xce\xa7" },
    { "Psi", "\xce\xa8" },
    { "Omega", "\xce\xa9" },
    /* Lowercase Greek letters */
    { "alpha", "\xce\xb1" },
    { "beta", "\xce\xb2" },
    { "gamma", "\xce\xb3" },
    { "delta", "\xce\xb4" },
    { "epsilon", "\xce\xb5" },
    { "zeta", "\xce\xb6" },
    { "eta", "\xce\xb7" },
    { "theta", "\xce\xb8" },
    { "thetasym", "\xcf\x91" },
    { "iota", "\xce\xb9" },
    { "kappa", "\xce\xba" },
    { "varkappa", "\xcf\xb0" },
    { "lambda", "\xce\xbb" },
    { "mu", "\xce\xbc" },
    { "nu", "\xce\xbd" },
    { "xi", "\xce\xbe" },
    { "omicron", "\xce\xbf" },
    { "pi", "\xcf\x80" },
    { "rho", "\xcf\x81" },
    { "varrho", "\xcf\xb1" },
    { "sigma", "\xcf\x83" },
    { "sigmaf", "\xcf\x82" },
    { "tau", "\xcf\x84" },
    { "upsilon", "\xcf\x85" },
    { "phi", "\xcf\x86" },
    { "chi", "\xcf\x87" },
    { "psi", "\xcf\x88" },
    { "omega", "\xcf\x89" },
    /* Math symbols */
    { "alefsym", "\xe2\x84\xb5" },
    { "and", "\xe2\x88\xa7" },
    { "ang", "\xe2\x88\xa0" },
    { "asymp", "\xe2\x89\x88" },
    { "cap", "\xe2\x88\xa9" },
    { "cong", "\xe2\x89\x85" },
    { "cup", "\xe2\x88\xaa" },
    { "deg", "\xc2\xb0" },
    { "divide", "\xc3\xb7" },
    { "empty", "\xe2\x88\x85" },
    { "equiv", "\xe2\x89\xa1" },
    { "exist", "\xe2\x88\x83" },
    { "fnof", "\xc6\x92" },
    { "forall", "\xe2\x88\x80" },
    { "ge", "\xe2\x89\xa5" },
    { "image", "\xe2\x84\x91" },
    { "infin", "\xe2\x88\x9e" },
    { "int", "\xe2\x88\xab" },
    { "isin", "\xe2\x88\x88" },
    { "lArr", "\xe2\x87\x90" },
    { "lang", "\xe2\x8c\xa9" },
    { "larr", "\xe2\x86\x90" },
    { "le", "\xe2\x89\xa4" },
    { "lfloor", "\xe2\x8c\x8a" },
    { "micro", "\xc2\xb5" },
    { "middot", "\xc2\xb7" },
    { "minus", "\xe2\x88\x92" },
    { "nabla", "\xe2\x88\x87" },
    { "ne", "\xe2\x89\xa0" },
    { "ni", "\xe2\x88\x8b" },
    { "not", "\xc2\xac" },
    { "notin", "\xe2\x88\x89" },
    { "nsub", "\xe2\x8a\x84" },
    { "oplus", "\xe2\x8a\x95" },
    { "or", "\xe2\x88\xa8" },
    { "otimes", "\xe2\x8a\x97" },
    { "part", "\xe2\x88\x82" },
    { "permil", "\xe2\x80\xb0" },
    { "perp", "\xe2\x8a\xa5" },
    { "piv", "\xcf\x96" },
    { "plusmn", "\xc2\xb1" },
    { "prod", "\xe2\x88\x8f" },
    { "prop", "\xe2\x88\x9d" },
    { "rArr", "\xe2\x87\x92" },
    { "radic", "\xe2\x88\x9a" },
    { "rang", "\xe2\x8c\xaa" },
    { "rarr", "\xe2\x86\x92" },
    { "rceil", "\xe2\x8c\x89" },
    { "real", "\xe2\x84\x9c" },
    { "rfloor", "\xe2\x8c\x8b" },
    { "sdot", "\xe2\x8b\x85" },
    { "sim", "\xe2\x88\xbc" },
    { "sub", "\xe2\x8a\x82" },
    { "sube", "\xe2\x8a\x86" },
    { "sum", "\xe2\x88\x91" },
    { "sup", "\xe2\x8a\x83" },
    { "there4", "\xe2\x88\xb4" },
    { "times", "\xc3\x97" },
    { "weierp", "\xe2\x84\x98" },
    /* Text stuff */
    { "bull", "\xe2\x80\xa2" },
    { "cent", "\xc2\xa2" },
    { "circ", "\xcb\x86" },
    { "copy", "\xc2\xa9" },
    { "curren", "\xc2\xa4" },
    { "Dagger", "\xe2\x80\xa1" },
    { "dagger", "\xe2\x80\xa0" },
    { "euro", "\xe2\x82\xac" },
    { "hellip", "\xe2\x80\xa6" },
    { "pound", "\xc2\xa3" },
    { "reg", "\xc2\xae" },
    { "sect", "\xc2\xa7" },
    { "trade", "\xe2\x84\xa2" },
    { "yen", "\xc2\xa5" },
};

static GwyInventory *entity_inventory = NULL;

static const gchar*
gwy_entity_get_name(gpointer item)
{
    return ((const GwyTextEntity*)item)->entity;
}

static const GType*
gwy_entity_get_traits(gint *ntraits)
{
    static const GType traits[] = { G_TYPE_STRING, G_TYPE_STRING };

    if (ntraits)
        *ntraits = G_N_ELEMENTS(traits);

    return traits;
}

static const gchar*
gwy_entity_get_trait_name(gint i)
{
    static const gchar *trait_names[] = { "entity", "utf8" };

    g_return_val_if_fail(i >= 0 && i < G_N_ELEMENTS(trait_names), NULL);
    return trait_names[i];
}

static void
gwy_entity_get_trait_value(gpointer item,
                           gint i,
                           GValue *value)
{
    switch (i) {
        case 0:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, ((const GwyTextEntity*)item)->entity);
        break;

        case 1:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, ((const GwyTextEntity*)item)->utf8);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_entities:
 *
 * Returns a constant inventory with all available entities.
 *
 * Returns: The entities as a #GwyInventory.
 **/
GwyInventory*
gwy_entities(void)
{
    static const GwyInventoryItemType gwy_entity_item_type = {
        0,
        NULL,
        NULL,
        gwy_entity_get_name,
        NULL,
        NULL,
        NULL,
        NULL,
        gwy_entity_get_traits,
        gwy_entity_get_trait_name,
        gwy_entity_get_trait_value,
    };

    if (!entity_inventory)
        entity_inventory = gwy_inventory_new_from_array(&gwy_entity_item_type,
                                                        sizeof(GwyTextEntity),
                                                        G_N_ELEMENTS(entities),
                                                        entities);
    return entity_inventory;
}

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
 *          If the returned string is not equal to @entities, it's owned by
 *          entities and must not be freed nor modified.
 **/
const gchar*
gwy_entities_entity_to_utf8(const gchar *entity)
{
    const GwyTextEntity *ent;

    ent = gwy_inventory_get_item(gwy_entities(), entity);
    return ent ? ent->utf8 : entity;
}

/**
 * gwy_entities_text_to_utf8:
 * @text: A nul-delimited string.
 *
 * Converts entities in a text to UTF-8.
 *
 * Returns: A newly allocated nul-delimited string containing the converted
 *          text.
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

/************************** Documentation ****************************/

/**
 * SECTION:gwyentities
 * @title: gwyentities
 * @short_description: Transform SGML-like symbol entities to UTF-8
 *
 * A subset of named SGML-like symbol entities (e.g.
 * <literal>&amp;alpha;</literal>), to be used namely in #GwySciText.
 *
 * Function gwy_entities_entity_to_utf8() converts a signle entity to UTF-8.
 * Function gwy_entities_text_to_utf8() converts a text containing entities to
 * UTF-8. An #GwyInventory with all available entities can be obtained with
 * gwy_entities().
 **/

/**
 * GwyTextEntity:
 * @entity: Bare entity name, without the leading ampersand and trailing
 *          semicolon.
 * @utf8: The corresponding character, in UTF-8.
 *
 * The type of text entity data.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
