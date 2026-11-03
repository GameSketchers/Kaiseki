@echo off
setlocal enabledelayedexpansion

:: 0. Otomatik Surum Numarasi Tespiti (KaisekiVersion.h uzerinden)
set KAISEKI_VER=1.0.0
for /f "tokens=3" %%v in ('findstr /r /c:"#define  *KAISEKI_VERSION_STRING" Kaiseki\include\KaisekiVersion.h 2^>nul') do (
    set KAISEKI_VER=%%~v
)

echo ========================================================
echo   KAISEKI - GITHUB RELEASES ^& INSTALLER PACKAGER
echo   Uygulama : Kaiseki
echo   Versiyon : v%KAISEKI_VER% (Dual-Arch x64 / x86)
echo   Paketler : Portable ZIP + Windows Installer (.msi)
echo ========================================================
echo.

taskkill /f /im Kaiseki.exe >nul 2>&1

:: 1. Visual Studio MSBuild / cl ile x64 ve x86 Derleme
echo [ADIM 1/5] Release ikili dosyalari derleniyor...
echo  - 64-bit (x64) Release derleniyor...
call build.bat x64
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] x64 Release derleme basarisiz oldu! Paketleme iptal edildi.
    exit /b 1
)

echo.
echo  - 32-bit (x86) Release derleniyor...
call build.bat x86
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] x86 Release derleme basarisiz oldu! Paketleme iptal edildi.
    exit /b 1
)

:: 2. Dagitim (dist) Klasorunu Temizle ve Olustur
echo.
echo [ADIM 2/5] Dagitim (dist) staging alanlari hazirlaniyor...
if exist dist rmdir /s /q dist
mkdir dist
mkdir dist\staging_x64
mkdir dist\staging_x86

:: 3. Ikili Dosyalari ve Belgeleri Staging Alanina Kopyala
echo [ADIM 3/5] Dosyalar staging alanina kopyalaniyor...
:: x64
copy /y bin\x64\Kaiseki.exe dist\staging_x64\ >nul
copy /y bin\x64\KaisekiCore.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiDex.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiUI.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiADB.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiSec.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiDiff.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiHex.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiRes.dll dist\staging_x64\ >nul
copy /y bin\x64\KaisekiAnim.dll dist\staging_x64\ >nul
copy /y README.md dist\staging_x64\ >nul
copy /y LICENSE dist\staging_x64\ >nul
mkdir dist\staging_x64\res >nul 2>&1
copy /y Kaiseki\res\Kaiseki.ico dist\staging_x64\res\ >nul 2>&1
copy /y Kaiseki\res\logo.png dist\staging_x64\res\ >nul 2>&1
copy /y Kaiseki\res\banner.png dist\staging_x64\res\ >nul 2>&1
copy /y Kaiseki\res\setup_banner.png dist\staging_x64\res\ >nul 2>&1
mkdir dist\staging_x64\lang >nul 2>&1
copy /y lang\*.xml dist\staging_x64\lang\ >nul 2>&1

:: x86
copy /y bin\x86\Kaiseki.exe dist\staging_x86\ >nul
copy /y bin\x86\KaisekiCore.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiDex.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiUI.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiADB.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiSec.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiDiff.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiHex.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiRes.dll dist\staging_x86\ >nul
copy /y bin\x86\KaisekiAnim.dll dist\staging_x86\ >nul
copy /y README.md dist\staging_x86\ >nul
copy /y LICENSE dist\staging_x86\ >nul
mkdir dist\staging_x86\res >nul 2>&1
copy /y Kaiseki\res\Kaiseki.ico dist\staging_x86\res\ >nul 2>&1
copy /y Kaiseki\res\logo.png dist\staging_x86\res\ >nul 2>&1
copy /y Kaiseki\res\banner.png dist\staging_x86\res\ >nul 2>&1
copy /y Kaiseki\res\setup_banner.png dist\staging_x86\res\ >nul 2>&1
mkdir dist\staging_x86\lang >nul 2>&1
copy /y lang\*.xml dist\staging_x86\lang\ >nul 2>&1

:: 4. Portable ZIP Paketlerini Olustur
echo.
echo [ADIM 4/5] Portable ZIP paketleri olusturuluyor...
powershell -NoProfile -Command ^
  "Write-Host '  -> dist\Kaiseki-v%KAISEKI_VER%-win-x64.zip olusturuluyor...';" ^
  "Compress-Archive -Path 'dist\staging_x64\*' -DestinationPath 'dist\Kaiseki-v%KAISEKI_VER%-win-x64.zip' -Force;" ^
  "Write-Host '  -> dist\Kaiseki-v%KAISEKI_VER%-win-x86.zip olusturuluyor...';" ^
  "Compress-Archive -Path 'dist\staging_x86\*' -DestinationPath 'dist\Kaiseki-v%KAISEKI_VER%-win-x86.zip' -Force;"

:: 5. Windows Installer (.msi) Kurulum Paketlerini Olustur
echo.
echo [ADIM 5/5] Windows Installer (.msi) kurulum paketleri olusturuluyor...
echo   -> dist\Kaiseki-v%KAISEKI_VER%-Setup-x64.msi olusturuluyor...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_msi.ps1 -arch x64 -version "%KAISEKI_VER%" -outPath "dist\Kaiseki-v%KAISEKI_VER%-Setup-x64.msi"
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] x64 MSI olusturulamadi!
    exit /b 1
)

echo   -> dist\Kaiseki-v%KAISEKI_VER%-Setup-x86.msi olusturuluyor...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_msi.ps1 -arch x86 -version "%KAISEKI_VER%" -outPath "dist\Kaiseki-v%KAISEKI_VER%-Setup-x86.msi"
if %ERRORLEVEL% NEQ 0 (
    echo [HATA] x86 MSI olusturulamadi!
    exit /b 1
)

:: Gecici Staging Dizinlerini Temizle
rmdir /s /q dist\staging_x64
rmdir /s /q dist\staging_x86

:: Ozet ve SHA-256 Dogrulama Tablosu
echo.
echo ========================================================
echo   [BASARILI] KAISEKI RELEASE VE KURULUM PAKETLERI HAZIR!
echo ========================================================
powershell -NoProfile -Command ^
  "Get-ChildItem -Path dist\* -Include *.zip, *.msi | ForEach-Object {" ^
  "  $hash = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash;" ^
  "  $sizeMb = [math]::Round($_.Length / 1MB, 2);" ^
  "  Write-Host ('Paket   : ' + $_.Name + ' (' + $sizeMb + ' MB)');" ^
  "  Write-Host ('SHA-256 : ' + $hash);" ^
  "  Write-Host '';" ^
  "}"
echo Tum paketler 'dist\' klasoru icerisinde yayinlanmaya hazirdir.
exit /b 0
