#!/usr/bin/python
# vim-syn-gen.py -- Generate vim syntax highligting from gtk-doc documentation
# Written by Yeti <yeti@physics.muni.cz>
# This file is in the public domain.
import re, glob, time, sys

# Highlight deprecated symbols too?
export_deprecated = True
# Vim syntax name (used for group prefix)
syntax_name = 'gwyddion'
# Files to scan (all will be merged to one output file)
file_glob = 'devel-docs/*/*-decl.txt'
# Regular expression enum constants should match
enum_re = 'GWY_[A-Z0-9_]+'
# Syntax file metadata
description = 'C Gwyddion'
maintainer = 'David Ne\\v{c}as (Yeti) <yeti@physics.muni.cz>'
url_base = 'http://trific.ath.cx/Ftp/vim/syntax'

# Default highlighting
types = {
    'CONSTANT': 'Constant',
    'MACRO': 'Macro',
    'FUNCTION': 'Function',
    'STRUCT': 'Type',
    'ENUM': 'Type',
    'TYPEDEF': 'Type',
    'USER_FUNCTION': 'Type'
}

#### Normally you don't want to change anything below
normalize = lambda x: x.title().replace('_', '')

hi_link_line = '  HiLink %s%s %s'
hi_links = '\n'.join([hi_link_line % (syntax_name, normalize(k), v)
                      for k, v in types.items()])

header = """\
" Vim syntax file
" Language: %s
" Maintainer: %s
" Last Change: %s
" URL: %s/%s.vim
""" % (description, maintainer, time.strftime('%Y-%m-%d'),
       url_base, syntax_name)

footer = """
" Default highlighting
if version >= 508 || !exists("did_%s_syntax_inits")
  if version < 508
    let did_%s_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif
%s
  delcommand HiLink
endif
""" % (syntax_name, syntax_name, hi_links)

re_decl = re.compile(r'<(?P<typ>' + r'|'.join(types.keys()) + r')>\n'
                     + r'<NAME>(?P<ident>\w+)</NAME>\n'
                     + r'(?P<body>.*?)'
                     + r'</(?P=typ)>\n',
                     re.S)
re_enum = re.compile(r'^\s+(?P<ident>' + enum_re + ')\s*=', re.M)

def restruct(match):
    """Given a match object, create an object whose attributes are the named
    matched groups."""

    class Struct:
        def __init__(self, mapping):
            self.__dict__.update(mapping)

    try:
        return Struct(match.groupdict())
    except TypeError:
        return None

decls = dict([(x, {}) for x in types])
for filename in glob.glob(file_glob):
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    for d in re_decl.finditer(text):
        d = restruct(d)
        if d.ident.startswith('_'):
            continue
        if not export_deprecated and d.body.find('<DEPRECATED/>') > -1:
            continue
        decls[d.typ][d.ident] = 1
        if d.typ == 'ENUM':
            for e in re_enum.finditer(d.body):
                decls['CONSTANT'][e.group('ident')] = 1

print header
for t, d in decls.items():
    d = d.keys()
    if not d:
        continue
    d.sort()
    print 'syn keyword %s%s %s' % (syntax_name, normalize(t), ' '.join(d))
print footer
