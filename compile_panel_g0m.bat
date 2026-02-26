cd /d "%~dp0"

cmake -S. -B cmake_build_G0M -D SOC_TYPE=G0M -G "Visual Studio 16 2019"
cmake --build cmake_build_G0M --config Debug

copy /y "cmake_build_G0M\Debug\FantasyPanel.exe" "C:\share\"
pause