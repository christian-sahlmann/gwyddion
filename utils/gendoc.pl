#!/usr/bin/perl
# @(#) $Id$
use warnings;
use strict;
use HTML::Entities;
use POSIX qw(getcwd);

my $man2html = '/usr/bin/man2html';
my $tidy = 'tidy -asxhtml -q';
my $unsafe_chars = "<>\"&";
my $base = $ENV{'HOME'} . "/PHP/software/gwyddion";
my $NEWS = "$base/NEWS.xhtml";
my $APIDOCS = "$base";
my $pkgname = "Gwyddion";

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
<a href=\"http://gwyddion.net/\">Gwyddion, an SPM (AFM, MFM, STM, NSOM) data analysis framework</a>,
<a href=\"http://trific.ath.cx/software/gwyddion/\">Gwyddion at trific.ath.cx</a>
</small></p>
";

sub gen_plaintext {
  my $filename = $_[0];
  my $output = $_[1];
  my $name = $_[2] ? $_[2] : $_[0];
  open FH, "<$filename" or die; $_ = <FH>; close FH;
  print "$filename\n";
  s/.*?\n\n//s;
  $_ = encode_entities($_, $unsafe_chars);
  open FH, ">$output" or die;
  print FH "<h1>$pkgname $name</h1>\n\n<pre class=\"main\">$_</pre>"; close FH;
}

sub gen_manpage {
  my $filename = $_[0];
  my $output = $_[1];
  $_ = qx($man2html <$filename | $tidy 2>/dev/null);
  print "$filename\n";
  s#(<[^>]*>)#\L$1\E#sg;
  s#<head>.*</head>##s;
  s#\007##sg;
  s#</?html[^>]*>##sg;
  s#</?body[^>]*>##sg;
  s#<!doctype[^>]*>##sg;
  s#\nSection:#\n<p>Section:#s;
  s#(Index</a>)#$1</p>#s;
  s#\sname="lb.."##sg;
  s#\sname="index"##sg;
  s#\scompact=".*?"##sg;
  s#^\s+##s;
  open FH, ">$output" or die; print FH $_; close FH;
}

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
        # Change warnings from titles to normal bold paragraphs
        s#<h3 class="title">Warning</h3>\n#<p><b class="warning">Warning:</b></p>#sg;
        s#(<style type="text/css">).*?(</style>)#$1$css$2#sg;
        s#<link rel="stylesheet".*?>#<style type="text/css">$css</style>#sg;
        s#(<head>\n)#$1<link rel="stylesheet" type="text/css" href="/CSS/colors-yeti.css"/>#sg;
        if ($f =~ /\/index.html$/) {
            s#href="([^/].*?).html\b#href="$1/#sg;
        }
        else {
            s#href="index.html\b#href="../#sg;
            s#href="([^/][^"]*).html\b#href="../$1/#sg;
        }
        s#href="/usr/share/gtk-doc/html/#href="http://developer.gnome.org/doc/API/2.0/#g;
        s#(</body>)#$footer$1#;
        $f =~ s/.*\///;
        $f =~ s/\.html$/.xhtml/;
        open FH, ">$APIDOCS/$dir/$f" or die; print FH $_; close FH;
    }
    chdir $oldcwd;
}

# vim: set ts=4 sw=4 et :
