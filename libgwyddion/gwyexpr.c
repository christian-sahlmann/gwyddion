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

#include "gwymacros.h"

#include <string.h>
#include <glib.h>

#include "gwymath.h"
#include "gwyexpr.h"

#ifndef HAVE_HYPOT
#define hypot(x, y) sqrt((x)*(x) + (y)*(y))
#endif

#ifndef HAVE_POW10
#define pow10(x) exp(G_LN10*(x))
#endif

#ifndef HAVE_CBRT
#define cbrt(x) pow((x), 1.0/3.0)
#endif

#define GWY_EXPR_SCOPE_GLOBAL 0

/* things that can appear on code stack */
typedef enum {
    /* negative values are reserved for variables */
    GWY_EXPR_CODE_CONSTANT = 0,
    GWY_EXPR_CODE_NEGATE = 1,
    GWY_EXPR_CODE_ADD,
    GWY_EXPR_CODE_SUBTRACT,
    GWY_EXPR_CODE_MULTIPLY,
    GWY_EXPR_CODE_DIVIDE,
    GWY_EXPR_CODE_MODULO,
    GWY_EXPR_CODE_POWER,
    GWY_EXPR_CODE_POW,
    GWY_EXPR_CODE_MIN,
    GWY_EXPR_CODE_MAX,
    GWY_EXPR_CODE_FMOD,
    GWY_EXPR_CODE_HYPOT,
    GWY_EXPR_CODE_ATAN2,
    GWY_EXPR_CODE_ABS,
    GWY_EXPR_CODE_SQRT,
    GWY_EXPR_CODE_CBRT,
    GWY_EXPR_CODE_SIN,
    GWY_EXPR_CODE_COS,
    GWY_EXPR_CODE_TAN,
    GWY_EXPR_CODE_ASIN,
    GWY_EXPR_CODE_ACOS,
    GWY_EXPR_CODE_ATAN,
    GWY_EXPR_CODE_EXP,
    GWY_EXPR_CODE_LN,
    GWY_EXPR_CODE_LOG,
    GWY_EXPR_CODE_POW10,
    GWY_EXPR_CODE_LOG10,
    GWY_EXPR_CODE_COSH,
    GWY_EXPR_CODE_SINH,
    GWY_EXPR_CODE_TANH,
} GwyExprOpCode;

typedef struct {
    const gchar *name;
    gdouble value;
} GwyExprConstant;

/* code stack item */
typedef struct {
    GwyExprOpCode type;
    gdouble value;
} GwyExprCode;

typedef struct {
    void (*function)(gdouble**);
    const gchar *name;
    gshort in_values;
    gshort out_values;
    GwyExprOpCode type;  /* consistency check: must be equal to position */
} GwyExprFunction;

/* Transitional tokenizer token:
 * can hold both initial GScanner tokens and final GwyExpr RPM stacks */
typedef struct _GwyExprToken GwyExprToken;
struct _GwyExprToken {
    /* This is either GTokenType or GwyExprOpCode */
    gint token;
    /* GwyExprCode values are stored in value.v_float */
    GTokenValue value;
    struct _GwyExprToken *rpn_block;
    GwyExprToken *prev;
    GwyExprToken *next;
};

struct _GwyExpr {
    /* Global context */
    GScanner *scanner;
    GHashTable *constants;
    /* Variables */
    GPtrArray *identifiers;    /* variable names */
    const gdouble *variables;    /* user-filled variable values */
    /* Tokens */
    GMemChunk *token_chunk;
    GwyExprToken *tokens;
    GwyExprToken *reservoir;    /* deleted tokens accumulate here */
    /* Compiled RPN representation */
    GwyExprCode *input;    /* compiled expression */
    guint in;    /* nonzero in is a mark of successful compilation */
    guint ilen;    /* allocated size */
    /* Execution stack */
    gdouble *stack;    /* stack */
    gdouble *sp;    /* stack pointer */
    guint slen;    /* allocated size */
};

#define make_function_1_1(name) \
    static void gwy_expr_##name(gdouble **s) { **s = name(**s); }

#define make_function_2_1(name) \
    static void gwy_expr_##name(gdouble **s) { --*s; **s = name(*(*s+1), **s); }

make_function_1_1(sqrt)
make_function_1_1(cbrt)
make_function_1_1(sin)
make_function_1_1(cos)
make_function_1_1(tan)
make_function_1_1(exp)
make_function_1_1(log)
make_function_1_1(pow10)
make_function_1_1(log10)
make_function_1_1(asin)
make_function_1_1(acos)
make_function_1_1(atan)
make_function_1_1(cosh)
make_function_1_1(sinh)
make_function_1_1(tanh)
make_function_1_1(fabs)
make_function_2_1(pow)
make_function_2_1(hypot)
make_function_2_1(atan2)
make_function_2_1(fmod)

void gwy_expr_negate(gdouble **s) { **s = -(**s); }
void gwy_expr_add(gdouble **s) { --*s; **s = *(*s+1) + **s; }
void gwy_expr_subtract(gdouble **s) { --*s; **s = *(*s+1) - **s; }
void gwy_expr_multiply(gdouble **s) { --*s; **s = *(*s+1) * **s; }
void gwy_expr_divide(gdouble **s) { --*s; **s = *(*s+1) / **s; }
void gwy_expr_max(gdouble **s) { --*s; **s = MAX(*(*s+1), **s); }
void gwy_expr_min(gdouble **s) { --*s; **s = MIN(*(*s+1), **s); }

