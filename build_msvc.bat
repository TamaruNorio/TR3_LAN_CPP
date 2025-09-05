@echo off
setlocal ENABLEEXTENSIONS

:: ===========================================================
::  TR3XM LAN sample - MSVC build script (absolute paths)
::  Usage: build_msvc.bat [debug|release|clean]
:: ===========================================================

:: ---- 1) Initialize Visual Studio (MSVC) environment ----
::     * Fixed path (your environment)
set "VSDEV_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools"
set "VSDEV=%VSDEV_DIR%\VsDevCmd.bat"

if not exist "%VSDEV%" (
  echo [ERROR] VsDevCmd.bat not found: "%VSDEV%"
  exit /b 1
)

:: Use short (8.3) path to avoid issues with spaces/fullwidth chars
for %%I in ("%VSDEV%") do set "VSDEV_SHORT=%%~sI"
call "%VSDEV_SHORT%" -arch=x64 -host_arch=x64 >nul

where cl >nul 2>&1
if errorlevel 1 (
  echo [ERROR] cl.exe not found after VsDevCmd. Please ensure C++ tools are installed.
  exit /b 1
)

:: ---- 2) Absolute project paths (based on this .bat location) ----
set "ROOT=%~dp0"
set "SRC_DIR=%ROOT%src"
set "INC_DIR=%ROOT%include"
set "OUT_DIR=%ROOT%build"
set "TARGET=tr3xm_lan"

:: ---- 3) Args ----
if /I "%~1"=="clean" goto :CLEAN
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: ---- 4) Flags (absolute /I; outputs go to build) ----
set "COMMON_CFLAGS=/nologo /std:c++17 /EHsc /W4 /utf-8 /I "%INC_DIR%" /I "%INC_DIR%\tr3" /D_CRT_SECURE_NO_WARNINGS"
set "LINK_BASE=/link Ws2_32.lib /OUT:"%OUT_DIR%\%TARGET%.exe""

if /I "%CONFIG%"=="release" (
  set "OPTCFLAGS=/O2 /DNDEBUG"
  set "LINKEXTRA="
) else (
  set "OPTCFLAGS=/Zi /Od"
  set "LINKEXTRA=/DEBUG"
)

echo [BUILD] CONFIG=%CONFIG%
echo [CFLAGS] %COMMON_CFLAGS% %OPTCFLAGS%
echo [LFLAGS] %LINK_BASE% %LINKEXTRA%

:: ---- 5) Compile & link all sources in src ----
pushd "%SRC_DIR%"
cl %COMMON_CFLAGS% %OPTCFLAGS% ^
  /Fo"%OUT_DIR%\\" ^
  /Fd"%OUT_DIR%\%TARGET%.pdb" ^
  *.cpp /Fe:"%OUT_DIR%\%TARGET%.exe" %LINK_BASE% %LINKEXTRA%
set "ERR=%ERRORLEVEL%"
popd

if not "%ERR%"=="0" (
  echo [BUILD] failed (ERRORLEVEL=%ERR%)
  exit /b %ERR%
)

echo [BUILD] done: "%OUT_DIR%\%TARGET%.exe"
exit /b 0

:: ---- 6) Clean ----
:CLEAN
echo [CLEAN] removing build outputs...
if exist "%OUT_DIR%\%TARGET%.exe" del /q "%OUT_DIR%\%TARGET%.exe" 2>nul
if exist "%OUT_DIR%\*.obj"        del /q "%OUT_DIR%\*.obj"        2>nul
if exist "%OUT_DIR%\*.pdb"        del /q "%OUT_DIR%\*.pdb"        2>nul
if exist "%OUT_DIR%\*.ilk"        del /q "%OUT_DIR%\*.ilk"        2>nul
rem legacy leftovers (old versions)
if exist "%SRC_DIR%\*.obj"        del /q "%SRC_DIR%\*.obj"        2>nul
if exist "%ROOT%\*.obj"           del /q "%ROOT%\*.obj"           2>nul
echo [CLEAN] done
exit /b 0
