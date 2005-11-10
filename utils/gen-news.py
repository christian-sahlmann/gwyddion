#!/usr/bin/python
# @(#) $Id$
import re, sys, time
from xml.sax.saxutils import escape

bugzilla_url = 'http://trific.ath.cx/bugzilla/show_bug.cgi?id='

def format_date(d):
    # Don't depend on locale
    months = (None, 'January', 'February', 'March', 'April',
              'May', 'June', 'July', 'August',
              'September', 'October', 'November', 'December')
    d = time.strptime(d, '%Y-%m-%d')
    return '%s %d, %d' % (months[d.tm_mon], d.tm_mday, d.tm_year)

in_list = False
in_item = False
in_para = False
version_list = []
text = []
for line in sys.stdin.readlines():
    line = line.rstrip()
    # Version
    m = re.match(r'^(?P<version>\d[0-9a-z.]*)\s+'
                 + r'\((?P<date>\d+-\d+-\d+)\)$', line)
    if m:
        ver = m.group('version')
        if ver.find('cvs') > -1:
            continue
        text.append('\n<h2 id="v%s">Version %s</h2>' % (ver, ver))
        text.append('<p>Released: %s.</p>' % format_date(m.group('date')))
        version_list.append(ver)
        continue
    # Component
    m = re.match(r'^(?P<component>[A-Z].*):$', line)
    if m:
        if in_para:
            text.append('</p>')
        in_para = False
        text.append('<p><b>%s</b></p>\n<ul>' % m.group('component'))
        in_list = True
        continue
    line = escape(line)
    # Transform bug #NN references to hyperlinks
    line = re.sub(r'[bB]ug #(\d+)',
                  '<a href="' + bugzilla_url + '\\1">\\g<0></a>',
                  line)
    # End of list
    if re.match(r'^\s*$', line):
        if in_item:
            text.append('</li>')
        if in_list:
            text.append('</ul>')
        in_item = False
        in_list = False
        continue
    # Begin of list/item
    if line.startswith('- '):
        if not in_list:
            if in_para:
                text.append('</p>')
                in_para = False
            text.append('<ul>')
            in_list = True
        if in_item:
            text.append('</li>')
        text.append('<li>%s' % line[2:])
        in_item = True
        continue
    # Begin of paragraph
    if not in_para and re.match(r'^[A-Z]', line):
        in_para = True
        text.append('<p>')
        text.append(line)
        continue
    if in_para:
        text.append(line)
        continue
    assert line.startswith('  ')
    assert in_item
    text.append(line[2:])

if in_item:
    text.append('</li>')
if in_list:
    text.append('</ul>')
if in_para:
    text.append('</p>')

# Split version list by major version, assuming there are two series
# formed by N.x and N.99.x versions
i = 0
version_lists = []
while True:
    s = str(i) + '.'
    l = [x for x in version_list if x.startswith(s)]
    if not l:
        break
    s += '99.'
    version_lists.append([x for x in l if not x.startswith(s)])
    version_lists.append([x for x in l if x.startswith(s)])
    i += 1
version_lists = [x for x in version_lists if x]
version_lists.reverse()

print '<p>Jump to news for version:<br/>'
l = []
for ver in version_lists:
    l.append(',\n'.join(['<a href="#v%s">%s</a>' % (x, x) for x in ver]))
print '<br/>\n'.join(l)
print '</p>'
print '\n'.join(text)
