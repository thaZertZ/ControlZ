@echo off
chcp 65001 >nul 2>&1
if /i "%~1" == "logserver" (
    g++ -std=c++23 -o logserver logserver.cpp -lws2_32
    exit /b 0
)
if /i "%~1" == "server" (
    g++ -std=c++23 -o server server.cpp -lws2_32
    exit /b 0
)
if /i "%~1" == "client" (
    g++ -std=c++23 -o client client.cpp -lws2_32
    exit /b 0
)

:LOGSERVER
echo COMPILING LOGSERVER
echo.
echo.
start /b cmd /c "g++ -std=c++23 -o logserver logserver.cpp -lws2_32"

:SERVER
echo.
echo.
echo COMPILING SERVER
echo.
echo.
start /b cmd /c "g++ -std=c++23 -o server server.cpp -lws2_32"

:CLIENT
echo.
echo.
echo COMPILING CLIENT
echo.
echo.
g++ -std=c++23 -o client client.cpp -lws2_32
echo.
echo.

exit /b 0
