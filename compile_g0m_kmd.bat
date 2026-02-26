cd /d "%~dp0"
rd /s /q "cmake_build_kmd_G0M"

cmake -S. -B cmake_build_kmd_G0M -D BUILD_KMD=1 -D SOC_TYPE=G0M -D ENABLE_PROCESS_STATS=1 -G "Visual Studio 16 2019"
cmake --build cmake_build_kmd_G0M --config Debug
pause