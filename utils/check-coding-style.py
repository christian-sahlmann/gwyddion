#!/usr/bin/python
from __future__ import generators
import re, sys, types

"""\
Check for some gwyddion coding style violations.

Can be set as makeprg in vim

    :set makeprg=check-coding-style.py\ %

and then simple :make performs the check.
"""

# Compatibility with Pyhton-2.2
if not __builtins__.__dict__.has_key('enumerate'):
    def enumerate(collection):
        i = 0
        it = iter(collection)
        while True:
            yield i, it.next()
            i += 1

def blstrip(s, col):
    b = len(s)
    s = s.lstrip()
    return s, col + b - len(s)

class Token:
    "A struct-like class holding information about a one token."
    string, char, ident, double, integer, punct = range(7)[1:]

    def __init__(self, line, column, typ, string):
        self.line, self.col, self.typ, self.string = line, column, typ, string
        self.end = column + len(string)    # one character *after*

def check_file(filename, lines):
    line_checks = [
        check_tab_characters,
        check_long_lines
    ]
    token_checks = [
        check_missing_spaces_around,
        check_missing_spaces_after,
        check_missing_spaces_before,
        check_singlular_opening_braces,
        check_keyword_spacing,
        check_multistatements,
        check_oneliners,
        check_eol_operators,
        check_function_call_spaces,
        check_return_case_parentheses,
        check_boolean_comparisons,
        check_boolean_arguments
    ]

    # Check trailing spaces and then pre-rstrip lines, after tokenization
    # we can lstrip them too
    warnings = []
    check_trailing_spaces(lines, warnings)
    lines = [l.rstrip() for l in lines]
    for check in line_checks:
        check(lines, warnings)
    tokens = tokenize(lines)
    find_matching_parentheses(tokens)
    lines = [l.lstrip() for l in lines]
    for check in token_checks:
        check(tokens, lines, warnings)
    warnings.sort()
    for w in warnings:
        print '%s:%d: %s' % (filename, w[0]+1, w[1])

def check_long_lines(lines, warnings):
    "Check for lines longer than 80 characters"
    for i, l in enumerate(lines):
        if len(l) > 80 and not l.startswith('/* vim: '):
            w = 'Line longer than 80 characters: %s'
            warnings.append((i, w % l.lstrip()))

def check_trailing_spaces(lines, warnings):
    "Check for trailing spaces"
    for i, l in enumerate(lines):
        if len(l) > 1 and l[-2].isspace():
            warnings.append((i, 'Trailing whitespace characters'))

def check_tab_characters(lines, warnings):
    "Check for presence of tab characters"
    for i, l in enumerate(lines):
        col = l.find('\t')
        if col > -1:
            warnings.append((i, 'Raw tab character (col %d)' % col))

def check_missing_spaces_around(tokens, lines, warnings):
    "Check for missing spaces around <, >, =, etc."
    operators = '<', '>', '&&', '||', '?', '{'
    for t in tokens:
        if t.typ != Token.punct:
            continue
        if t.string not in operators and t.string.find('=') == -1:
            continue
        mbefore = t.prec.line == t.line and t.prec.end == t.col
        mafter = t.succ.line == t.line and t.end == t.succ.col
        w = (None,
             'Missing space before `%s\' (col %d): %s',
             'Missing space after `%s\' (col %d): %s',
             'Missing spaces around `%s\' (col %d): %s')[mbefore + 2*mafter]
        if w:
            warnings.append((t.line, w % (t.string, t.col, lines[t.line])))

def check_missing_spaces_after(tokens, lines, warnings):
    "Check for missing spaces after comma, colon"
    operators = ',', ':'
    for t in tokens:
        if t.typ != Token.punct or t.string not in operators:
            continue
        if t.succ.line == t.line and t.end == t.succ.col:
            w = 'Missing space after `%s\' (col %d): %s'
            warnings.append((t.line, w % (t.string, t.col, lines[t.line])))

def check_missing_spaces_before(tokens, lines, warnings):
    "Check for missing spaces before }"
    operators = '}',
    for t in tokens:
        if t.typ != Token.punct or t.string not in operators:
            continue
        if t.prec.line == t.line and t.prec.end == t.col:
            w = 'Missing space before `%s\' (col %d): %s'
            warnings.append((t.line, w % (t.string, t.col, lines[t.line])))

def check_singlular_opening_braces(tokens, lines, warnings):
    "Check for inappropriate { on a separate line"
    for t in tokens:
        if t.bracelevel == 0 or t.typ != Token.punct or t.string != '{':
            continue
        if t.line > t.prec.line and t.prec.typ == Token.punct \
           and t.prec.string == ')':
            w = 'Opening brace on a separate line (col %d)'
            warnings.append((t.line, w % t.col))

