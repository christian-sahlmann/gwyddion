#!/usr/bin/python
# vim: set fileencoding=utf-8 :
# @(#) $Id$
import re, sys, locale
from xml.sax.saxutils import escape

cyrillic_translit = u'АAБBВVГGДDЕEЁEЖZЗZИIЙJКKЛLМMНNОOПPРRСSТTУUФFХHЦCЧCШSЩSЪZЫZЬZЭEЮUЯA'
cyrillic_map = dict((cyrillic_translit[2*i], cyrillic_translit[2*i + 1])
                    for i in range(len(cyrillic_translit)/2))

locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')

contrib_re = re.compile(ur'(?ms)^(?P<name>\S[^<>]+?)\s+'
                        ur'<(?P<email>[^<> ]+)>\n'
                        ur'(?P<what>(?:^ +\S[^\n]+\n)+)')

def sortkey(x):
    last = [y for y in x.split() if not y.startswith(u'(')][-1]
    x = last + u' ' + x
    if x[0] in cyrillic_map:
        x = cyrillic_map[x[0]] + x
    return locale.strxfrm(x.encode('utf-8'))

def parse_contributors(text):
    contributors = {}
    for m in contrib_re.finditer(text):
        name, email = m.group('name'), m.group('email')
        what = re.sub(ur'(?s)\s+', ur' ', m.group('what').strip())
        contributors[name] = (email, what)
    return contributors

def format_list(text, section):
    header_re = re.compile(ur'(?ms)^=== ' + section + ur' ===(?P<body>.*?)^$')
    contributors = parse_contributors(header_re.search(text).group('body'))
    sectid = re.sub(ur'[^a-zA-Z]', '', section.lower())
    out = [u'<p id="%s"><b>%s:</b></p>' % (sectid, section)]
    out.append(u'<dl>')
    for name in sorted(contributors.keys(), key=sortkey):
        email, what = contributors[name]
        out.append(u'<dt>%s, <code>%s</code></dt>' % (name, email))
        out.append(u'<dd>%s</dd>' % what)
    out.append(u'</dl>')
    return u'\n'.join(out).encode('utf-8')

text = sys.stdin.read().decode('utf-8')
print format_list(text, 'Developers')
print format_list(text, 'Translators')
