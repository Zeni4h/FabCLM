@echo off
if not exist "bin" mkdir bin
echo Compiling FabCLM (C++20)...
g++ -std=c++20 -O2 -Iinclude src/main.cpp src/config.cpp src/http_client.cpp src/auth.cpp src/fabric_client.cpp src/azure_monitor_client.cpp src/dax_client.cpp src/terminal_ui.cpp src/cli.cpp -lwinhttp -o bin\fabclm.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful: bin\fabclm.exe
) else (
    echo Build failed.
)
