#!/usr/bin/python
from __future__ import generators
import sys, re

debug = False

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

if debug:
    print 'Objects from %s:' % object_file, objects

fh = file(section_file, 'r')
lines = fh.readlines()
fh.close()

fh = file(section_file, 'w')
addme = ''
added = False
for i, l in enumerate(lines):
    if l.strip() == addme or l.strip() == addme + 'Class':
        if debug:
            print 'Skipping matching %s' % l.strip()
        l = ''
    if addme and not added:
        if debug:
            print 'Adding %s, %sClass' % (addme, addme)
        fh.write(addme + '\n')
        fh.write(addme + 'Class\n')
        added = True
    m = title_re.match(l)
    if m:
        added = False
        addme = m.group('object')
        if debug:
            print 'Object-like declaration %s' % addme
        if not objects.has_key(addme):
            if debug:
                print 'Type %s is not in objects' % addme
            addme = ''
    fh.write(l)
fh.close()
