#!/usr/bin/python
import re, glob, time

syntax_name = 'gwyddion'

types = {
    'CONSTANT': 'Constant',
    'MACRO': 'Macro',
    'FUNCTION': 'Function',
    'STRUCT': 'Type',
    'ENUM': 'Type',
    'TYPEDEF': 'Type',
    'USER_FUNCTION': 'Type'
}

normalize = lambda x: x.title().replace('_', '')

hi_link_line = '  HiLink %s%s %s'
hi_links = '\n'.join([hi_link_line % (syntax_name, normalize(k), v)
                      for k, v in types.items()])

header = """\
" Vim syntax file
" Language: C Gwyddion
" Maintainer: David Ne\\v{c}as (Yeti) <yeti@physics.muni.cz>
" Last Change: %s
" URL: http://trific.ath.cx/Ftp/vim/syntax/%s.vim
""" % (time.strftime('%Y-%m-%d'), syntax_name)

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
re_enum = re.compile(r'^\s+(?P<ident>GWY_[A-Z0-9_]+)\s*=', re.M)

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
for filename in glob.glob('devel-docs/*/*-decl.txt'):
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    for d in re_decl.finditer(text):
        d = restruct(d)
        if d.ident.startswith('_'):
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
