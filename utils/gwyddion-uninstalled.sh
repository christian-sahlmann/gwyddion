#!/bin/sh
# Fake installroot
inst=.run-uninstalled-root
echo "Making $inst..."
test -d $inst && rm -r $inst
if test -e $inst; then 
  echo "Remove $inst, it's in the way!" 1>&2
  exit 1
fi
cwd=`pwd`
mkdir $inst

# Environment variables with top-levels directories
echo "Base directory structure..."
GWYDDION_DATADIR="$cwd/$inst/share"
GWYDDION_LIBDIR="$cwd/$inst/lib"
GWYDDION_LIBEXECDIR="$cwd/$inst/libexec"
GWYDDION_LOCALEDIR="$cwd/$inst/share/locale"
export GWYDDION_DATADIR GWYDDION_LIBDIR GWYDDION_LIBEXECDIR GWYDDION_LOCALEDIR
mkdir \
  "$GWYDDION_LIBEXECDIR" "$GWYDDION_LIBEXECDIR/gwyddion" \
  "$GWYDDION_LIBDIR" "$GWYDDION_LIBDIR/gwyddion" \
  "$GWYDDION_DATADIR" "$GWYDDION_DATADIR/gwyddion" \
  "$GWYDDION_LOCALEDIR"

# Modules
echo "Modules..."
mkdir "$GWYDDION_LIBDIR/gwyddion/modules"
for t in file graph layer process tool; do
  mkdir "$GWYDDION_LIBDIR/gwyddion/modules/$t"
  if test $t = tool; then
    d=tools
  else
    d=$t
  fi
  ln -s "$cwd/modules/$d/.libs/"*.so "$GWYDDION_LIBDIR/gwyddion/modules/$t/"
done

# Plug-ins
# Too much pain for too little gain, would have to figure out installation for
# each language helper modules.
echo "Plugins... (skipped)"

# Pixmaps
echo "Pixmaps..."
mkdir "$GWYDDION_DATADIR/gwyddion/pixmaps"
ln -s "$cwd/pixmaps/"*.png "$cwd/pixmaps/gwyddion.ico" \
  "$GWYDDION_DATADIR/gwyddion/pixmaps/"

# Gradients, GL materials
echo "Data..."
for t in gradients glmaterials; do
  mkdir "$GWYDDION_DATADIR/gwyddion/$t"
  ln -s "$cwd/data/$t/"* "$GWYDDION_DATADIR/gwyddion/$t/"
  rm -f "$GWYDDION_DATADIR/gwyddion/$t/"[Mm]akefile*
done

# Locale
echo "Translations..."
for f in po/*.gmo; do
  b=`echo $f | sed 's:po/\(.*\)\.gmo:\1:'`
  mkdir "$GWYDDION_LOCALEDIR/$b" "$GWYDDION_LOCALEDIR/$b/LC_MESSAGES"
  ln -s "$cwd/$f" "$GWYDDION_LOCALEDIR/$b/LC_MESSAGES/gwyddion.mo"
done

# Run!
echo "Go!"
exec ./libtool --mode=execute ./app/gwyddion "$@"
