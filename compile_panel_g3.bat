cd /d "%~dp0"

cmake -S. -B cmake_build_G3 -D SOC_TYPE=G3 -G "Visual Studio 16 2019"
cmake --build cmake_build_G3 --config Debug

copy /y "cmake_build_G3\Debug\FantasyPanel.exe" "C:\share\"
pause