def check_keyword_spacing(tokens, lines, warnings):
    "Check for missing spaces after for, while, etc."
    keywlist = 'if', 'for', 'while', 'switch'
    keywords = dict([(x, 1) for x in keywlist])
    for t in tokens:
        if t.typ != Token.ident or t.string not in keywords:
            continue
        if t.succ.line == t.line and t.end == t.succ.col:
            w = 'Missing space after `%s\' (col %d): %s'
            warnings.append((t.line, w % (t.string, t.col, lines[t.line])))

def check_multistatements(tokens, lines, warnings):
    "Check for more than one statemen on one line"
    for t in tokens:
        if t.typ != Token.punct or t.string != ';' or t.parenlevel > 0:
            continue
        if t.succ.line == t.line:
            w = 'More than one statement on a line (col %d): %s'
            warnings.append((t.line, w % (t.succ.col, lines[t.line])))

def check_oneliners(tokens, lines, warnings):
    "Check for if, else, statements on the same line."
    for t in tokens:
        if t.typ != Token.ident:
            continue
        if t.string == 'else':
            succ = t.succ
            if succ.typ == Token.punct and succ.string == '{':
                succ = succ.succ
            if succ.line > t.line:
                continue
            if succ.typ == Token.ident and succ.string == 'if':
                continue
            w = 'Statement for `%s\' on the same line (col %d): %s'
            warnings.append((succ.line,
                             w % (t.string, succ.col, lines[succ.line])))
            continue
        if t.string in ('for', 'while', 'if'):
            # catch do-while
            if t.string == 'while':
                prec = t.prec
                if prec.typ == Token.punct and prec.string == '}':
                    prec = prec.matching.prec
                    if prec.typ == Token.ident and prec.string == 'do':
                        continue
            succ = t.succ
            if succ.typ != Token.punct or succ.string != '(':
                continue
            m = succ.matching
            succ = m.succ
            if succ.typ == Token.punct and succ.string == '{':
                succ = succ.succ
            if succ.line > m.line:
                continue
            w = 'Statement for `%s\' on the same line (col %d): %s'
            warnings.append((succ.line,
                             w % (t.string, succ.col, lines[succ.line])))
            continue

def check_eol_operators(tokens, lines, warnings):
    "Check for operators on the end of line."
    # XXX: don't check `=', not always sure about the style
    oplist = '&&', '||', '+', '-', '*', '/', '%', '|', '&', '^', \
             '==', '!=', '<', '>', '<=', '>=', '?' #, '='
    operators = dict([(x, 1) for x in oplist])
    for t in tokens:
        if t.bracelevel == 0:
            continue
        if t.typ != Token.punct or t.string not in operators:
            continue
        if t.line == t.succ.line:
            continue
        w = 'Line ends with an operator `%s\' (col %d): %s'
        warnings.append((t.line, w % (t.string, t.col, lines[t.line])))

def check_function_call_spaces(tokens, lines, warnings):
    "Check for function calls having the silly GNU spaces before (."
    keywlist = 'if', 'for', 'while', 'switch', 'return', 'case', 'goto'
    keywords = dict([(x, 1) for x in keywlist])
    for t in tokens:
        if t.bracelevel == 0:
            continue
        if t.typ != Token.punct or t.string != '(':
            continue
        prec = t.prec
        if prec.typ != Token.ident or prec.string in keywords:
            continue
        if prec.line == t.line and prec.end == t.col or prec.line < t.line:
            continue
        m = t.matching.succ
        if m.typ == Token.punct and m.string == '(':
            continue
        w = 'Space between function name and parenthesis (col %d): %s'
        warnings.append((t.line, w % (t.col, lines[t.line])))

def check_return_case_parentheses(tokens, lines, warnings):
    keywlist = 'return', 'case', 'goto'
    keywords = dict([(x, 1) for x in keywlist])
    for t in tokens:
        if t.typ != Token.punct or t.string != '(':
            continue
        if t.prec.string not in keywords:
            continue
        if t.matching.succ.typ != Token.punct or t.matching.succ.string != ';':
            continue
        w = 'Extra return/case/goto parentheses (col %d)'
        warnings.append((t.line, w % (t.col)))

def check_boolean_comparisons(tokens, lines, warnings):
    keywlist = 'TRUE', 'FALSE'
    keywords = dict([(x, 1) for x in keywlist])
    for t in tokens:
        if t.string not in keywords:
            continue
        prec = t.prec
        if prec.typ != Token.punct or (prec.string != '!='
                                       and prec.string != '=='):
            continue
        w = 'Comparison to TRUE or FALSE (col %d)'
        warnings.append((t.line, w % (t.col)))

