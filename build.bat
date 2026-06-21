@echo off
setlocal

:: 64-Bit Compiler (original Installation)
set MINGW64=C:\mingw64\bin
:: 32-Bit Compiler (in mingw32 Unterordner)
set MINGW32=C:\mingw64\mingw32\bin

echo Baue 64-Bit Plugin (xytags.wdx64) ...
%MINGW64%\x86_64-w64-mingw32-g++.exe -shared -O2 -o xytags.wdx64 xytags.cpp ^
    -static -static-libgcc -static-libstdc++ ^
    -lkernel32 -lshlwapi ^
    -Wl,--kill-at -s
if errorlevel 1 ( echo FEHLER 64-Bit! & pause & exit /b 1 )
echo OK: xytags.wdx64

echo.
echo Baue 32-Bit Stub (xytags.wdx) ...
%MINGW32%\i686-w64-mingw32-g++.exe -m32 -shared -O2 -o xytags.wdx xytags_stub32.cpp ^
    -static -static-libgcc -static-libstdc++ ^
    -lkernel32 ^
    -Wl,--kill-at -s
if errorlevel 1 ( echo FEHLER 32-Bit! & pause & exit /b 1 )
echo OK: xytags.wdx

echo.
echo Beide Plugins erfolgreich gebaut.
pause
