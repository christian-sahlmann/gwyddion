deltree /y gwyddion-cvs
wget -t 1 http://trific.ath.cx/Ftp/gwyddion/gwyddion-1.99.6cvs20060317.tar.gz
"c:\Program Files\7-Zip\7z.exe" x -y gwyddion-1.99.6cvs20060317.tar.gz
"c:\Program Files\7-Zip\7z.exe" x -y gwyddion-1.99.6cvs20060317.tar
ren gwyddion-1.99.6cvs20060317 gwyddion-cvs
copy unix2dos.exe "gwyddion-cvs\unix2dos.exe"
copy upload.bat "gwyddion-cvs\upload.bat"
cd gwyddion-cvs
PATH="c:\program files\microsoft visual studio\common\msdev98\bin;c:\program files\microsoft visual studio\vc98\bin;c:\windows;c:\windows\command"
nmake -f makefile.msc -x make_log > make_log2
nmake -f makefile.msc install -x install_log > install_log2
unix2dos.exe data\gwyddion.iss inst\gwyddion.iss
cd inst
"c:\Program Files\Inno Setup 4\iscc.exe" gwyddion.iss
ren Gwyddion-1.99.6cvs20060317.exe Gwyddion-cvs.exe
cd ..
upload.bat download/test inst\Gwyddion-cvs.exe
upload.bat logs make_log2
upload.bat logs install_log2
