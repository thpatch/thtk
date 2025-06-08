#!/bin/bash

if [ "$1" == "" ]; then
  echo "No version given."
  exit 1
fi

releasepath=thtk-bin-$1
echo Generating $releasepath
rm -rf $releasepath
mkdir $releasepath
rm -rf $releasepath-pdbs
mkdir $releasepath-pdbs

for i in thanm thanm.old thecl thdat thmsg thstd; do
  echo $i
  echo .ds doc-default-operating-system thtk | groff -mdoc -Tutf8 -P-buoc - $i/$i.1 | unix2dos > $releasepath/README.$i.txt
  cp build/$i/RelWithDebInfo/$i.exe $releasepath/
  cp build/$i/RelWithDebInfo/$i.pdb $releasepath-pdbs/
done
cp build/thtk/RelWithDebInfo/thtk.dll $releasepath/
cp build/thtk/RelWithDebInfo/thtk.lib $releasepath/
cp build/thtk/RelWithDebInfo/thtk.pdb $releasepath-pdbs/
cp "$(cygpath "$VCToolsRedistDir")/${THTK_ARCH:-x86}/Microsoft.VC143.OPENMP/vcomp140.dll" $releasepath/

copy_doc() {
  while [ "$1" != "" ]; do
    cat $1 | unix2dos > $releasepath/$1.txt
    shift
  done
}
copy_doc COPYING.{libpng,zlib} COPYING README NEWS

zip -r -9 $releasepath.zip $releasepath
zip -r -9 $releasepath-pdbs.zip $releasepath-pdbs
