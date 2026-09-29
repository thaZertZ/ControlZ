@echo off
setlocal enabledelayedexpansion enableextensions
chcp 65001 >nul 2>&1
pushd "%~dp0"

set /a TOTAL_TESTS=0
for /f "tokens=*" %%X in (' dir /b ') do (
    if "%%~xX" == ".cpp" set /a TOTAL_TESTS+=1
)
set /a PASSED_TESTS=0
set /a NON_COMPILED=0

for /f "tokens=*" %%X in (' dir /b ') do (
    if "%%~xX" == ".cpp" (
        g++ -std=c++23 -Wall -Wextra -Werror -pedantic "%%~X" -o "%%~nX"
        if !errorlevel! == 0 (
            echo [i] Compiled "%%~nX"
            call ".\%%~nX.exe" && set /a PASSED_TESTS+=1
            echo [i] Ran "%%~nX"
        ) else (
            [i] Failed to compile "%%~nX"
            set /a NON_COMPILED+=1
        )
    )
)

del /f /q ".\*.exe" >nul 2>&1
echo.
if not !NON_COMPILED! == 0 (
    echo [i] !PASSED_TESTS!/!TOTAL_TESTS! tests passed (!NON_COMPILED! did not compile successfully)
) else (
    echo [i] !PASSED_TESTS!/!TOTAL_TESTS! tests passed
)
popd
endlocal
