@echo off

REM change working directory to repository directory
cd /D "%~dp0/.."

py -3.11 ./setup.py bdist_wheel
py -3.10 ./setup.py bdist_wheel
py -3.9 ./setup.py bdist_wheel
py -3.8 ./setup.py bdist_wheel
py -3.7 ./setup.py bdist_wheel
py -3.6 ./setup.py bdist_wheel
