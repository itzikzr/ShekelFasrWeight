@echo off
cd /d "%~dp0"
pip show pyserial >nul 2>&1 || pip install pyserial
python scale_sampler.py
if errorlevel 1 (
    echo.
    echo ==== The program exited with an error ====
    pause
)
