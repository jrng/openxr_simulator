@echo off

set RUNTIME_FILENAME=%~f1
set MANIFEST_DIRECTORY=%LOCALAPPDATA%\openxr\1
set MANIFEST_FILENAME=%MANIFEST_DIRECTORY%\active_runtime.json

if "%RUNTIME_FILENAME%"=="" (
    echo usage: %0 ^<runtime-path^>
    exit /b
)

if not exist "%RUNTIME_FILENAME%" (
    echo --^> error: Could not find runtime '%RUNTIME_FILENAME%'.
    exit /b
)

echo  runtime to register: %RUNTIME_FILENAME%
echo      openxr manifest: %MANIFEST_FILENAME%
echo.

if not exist "%MANIFEST_DIRECTORY%" (
    echo --^> Create manifest directory '%MANIFEST_DIRECTORY%'.
    mkdir "%MANIFEST_DIRECTORY%"
)

REM TODO: backup old manifest file

(
    echo {
    echo   "file_format_version": "1.0.0",
    echo   "runtime": {
    echo     "library_path": "%RUNTIME_FILENAME:\=\\%"
    echo   }
    echo }
) > "%MANIFEST_FILENAME%"

reg add "HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\OpenXR\1" /v "ActiveRuntime" /t REG_SZ /d "%MANIFEST_FILENAME%" /f

if not %ERRORLEVEL% equ 0 (
    echo --^> error: Could not write registry entry. Run the script as administrator.
    exit /b
)

echo --^> Successfully registered runtime '%RUNTIME_FILENAME%'.
