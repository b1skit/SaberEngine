@echo off
setlocal

echo Configuring git submodules...
call git submodule init
call git submodule update

echo Configuring vcpkg...
call .\Source\Dependencies\vcpkg\bootstrap-vcpkg.bat -disableMetrics
call .\Source\Dependencies\vcpkg\vcpkg.exe integrate install

REM Pause for 10 seconds
for /l %%i in (5,-1,1) do (
    echo Closing in %%i seconds...
    timeout /t 1 /nobreak >nul
)

exit