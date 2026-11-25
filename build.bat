@echo off
setlocal
rem needs vs 2022 build tools
rem   build.bat        tests dll and zip
rem   build.bat tests  tests only

if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto compile
if defined VCVARS goto setup
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto nocompiler
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS goto nocompiler
:setup
call "%VCVARS%" >nul 2>nul
if errorlevel 1 goto nocompiler

:compile
cd /d "%~dp0"
set "NAME=CustomAppearancePresets"
set "VERSION=1.0.0"
if not exist build mkdir build

echo [1/4] unit tests
if not exist tests\tests.cpp goto notests
cl /nologo /std:c++20 /EHsc /W4 /O2 /Fo:build\ /Fe:build\tests.exe tests\tests.cpp src\presets.cpp src\config.cpp >build\tests_build.log 2>&1
if errorlevel 1 (type build\tests_build.log & exit /b 1)
build\tests.exe
if errorlevel 1 (echo unit tests failed & exit /b 1)
:notests
if /i "%~1"=="tests" exit /b 0

echo [2/4] UE4SS import library
lib /nologo /def:src\ue4ss.def /machine:x64 /out:build\UE4SS.lib >build\lib.log 2>&1
if errorlevel 1 (type build\lib.log & exit /b 1)

echo [3/4] main.dll
cl /nologo /std:c++20 /EHa /MD /O2 /Zi /W3 /wd4251 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Fo:build\ /Fd:build\vc.pdb /LD ^
   src\mod.cpp src\ui.cpp src\game.cpp src\engine.cpp src\files.cpp src\presets.cpp src\config.cpp ^
   /Fe:build\main.dll /link build\UE4SS.lib user32.lib shell32.lib comdlg32.lib /DEBUG /PDB:build\main.pdb /PDBALTPATH:main.pdb /OPT:REF /OPT:ICF >build\dll_build.log 2>&1
if errorlevel 1 (type build\dll_build.log & exit /b 1)

echo [4/4] dist\%NAME% and the Vortex zip
if exist dist\%NAME% rmdir /s /q dist\%NAME%
set "MOD=dist\%NAME%\Pal\Binaries\Win64\ue4ss\Mods\%NAME%"
mkdir "%MOD%\dlls"
copy /y build\main.dll "%MOD%\dlls\main.dll" >nul
copy /y config.ini "%MOD%\config.ini" >nul
type nul > "%MOD%\enabled.txt"
rem tar writes forward slash paths
"%SystemRoot%\System32\tar.exe" -a -c -f dist\%NAME%-%VERSION%.zip -C dist\%NAME% Pal
if errorlevel 1 (echo could not create the zip & exit /b 1)
echo build ok: dist\%NAME% and dist\%NAME%-%VERSION%.zip
exit /b 0

:nocompiler
echo could not find the vs 2022 build tools
echo set VCVARS to your vcvars64.bat and try again
exit /b 1
