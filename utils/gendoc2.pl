#!/usr/bin/perl
# @(#) $Id$
use warnings;
use strict;
use HTML::Entities;
use POSIX qw(getcwd);

my $tidy = 'tidy -asxhtml -q';
my $unsafe_chars = "<>\"&";
my $base = $ENV{'HOME'} . "/Projects/Gwyddion/Web/documentation";
my $APIDOCS = "$base";
my $pkgname = "Gwyddion";

undef $/;

my $footer =
"
</div>
<?php include('../../_leftmenu.php'); ?>
";

chdir "devel-docs";
foreach my $dir (glob "*") {
    next if !-d $dir;
    my $oldcwd = getcwd;
    chdir $dir;
    foreach my $f (glob "html/*.html") {
        print "$dir/$f\n";
        $_ = qx(cat $f | sed -e 's:</*gtkdoc[^>]*>::gi' | $tidy 2>/dev/null);
        s#((?:class|rel)=".*?")#\L$1\E#sg;
        s#<body[^>]*>#<body>#s;
        s#(<.*?)(>[^<]*)<a( id=".*?").*?>(.*?)</a>#$1$3$2$4#sg;
        s#</(\w+) .*?>#</$1>#sg;
        s#(&nbsp;| ):##sg;
        s#&\#13;\n+##sg;
        s#\.html#\.php#sg;
        s#\s*<div[^>]*>##sg;
        s#\s*</div>##sg;
        s#\s*</a>#</a>#sg;
        s#(Since:? [\d.]+)#<span class="api-since">$1</span>#sg;
        s#(<pre\b[^>]*>)\n+#$1\n#sg;
        s#(<td .*?>\s*)<p>(.*?)</p>(\s*</td>)#$1$2$3#sg;
        s#<img\s+src=".*? alt="(.*?)"\s+/>#$1#sg;
        s# cellpadding=".*?"##sg;
        s# cellspacing=".*?"##sg;
        s#<h2><span class="refentrytitle">(.*?)</span></h2>#<h1>$1</h1>#s;
        s#(<table class="navigation".*?</table>)#<div class="topnote">$1</div>#s;
        s#(.*)(<table class="navigation".*?</table>)#$1<div class="botnote">$2</div>#s;
        s#</td>\s*<td><a#&nbsp;<a#sg;
        s#(<tr valign="middle">\s*<td)>#$1 align="left">#s;
        s#(</td>\s*)<th#$1<td#s;
        s#(</th>\s*<td)#$1 align="right"#s;
        s#\bth>#td>#g;
        my $links = '';
        foreach my $lnk ( 'home', 'next', 'previous', 'up' ) {
            if (m#<link rel="$lnk"\s+href="(.*?)"\s+title="(.*?)" />#) {
                $links .= "<link rel=\"$lnk\" href=\"$1\" title=\"$2\"/>\n";
            }
        }
        m#<title>(.*?)</title>#;
        my $title = $1;
        s#<head>.*?</head>\n#<head><meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>\n<title>$title</title>\n<link rel="stylesheet" type="text/css" href="/main.css"/>\n<link rel="shortcut icon" type="image/x-icon" href="/favicon.ico"/>\n$links</head>#sg;
        s#(<body>\n)#$1<div id="Main">\n#sg;
        s#(</body>)#$footer$1#;
        $f =~ s/.*\///;
        $f =~ s/\.html$/.php/;
        open FH, ">$APIDOCS/$dir/$f" or die; print FH $_; close FH;
    }
    chdir $oldcwd;
}

# vim: set ts=4 sw=4 et :
