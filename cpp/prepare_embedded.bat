@echo off
setlocal
set "ROOT=%~dp0"
set "EMB=%ROOT%launcher\embedded"
if not exist "%EMB%" mkdir "%EMB%"

REM 按需修改源路径；存在则覆盖 embedded 内文件
if exist "%ROOT%x64\Release\public.blb" copy /Y "%ROOT%x64\Release\public.blb" "%EMB%\public.blb"
if exist "%ROOT%x64\Debug\public.blb" copy /Y "%ROOT%x64\Debug\public.blb" "%EMB%\public.blb"

REM 示例：把待保护程序复制为 payload.exe（请改成你的 exe 路径）
if exist "%ROOT%..\YourApp.exe" copy /Y "%ROOT%..\YourApp.exe" "%EMB%\payload.exe"

echo 已尝试更新 "%EMB%" 下的 public.blb / payload.exe，请检查后再生成 BlLauncher。
exit /b 0
