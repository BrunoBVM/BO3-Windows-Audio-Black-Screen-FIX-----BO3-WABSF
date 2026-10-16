@echo off
REM Requires MinGW-w64 (x64) g++ on PATH, e.g. winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
g++ -shared -O2 -static -o dsound.dll src\dsound.cpp src\dsound.def
if errorlevel 1 (echo Build failed) else (echo Built dsound.dll)
