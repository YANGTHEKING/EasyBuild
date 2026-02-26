cd /d "%~dp0"

cmake -S. -B cmake_build_umd_G0M  -D SOC_TYPE=G0M -D IMGDDK119=1 -G   "Visual Studio 16 2019"
cmake --build cmake_build_umd_G0M  --config Debug

cmake -S. -B cmake_build_umd_G0M_win32  -D SOC_TYPE=G0M -G  "Visual Studio 16 2019" -DCONFIG=wnow32
cmake --build cmake_build_umd_G0M_win32  --config Debug

copy /y "cmake_build_umd_G0M\Debug\fantumd64.dll" "C:\share\"
copy /y "cmake_build_umd_G0M_win32\Debug\fantumd32.dll" "C:\share\"
pause