#!/usr/bin/python
# @(#) $Id$
import re, sys, time
from xml.sax.saxutils import escape

def format_date(d):
    # Don't depend on locale
    months = (None, 'January', 'February', 'March', 'April',
              'May', 'June', 'July', 'August',
              'September', 'October', 'November', 'December')
    d = time.strptime(d, '%Y-%m-%d')
    return '%d %s %d' % (d.tm_mday, months[d.tm_mon], d.tm_year)

in_list = False
in_item = False
in_para = False
version_list = []
out = []

version_block_re = re.compile(r'(?ms)^(?P<version>\d[0-9.]*)\s+'
                              + r'\((?P<date>\d+-\d+-\d+)\)$'
                              + r'(?P<news>.*?)\n{3,}')
news_group_re = re.compile(r'(?ms)^(?P<header>[\w ]+):\n'
                           + r'(?P<items>- .*?)\n{2,}')
news_item_re = re.compile(r'(?m)^- (?P<item>.*\n(?:  .*\n)*)')
colon_re = re.compile(r'[\w)]: ')

text = sys.stdin.read().strip() + '\n\n\n'
textpos = 0
while textpos < len(text):
    verblock = version_block_re.match(text, textpos)
    if not verblock:
        sys.stderr.write('Cannot parse "%.30s" as version header.\n'
                         % text[textpos:])
        sys.exit(1)

    version = verblock.group('version')
    date = verblock.group('date')
    version_list.append(version)
    out.append('')
    out.append('<h2 id="v%s">Version %s</h2>' % (version, version))
    out.append('<p>Released: %s.</p>' % format_date(date))

    news = verblock.group('news').strip() + '\n\n'
    newspos = 0
    while newspos < len(news):
        newsgroup = news_group_re.match(news, newspos)
        if not newsgroup:
            sys.stderr.write('Cannot parse "%.30s" as news group header.\n'
                             % news[newspos:])
            sys.exit(1)

        header = newsgroup.group('header')
        out.append('<p><b>%s</b></p>' % header)
        out.append('<ul>')

        items = newsgroup.group('items').strip() + '\n'
        itempos = 0
        while itempos < len(items):
            newsitem = news_item_re.match(items, itempos)
            if not newsitem:
                sys.stderr.write('Cannot parse "%.30s" as news item.\n'
                                 % items[itempos:])
                sys.exit(1)

            item = newsitem.group('item')
            item = re.sub(r'(?s)\s{2,}', r' ', item)

            colon = colon_re.search(item)
            if not colon:
                sys.stderr.write('Missing colon in news item "%.30s".\n' % item)
                sys.exit(1)

            label = item[:colon.start()+1]
            content = item[colon.end():]
            out.append('<li><i>%s</i>: %s</li>' % (label, content))

            itempos = newsitem.end()

        out.append('</ul>')
        newspos = newsgroup.end()

    textpos = verblock.end()

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
print '\n'.join(out)
