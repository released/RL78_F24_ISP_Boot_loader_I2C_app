@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\device_github_publish.ps1" %*
exit /b %errorlevel%
