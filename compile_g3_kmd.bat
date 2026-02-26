cd /d "%~dp0"
rd /s /q "cmake_build_kmd_G3"

cmake -S. -B cmake_build_kmd_G3 -D IMGDDK119=1 -D BUILD_KMD=1 -D SUPPORT_UNIQ=1 -D SOC_TYPE=G3 -G "Visual Studio 16 2019"
cmake --build cmake_build_kmd_G3 --config Debug
pause