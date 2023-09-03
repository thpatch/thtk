#!/bin/sh
# Script to test anm rebuilding
#
# This tests two main scenarios:
# - regular unpacking to separate dirs
#   (one dir per anm file)
# - unpacking with -uu option
# - unpacking with -X option
#
# This doesn't test the most common way to extract ANM (no flags, everything in
# one dir), because that's clearly the wrong way to do it.
#
# Summary of directories being created:
# -X   |-uu |ZUN| separate
# flag |flag|   | dirs
# -----|----|---|----------
# i    |g   | a |   c     e | packed
#  \   | \  |/|\|  / \   /
#   \  |  \ / | \ /   \ /
#    h |   f| | |b     d    | unpacked
#     \       /
#      \-----/
#
# Comparisions being done:
# a = c (rebuilding)
# b = d (consistency)
# c = e (consistency)
# a = g (rebuilding)
# a = i (rebuilding)
# The a = c and a = i comparisions can't be practically done for th19... at least for now.
script_dir=$(cd "$(dirname "$0")" || exit 1; pwd) || exit 1
: "${THDAT:=$script_dir/../build/thdat/thdat}"
: "${THANM:=$script_dir/../build/thanm/thanm}"
: "${ANMTEMP:=/tmp/anmtemp}"
if [ ! -d "$DATDIR" ]; then
    echo "Usage: [THDAT=thdat] [THANM=thanm] [ANMTMP=/tmp/anmtemp] DATDIR=dats $0"
    exit 1
fi
DATDIR=$(cd "$DATDIR" || exit 1; pwd) || exit 1
#(cd "$DATDIR" || exit 1; sha256sum -c "$script_dir/datsums.txt")
versionfromfn() {
    if [ "$1" = "alcostg.dat" ]; then
        echo 103
    else
        v=${1#th}
        echo "${v%.dat}"
    fi
}
THANMFLAGS=
for datfile in \
    th06.dat th07.dat th08.dat th09.dat \
    th095.dat th10.dat alcostg.dat th11.dat \
    th12.dat th125.dat th128.dat th13.dat \
    th14.dat th143.dat th15.dat th16.dat \
    th165.dat th17.dat th18.dat th185.dat \
    th19.dat
do
    version=$(versionfromfn $datfile)
    echo "$version"
    rm -rf "$ANMTEMP"
    if [ "$datfile" = "th06.dat" ]; then
        datfiles="紅魔郷CM.DAT 紅魔郷ED.DAT 紅魔郷IN.DAT 紅魔郷MD.DAT 紅魔郷ST.DAT 紅魔郷TL.DAT"
    else
        datfiles="$datfile"
    fi
    mkdir -p "$ANMTEMP/a" "$ANMTEMP/b" "$ANMTEMP/c" "$ANMTEMP/d" "$ANMTEMP/e" "$ANMTEMP/f" "$ANMTEMP/g" "$ANMTEMP/h" "$ANMTEMP/i"
    anmfiles=$(cd "$ANMTEMP/a" || exit 1; for i in $datfiles; do "$THDAT" -gx"$version" "$DATDIR/$i" "*anm" >/dev/null; done; ls) || exit 1
    for i in $anmfiles; do
        mkdir -p "$ANMTEMP/b/$i"
        "$THANM" $THANMFLAGS -l"$version" "$ANMTEMP/a/$i" >"$ANMTEMP/b/$i.txt"
        (cd "$ANMTEMP/b/$i" || exit 1; "$THANM" $THANMFLAGS -x"$version" "$ANMTEMP/a/$i") || exit 1
    done
    for i in $anmfiles; do
        (cd "$ANMTEMP/b/$i" || exit 1; "$THANM" $THANMFLAGS -c"$version" "$ANMTEMP/c/$i" "$ANMTEMP/b/$i.txt") || exit 1
    done
    for i in $anmfiles; do
        mkdir -p "$ANMTEMP/d/$i"
        "$THANM" $THANMFLAGS -l"$version" "$ANMTEMP/c/$i" >"$ANMTEMP/d/$i.txt"
        (cd "$ANMTEMP/d/$i" || exit 1; "$THANM" $THANMFLAGS -x"$version" "$ANMTEMP/c/$i") || exit 1
    done
    for i in $anmfiles; do
        (cd "$ANMTEMP/d/$i" || exit 1; "$THANM" $THANMFLAGS -c"$version" "$ANMTEMP/e/$i" "$ANMTEMP/d/$i.txt") || exit 1
    done
    for i in $anmfiles; do
        "$THANM" $THANMFLAGS -uul"$version" "$ANMTEMP/a/$i" >"$ANMTEMP/f/$i.txt"
        (cd "$ANMTEMP/f" || exit 1; "$THANM" $THANMFLAGS -uux"$version" "$ANMTEMP/a/$i") || exit 1
    done
    for i in $anmfiles; do
        (cd "$ANMTEMP/f" || exit 1; "$THANM" $THANMFLAGS -uuc"$version" "$ANMTEMP/g/$i" "$ANMTEMP/f/$i.txt") || exit 1
    done
    for i in $anmfiles; do
        "$THANM" $THANMFLAGS -l"$version" "$ANMTEMP/a/$i" >"$ANMTEMP/h/$i.txt"
    done
    (cd "$ANMTEMP/h" || exit 1; "$THANM" $THANMFLAGS -X"$version" "$ANMTEMP/a/"*.anm) || exit 1
    for i in $anmfiles; do
        (cd "$ANMTEMP/h" || exit 1; "$THANM" $THANMFLAGS -c"$version" "$ANMTEMP/i/$i" "$ANMTEMP/h/$i.txt") || exit 1
    done
    diff -rq "$ANMTEMP/a" "$ANMTEMP/c"
    diff -rq "$ANMTEMP/b" "$ANMTEMP/d"
    diff -rq "$ANMTEMP/c" "$ANMTEMP/e"
    diff -rq "$ANMTEMP/a" "$ANMTEMP/g"
    diff -rq "$ANMTEMP/a" "$ANMTEMP/i"
done