def check_boolean_arguments(tokens, lines, warnings):
    "Check for boolean arguments passed as 0, 1."
    functions = {
        'g_array_new': (1, 2),
        'g_array_sized_new': (1, 2),
        'g_array_free': 2,
        'g_ptr_array_free': 2,
        'g_string_free': 2,
        'gtk_hbox_new': 1,
        'gdk_draw_rectangle': 3,
        'gtk_vbox_new': 1,
        'gtk_box_pack_start': (3, 4),
        'gwy_data_field_new': 5,
        'gwy_data_line_new': 3,
        'gwy_data_field_invert': (2, 3, 4)
    }
    for i, t in enumerate(tokens):
        if t.typ != Token.ident or t.string not in functions:
            continue
        args = gimme_function_arguments(tokens, i+1)
        indices = functions[t.string]
        if type(indices) != types.TupleType:
            indices = (indices,)
        for ind in indices:
            arg = args[ind-1]
            if len(arg) > 1:
                continue
            arg = arg[0]
            if arg.string in ('0', '1'):
                w = 'Boolean argument %d of %s passed as number (col %d): %s'
                warnings.append((arg.line, w % (ind, t.string,
                                                arg.col, lines[arg.line])))

def tokenize(lines):
    "`Parse' a C file returning a sequence of Tokens"
    re_com = re.compile(r'/\*.*?\*/|//.*')
    re_mac = re.compile(r'#.*')
    re_str = re.compile(r'"([^\\"]+|\\"|\\[^"])*"')
    re_chr = re.compile(r'\'(?:.|\\.|\\[0-7]{3}|\\x[0-9a-f]{2})\'')
    re_id = re.compile(r'\b[A-Za-z_]\w*\b')
    # this eats some integers too
    re_dbl = re.compile(r'\b(\d*\.\d+|\d+\.?)(?:[Ee][-+]?\d+)?[FfLl]?\b')
    re_int = re.compile(r'\b(?:0[xX][a-fA-F0-9]+|0[0-7]+|[-+]?\d+)[LlUu]*\b')
    re_eq = re.compile(r'(?:[-+*%/&|!<>=^]|&&|>>|<<|\|\|)?=')
    re_grp = re.compile(r'&&|<<|>>|->|::|\.\*|->\*|\|\|')
    re_sin = re.compile(r'[][(){};:?,.+~!%^&*|/^<>]|-')
    re_tokens = re_com, re_str, re_chr, re_id, re_dbl, re_int, \
                re_eq, re_grp, re_sin
    token_ids = {
        re_com: 0, re_str: Token.string, re_chr: Token.char,
        re_id: Token.ident, re_dbl: Token.double, re_int: Token.integer,
        re_eq: Token.punct, re_grp: Token.punct, re_sin: Token.punct
    }

    tokens = []
    in_comment = False
    in_macro = False
    for i, l in enumerate(lines):
        l, col = blstrip(l, 0)
        # special processing when we are inside a multiline comment or macro
        if in_comment:
            base = l.find('*/')
            if base == -1:
                continue
            l = l[base+2:]
            col += base+2
            l, col = blstrip(l, col)
            in_comment = False
        elif in_macro:
            in_macro = l.endswith('\\')
            continue
        elif l.startswith('#'):
            if l.endswith('\\'):
                in_macro = True
            continue

        while l:
            if l.startswith('/*') and l.find('*/') == -1:
                in_comment = True
                break
            for r in re_tokens:
                m = r.match(l)
                if m:
                    if token_ids[r]:
                        tokens.append(Token(i, col, token_ids[r], m.group()))
                    col += m.end()
                    l, col = blstrip(l[m.end():], col)
                    break
            if not m:
                sys.stderr.write('*** ERROR: Completely ugly code '
                                 + '(trying to sync): %s\n' % l)
                l = ''

    # Make tokens a doubly linked list
    for i, t in enumerate(tokens):
        if i:
            t.prec = tokens[i-1]
        else:
            t.prec = None

        try:
            t.succ = tokens[i+1]
        except IndexError:
            t.succ = None
    return tokens

def find_matching_parentheses(tokens):
    "Add `matching' attribute to each token that is a parenthesis."
    pairs = {'}': '{', ')': '(', ']': '['}
    stacks = {'{': [], '(': [], '[': []}
    for i, t in enumerate(tokens):
        if t.string in pairs:
            p = stacks[pairs[t.string]].pop()
            t.matching = p
            p.matching = t
        t.parenlevel = len(stacks['('])
        t.bracelevel = len(stacks['{'])
        if t.string in stacks:
            stacks[t.string].append(t)

def gimme_function_arguments(tokens, i):
    "Given an opening parenthesis token, return function argument list."
    t = tokens[i]
    assert t.typ == Token.punct and t.string == '('
    level = t.parenlevel
    args = []
    arg = []
    while True:
        i += 1
        t = tokens[i]
        if t.parenlevel == level+1 and t.typ == Token.punct and t.string == ',':
            args.append(arg)
            arg = []
        elif t.parenlevel == level:
            assert t.typ == Token.punct and t.string == ')'
            args.append(arg)
            break
        else:
            arg.append(t)
    return args

###### MAIN #################################################################
if len(sys.argv) < 2:
    check_file('STDIN', sys.stdin.readlines())
else:
    for filename in sys.argv[1:]:
        fh = file(filename, 'r')
        check_file(filename, fh.readlines())
        fh.close()