static const GwyExprFunction call_table[] = {
    { NULL,                NULL,     0,  0,  0                       },
    { gwy_expr_negate,     "~",      1,  1,  GWY_EXPR_CODE_NEGATE,   },
    { gwy_expr_add,        "+",      2,  1,  GWY_EXPR_CODE_ADD,      },
    { gwy_expr_subtract,   "-",      2,  1,  GWY_EXPR_CODE_SUBTRACT, },
    { gwy_expr_multiply,   "*",      2,  1,  GWY_EXPR_CODE_MULTIPLY, },
    { gwy_expr_divide,     "/",      2,  1,  GWY_EXPR_CODE_DIVIDE,   },
    { gwy_expr_fmod,       "%",      2,  1,  GWY_EXPR_CODE_MODULO,   },
    { gwy_expr_pow,        "^",      2,  1,  GWY_EXPR_CODE_POWER,    },
    { gwy_expr_pow,        "pow",    2,  1,  GWY_EXPR_CODE_POW,      },
    { gwy_expr_min,        "min",    2,  1,  GWY_EXPR_CODE_MIN,      },
    { gwy_expr_max,        "max",    2,  1,  GWY_EXPR_CODE_MAX,      },
    { gwy_expr_fmod,       "mod",    2,  1,  GWY_EXPR_CODE_FMOD,     },
    { gwy_expr_hypot,      "hypot",  2,  1,  GWY_EXPR_CODE_HYPOT,    },
    { gwy_expr_atan2,      "atan2",  2,  1,  GWY_EXPR_CODE_ATAN2,    },
    { gwy_expr_fabs,       "abs",    1,  1,  GWY_EXPR_CODE_ABS,      },
    { gwy_expr_sqrt,       "sqrt",   1,  1,  GWY_EXPR_CODE_SQRT,     },
    { gwy_expr_cbrt,       "cbrt",   1,  1,  GWY_EXPR_CODE_CBRT,     },
    { gwy_expr_sin,        "sin",    1,  1,  GWY_EXPR_CODE_SIN,      },
    { gwy_expr_cos,        "cos",    1,  1,  GWY_EXPR_CODE_COS,      },
    { gwy_expr_tan,        "tan",    1,  1,  GWY_EXPR_CODE_TAN,      },
    { gwy_expr_asin,       "asin",   1,  1,  GWY_EXPR_CODE_ASIN,     },
    { gwy_expr_acos,       "acos",   1,  1,  GWY_EXPR_CODE_ACOS,     },
    { gwy_expr_atan,       "atan",   1,  1,  GWY_EXPR_CODE_ATAN,     },
    { gwy_expr_exp,        "exp",    1,  1,  GWY_EXPR_CODE_EXP,      },
    { gwy_expr_log,        "ln",     1,  1,  GWY_EXPR_CODE_LN,       },
    { gwy_expr_log,        "log",    1,  1,  GWY_EXPR_CODE_LOG,      },
    { gwy_expr_pow10,      "pow10",  1,  1,  GWY_EXPR_CODE_POW10,    },
    { gwy_expr_log10,      "log10",  1,  1,  GWY_EXPR_CODE_LOG10,    },
    { gwy_expr_cosh,       "cosh",   1,  1,  GWY_EXPR_CODE_COSH,     },
    { gwy_expr_sinh,       "sinh",   1,  1,  GWY_EXPR_CODE_SINH,     },
    { gwy_expr_tanh,       "tanh",   1,  1,  GWY_EXPR_CODE_TANH,     },
};

static const GwyExprConstant constant_table[] = {
    { "Pi",      G_PI            },
    { "E",       G_E             },
    /*
    { "c",       2.99792458e8    },
    { "h",       1.0545887e-34   },
    { "k",       1.380662e-23    },
    { "G",       6.672e-11       },
    { "e",       1.6021982e-19   },
    */
};

static const GScannerConfig scanner_config = {
    /* character sets */
    " \t\n\r",
    G_CSET_a_2_z G_CSET_A_2_Z,
    G_CSET_a_2_z G_CSET_A_2_Z "0123456789_",
    NULL,
    /* case sensitive */
    TRUE,
    /* comments */
    FALSE, FALSE, FALSE,
    /* identifiers */
    TRUE, TRUE, FALSE, TRUE,
    /* number formats */
    FALSE, FALSE, TRUE, TRUE, FALSE,
    /* strings */
    FALSE, FALSE,
    /* conversions */
    TRUE, TRUE, FALSE, TRUE, FALSE,
    /* options */
    FALSE, FALSE, 0,
};

static GQuark error_domain = 0;
static gboolean table_sanity_checked = FALSE;

static gboolean
gwy_expr_check_call_table_sanity(void)
{
    gboolean ok = TRUE;
    guint i;

    for (i = 1; i < G_N_ELEMENTS(call_table); i++) {
        if (call_table[i].type != i) {
            g_critical("Inconsistent call table at pos %u\n", i);
            ok = FALSE;
        }
    }

    return ok;
}

/****************************************************************************
 *
 *  Execution
 *
 ****************************************************************************/

