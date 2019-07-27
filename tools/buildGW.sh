#!/bin/bash
dir=$(readlink -f $(dirname $0)/..)

$dir/tools/buildFathomGW.sh

d="-DDEBUG_TOOL"
v="dev"
t="-march=native"

if [ -n "$1" ] ; then
   v=$1
   shift
fi

if [ -n "$1" ] ; then
   t=$1
   shift
fi

x86_64-w64-mingw32-g++ -v
echo "version $v"
echo "definition $d"
echo "target $t"

exe=minic_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   exe=${exe}_${tname}
fi
exe=${exe}.exe

lib=fathom_${v}_mingw_x64
if [ "$t" != "-march=native" ]; then
   tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
   lib=${lib}_${tname}
fi
lib=${lib}.o

echo "Building $exe"

x86_64-w64-mingw32-g++ minic.cc $dir/Fathom/src/$lib $d -DNDEBUG -I$dir/Fathom/src -O3 -flto $t -static -static-libgcc -static-libstdc++ -std=c++14 -o $dir/Dist/$exe -Wl,-Bstatic -lpthread
x86_64-w64-mingw32-strip $dir/Dist/$exe

