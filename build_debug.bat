@echo off
setlocal

set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

echo ========================================================
echo   KAISEKI STUDIO - VISUAL STUDIO DEBUG BUILD RUNNER
echo   Yapilandirma: Debug ^| Mimari: %ARCH%
echo ========================================================
echo.

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
    exit /b 1
)

echo [1/2] Visual Studio ile Debug derlemesi yapiliyor...
"%MSBUILD%" "Kaiseki\Kaiseki.sln" /p:Configuration=Debug /p:Platform=%ARCH% /t:Build /m /v:m
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [HATA] Visual Studio Debug derlemesi basarisiz oldu!
    exit /b 1
)

echo.
echo [2/2] Ciktilar kopyalaniyor...
if not exist bin\debug\%ARCH% mkdir bin\debug\%ARCH%
copy /y Kaiseki\bin\*.exe bin\debug\%ARCH%\ >nul 2>&1
copy /y Kaiseki\bin\*.dll bin\debug\%ARCH%\ >nul 2>&1
copy /y Kaiseki\bin\*.pdb bin\debug\%ARCH%\ >nul 2>&1
copy /y Kaiseki\bin\*.lib bin\debug\%ARCH%\ >nul 2>&1

echo.
echo ========================================================
echo [BASARILI] Kaiseki Debug (%ARCH%) basariyla olusturuldu!
echo PDB ve Ciktilar: bin\debug\%ARCH%\
echo ========================================================
exit /b 0
