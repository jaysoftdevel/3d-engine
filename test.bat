@echo off
setlocal DisableDelayedExpansion
"%~dp0tools\launch-python.bat" "%~dp0tools\build_windows.py" --root "%~dp0." --test %*
