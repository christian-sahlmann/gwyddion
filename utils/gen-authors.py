#!/usr/bin/python
# @(#) $Id$
import re, sys, locale
from xml.sax.saxutils import escape

locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')

contrib_re = re.compile(r'(?ms)^(?P<name>\S[^<>]+?)\s+'
                        r'<(?P<email>[^<> ]+)>\n'
                        r'(?P<what>(?:^ +\S[^\n]+\n)+)')

def sortkey(x):
    x = x.split()[-1] + ' ' + x
    return locale.strxfrm(x.encode('utf-8'))

def parse_contributors(text):
    contributors = {}
    for m in contrib_re.finditer(text):
        name, email = m.group('name'), m.group('email')
        what = re.sub(r'(?s)\s+', r' ', m.group('what').strip())
        contributors[name] = (email, what)
    return contributors

def format_list(text, section):
    header_re = re.compile(r'(?ms)^=== ' + section + ' ===(?P<body>.*?)^$')
    contributors = parse_contributors(header_re.search(text).group('body'))
    sectid = re.sub(r'[^a-zA-Z]', '', section.lower())
    out = [u'<p id="%s">%s:</p>' % (sectid, section)]
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
