#!/usr/bin/python
import sys

lines = sys.stdin.readlines()
print lines[1].strip()

fmt = '{ %s, { %s, %s, %s, %s } },'
end = None
for line in lines[3:]:
    fields = line.split(' ')
    start = fields[0:1] + fields[3:7]
    assert not end or start == end
    end = fields[2:3] + fields[7:11]
    print fmt % tuple(start)
print fmt % tuple(end)