/**
 * gwy_expr_stack_interpret:
 * @expr: An expression.
 *
 * Runs code in @expr->input, at the end, result is in @expr->stack[0].
 *
 * No checking is done, use gwy_expr_stack_check_executability() beforehand.
 **/
static inline void
gwy_expr_stack_interpret(GwyExpr *expr)
{
    guint i;

    expr->sp = expr->stack - 1;
    for (i = 0; i < expr->in; i++) {
        GwyExprCode *code = expr->input + i;

        if (code->type == GWY_EXPR_CODE_CONSTANT)
            *(++expr->sp) = code->value;
        else if ((gint)code->type > 0)
            call_table[code->type].function(&expr->sp);
        else
            *(++expr->sp) = expr->variables[-(gint)code->type];
    }
}

/**
 * gwy_expr_stack_check_executability:
 * @expr: An expression.
 *
 * Checks whether a stack is executable and assures it's large enough.
 *
 * Returns: %TRUE if stack is executable, %FALSE if it isn't.
 **/
static gboolean
gwy_expr_stack_check_executability(GwyExpr *expr)
{
    guint i;
    gint nval, max;

    nval = max = 0;
    for (i = 0; i < expr->in; i++) {
        if (expr->input[i].type == GWY_EXPR_CODE_CONSTANT
            || (gint)expr->input[i].type < 0)
            nval++;
        else if (expr->input[i].type > 0) {
            nval -= call_table[expr->input[i].type].in_values;
            nval += call_table[expr->input[i].type].out_values;
            if (!nval)
                return FALSE;
        }
        if (nval > max)
            max = nval;
    }
    if (nval != 1)
        return FALSE;

    if (expr->slen < max) {
        expr->slen = max;
        expr->stack = g_renew(gdouble, expr->stack, expr->slen);
    }

    return TRUE;
}

G_GNUC_UNUSED static void
gwy_expr_print_stack(GwyExpr *expr)
{
    guint i;
    GwyExprCode *code;

    for (i = 0; i < expr->in; i++) {
        code = expr->input + i;
        if ((gint)code->type > 0) {
            g_print("Function %s\n",
                    call_table[code->type].name);
        }
        else if ((gint)code->type < 0) {
            g_print("Argument %s\n",
                    (gchar*)g_ptr_array_index(expr->identifiers, -code->type));
        }
        else
            g_print("Constant %g\n", code->value);
    }
}

/****************************************************************************
 *
 *  Reimplementation of interesting parts of GList
 *
 ****************************************************************************/

/**
 * gwy_expr_token_list_last:
 * @tokens: A list of tokens.
 *
 * Returns last token in list.
 *
 * Returns: The last token, %NULL if list is empty.
 **/
static inline GwyExprToken*
gwy_expr_token_list_last(GwyExprToken *tokens)
{
    if (G_UNLIKELY(!tokens))
        return NULL;
    while (tokens->next)
        tokens = tokens->next;

    return tokens;
}

/* XXX: used only once */
/**
 * gwy_expr_token_list_length:
 * @tokens: A list of tokens.
 *
 * Counts tokens in a list.
 *
 * Returns: Token list length.
 **/
static inline guint
gwy_expr_token_list_length(GwyExprToken *tokens)
{
    guint i = 0;

    while (tokens) {
        i++;
        tokens = tokens->next;
    }

    return i;
}

/**
 * gwy_expr_token_list_prepend:
 * @tokens: A list of tokens.
 * @token: A token.
 *
 * Prepends a token before a list.
 *
 * Returns: The new list head, that is @token.
 **/
static inline GwyExprToken*
gwy_expr_token_list_prepend(GwyExprToken *tokens,
                            GwyExprToken *token)
{
    token->next = tokens;
    if (tokens)
        tokens->prev = token;

    return token;
}

/* XXX: used only once */
/**
 * gwy_expr_token_list_reverse:
 * @tokens: A list of tokens.
 *
 * Reverts the order in a token list.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
gwy_expr_token_list_reverse(GwyExprToken *tokens)
{
    GwyExprToken *end;

    end = NULL;
    while (tokens) {
        end = tokens;
        tokens = end->next;
        end->next = end->prev;
        end->prev = tokens;
    }

    return end;
}

/**
 * gwy_expr_token_list_concat:
 * @head: First token list.
 * @tail: Second token list.
 *
 * Concatenates two token lists.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
gwy_expr_token_list_concat(GwyExprToken *head,
                           GwyExprToken *tail)
{
    GwyExprToken *end;

    if (!head)
        return tail;

    end = gwy_expr_token_list_last(head);
    end->next = tail;
    if (tail)
        tail->prev = end;

    return head;
}

/**
 * gwy_expr_token_list_delete_token:
 * @expr: An expression.
 * @tokens: A list of tokens.
 * @token: A token.
 *
 * Unlinks a token from list and deletes it.
 *
 * Note if token holds some data (including RPN block), it must be freed first.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
gwy_expr_token_list_delete_token(GwyExpr *expr,
                                 GwyExprToken *tokens,
                                 GwyExprToken *token)
{
    if (token->prev)
        token->prev->next = token->next;
    else
        tokens = token->next;

    if (token->next)
        token->next->prev = token->prev;

    token->prev = NULL;
    expr->reservoir = gwy_expr_token_list_prepend(expr->reservoir, token);

    return tokens;
}

/**
 * gwy_expr_token_list_insert:
 * @tokens: A list of tokens.
 * @before: A token to insert @token before.
 * @token: Token to insert.
 *
 * Inserts a token into a token list before given token.
 *
 * Returns: The new list head.
 **/
