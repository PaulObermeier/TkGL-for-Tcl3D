"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
cd win
nmake /f makefile.vc clean
nmake TCLDIR=..\tcl8 TKDIR=..\tk8 /f makefile.vc
nmake TCLDIR=..\tcl9 TKDIR=..\tk9 /f makefile.vc
cd ..
