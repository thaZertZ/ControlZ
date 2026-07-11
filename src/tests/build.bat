@echo off
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

REM Nothing here works lmao

for /f "skip=1 delims=" %%X in (' certutil -hashfile "server.cpp" SHA512 ') do ( if not defined SERVER_HASH set "SERVER_HASH=%%X" )
for /f "delims=" %%X in ("temp\server.hash") do (
    if "%%X" == "!SERVER_HASH!" (
        echo SKIPPED SERVER
        goto :CLIENT
    )
)
<nul set /p "dummy=!SERVER_HASH!" > "temp\server.hash"

echo COMPILING SERVER
echo.
echo.
start /b cmd /c "g++ -std=c++23 -o server server.cpp -lws2_32"

:CLIENT
for /f "skip=1 delims=" %%X in (' certutil -hashfile "client.cpp" SHA512 ') do ( if not defined CLIENT_HASH set "CLIENT_HASH=%%X" )
for /f "delims=" %%X in ("temp\client.hash") do (
    if "%%X" == "!CLIENT_HASH!" (
        echo SKIPPED CLIENT
        goto :DONE
    )
)
<nul set /p "dummy=!CLIENT_HASH!" > "temp\client.hash"

echo.
echo.
echo COMPILING CLIENT
echo.
echo.
g++ -std=c++23 -o client client.cpp -lws2_32
echo.
echo.

:DONE
endlocal
exit /b 0
