@echo off
setlocal enabledelayedexpansion enableextensions
set /a TOTAL_TESTS=6
set /a PASSED_TESTS=0
goto :TEST

:COMP_EXEC
g++ -std=c++23 -Wall -Wextra -Werror -pedantic "%~1.cpp" -o "%~1" || exit /b -1
echo [i] Compiled "%~1"
call ".\%~1.exe" || exit /b -1
echo [i] Ran "%~1"
set /a PASSED_TESTS+=1
exit /b 0

:TEST

call :COMP_EXEC "common"   || goto :CLEANUP
call :COMP_EXEC "time"     || goto :CLEANUP
call :COMP_EXEC "mappings" || goto :CLEANUP
call :COMP_EXEC "user"     || goto :CLEANUP
call :COMP_EXEC "envelope" || goto :CLEANUP
call :COMP_EXEC "dms"      || goto :CLEANUP

:CLEANUP
del /f /q ".\*.exe" >nul 2>&1
echo.
echo [i] !PASSED_TESTS!/!TOTAL_TESTS! tests passed
endlocal
