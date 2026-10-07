@echo off
setlocal
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if defined PROCESSOR_ARCHITEW6432 set "PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%PS%" (
  echo ERROR: Windows PowerShell 5.1 is unavailable. Repair Windows using DISM and SFC; setup cannot run yet.
  pause
  exit /b 1
)
"%PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup.ps1" %*
set "RESULT=%ERRORLEVEL%"
if defined OW_SETUP_NO_PAUSE goto finish
:arguments
if "%~1"=="" goto show_result
if /i "%~1"=="-NonInteractive" goto finish
shift
goto arguments
:show_result
if "%RESULT%"=="0" (echo Setup completed. The worker runs from the system tray.) else (echo Setup stopped. Read the failure and report path above. Your saved identity and job journal are retained.)
pause
:finish
exit /b %RESULT%
