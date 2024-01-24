#!/bin/bash
set -e

TCL8_VERSION=8.6.13
TCL9_VERSION=9.0b1
TCL8_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL8_VERSION-src.tar.gz
TK8_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL8_VERSION-src.tar.gz
TCL9_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL9_VERSION-src.tar.gz
TK9_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL9_VERSION-src.tar.gz

mkdir -p dist/Tkgl1.0
rm -rf tcl9 tk9 tcl8 tk8
mkdir tcl9 tk9 tcl8 tk8

if ! [ -e tcl9.tar.gz ] || ! [ -e tcl9 ] ; then
    wget --no-check-certificate -O tcl9.tgz $TCL9_URL
    tar xf tcl9.tgz --directory=tcl9 --strip-components=1
fi
if ! [ -e tk9.tar.gx $Tk9_URL ] || ! [ -e tk9 ] ; then
    wget --no-check-certificate -O tk9.tar.gz $TK9_URL
    tar xf tk9.tar.gz --directory=tk9 --strip-components=1
fi
cd tcl9/macosx
./configure
cd ../../tk9/macosx
./configure
cd ../..

if ! [ -e tcl8.tar.gz ] || ! [ -e tcl8 ] ; then
    wget --no-check-certificate -O tcl8.tgz $TCL9_URL
    tar xf tcl8.tgz --directory=tcl8 --strip-components=1
fi
if ! [ -e tk8.tar.gx $Tk8_URL ] || ! [ -e tk8 ] ; then
    wget --no-check-certificate -O tk8.tgz $TK9_URL
    tar xf tk8.tgz --directory=tk8 --strip-components=1
fi
cd tcl8/macosx
./configure
cd ../../tk8/macosx
./configure
cd ../..
cp ci/macOS/mac_configure configure
./configure --with-tcl=tcl8/macosx --with-tk8/macosx
make
./configure --with-tcl=tcl9/macosx --with-tk=tk9/macosx
make
mv libTkgl1.0.dylib dist/Tkgl1.0
mv libtcl9Tkgl1.0.dylib Tkgl1.0
mv pkgIndex.tcl dist/Tkgl1.0
