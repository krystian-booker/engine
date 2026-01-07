@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
cmake --build "C:\Users\krystian\source\repos\engine\out\build\Qt-Debug" --target environment_demo
