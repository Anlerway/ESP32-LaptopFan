@echo off
rem Build the firmware with a fixed-length placeholder device name and copy the
rem three flash images into flasher\assets (they are embedded into the flasher).
setlocal
set "CLI=D:\APP\Arduino\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
set "CFG=C:\Users\ASUS\.arduinoIDE\arduino-cli.yaml"
set "SKETCH=%~dp0..\..\LAPTOP_FAN"
set "OUT=%~dp0..\out"
"%CLI%" --config-file "%CFG%" compile --fqbn esp32:esp32:esp32 --output-dir "%OUT%" --build-property "compiler.cpp.extra_flags=-DBT_DEVICE_NAME=\"FAN-NAME-PLACEHOLDER-XXX\"" "%SKETCH%"
if errorlevel 1 exit /b 1
copy /y "%OUT%\LAPTOP_FAN.ino.bootloader.bin" "%~dp0..\assets\bootloader.bin" >nul
copy /y "%OUT%\LAPTOP_FAN.ino.partitions.bin" "%~dp0..\assets\partitions.bin" >nul
copy /y "%OUT%\LAPTOP_FAN.ino.bin" "%~dp0..\assets\app.bin" >nul
echo assets updated
