@echo off
REM 若 VS 生成后事件未执行，可双击本脚本：把 bl_launcher 复制为 shell_template
setlocal
set "D=%~dp0"
set "OK=0"
if exist "%D%x64\Release\bl_launcher.exe" (
  copy /Y "%D%x64\Release\bl_launcher.exe" "%D%x64\Release\shell_template.exe"
  echo [OK] "%D%x64\Release\shell_template.exe"
  set "OK=1"
)
if exist "%D%x64\Debug\bl_launcher.exe" (
  copy /Y "%D%x64\Debug\bl_launcher.exe" "%D%x64\Debug\shell_template.exe"
  echo [OK] "%D%x64\Debug\shell_template.exe"
  set "OK=1"
)
if "%OK%"=="0" (
  echo 未找到 x64\Release 或 x64\Debug 下的 bl_launcher.exe，请先编译 BlLauncher。
  echo 若输出目录不是 cpp\x64\...，请手动复制：copy bl_launcher.exe shell_template.exe
  exit /b 1
)
exit /b 0
