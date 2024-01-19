#!/bin/bash
set -e

TCL8_VERSION=8.6.13
TCL9_VERSION=9.0b1
TCL8_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL8_VERSION-src.tar.gz
TK8_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL8_VERSION-src.tar.gz
TCL9_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL9_VERSION-src.tar.gz
TK9_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL9_VERSION-src.tar.gz

mkdir Togl3.0
rm -rf tcl9 tk9 tcl8 tk8
mkdir tcl9 tk9 tcl8 tk8

echo downloading tcl/tk source ...
wget --no-check-certificate -O tcl9.tar.gz $TCL9_URL
wget --no-check-certificate -O tk9.tar.gz $TK9_URL
echo extracting source ...
tar xf tcl9.tar.gz --directory=tcl9 --strip-components=1
tar xf tk9.tar.gz --directory=tk9 --strip-components=1
cd tcl9
echo building tcl9 ...
make -j4 CFLAGS="-arch x86_64 -arch arm64 -mmacosx-version-min=10.9" -C macosx
cd ../tk9
echo building tk9 ...
make -j4 CFLAGS="-arch x86_64 -arch arm64 -mmacosx-version-min=10.9" -C macosx
cd ..
cd togl3
cp ci/macOS/mac_configure configure
./configure --with-tcl=../build/tcl/Tcl.framework --with-tk=../build/tk/Tk.framework
make
cd ..

mv togl3/libtcl9Togl3.0.dylib Togl3.0
rm -rf build

echo downloading tcl/tk source ...
wget --no-check-certificate -O tcl8.tar.gz $TCL8_URL
wget --no-check-certificate -O tk8.tar.gz $TK8_URL
echo extracting source ...
tar xf tcl8.tar.gz --directory=tcl8 --strip-components=1
tar xf tk8.tar.gz --directory=tk8 --strip-components=1
cd tcl8
echo building tcl8 ...
make -j4 CFLAGS="-arch x86_64 -arch arm64 -mmacosx-version-min=10.9" -C macosx
cd ../tk8
echo building tk8 ...
make -j4 CFLAGS="-arch x86_64 -arch arm64 -mmacosx-version-min=10.9" -C macosx
cd ../togl3
make clean
./configure --with-tcl=../build/tcl/Tcl.framework --with-tk=../build/tk/Tk.framework
make
cd ..

mv togl3/libTogl3.0.dylib Togl3.0
mv togl3/pkgIndex.tcl Togl3.0
