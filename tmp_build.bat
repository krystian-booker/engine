@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cmake --build "C:\Users\Krystian\source\repos\engine\out\build\Qt-Debug" --target shadow_validation
