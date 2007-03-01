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
    { "Alpha", "Α" },
    { "Beta", "Β" },
    { "Gamma", "Γ" },
    { "Delta", "Δ" },
    { "Epsilon", "Ε" },
    { "Zeta", "Ζ" },
    { "Eta", "Η" },
    { "Theta", "Θ" },
    { "Iota", "Ι" },
    { "Kappa", "Κ" },
    { "Lambda", "Λ" },
    { "Mu", "Μ" },
    { "Nu", "Ν" },
    { "Xi", "Ξ" },
    { "Omicron", "Ο" },
    { "Pi", "Π" },
    { "Rho", "Ρ" },
    { "Sigma", "Σ" },
    { "Tau", "Τ" },
    { "Upsilon", "Υ" },
    { "Phi", "Φ" },
    { "Chi", "Χ" },
    { "Psi", "Ψ" },
    { "Omega", "Ω" },
    /* Lowercase Greek letters */
    { "alpha", "α" },
    { "beta", "β" },
    { "gamma", "γ" },
    { "delta", "δ" },
    { "epsilon", "ε" },
    { "varepsilon", "ϵ" },
    { "zeta", "ζ" },
    { "eta", "η" },
    { "theta", "θ" },
    { "thetasym", "ϑ" },
    { "iota", "ι" },
    { "kappa", "κ" },
    { "varkappa", "ϰ" },
    { "lambda", "λ" },
    { "mu", "μ" },
    { "nu", "ν" },
    { "xi", "ξ" },
    { "omicron", "ο" },
    { "pi", "π" },
    { "rho", "ρ" },
    { "varrho", "ϱ" },
    { "sigma", "σ" },
    { "sigmaf", "ς" },
    { "tau", "τ" },
    { "upsilon", "υ" },
    { "phi", "φ" },
    { "chi", "χ" },
    { "psi", "ψ" },
    { "omega", "ω" },
    /* Math symbols */
    { "alefsym", "א" },
    { "and", "∧" },
    { "ang", "∠" },
    { "asymp", "≈" },
    { "cap", "∩" },
    { "cong", "≅" },
    { "cup", "∪" },
    { "deg", "°" },
    { "divide", "÷" },
    { "empty", "∅" },
    { "equiv", "≡" },
    { "exist", "∃" },
    { "fnof", "ƒ" },
    { "forall", "∀" },
    { "ge", "≥" },
    { "image", "ℑ" },
    { "infin", "∞" },
    { "int", "∫" },
    { "isin", "∈" },
    { "lArr", "⇐" },
    { "lang", "〈" },
    { "larr", "←" },
    { "le", "≤" },
    { "lfloor", "⌊" },
    { "micro", "µ" },
    { "middot", "·" },
    { "minus", "−" },
    { "nabla", "∇" },
    { "ne", "≠" },
    { "ni", "∋" },
    { "not", "¬" },
    { "notin", "∉" },
    { "nsub", "⊄" },
    { "oplus", "⊕" },
    { "or", "∨" },
    { "otimes", "⊗" },
    { "part", "∂" },
    { "permil", "‰" },
    { "perp", "⊥" },
    { "piv", "ϖ" },
    { "plusmn", "±" },
    { "prod", "∏" },
    { "prop", "∝" },
    { "rArr", "⇒" },
    { "radic", "√" },
    { "rang", "〉" },
    { "rarr", "→" },
    { "rceil", "⌉" },
    { "real", "ℜ" },
    { "rfloor", "⌋" },
    { "sdot", "⋅" },
    { "sim", "∼" },
    { "sub", "⊂" },
    { "sube", "⊆" },
    { "sum", "∑" },
    { "sup", "⊃" },
    { "there4", "∴" },
    { "times", "×" },
    { "weierp", "℘" },
    /* Text stuff */
    { "bull", "•" },
    { "cent", "¢" },
    { "circ", "ˆ" },
    { "copy", "©" },
    { "curren", "¤" },
    { "Dagger", "‡" },
    { "dagger", "†" },
    { "euro", "€" },
    { "hellip", "…" },
    { "pound", "£" },
    { "reg", "®" },
    { "sect", "§" },
    { "trade", "™" },
    { "yen", "¥" },
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
