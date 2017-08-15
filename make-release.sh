#!/bin/bash

if [ "$1" == "" ]; then
  echo "No version given."
  exit 1
fi

releasepath=thtk-bin-$1
echo Generating $releasepath
rm -rf $releasepath
mkdir $releasepath

for i in thanm thecl thdat thmsg; do
  echo $i
  groff -mdoc -Tutf8 $i/$i.1 | perl -pe 's/\e\[?.*?[\@-~]//g' | unix2dos > $releasepath/README.$i.txt
  cp build/$i/Release/$i.exe $releasepath/
done
cp build/thtk/Release/thtk.dll $releasepath/
cp "$(cygpath "$VSSDK140Install")/../VC/redist/x86/Microsoft.VC140.OPENMP/vcomp140.dll" $releasepath/

copy_doc() {
  while [ "$1" != "" ]; do
    cat $1 | unix2dos > $releasepath/$1.txt
    shift
  done
}
copy_doc COPYING.{bison,flex,libpng,zlib} COPYING README NEWS

zip -r -9 $releasepath.zip $releasepath
