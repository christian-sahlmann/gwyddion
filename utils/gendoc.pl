#!/usr/bin/perl
# @(#) $Id$
use warnings;
use strict;
use HTML::Entities;
use POSIX qw(getcwd);

my $unsafe_chars = "<>\"&";
my $base = $ENV{'HOME'} . "/PHP/software/gwyddion";
my $NEWS = "$base/NEWS.xhtml";
my $APIDOCS = "$base";

undef $/;

my $css =
".synopsis { background: \#dedede; border: solid 1px black; padding: 0.5em; }
.variablelist { padding: 4px; margin-left: 3em; }
.programlisting { background: \#d7e3d4; padding: 1em; }
.navigation, .navigation a { background: \#1c701c; color: white; }
.navigation a:hover, .navigation a:active { background: white; color: \#1c701c; } h1, h2, h3 { font-family: sans-serif; }
.title { font-size: large; }
";

my $footer =
"<p><small>See also:
<a href=\"http://gwyddion.net/\">Gwyddion.net (official home page)</a>,
<a href=\"http://trific.ath.cx/software/gwyddion/\">Gwyddion at trific.ath.cx</a>
</small></p>
";

#open FH, "<NEWS" or die; $_ = <FH>; close FH;
#print "NEWS\n";
#s/.*?\n\n//s;
#$_ = encode_entities($_, $unsafe_chars);
#open FH, ">$NEWS" or die;
#print FH "<h1>Enca News</h1>\n\n<pre class=\"main\">$_</pre>"; close FH;

chdir "devel-docs";
foreach my $dir (glob "*") {
    next if !-d $dir;
    my $oldcwd = getcwd;
    chdir $dir;
    my $ok = 0;
    foreach my $f (glob "html/*.html") {
        print "$dir/$f\n";
        $ok = 1;
        $_ = qx(cat $f | sed -e 's:</*gtkdoc[^>]*>::gi' | tidy -asxhtml -q 2>/dev/null);
        s#((?:class|rel)=".*?")#\L$1\E#sg;
        s#<body[^>]*>#<body>#s;
        s#(<.*?)(>[^<]*)<a( id=".*?").*?>(.*?)</a>#$1$3$2$4#sg;
        s#(&nbsp;| ):##sg;
        s#&\#13;\n+##sg;
        s#(<td .*?>\s*)<p>(.*?)</p>(\s*</td>)#$1$2$3#sg;
        s#<img\s+src=".*? alt="(.*?)"\s+/>#$1#sg;
        s#<meta name="generator".*?>##sgi;
        s#(<style type="text/css">).*?(</style>)#$1$css$2#sg;
        s#(<head>\n)#$1<link rel="stylesheet" type="text/css" href="/CSS/colors-yeti.css"/>#sg;
        if ($f =~ /\/index.html$/) {
            s#href="(.*?).html\b#href="$1/#sg;
        }
        else {
            s#href="index.html\b#href="../#sg;
            s#href="([^"]*).html\b#href="../$1/#sg;
        }
        s#(</body>)#$footer$1#;
        $f =~ s/.*\///;
        $f =~ s/\.html$/.xhtml/;
        open FH, ">$APIDOCS/$dir/$f" or die; print FH $_; close FH;
    }
    if ($ok) {
        chdir "$APIDOCS/$dir";
        symlink "index.xhtml", "main.xhtml";
    }
    chdir $oldcwd;
}

# vim: set ts=4 sw=4 et :
