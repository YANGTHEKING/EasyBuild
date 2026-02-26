cd /d "%~dp0"

cmake -S. -B cmake_build_umd_G3  -D IMGDDK119=1 -D SOC_TYPE=G3  -D SUPPORT_UNIQ=1 -G "Visual Studio 16 2019"
cmake --build cmake_build_umd_G3  --config Debug

cmake -S. -B cmake_build_umd_G3_win32  -D IMGDDK119=1 -D SOC_TYPE=G3  -D SUPPORT_UNIQ=1 -G "Visual Studio 16 2019" -DCONFIG=wnow32
cmake --build cmake_build_umd_G3_win32  --config Debug

copy /y "cmake_build_umd_G3\Debug\fantumd64.dll" "C:\share\"
copy /y "cmake_build_umd_G3_win32\Debug\fantumd32.dll" "C:\share\"
pause