static inline GwyExprToken*
gwy_expr_token_list_insert(GwyExprToken *tokens,
                           GwyExprToken *before,
                           GwyExprToken *token)
{
    if (!before->prev)
        return gwy_expr_token_list_prepend(tokens, token);

    token->prev = before->prev;
    token->next = before;
    before->prev->next = token;
    before->prev = token;

    return tokens;
}

/**
 * gwy_expr_token_new0:
 * @expr: An expression.
 *
 * Allocates (or revives) and initialized a new token.
 *
 * Returns: A new token, initialized to zero/%NULL.
 **/
static inline GwyExprToken*
gwy_expr_token_new0(GwyExpr *expr)
{
    GwyExprToken *token;

    if (G_UNLIKELY(!expr->reservoir))
        return g_chunk_new0(GwyExprToken, expr->token_chunk);

    token = expr->reservoir;
    expr->reservoir = token->next;
    if (expr->reservoir)
        expr->reservoir->prev = NULL;
    memset(token, 0, sizeof(GwyExprToken));

    return token;
}

/**
 * gwy_expr_token_list_delete:
 * @expr: An expression.
 * @tokens: A token list.
 *
 * Actually frees token list.
 *
 * Assumes each token is either unconverted scanner token, or it has non-empty
 * RPN block.  In either case it frees the RPN block, or strings of uncoverted
 * scanner tokens.
 **/
static void
gwy_expr_token_list_delete(GwyExpr *expr,
                           GwyExprToken *tokens)
{
    GwyExprToken *t;

    for (t = tokens; t; t = t->next) {
        if (t->rpn_block) {
            expr->reservoir = gwy_expr_token_list_concat(t->rpn_block,
                                                         expr->reservoir);
        }
        else {
            switch (t->token) {
                case G_TOKEN_STRING:
                case G_TOKEN_IDENTIFIER:
                case G_TOKEN_IDENTIFIER_NULL:
                case G_TOKEN_COMMENT_SINGLE:
                case G_TOKEN_COMMENT_MULTI:
                g_free(t->value.v_string);
                break;

                default:
                break;
            }
        }
    }
    expr->reservoir = gwy_expr_token_list_concat(tokens, expr->reservoir);
}

/****************************************************************************
 *
 *  Infix -> RPN convertor, tokenizer
 *
 ****************************************************************************/

/**
 * gwy_expr_scan_tokens:
 * @expr: An expression.
 * @err: Location to store scanning error to.
 *
 * Scans input to tokens, filling @expr->tokens.
 *
 * Returns: %TRUE on success, %FALSE if parsing failed.
 **/
static gboolean
gwy_expr_scan_tokens(GwyExpr *expr,
                     GError **err)
{
    GScanner *scanner;
    GwyExprToken *tokens = NULL, *t;
    GTokenType token;

    if (expr->tokens) {
        g_warning("Token list residua from last run");
        gwy_expr_token_list_delete(expr, expr->tokens);
    }
    expr->tokens = NULL;

    scanner = expr->scanner;
    while ((token = g_scanner_get_next_token(scanner))) {
        switch (token) {
            case G_TOKEN_LEFT_PAREN:
            case G_TOKEN_RIGHT_PAREN:
            case G_TOKEN_COMMA:
            case '*':
            case '/':
            case '-':
            case '+':
            case '%':
            case '^':
            case '~':
            case G_TOKEN_FLOAT:
            case G_TOKEN_SYMBOL:
            case G_TOKEN_IDENTIFIER:
            t = gwy_expr_token_new0(expr);
            t->token = token;
            t->value = expr->scanner->value;
            tokens = gwy_expr_token_list_prepend(tokens, t);
            /* XXX: steal token value from scanner to avoid string duplication
             * and freeing */
            scanner->value.v_string = NULL;
            scanner->token = G_TOKEN_NONE;
            break;

            default:
            g_set_error(err, error_domain, GWY_EXPR_ERROR_INVALID_TOKEN,
                        "Invalid token");
            gwy_expr_token_list_delete(expr, tokens);
            return FALSE;
            break;
        }
    }

    if (!tokens) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_EMPTY, "No tokens");
        return FALSE;
    }

    expr->tokens = gwy_expr_token_list_reverse(tokens);
    return TRUE;
}

/**
 * gwy_expr_rectify_token_list:
 * @expr: An expression.
 *
 * Converts some human-style notations in @expr->tokens to stricter ones.
 *
 * Namely it: Removes unary +, changes unary - to ~ operator, adds
 * multiplication operators between adjacent values, converts ~ to operator.
 **/
