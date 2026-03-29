@echo off
setlocal

set "ROOT=%~dp0"
set "BUILD=%ROOT%build"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [错误] 找不到 vswhere.exe。请安装 Visual Studio 2019/2022 或 Build Tools。
  exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo [错误] 未检测到 MSVC 工具集。请在 Visual Studio Installer 中勾选「使用 C++ 的桌面开发」。
  exit /b 1
)

call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 || exit /b 1

where cmake >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>nul
if errorlevel 1 (
  echo [错误] 未找到 cmake.exe。请安装 CMake ^(https://cmake.org/download/^) 或在 VS Installer 中勾选「C++ CMake 工具」。
  exit /b 1
)

cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 16 2019" -A x64
if errorlevel 1 (
  cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64
  if errorlevel 1 (
    echo [错误] CMake 找不到 VS 生成器，请安装 VS2019/2022 的「使用 C++ 的桌面开发」。
    exit /b 1
  )
)

cmake --build "%BUILD%" --config Release || exit /b 1

echo.
echo [完成] 可执行文件通常在：
echo   "%BUILD%\Release\bl_launcher.exe"
echo   "%BUILD%\Release\bl_keygen.exe"
exit /b 0
