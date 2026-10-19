@echo off
rem Builds build\TARDIS.asi with MSVC. third_party\shv holds the ScriptHookV SDK (dev-c.com, not redistributable).
setlocal
set HERE=%~dp0
set SRC=%HERE%src
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
if not exist "%HERE%build\obj" mkdir "%HERE%build\obj"
cl /nologo /LD /O2 /EHsc /std:c++20 /MT /W3 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /wd4244 /I "%HERE%third_party\shv" /I "%HERE%third_party\miniaudio" ^
  "%SRC%\main.cpp" "%SRC%\domain\flight_model.cpp" "%SRC%\domain\pilot_camera.cpp" "%SRC%\app\tardis.cpp" ^
  "%SRC%\infra\gta_world.cpp" "%SRC%\infra\gta_shield.cpp" "%SRC%\infra\ini.cpp" "%SRC%\infra\log.cpp" "%SRC%\infra\ma_audio.cpp" "%SRC%\infra\ma_impl.cpp" ^
  /Fo"%HERE%build\obj\\" /Fe"%HERE%build\TARDIS.asi" ^
  /link "%HERE%third_party\shv\ScriptHookV.lib" user32.lib
