set -e
rm -rf Togl-3.0
mkdir Togl-3.0
./configure --with-tcl=/usr/local/Tcl8/lib --with-tk=/usr/local/Tk8/lib
make clean
make
mv libTogl3.0.so Togl-3.0
./configure --with-tcl=/usr/local/Tcl9/lib --with-tk=/usr/local/Tk9/lib
make clean
make
mv libtcl9Togl3.0.so Togl-3.0
mv pkgIndex.tcl Togl-3.0
