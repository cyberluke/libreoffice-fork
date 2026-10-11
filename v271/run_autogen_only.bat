@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set FORCE_AUTOGEN=1
set AUTOGEN_ONLY=1
C:\msys64\usr\bin\bash.exe -lc "cd /d/_SATIN_AI/LibreOffice && ./v271/build_win.sh"