static void
gwy_expr_rectify_token_list(GwyExpr *expr)
{
    GwyExprToken *t, *prev;

    for (t = expr->tokens; t; ) {
        prev = t->prev;

        switch (t->token) {
            /* convert unary - to negate operation */
            case '-':
            if (!prev || (prev->token != G_TOKEN_FLOAT
                          && prev->token != G_TOKEN_IDENTIFIER
                          && prev->token != G_TOKEN_RIGHT_PAREN)) {
                t->token = G_TOKEN_SYMBOL;
                t->value.v_symbol = GUINT_TO_POINTER(GWY_EXPR_CODE_NEGATE);
            }
            t = t->next;
            break;

            /* remove unary + */
            case '+':
            if (!prev || (prev->token != G_TOKEN_FLOAT
                          && prev->token != G_TOKEN_IDENTIFIER
                          && prev->token != G_TOKEN_RIGHT_PAREN)) {
                prev = t;
                t = t->next;
                expr->tokens = gwy_expr_token_list_delete_token(expr,
                                                                expr->tokens,
                                                                prev);
            }
            else
                t = t->next;
            break;

            /* add proper multiplications */
            case G_TOKEN_LEFT_PAREN:
            case G_TOKEN_FLOAT:
            case G_TOKEN_IDENTIFIER:
            case G_TOKEN_SYMBOL:
            if (prev && (prev->token == G_TOKEN_FLOAT
                         || prev->token == G_TOKEN_RIGHT_PAREN
                         || prev->token == G_TOKEN_IDENTIFIER)) {
                prev = gwy_expr_token_new0(expr);
                prev->token = '*';
                expr->tokens = gwy_expr_token_list_insert(expr->tokens,
                                                          t, prev);
            }
            t = t->next;
            break;

            /* convert ~ to function */
            case '~':
            t->token = G_TOKEN_SYMBOL;
            t->value.v_symbol = GUINT_TO_POINTER(GWY_EXPR_CODE_NEGATE);
            break;

            default:
            t = t->next;
            break;
        }
    }
}

/**
 * gwy_expr_parse:
 * @expr: An expression.
 * @text: String containing expression to parse.
 * @err: Location to store parsing error to
 *
 * Parses an expression to list of tokens, filling @expr->tokens.
 *
 * Returns: A newly allocated token list, %NULL on failure.
 **/
static gboolean
gwy_expr_parse(GwyExpr *expr,
               const gchar *text,
               GError **err)
{
    guint i;

    if (!expr->scanner) {
        expr->scanner = g_scanner_new(&scanner_config);

        for (i = 1; i < G_N_ELEMENTS(call_table); i++) {
            if (!call_table[i].name || !g_ascii_isalpha(call_table[i].name[0]))
                continue;
            g_scanner_scope_add_symbol(expr->scanner, GWY_EXPR_SCOPE_GLOBAL,
                                       call_table[i].name, GUINT_TO_POINTER(i));
        }
        g_scanner_set_scope(expr->scanner, GWY_EXPR_SCOPE_GLOBAL);
        expr->scanner->input_name = "expression";
    }

    g_scanner_input_text(expr->scanner, text, strlen(text));

    if (!gwy_expr_scan_tokens(expr, err)) {
        g_assert(!err || *err);
        return FALSE;
    }
    gwy_expr_rectify_token_list(expr);

    return TRUE;
}

/**
 * gwy_expr_transform_values:
 * @expr: An expression.
 *
 * Converts constants to single-items RPN lists and indexes identifiers.
 *
 * %G_TOKEN_IDENTIFIER strings are freed, they are converted to single-item
 * RPN lists too, with type as minus index in returned array of name.
 *
 * Modifies @expr->tokens and fills @expr->identifiers.
 *
 * Returns: %TRUE on success, %FALSE if transformation failed.
 **/
static gboolean
gwy_expr_transform_values(GwyExpr *expr)
{
    GwyExprToken *code, *t;
    gdouble *cval;
    guint i;

    if (!expr->identifiers) {
        expr->identifiers = g_ptr_array_new();
        /* pos 0 is always unused */
        g_ptr_array_add(expr->identifiers, NULL);
    }
    else
        g_ptr_array_set_size(expr->identifiers, 1);

    for (t = expr->tokens; t; t = t->next) {
        if (t->token == G_TOKEN_FLOAT) {
            code = gwy_expr_token_new0(expr);
            code->token = GWY_EXPR_CODE_CONSTANT;
            code->value.v_float = t->value.v_float;
            t->rpn_block = gwy_expr_token_list_prepend(t->rpn_block, code);
            continue;
        }
        else if (t->token != G_TOKEN_IDENTIFIER)
            continue;

        if (expr->constants) {
            if ((cval = g_hash_table_lookup(expr->constants,
                                            t->value.v_identifier))) {
                code = gwy_expr_token_new0(expr);
                code->token = GWY_EXPR_CODE_CONSTANT;
                code->value.v_float = *cval;
                t->rpn_block = gwy_expr_token_list_prepend(t->rpn_block, code);
                continue;
            }
        }
        for (i = 1; i < expr->identifiers->len; i++) {
            if (strcmp(t->value.v_identifier,
                       g_ptr_array_index(expr->identifiers, i)) == 0) {
                g_free(t->value.v_identifier);
                break;
            }
        }
        if (i == expr->identifiers->len)
            g_ptr_array_add(expr->identifiers, t->value.v_identifier);
        code = gwy_expr_token_new0(expr);
        code->token = -i;
        t->rpn_block = gwy_expr_token_list_prepend(t->rpn_block, code);
    }

    return TRUE;
}

