#!/bin/bash
set -e

TCL8_VERSION=8.6.13
TCL9_VERSION=9.0b1
TCL8_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL8_VERSION-src.tar.gz
TK8_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL8_VERSION-src.tar.gz
TCL9_URL=https://prdownloads.sourceforge.net/tcl/tcl$TCL9_VERSION-src.tar.gz
TK9_URL=https://prdownloads.sourceforge.net/tcl/tk$TCL9_VERSION-src.tar.gz

mkdir -p dist/Togl3.0
rm -rf tcl9 tk9 tcl8 tk8
mkdir tcl9 tk9 tcl8 tk8

echo downloading tcl/tk source ...
wget --no-check-certificate -O tcl9.tar.gz $TCL9_URL
wget --no-check-certificate -O tk9.tar.gz $TK9_URL
echo extracting source ...
tar xf tcl9.tar.gz --directory=tcl9 --strip-components=1
tar xf tk9.tar.gz --directory=tk9 --strip-components=1
cd tcl9/unix
echo building tcl9 ...
./configure
make -j4
cd ../../tk9/unix
echo building tk9 ...
./configure --with-tcl=../../tcl9/unix
make -j4
cd ../..
cp ci/linux/linux_configure configure
./configure --with-tcl=tcl9/unix --with-tk=tk9/unix
make
mv libtcl9Togl3.0.so dist/Togl3.0
rm -rf build

echo downloading tcl/tk source ...
wget --no-check-certificate -O tcl8.tar.gz $TCL8_URL
wget --no-check-certificate -O tk8.tar.gz $TK8_URL
echo extracting source ...
tar xf tcl8.tar.gz --directory=tcl8 --strip-components=1
tar xf tk8.tar.gz --directory=tk8 --strip-components=1
cd tcl8/unix
echo building tcl8 ...
./configure
make -j4
cd ../../tk8/unix
echo building tk8 ...
./configure --with-tcl=../../tcl8/unix
make -j4
cd ../..
./configure --with-tcl=tcl8/unix --with-tk=tk8/unix
make
mv libTogl3.0.so dist/Togl3.0
mv pkgIndex.tcl dist/Togl3.0
