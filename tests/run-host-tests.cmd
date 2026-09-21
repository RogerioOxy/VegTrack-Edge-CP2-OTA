@echo off
python refresh_functions.py
if errorlevel 1 exit /b 1

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist build mkdir build

cl /nologo /EHsc /utf-8 /O2 actual_functions_test.cpp /Febuild\actual_functions_test.exe /Fobuild\actual_functions_test.obj

if errorlevel 1 exit /b 1

build\actual_functions_test.exe

