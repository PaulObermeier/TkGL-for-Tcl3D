@echo 0ff
rem NOTE: github runners use enterprise visual studio
set enterprise="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set community="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

if exist %enterprise% goto ent else goto comm
.ent
    set varsall=%enterprise%
    goto loadenv
.comm
    set varsall=%community%
    goto loadenv
.loadenv
    %varsall% x86_amd64

cd tcl8\win
nmake INSTAKKDIR=..\..\TclTk8 /f makefile.vc
nmake INSTALLDIR=..\..\TclTk8 /f makefile.vc install
cd ..\..\tk8\win
nmake INSTALLDIR=..\..\TclTk8 /f makefile.vc
nmake INSTALLDIR=..\..\TclTk8 /f makefile.vc install
cd ..\..\tcl9\win
nmake INSTALLDIR=..\..\TclTk9 /f makefile.vc
nmake INSTALLDIR=..\..\TclTk9 /f makefile.vc install
cd ..\..\tk9\win
nmake INSTALLDIR=..\..\TclTk9 /f makefile.vc
nmake INSTALLDIR=..\..\TclTk9 /f makefile.vc install


