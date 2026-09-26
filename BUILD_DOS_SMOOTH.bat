@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ============================================
echo  SGSO DOS - Smooth DOS build
echo ============================================
echo.

REM Open Watcom normally provides owsetenv.bat.  It is not enough to install
REM Watcom: the environment script must be called in this command session.
set "OWSETENV="

if exist "%WATCOM%\owsetenv.bat" set "OWSETENV=%WATCOM%\owsetenv.bat"
if not defined OWSETENV if exist "C:\WATCOM\owsetenv.bat" set "OWSETENV=C:\WATCOM\owsetenv.bat"
if not defined OWSETENV if exist "%ProgramFiles%\OpenWatcom\owsetenv.bat" set "OWSETENV=%ProgramFiles%\OpenWatcom\owsetenv.bat"
if not defined OWSETENV if exist "%ProgramFiles(x86)%\OpenWatcom\owsetenv.bat" set "OWSETENV=%ProgramFiles(x86)%\OpenWatcom\owsetenv.bat"
if not defined OWSETENV if exist "%ProgramFiles%\Open Watcom\owsetenv.bat" set "OWSETENV=%ProgramFiles%\Open Watcom\owsetenv.bat"
if not defined OWSETENV if exist "%ProgramFiles(x86)%\Open Watcom\owsetenv.bat" set "OWSETENV=%ProgramFiles(x86)%\Open Watcom\owsetenv.bat"

if defined OWSETENV (
    echo Found Open Watcom environment:
    echo   %OWSETENV%
    echo.
    call "%OWSETENV%"
    if errorlevel 1 (
        echo.
        echo ERROR: Open Watcom environment setup failed.
        goto :fail
    )
) else (
    echo Could not automatically find owsetenv.bat.
    echo.
    echo If Open Watcom is installed, locate its owsetenv.bat and run it first,
    echo then run this script again. A typical location is:
    echo   C:\WATCOM\owsetenv.bat
    echo.
    echo You can also set WATCOM manually, for example:
    echo   set WATCOM=C:\WATCOM
    echo   call C:\WATCOM\owsetenv.bat
    goto :fail
)

echo.
echo Checking compiler...
echo WATCOM=%WATCOM%
where wcl
if errorlevel 1 (
    echo.
    echo ERROR: wcl.exe is still not on PATH after owsetenv.bat.
    echo Check that the Open Watcom C/C++ package is installed, not only another
    echo Open Watcom component, and that its installation is complete.
    goto :fail
)

for /f "delims=" %%I in ('where wcl') do (
    echo Compiler: %%I
    goto :compiler_found
)
:compiler_found

echo.
echo Building SG8.EXE...
cd /d "%~dp0dos_src"
wcl -bt=dos -ml -0 -ox sg8.c -fe=..\dos\SG8.EXE
if errorlevel 1 goto :build_fail

echo Building SETUP.EXE...
wcl -bt=dos -ms -0 -ox setup.c -fe=..\dos\SETUP.EXE
if errorlevel 1 goto :build_fail

echo Building INSTALL.EXE...
wcl -bt=dos -ms -0 -ox install.c -fe=..\dos\INSTALL.EXE
if errorlevel 1 goto :build_fail

cd /d "%~dp0"
echo.
echo ============================================
echo  BUILD SUCCESSFUL
echo ============================================
echo.
echo New DOS binaries:
echo   dos\SG8.EXE
echo   dos\SETUP.EXE
echo   dos\INSTALL.EXE
echo.
pause
exit /b 0

:build_fail
cd /d "%~dp0"
echo.
echo ============================================
echo  BUILD FAILED
echo ============================================
echo.
echo The compiler returned an error above. Keep this window open and send
echo the error text if you want me to fix the build.
pause
exit /b 1

:fail
cd /d "%~dp0"
echo.
echo ============================================
echo  WATCOM SETUP NOT READY
echo ============================================
echo.
echo This window is intentionally kept open so the error can be read.
pause
exit /b 1
