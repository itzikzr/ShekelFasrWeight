@echo off
cd /d "%~dp0"
pip show pyserial >nul 2>&1 || pip install pyserial
pip show openpyxl >nul 2>&1 || pip install openpyxl
python scale_sampler.py
if errorlevel 1 (
    echo.
    echo ==== The program exited with an error ====
    pause
)
