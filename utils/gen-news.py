#!/usr/bin/python
import re, sys

in_list = False
in_item = False
for line in sys.stdin.readlines():
    line = line.rstrip()
    m = re.match(r'^(?P<version>\d[0-9a-z.]*)$', line)
    if m:
        print '\n<h2>Version %s</h2>' % m.group('version')
        continue
    m = re.match(r'^(?P<component>[A-Z].*):$', line)
    if m:
        print '<b>%s</b>\n<ul>' % m.group('component')
        in_list = True
        continue
    m = re.match(r'^\s*$', line)
    if m:
        if in_item:
            print '</li>'
        if in_list:
            print '</ul>'
        in_item = False
        in_list = False
        continue
    if line.startswith('- '):
        if not in_list:
            print '<ul>'
            in_list = True
        if in_item:
            print '</li>'
        print '<li>%s' % line[2:]
        in_item = True
        continue
    assert line.startswith('  ')
    assert in_item
    print line[2:]

if in_item:
    print '</li>'
if in_list:
    print '</ul>'
