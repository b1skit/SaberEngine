@echo off
REM Change working directory to \SaberEngine\SaberEngine\DroidShaderBurner\
cd /d "%~dp0SaberEngine\DroidShaderBurner"

REM Execute:
@echo on
..\..\Build\x64\Release\DroidShaderBurner\DroidShaderBurner.exe -buildconfig Release -projectRoot D:\Projects\Development\SaberEngine\ -cleanandrebuild -shadersOnly

@echo off
REM Move back to the script directory
cd ..\..\

REM Pause to see the output
timeout /t 7