@echo off
rem Builds and runs the domain + application tests (no GTA, no ScriptHookV).
setlocal
set HERE=%~dp0
set SRC=%HERE%..\src
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if not exist "%HERE%..\build\tests" mkdir "%HERE%..\build\tests"
cl /nologo /EHsc /std:c++20 /W3 /DNOMINMAX "%HERE%tests.cpp" "%SRC%\app\tardis.cpp" "%SRC%\domain\flight_model.cpp" "%SRC%\domain\pilot_camera.cpp" ^
  /Fo"%HERE%..\build\tests\\" /Fe"%HERE%..\build\tests\tests.exe" || exit /b 1
"%HERE%..\build\tests\tests.exe"
