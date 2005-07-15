#!/usr/bin/perl
# @(#) $Id$
use warnings;
use strict;
use HTML::Entities;
use POSIX qw(getcwd);

# This script transforms gtk-doc documentation to a form directly usable on
# gwyddion.net with left bar, etc.  I should rather learn DSSSL...

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
        $_ = qx(sed -e 's:</*gtkdoc[^>]*>::gi' $f | $tidy 2>/dev/null);
        # Lowercase attributes
        s#((?:class|rel)=".*?")#\L$1\E#sg;
        # Remove <body> attributes
        s#<body[^>]*>#<body>#s;
        # Move id= attributes directly to elements
        s#(<[^/].*?)(>[^<]*)<a( id=".*?").*?>(.*?)</a>#$1$3$2$4#sg;
        # Remove spaces before colons
        s#>(&nbsp;| | ):<#>:<#sg;
        # Remove silly EOLs
        s#&\#13;\n+##sg;
        # Remove <div> attributes and whitespace before
        s#\s*<div\b[^>]*>##sg;
        # Remove whitespace before </div>
        s#\s*</div>##sg;
        # Remove whitespace before </a>
        s#\s*</a>#</a>#sg;
        # Add "api-since" class to Since: version
        s#(Since:? [\d.]+)#<span class="api-since">$1</span>#sg;
        # Remove leading empty lines from preforematted text
        s#(<pre\b[^>]*>)\n+#$1\n#sg;
        # Remove <p> inside <td> (XXX: what about multi-para?)
        s#(<td .*?>\s*)<p>(.*?)</p>(\s*</td>)#$1$2$3#sg;
        # Replace images that have alt= (that is navigation) with alt text
        s#<img\s+src=".*? alt="(.*?)"\s+/>#$1#sg;
        s#\s+cellpadding=".*?"##sg;
        s#\s+cellspacing=".*?"##sg;
        s#<meta name="generator".*?>##sgi;
        # Change .html links to .php, unless they are to local gtk-doc docs
        s#href="([^/][^"]*)\.html\b#href="$1.php#sg;
        # Add navigation
        my $add_topnote = s#<table class="navigation" width="100%"\s*>\s*<tr>\s*<th valign="middle">\s*<p class="title">(.*?)</p>\s*</th>\s*</tr>\s*</table>\s*<hr\s*/>#<h1>$1</h1>#sg;
        s#<h2><span class="refentrytitle">(.*?)</span></h2>#<h1>$1</h1>#s;
        s#<h2 class="title"(.*?)</h2>#<h1$1</h1>#s;
        # Change warnings from titles to normal bold paragraphs
        s#<h3 class="title">Warning</h3>\n#<p><b class="warning">Warning:</b></p>#sg;
        if ( !$add_topnote ) { s#(<table class="navigation".*?</table>)#<div class="topnote">$1</div>#s; }
        s#(.*)(<table class="navigation".*?</table>)#$1<div class="botnote">$2</div>#s;
        s#</td>\s*<td><a#&nbsp;<a#sg;
        s#(<tr valign="middle">\s*<td)>#$1 align="left">#s;
        s#(</td>\s*)<th#$1<td#s;
        s#(</th>\s*<td)#$1 align="right"#s;
        s#(<td[^>]*) align="([^"]*)"#$1 style="text-align:$2"#sg;
        s#<th\b#<th#g;
        s#\bth>#td>#g;
        my $links = '';
        foreach my $lnk ( 'home', 'next', 'previous', 'up' ) {
            if (m#<link rel="$lnk"\s+href="(.*?)"\s+title="(.*?)" />#) {
                $links .= "<link rel=\"$lnk\" href=\"$1\" title=\"$2\"/>\n";
            }
        }
        s#href="/usr/share/gtk-doc/html/#href="http://developer.gnome.org/doc/API/2.0/#g;
        m#<title>(.*?)</title>#;
        my $title = $1;
        s#<head>.*?</head>\n#<head><meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>\n<title>$title</title>\n<link rel="stylesheet" type="text/css" href="/main.css"/>\n<!--[if IE]>\n<style> \#LeftMenu { position: absolute; } </style>\n<![endif]-->\n<link rel="shortcut icon" type="image/x-icon" href="/favicon.ico"/>\n$links</head>#sg;
        s#(<body>\n)#$1<div id="Main">\n#sg;
        s#(</body>)#$footer$1#;
        if ( $add_topnote ) { s#(<div id="Main">\n)#$1<?php include('../../_topnote.php'); ?>\n#s; }
        $f =~ s/.*\///;
        $f =~ s/\.html$/.php/;
        open FH, ">$APIDOCS/$dir/$f" or die; print FH $_; close FH;
    }
    chdir $oldcwd;
}

# vim: set ts=4 sw=4 et :
