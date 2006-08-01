#!/usr/bin/python
import re, sys

stat_types = 'translated', 'fuzzy', 'untranslated'
total_width = 200

def parse(fh):
    tstats = []
    total = 0
    for line in fh:
        m = re.match(r'TRANSLATION\s+(?P<content>.*)\n', line)
        if not m:
            continue

        line = m.group('content')
        m = re.match(r'(?P<lang>[a-zA-Z_@.]+):', line)
        if not m:
            sys.stderr.write('Malformed TRANSLATION line: %s\n' % line)
            continue

        lang = {'lang': m.group('lang')}

        if lang['lang'] == 'total':
            m = re.search(r'\d+', line)
            if m:
                total = int(m.group(0))
            else:
                sys.stderr.write('Malformed TRANSLATION line: %s\n' % line)
            continue
        else:
            sum = 0
            for x in stat_types:
                m = re.search(r'\b(?P<count>\d+) %s (message|translation)' % x,
                              line)
                if m:
                    lang[x] = int(m.group('count'))
                    sum += lang[x]
            lang['total'] = sum
        tstats.append(lang)

    return tstats, total

def format_lang(lang):
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
    sum = float(lang['total'])

    # Redistribute rounding errors in box widths to achieve fixed total width
    w = {}
    sw = 0
    for x in stat_types:
        if lang.has_key(x):
            w[x] = int(lang[x]/sum*total_width + 0.5)
            sw += w[x]
    while sw != total_width:
        if sw > total_width:
            m, mx = 1.0e38, None
            for x in stat_types:
                if lang.has_key(x) and lang[x]/sum*total_width - w[x] < m:
                    mx = x
            w[mx] -= 1
            sw -= 1
        else:
            m, mx = 1.0e38, None
            for x in stat_types:
                if lang.has_key(x) and w[x] - lang[x]/sum*total_width < m:
                    mx = x
            w[mx] += 1
            sw += 1

    for x in stat_types:
        if not lang.has_key(x):
            stats.append(statfmt % (0, 0.0))
            continue
        box.append(boxfmt % (x, w[x]))
        stats.append(statfmt % (lang[x], lang[x]/sum*100.0))

    return rowfmt % (lang['lang'], '\n'.join(stats), sum, ''.join(box))

tstats, total = parse(sys.stdin)

print """<table summary="Translation statistics" class="translation-stats">
<thead>
<tr>
  <th class="odd">Language</th>
  <th>Translated</th><th class="odd">%</th>
  <th>Fuzzy</th><th class="odd">%</th>
  <th>Untranslated</th><th class="odd">%</th>
  <th>Total</th>
  <th>Graph</th>
</tr>
</thead>
<tbody>"""

for x in tstats:
    print format_lang(x).encode('utf-8')

print """</tbody>
</table>"""
