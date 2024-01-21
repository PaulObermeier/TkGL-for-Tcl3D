"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
cd tcl8\win
nmake /f makefile.vc
cd ..\..\tk8\win 
nmake TCLDIR=..\..\tcl8 /f makefile.vc
cd ..\..\tcl9\win
nmake /f makefile.vc
cd ..\..\tk8\win
nmake TCLDIR=..\..\tcl9 /f makefile.vc

