#!/usr/bin/python
import re, sys

stat_types = 'good', 'fuzzy', 'bad'
total_width = 200

def parse(fh):
    stats = []
    for line in fh:
        line = line.split()
        group = line[0]
        del line[0]
        line = [int(x) for x in line]
        stat = {'group': group, 'total': sum(line)}
        for i, x in enumerate(stat_types):
            stat[x] = line[i]
        stats.append(stat)
    return stats

def format_row(stat):
    rowfmt = """<tr>
<th class="odd">%s</th>
%s
<td>%d</td>
<td>%s</td>
</tr>"""

    statfmt = """<td>%d</td><td class="odd">%.2f</td>""";

    boxfmt = u"""<span class="stat %s" style="padding-right: %dpx;">\ufeff</span>""";

    box = []
    stats = []
    sum = float(stat['total'])

    # Redistribute rounding errors in box widths to achieve fixed total width
    w = {}
    sw = 0
    for x in stat_types:
        if stat.has_key(x):
            w[x] = int(stat[x]/sum*total_width + 0.5)
            sw += w[x]
    while sw != total_width:
        if sw > total_width:
            m, mx = 1.0e38, None
            for x in stat_types:
                if stat.has_key(x) and stat[x]/sum*total_width - w[x] < m:
                    mx = x
            w[mx] -= 1
            sw -= 1
        else:
            m, mx = 1.0e38, None
            for x in stat_types:
                if stat.has_key(x) and w[x] - stat[x]/sum*total_width < m:
                    mx = x
            w[mx] += 1
            sw += 1

    for x in stat_types:
        if not stat.has_key(x):
            stats.append(statfmt % (0, 0.0))
            continue
        box.append(boxfmt % (x, w[x]))
        stats.append(statfmt % (stat[x], stat[x]/sum*100.0))

    return rowfmt % (stat['group'], '\n'.join(stats), sum, ''.join(box))

stats = parse(sys.stdin)

print """<table summary="Documentation statistics" class="stats">
<thead>
<tr>
  <th class="odd">Library</th>
  <th>Fully</th><th class="odd">%</th>
  <th>Partially</th><th class="odd">%</th>
  <th>Missing</th><th class="odd">%</th>
  <th>Total</th>
  <th>Graph</th>
</tr>
</thead>
<tbody>"""

for x in stats:
    print format_row(x).encode('utf-8')

print """</tbody>
</table>"""
