#!/usr/bin/python
import re, sys

in_list = False
in_item = False
in_para = False
version_list = []
text = []
for line in sys.stdin.readlines():
    line = line.rstrip()
    # Version
    m = re.match(r'^(?P<version>\d[0-9a-z.]*)$', line)
    if m:
        ver = m.group('version')
        if ver.find('cvs') > -1:
            continue
        text.append('\n<h2 id="v%s">Version %s</h2>' % (ver, ver))
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

print '<p>News for version:'
print ',\n'.join(['<a href="#v%s">%s</a>' % (x, x) for x in version_list]) + '.'
print '</p>'
print '\n'.join(text)