/**
 * gwy_expr_transform_infix_ops:
 * @expr: An expression.
 * @tokens: A token list.
 * @right_to_left: %TRUE to process operators right to left, %FALSE to
 *                 left to right.
 * @operators: String containing one-letter operators to convert.
 * @codes: Opcodes corresponding to @operators.
 * @err: Location to store conversion error to.
 *
 * Transforms a set of infix operators of equal priority to RPN blocks.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
gwy_expr_transform_infix_ops(GwyExpr *expr,
                             GwyExprToken *tokens,
                             gboolean right_to_left,
                             const gchar *operators,
                             const GwyExprOpCode *codes,
                             GError **err)
{
    GwyExprToken *prev, *next, *code, *t;
    guint i;

    for (t = right_to_left ? gwy_expr_token_list_last(tokens) : tokens;
         t;
         t = right_to_left ? t->prev : t->next) {
        if (t->rpn_block)
            continue;
        for (i = 0; operators[i]; i++) {
            if (t->token == (guint)operators[i])
                break;
        }
        if (!operators[i])
            continue;

        /* Check arguments */
        prev = t->prev;
        next = t->next;
        if (!next || !prev) {
            g_set_error(err, error_domain, GWY_EXPR_ERROR_MISSING_ARGUMENT,
                        "Missing operator %c argument", operators[i]);
            gwy_expr_token_list_delete(expr, tokens);
            return NULL;
        }
        if (!prev->rpn_block || !next->rpn_block) {
            g_set_error(err, error_domain, GWY_EXPR_ERROR_INVALID_ARGUMENT,
                        "Invalid operator %c argument", operators[i]);
            gwy_expr_token_list_delete(expr, tokens);
            return NULL;
        }

        /* Convert */
        code = gwy_expr_token_new0(expr);
        code->token = codes[i];
        prev->rpn_block = gwy_expr_token_list_concat(prev->rpn_block, code);
        prev->rpn_block = gwy_expr_token_list_concat(next->rpn_block,
                                                     prev->rpn_block);
        prev->token = G_TOKEN_NONE;
        t = prev;
        tokens = gwy_expr_token_list_delete_token(expr, tokens, t->next);
        tokens = gwy_expr_token_list_delete_token(expr, tokens, t->next);
    }

    return tokens;
}

/**
 * gwy_expr_transform_functions:
 * @expr: An expression.
 * @tokens: A token list.
 * @err: Location to store conversion error to.
 *
 * Transforms function calls and unary operators to RPN blocks.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
gwy_expr_transform_functions(GwyExpr *expr,
                             GwyExprToken *tokens,
                             GError **err)
{
    GwyExprToken *arg, *code, *t;
    guint func, nargs, i;

    for (t = gwy_expr_token_list_last(tokens); t; t = t->prev) {
        if (t->token != G_TOKEN_SYMBOL)
            continue;

        func = GPOINTER_TO_UINT(t->value.v_symbol);
        nargs = call_table[func].in_values;
        /* Check arguments */
        for (i = 0, arg = t->next; i < nargs; i++, arg = arg->next) {
            if (!arg) {
                g_set_error(err, error_domain, GWY_EXPR_ERROR_MISSING_ARGUMENT,
                            "Missing %s argument", call_table[func].name);
                gwy_expr_token_list_delete(expr, tokens);
                return NULL;
            }
            if (!arg->rpn_block) {
                g_set_error(err, error_domain, GWY_EXPR_ERROR_INVALID_ARGUMENT,
                            "Invalid %s argument", call_table[func].name);
                gwy_expr_token_list_delete(expr, tokens);
                return NULL;
            }
        }

        /* Convert */
        code = gwy_expr_token_new0(expr);
        code->token = func;
        t->rpn_block = gwy_expr_token_list_prepend(t->rpn_block, code);

        for (i = 0; i < nargs; i++) {
            arg = t->next;
            t->token = G_TOKEN_NONE;
            t->rpn_block = gwy_expr_token_list_concat(arg->rpn_block,
                                                      t->rpn_block);
            tokens = gwy_expr_token_list_delete_token(expr, tokens, arg);
        }
    }

    return tokens;
}

/**
 * gwy_expr_transform_to_rpn_real:
 * @expr: An expression.
 * @tokens: A parenthesized list of tokens.
 * @err: Location to store conversion error to
 *
 * Recursively converts list of tokens in human (infix) notation to RPN.
 *
 * Returns: Converted @tokens (it's changed in place), %NULL on failure.
 **/
