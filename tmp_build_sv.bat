@echo off
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=amd64 >/dev/null 2>&1
cd /d "C:\Users\Krystian\source\repos\engine"
cmake --build out/build/Qt-Debug --target shadow_validation 2>&1
