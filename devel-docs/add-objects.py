#!/usr/bin/python
from __future__ import generators
import sys, re

# Compatibility with Pyhton-2.2
if not __builtins__.__dict__.has_key('enumerate'):
    def enumerate(collection):
        i = 0
        it = iter(collection)
        while True:
            yield i, it.next()
            i += 1

if len(sys.argv) != 3:
    print sys.argv[0], 'SECTION_FILE OBJECT_FILE'
    sys.exit(1)

title_re = re.compile(r'<TITLE>(?P<object>\w+)</TITLE>')
section_file = sys.argv[1]
object_file = sys.argv[2]

fh = file(object_file, 'r')
objects = dict([(s.strip(), 1) for s in fh.readlines()])
fh.close()

fh = file(section_file, 'r')
lines = fh.readlines()
fh.close()

fh = file(section_file, 'w')
addme = ''
for i, l in enumerate(lines):
    if l.strip() == addme.strip():
        addme = ''
    if addme:
        fh.write(addme)
        addme = ''
    m = title_re.match(l)
    if m:
        addme = m.group('object')
        if not objects.has_key(addme):
            addme = ''
        else:
            addme += '\n'
    fh.write(l)
fh.close()