static GwyExprToken*
gwy_expr_transform_to_rpn_real(GwyExpr *expr,
                               GwyExprToken *tokens,
                               GError **err)
{
    GwyExprOpCode pow_operators[] = {
        GWY_EXPR_CODE_POWER,
    };
    GwyExprOpCode mult_operators[] = {
        GWY_EXPR_CODE_MULTIPLY, GWY_EXPR_CODE_DIVIDE, GWY_EXPR_CODE_MODULO,
    };
    GwyExprOpCode add_operators[] = {
        GWY_EXPR_CODE_ADD, GWY_EXPR_CODE_SUBTRACT,
    };
    GwyExprToken *t, *subblock, *remaindr = NULL;
    static guint level = 0;

    if (!tokens) {
        g_warning("Empty token list");
        return NULL;
    }
    level++;

    if (tokens->token != G_TOKEN_LEFT_PAREN) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_OPENING_PAREN,
                    "Missing opening parenthesis");
        goto FAIL;
    }
    tokens = gwy_expr_token_list_delete_token(expr, tokens, tokens);

    /* isolate the list (parenthesization level) we are responsible for */
    for (t = tokens; t; t = t->next) {
        /* split rest of list to remainder */
        if (t->token == G_TOKEN_RIGHT_PAREN) {
            if (t->next) {
                remaindr = t->next;
                remaindr->prev = NULL;
                t->next = NULL;
            }
            tokens = gwy_expr_token_list_delete_token(expr, tokens, t);
            break;
        }
        else if (t->token == G_TOKEN_LEFT_PAREN) {
            subblock = t;
            if (t->prev) {
                t = t->prev;
                t->next = NULL;
            }
            else
                t = tokens = NULL;
            subblock->prev = NULL;
            subblock = gwy_expr_transform_to_rpn_real(expr, subblock, err);
            if (!subblock) {
                g_assert(!err || *err);
                goto FAIL;
            }
            if (t) {
                t->next = subblock;
                subblock->prev = t;
            }
            else
                t = tokens = subblock;
        }
    }
    /* missing right parenthesis or empty parentheses */
    if (!t) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_CLOSING_PAREN,
                    "Missing closing parenthesis");
        goto FAIL;
    }
    if (!tokens) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_EMPTY_PARENTHESES,
                    "Empty parentheses");
        goto FAIL;
    }

    /* 0. Remove commas */
    for (t = tokens; t; t = t->next) {
        if (t->token == G_TOKEN_COMMA) {
            if (!t->next || !t->prev) {
                g_set_error(err, error_domain, GWY_EXPR_ERROR_STRAY_COMMA,
                            "Stray comma");
                goto FAIL;
            }
            t = t->prev;
            tokens = gwy_expr_token_list_delete_token(expr, tokens, t->next);
        }
    }

    /* 1. Functions */
    if (!(tokens = gwy_expr_transform_functions(expr, tokens, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 2. Power operator */
    if (!(tokens = gwy_expr_transform_infix_ops(expr, tokens, TRUE, "^",
                                                pow_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 3. Multiplicative operators */
    if (!(tokens = gwy_expr_transform_infix_ops(expr, tokens, FALSE, "*/%",
                                                mult_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* 4. Additive operators */
    if (!(tokens = gwy_expr_transform_infix_ops(expr, tokens, FALSE, "+-",
                                                add_operators, err))) {
        g_assert(!err || *err);
        goto FAIL;
    }

    /* Check */
    for (t = tokens; t; t = t->next) {
        if (!t->rpn_block) {
            g_set_error(err, error_domain, GWY_EXPR_ERROR_GARBAGE,
                        "Stray symbol %d", t->token);
            goto FAIL;
        }
    }

    tokens = gwy_expr_token_list_concat(tokens, remaindr);
    level--;
    return tokens;

FAIL:
    gwy_expr_token_list_delete(expr, tokens);
    gwy_expr_token_list_delete(expr, remaindr);
    level--;
    return NULL;
}

/**
 * gwy_expr_transform_to_rpn:
 * @expr: An expression.
 * @err: Location to store conversion error to.
 *
 * Converts list of tokens from parser to RPN stack.
 *
 * @expr->tokens is destroyed by the conversion and set to %NULL,
 * @expr->input is filled with opcodes, stack is checked for executability
 * and eventually resized.
 *
 * Returns: A newly created RPN stack, %NULL on failure.
 **/
static gboolean
gwy_expr_transform_to_rpn(GwyExpr *expr,
                          GError **err)
{
    GwyExprToken *t;
    guint i;

    /* parenthesize token list */
    t = gwy_expr_token_new0(expr);
    t->token = G_TOKEN_RIGHT_PAREN;
    expr->tokens = gwy_expr_token_list_concat(expr->tokens, t);
    t = gwy_expr_token_new0(expr);
    t->token = G_TOKEN_LEFT_PAREN;
    expr->tokens = gwy_expr_token_list_prepend(expr->tokens, t);

    expr->tokens = gwy_expr_transform_to_rpn_real(expr, expr->tokens, err);
    if (!expr->tokens) {
        g_assert(!err || *err);
        return FALSE;
    }

    if (expr->tokens->next) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_GARBAGE,
                    "Trailing garbage");
        gwy_expr_token_list_delete(expr, expr->tokens);
        expr->tokens = NULL;
        return FALSE;
    }

    expr->in = gwy_expr_token_list_length(expr->tokens->rpn_block);
    if (expr->in > expr->ilen) {
        expr->ilen = expr->in;
        expr->input = g_renew(GwyExprCode, expr->input, expr->ilen);
    }
    for (t = expr->tokens->rpn_block, i = 0; t; t = t->next, i++) {
        expr->input[i].type = t->token;
        expr->input[i].value = t->value.v_float;
    }
    gwy_expr_token_list_delete(expr, expr->tokens);
    expr->tokens = NULL;

    if (!gwy_expr_stack_check_executability(expr)) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_NOT_EXECUTABLE,
                    "Stack not executable");
        return FALSE;
    }

    return TRUE;
}

/****************************************************************************
 *
 *  High level, public API
 *
 ****************************************************************************/

/**
 * gwy_expr_new:
 *
 * Creates a new expression evaluator.
 *
 * Returns: A newly created expression evaluator.
 *
 * Since: 1.9
 **/
GwyExpr*
gwy_expr_new(void)
{
    GwyExpr *expr;
    guint i;

    if (!error_domain)
        error_domain = g_quark_from_static_string("GWY_EXPR_ERROR");

    if (!table_sanity_checked) {
        if (!gwy_expr_check_call_table_sanity())
            return NULL;
        table_sanity_checked = TRUE;
    }

    expr = g_new0(GwyExpr, 1);
    expr->token_chunk = g_mem_chunk_new("GwyExprToken",
                                        sizeof(GwyExprToken),
                                        32*sizeof(GwyExprToken),
                                        G_ALLOC_ONLY);

    /* FIXME: manipulate constants through API */
    expr->constants = g_hash_table_new(g_str_hash, g_str_equal);
    for (i = 0; i < G_N_ELEMENTS(constant_table); i++)
        g_hash_table_insert(expr->constants,
                            (gpointer)constant_table[i].name,
                            (gpointer)&constant_table[i].value);

    return expr;
}

/**
 * gwy_expr_free:
 * @expr: An expression evaluator.
 *
 * Frees all memory used by and expression evaluator.
 *
 * Since: 1.9
 **/
void
gwy_expr_free(GwyExpr *expr)
{
    gwy_expr_token_list_delete(expr, expr->tokens);
    g_mem_chunk_destroy(expr->token_chunk);
    if (expr->identifiers)
       g_ptr_array_free(expr->identifiers, TRUE);
    if (expr->scanner)
        g_scanner_destroy(expr->scanner);
    if (expr->constants)
        g_hash_table_destroy(expr->constants);
    g_free(expr->input);
    g_free(expr->stack);
    g_free(expr);
}

/**
 * gwy_expr_evaluate:
 * @expr: An expression evaluator.
 * @text: String containing expression to evaluate.
 * @result: Location to store result to.
 * @err: Location to store compilation error to.
 *
 * Evaulates an arithmetic expression.
 *
 * Returns: %TRUE on success, %FALSE if evaluation failed.
 *
 * Since: 1.9
 **/
gboolean
gwy_expr_evaluate(GwyExpr *expr,
                  const gchar *text,
                  gdouble *result,
                  GError **err)
{
    if (!gwy_expr_compile(expr, text, err))
        return FALSE;

    if (expr->identifiers->len > 1) {
        g_set_error(err, error_domain, GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS,
                    "Unresolved identifiers");
        return FALSE;
    }

    gwy_expr_stack_interpret(expr);
    *result = *expr->stack;

    return TRUE;
}

/**
 * gwy_expr_compile:
 * @expr: An expression evaluator.
 * @text: String containing expression to compile.
 * @err: Location to store compilation error to.
 *
 * Compiles an expression for later execution.
 *
 * This function is useful for expressions with variables.  For normal
 * arithmetic expressions it's easier to use gwy_expr_evaluate().
 *
 * Returns: %TRUE on success, %FALSE if compilation failed.
 *
 * Since: 1.9
 **/
gboolean
gwy_expr_compile(GwyExpr *expr,
                 const gchar *text,
                 GError **err)
{
    g_return_val_if_fail(expr, FALSE);
    g_return_val_if_fail(text, FALSE);

    if (!gwy_expr_parse(expr, text, err)
        || !gwy_expr_transform_values(expr)
        || !gwy_expr_transform_to_rpn(expr, err)) {
        g_assert(!err || *err);
        expr->in = 0;
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_expr_get_variables:
 * @expr: An expression evaluator.
 * @names: Location to store list of variable names to (may be also %NULL).
 *         The string array returned in this argument in owned by @expr and
 *         is valid only until next gwy_expr_compile(), gwy_expr_evaluate(),
 *         eventually gwy_expr_free().
 *
 * Get the number, names, and indices of unresolved identifiers in @expr.
 *
 * It is an error to call this function after an unsuccessful compilation.
 *
 * The position of each variable in @names corresponds to value position in
 * @values array in gwy_expr_execute() call.
 *
 * Returns: The number of unresolved identifiers, that is length of array
 *          stored to @names.  On failure, -1 is returned.
 *
 * Since: 1.9
 **/
gint
gwy_expr_get_variables(GwyExpr *expr,
                       gchar ***names)
{
    g_return_val_if_fail(expr, -1);
    g_return_val_if_fail(names, -1);

    if (!expr->in)
        return -1;

    if (names)
        *names = (gchar**)expr->identifiers->pdata + 1;

    return expr->identifiers->len - 1;
}

/**
 * gwy_expr_execute:
 * @expr: An expression evaluator.
 * @values: Array with variable values.  Variable list can be obtained
 *          by gwy_expr_get_variables().
 *
 * Executes a compiled expression with variables, substituting given values.
 *
 * Returns: The result.
 *
 * Since: 1.9
 **/
gdouble
gwy_expr_execute(GwyExpr *expr,
                 const gdouble *values)
{
    expr->variables = values - 1;
    gwy_expr_stack_interpret(expr);
    return expr->stack[0];
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
