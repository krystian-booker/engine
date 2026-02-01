@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja
REM QTDIR is auto-detected by CMakeLists.txt - no need to set manually
echo Configuring...
cmake --preset Qt-Debug -S C:\Users\krystian\source\repos\engine
echo Building...
cmake --build out/build/Qt-Debug 2>&1
