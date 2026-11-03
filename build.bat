@echo off
setlocal

set TARGET_ARCH=%1
if "%TARGET_ARCH%"=="" set TARGET_ARCH=all

echo ========================================================
echo   KAISEKI - NATIVE C++ DUAL-ARCH BUILD RUNNER
echo   Hedef Mimari: %TARGET_ARCH% ^| Yapilandirma: Release
echo ========================================================
echo.

:: 1. Locate Visual Studio 2022 MSBuild
set "MSBUILD="
if exist "E:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=E:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) else (
    for /f "usebackq tokens=*" %%i in (`where msbuild 2^>nul`) do (
        set "MSBUILD=%%i"
        goto :found_msbuild
    )
)

:found_msbuild
if "%MSBUILD%"=="" (
    echo [HATA] Visual Studio MSBuild.exe bulunamadi!
    echo Lutfen Visual Studio 2022 C++ yuklemesini dogrulayin.
    exit /b 1
)

echo [1/3] Visual Studio MSBuild bulundu:
echo "%MSBUILD%"
echo.

if /i "%TARGET_ARCH%"=="all" (
    call :build_arch x64
    if errorlevel 1 exit /b 1
    call :build_arch x86
    if errorlevel 1 exit /b 1
) else (
    call :build_arch %TARGET_ARCH%
    if errorlevel 1 exit /b 1
)

echo.
echo ========================================================
echo [BASARILI] Kaiseki ve tum DLL kutuphaneleri
echo Visual Studio ile basariyla derlendi!
echo.
echo Cikti Dizinleri:
echo   - bin\x64\Kaiseki.exe (ve 9 DLL)
echo   - bin\x86\Kaiseki.exe (ve 9 DLL)
echo ========================================================
exit /b 0

:build_arch
set "CURRENT_ARCH=%~1"
echo.
echo --------------------------------------------------------
echo [*] Derleniyor: Release ^| Platform: %CURRENT_ARCH%
echo --------------------------------------------------------
"%MSBUILD%" "Kaiseki\Kaiseki.sln" /p:Configuration=Release /p:Platform=%CURRENT_ARCH% /t:Rebuild /m /v:m
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] %CURRENT_ARCH% derlemesi basarisiz oldu!
    exit /b 1
)

if not exist "bin\%CURRENT_ARCH%\Kaiseki.exe" (
    echo [HATA] bin\%CURRENT_ARCH%\Kaiseki.exe bulunamadi!
    exit /b 1
)

if not exist "bin\%CURRENT_ARCH%\res" mkdir "bin\%CURRENT_ARCH%\res"
copy /y "Kaiseki\res\Kaiseki.ico" "bin\%CURRENT_ARCH%\res\" >nul 2>&1
copy /y "Kaiseki\res\logo.png" "bin\%CURRENT_ARCH%\res\" >nul 2>&1
copy /y "Kaiseki\res\banner.png" "bin\%CURRENT_ARCH%\res\" >nul 2>&1

if not exist "bin\%CURRENT_ARCH%\lang" mkdir "bin\%CURRENT_ARCH%\lang"
copy /y "lang\*.xml" "bin\%CURRENT_ARCH%\lang\" >nul 2>&1

echo [+] %CURRENT_ARCH% basariyla hazirlandi ve kopyalandi.
exit /b 0
