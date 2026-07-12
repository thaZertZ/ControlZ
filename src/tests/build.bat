@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

echo COMPILING LOGSERVER
echo.
echo.
start /b cmd /c "g++ -std=c++23 -o logserver logserver.cpp -lws2_32"

echo.
echo.
echo COMPILING SERVER
echo.
echo.
start /b cmd /c "g++ -std=c++23 -o server server.cpp -lws2_32"

echo.
echo.
echo COMPILING CLIENT
echo.
echo.
g++ -std=c++23 -o client client.cpp -lws2_32
echo.
echo.

endlocal
exit /b 0
