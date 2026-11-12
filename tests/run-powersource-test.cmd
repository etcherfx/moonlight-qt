@echo off
rem Builds and runs the power source unit tests with MSVC.
rem Usage: run-powersource-test.cmd [output dir]
setlocal
set "REPO=%~dp0.."
set "OUT=%~1"
if "%OUT%"=="" set "OUT=%TEMP%\moonlight-powersource-test"
if not exist "%OUT%" mkdir "%OUT%" || exit /b 1

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" amd64 >nul || exit /b 1

cl /nologo /std:c++17 /EHsc /W4 /WX /I "%REPO%\app" "%REPO%\tests\powersource_test.cpp" /Fe:"%OUT%\powersource_test.exe" /Fo:"%OUT%\\" || exit /b 1
"%OUT%\powersource_test.exe"
exit /b %ERRORLEVEL%
