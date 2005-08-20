#!/usr/bin/python
import sys

lines = sys.stdin.readlines()
name = lines[1].strip().split(':')[1].strip()
print name
fh = file(name, 'w')

fmt = '%s %s %s %s %s\n'
end = None
fh.write('Gwyddion resource GwyGradient\n')
for line in lines[3:]:
    fields = line.split(' ')
    start = fields[0:1] + fields[3:7]
    assert not end or start == end
    end = fields[2:3] + fields[7:11]
    fh.write(fmt % tuple(start))
fh.write(fmt % tuple(end))
