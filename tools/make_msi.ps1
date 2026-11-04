param(
    [string]$arch = "x64",
    [string]$version = "1.0.0",
    [string]$outPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($outPath)) {
    $outPath = "dist\Kaiseki-v$version-Setup-$arch.msi"
}

$outFullPath = [System.IO.Path]::GetFullPath($outPath)
$outDir = [System.IO.Path]::GetDirectoryName($outFullPath)
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}
if (Test-Path $outFullPath) { Remove-Item $outFullPath -Force }

$tempCabDir = "temp_cab_$arch"
if (Test-Path $tempCabDir) { Remove-Item $tempCabDir -Recurse -Force }
New-Item -ItemType Directory -Path $tempCabDir | Out-Null

# 1. Paketlenecek Dosyalar
$files = @(
    @{ Id = "f_exe";     Name = "Kaiseki.exe";     Path = "bin\$arch\Kaiseki.exe";     Comp = "c_exe";     Dir = "INSTALLDIR" },
    @{ Id = "f_core";    Name = "KaisekiCore.dll"; Path = "bin\$arch\KaisekiCore.dll"; Comp = "c_core";    Dir = "INSTALLDIR" },
    @{ Id = "f_dex";     Name = "KaisekiDex.dll";  Path = "bin\$arch\KaisekiDex.dll";  Comp = "c_dex";     Dir = "INSTALLDIR" },
    @{ Id = "f_ui";      Name = "KaisekiUI.dll";   Path = "bin\$arch\KaisekiUI.dll";   Comp = "c_ui";      Dir = "INSTALLDIR" },
    @{ Id = "f_adb";     Name = "KaisekiADB.dll";  Path = "bin\$arch\KaisekiADB.dll";  Comp = "c_adb";     Dir = "INSTALLDIR" },
    @{ Id = "f_sec";     Name = "KaisekiSec.dll";  Path = "bin\$arch\KaisekiSec.dll";  Comp = "c_sec";     Dir = "INSTALLDIR" },
    @{ Id = "f_diff";    Name = "KaisekiDiff.dll"; Path = "bin\$arch\KaisekiDiff.dll"; Comp = "c_diff";    Dir = "INSTALLDIR" },
    @{ Id = "f_hex";     Name = "KaisekiHex.dll";  Path = "bin\$arch\KaisekiHex.dll";  Comp = "c_hex";     Dir = "INSTALLDIR" },
    @{ Id = "f_res";     Name = "KaisekiRes.dll";  Path = "bin\$arch\KaisekiRes.dll";  Comp = "c_res";     Dir = "INSTALLDIR" },
    @{ Id = "f_anim";    Name = "KaisekiAnim.dll"; Path = "bin\$arch\KaisekiAnim.dll"; Comp = "c_anim";    Dir = "INSTALLDIR" },
    @{ Id = "f_readme";  Name = "README.md";       Path = "README.md";                 Comp = "c_readme";  Dir = "INSTALLDIR" },
    @{ Id = "f_license"; Name = "LICENSE";         Path = "LICENSE";                   Comp = "c_license"; Dir = "INSTALLDIR" },
    @{ Id = "f_ico";     Name = "Kaiseki.ico";     Path = "Kaiseki\res\Kaiseki.ico";   Comp = "c_ico";     Dir = "RESDIR" },
    @{ Id = "f_logo";    Name = "logo.png";        Path = "Kaiseki\res\logo.png";      Comp = "c_logo";    Dir = "RESDIR" },
    @{ Id = "f_banner";  Name = "banner.png";      Path = "Kaiseki\res\banner.png";    Comp = "c_banner";  Dir = "RESDIR" },
    @{ Id = "f_sbanner"; Name = "setup_banner.png";Path = "Kaiseki\res\setup_banner.png";Comp = "c_sbanner"; Dir = "RESDIR" }
)

if (Test-Path "lang") {
    $langFiles = Get-ChildItem -Path "lang" -Filter "*.xml"
    $lIdx = 1
    foreach ($lf in $langFiles) {
        $files += @{ Id = "f_lang_$lIdx"; Name = $lf.Name; Path = "lang\$($lf.Name)"; Comp = "c_lang_$lIdx"; Dir = "LANGDIR" }
        $lIdx++
    }
}

# 2. Data1.cab Olusturma
$ddfPath = "$tempCabDir\cabinet.ddf"
$ddfContent = @(
    ".Set CabinetNameTemplate=Data1.cab",
    ".Set DiskDirectoryTemplate=$tempCabDir\",
    ".Set CompressionType=LZX",
    ".Set CompressionMemory=21",
    ".Set UniqueFiles=OFF",
    ".Set Cabinet=ON",
    ".Set MaxDiskSize=0"
)
foreach ($f in $files) {
    $ddfContent += "`"$($f.Path)`" $($f.Id)"
}
[System.IO.File]::WriteAllLines($ddfPath, $ddfContent, [System.Text.Encoding]::Default)

& makecab /F $ddfPath | Out-Null
$cabPath = "$tempCabDir\Data1.cab"

# 2.1 License RTF Olusturma (GPLv3)
$licenseText = [System.IO.File]::ReadAllText("LICENSE")
$escaped = $licenseText.Replace('\', '\\').Replace('{', '\{').Replace('}' , '\}')
$escaped = $escaped -replace '(?m)^([ \t]*)(GNU GENERAL PUBLIC LICENSE)([ \t]*)$', '{\qc\b\fs22 $2\par}'
$escaped = $escaped -replace '(?m)^([ \t]*)(Version 3, 29 June 2007)([ \t]*)$', '{\qc\i\fs18 $2\par\par}'
$escaped = $escaped -replace '(?m)^([ \t]*)(Preamble)([ \t]*)$', '{\b\fs20 $2\par}'
$escaped = $escaped -replace '(?m)^([ \t]*)(TERMS AND CONDITIONS.*)$', '{\b\fs20 $2\par}'
$rtfBody = $escaped -replace "`r?`n", "\par`r`n"
$rtfContent = "{\rtf1\ansi\ansicpg1252\deff0{\fonttbl{\f0\fnil\fcharset0 Segoe UI;}}\viewkind4\uc1\pard\f0\fs18 $rtfBody}"
$rtfPath = "$tempCabDir\license.rtf"
[System.IO.File]::WriteAllText($rtfPath, $rtfContent, [System.Text.Encoding]::ASCII)

# 3. Profesyonel GDI+ Gorselleri (493x58 BannerBmp, 493x312 DialogBmp)
Add-Type -AssemblyName System.Drawing

# 3.1 BannerBmp (493x58): Temiz beyaz ust bar + Sagda Kaiseki logosu + Ust mavi vurgu
$bmpBanner = New-Object System.Drawing.Bitmap(493, 58)
$gBanner = [System.Drawing.Graphics]::FromImage($bmpBanner)
$gBanner.Clear([System.Drawing.Color]::White)

# 2px Kaiseki Mavisi Vurgu
$brushAccent = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(37, 99, 235))
$gBanner.FillRectangle($brushAccent, 0, 0, 493, 2)
$brushAccent.Dispose()

# Sagda Kaiseki Logosu (48x48)
$logoPath = "Kaiseki\res\logo.png"
if (Test-Path $logoPath) {
    $imgLogo = [System.Drawing.Image]::FromFile($logoPath)
    $gBanner.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gBanner.DrawImage($imgLogo, (493 - 54), 5, 48, 48)
    $imgLogo.Dispose()
}
$penBorder = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(226, 232, 240), 1)
$gBanner.DrawLine($penBorder, 0, 57, 493, 57)
$penBorder.Dispose()
$gBanner.Dispose()

$bannerBmpPath = "$tempCabDir\Banner.bmp"
$bmpBanner.Save($bannerBmpPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmpBanner.Dispose()

# 3.2 DialogBmp (493x312): Sol panelli Kaiseki afisi + Temiz beyaz icerik alani
$bmpDialog = New-Object System.Drawing.Bitmap(493, 312)
$gDialog = [System.Drawing.Graphics]::FromImage($bmpDialog)
$gDialog.Clear([System.Drawing.Color]::White)

$rectLeft = New-Object System.Drawing.Rectangle(0, 0, 164, 312)
$setupBannerPath = "Kaiseki\res\setup_banner.png"
if (Test-Path $setupBannerPath) {
    $imgSB = [System.Drawing.Image]::FromFile($setupBannerPath)
    $gDialog.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gDialog.DrawImage($imgSB, $rectLeft)
    $imgSB.Dispose()
} else {
    $brushGrad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rectLeft,
        [System.Drawing.Color]::FromArgb(15, 23, 42),
        [System.Drawing.Color]::FromArgb(30, 58, 138),
        [System.Drawing.Drawing2D.LinearGradientMode]::Vertical
    )
    $gDialog.FillRectangle($brushGrad, $rectLeft)
    $brushGrad.Dispose()
}

$penSep = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(226, 232, 240), 1)
$gDialog.DrawLine($penSep, 164, 0, 164, 312)
$penSep.Dispose()
$gDialog.Dispose()

$dialogBmpPath = "$tempCabDir\Dialog.bmp"
$bmpDialog.Save($dialogBmpPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmpDialog.Dispose()

# 4. MSI Veritabani Olusturma ve UTF-8 (65001) ForceCodepage
$wi = New-Object -ComObject WindowsInstaller.Installer
$db = $wi.OpenDatabase($outFullPath, 3)

$idtCodepage = "$tempCabDir\_ForceCodepage.idt"
[System.IO.File]::WriteAllLines($idtCodepage, @("", "", "65001`t_ForceCodepage"), [System.Text.Encoding]::ASCII)
$db.Import([System.IO.Path]::GetFullPath($tempCabDir), "_ForceCodepage.idt")

function ExecSql($db, $sql) {
    $v = $db.OpenView($sql)
    $v.Execute()
    $v.Close()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($v) | Out-Null
}

function ExecParam($db, $wi, $sql, [array]$params) {
    $v = $db.OpenView($sql)
    $rec = $wi.CreateRecord($params.Count)
    for ($i = 0; $i -lt $params.Count; $i++) {
        $val = $params[$i]
        if ($val -is [int] -or $val -is [long]) {
            $rec.IntegerData($i + 1) = [int]$val
        } else {
            $rec.StringData($i + 1) = [string]$val
        }
    }
    $v.Execute($rec)
    $v.Close()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($rec) | Out-Null
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($v) | Out-Null
}

# Tablolar
ExecSql $db "CREATE TABLE Component (Component CHAR(72) NOT NULL, ComponentId CHAR(38), Directory_ CHAR(72) NOT NULL, Attributes SHORT NOT NULL, Condition CHAR(255), KeyPath CHAR(72) PRIMARY KEY Component)"
ExecSql $db "CREATE TABLE Directory (Directory CHAR(72) NOT NULL, Directory_Parent CHAR(72), DefaultDir CHAR(255) NOT NULL PRIMARY KEY Directory)"
ExecSql $db "CREATE TABLE Feature (Feature CHAR(38) NOT NULL, Feature_Parent CHAR(38), Title CHAR(64), Description CHAR(255), Display SHORT, Level SHORT NOT NULL, Directory_ CHAR(72), Attributes SHORT NOT NULL PRIMARY KEY Feature)"
ExecSql $db "CREATE TABLE FeatureComponents (Feature_ CHAR(38) NOT NULL, Component_ CHAR(72) NOT NULL PRIMARY KEY Feature_, Component_)"
ExecSql $db "CREATE TABLE File (File CHAR(72) NOT NULL, Component_ CHAR(72) NOT NULL, FileName CHAR(255) NOT NULL, FileSize LONG NOT NULL, Version CHAR(72), Language CHAR(20), Attributes SHORT, Sequence SHORT NOT NULL PRIMARY KEY File)"
ExecSql $db "CREATE TABLE Media (DiskId SHORT NOT NULL, LastSequence SHORT NOT NULL, DiskPrompt CHAR(64), Cabinet CHAR(255), VolumeLabel CHAR(32), Source CHAR(72) PRIMARY KEY DiskId)"
ExecSql $db "CREATE TABLE Registry (Registry CHAR(72) NOT NULL, Root SHORT NOT NULL, ``Key`` CHAR(255) NOT NULL, Name CHAR(255), Value CHAR(0), Component_ CHAR(72) NOT NULL PRIMARY KEY Registry)"
ExecSql $db "CREATE TABLE CustomAction (Action CHAR(72) NOT NULL, Type SHORT NOT NULL, Source CHAR(72), Target CHAR(255), ExtendedType LONG PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE Binary (Name CHAR(72) NOT NULL, Data OBJECT NOT NULL PRIMARY KEY Name)"
ExecSql $db "CREATE TABLE TextStyle (TextStyle CHAR(72) NOT NULL, FaceName CHAR(32) NOT NULL, Size SHORT NOT NULL, Color LONG, StyleBits SHORT PRIMARY KEY TextStyle)"
ExecSql $db "CREATE TABLE Dialog (Dialog CHAR(72) NOT NULL, HCentering SHORT NOT NULL, VCentering SHORT NOT NULL, Width SHORT NOT NULL, Height SHORT NOT NULL, Attributes LONG NOT NULL, Title CHAR(128), Control_First CHAR(72) NOT NULL, Control_Default CHAR(72), Control_Cancel CHAR(72) PRIMARY KEY Dialog)"
ExecSql $db "CREATE TABLE Control (Dialog_ CHAR(72) NOT NULL, Control CHAR(72) NOT NULL, Type CHAR(20) NOT NULL, X SHORT NOT NULL, Y SHORT NOT NULL, Width SHORT NOT NULL, Height SHORT NOT NULL, Attributes LONG NOT NULL, Property CHAR(50), Text CHAR(0), Control_Next CHAR(72), Help CHAR(50) PRIMARY KEY Dialog_, Control)"
ExecSql $db "CREATE TABLE ControlEvent (Dialog_ CHAR(72) NOT NULL, Control_ CHAR(72) NOT NULL, Event CHAR(50) NOT NULL, Argument CHAR(255) NOT NULL, Condition CHAR(255), Ordering SHORT PRIMARY KEY Dialog_, Control_, Event, Argument, Condition)"
ExecSql $db "CREATE TABLE ControlCondition (Dialog_ CHAR(72) NOT NULL, Control_ CHAR(72) NOT NULL, Action CHAR(50) NOT NULL, Condition CHAR(255) NOT NULL PRIMARY KEY Dialog_, Control_, Action, Condition)"
ExecSql $db "CREATE TABLE EventMapping (Dialog_ CHAR(72) NOT NULL, Control_ CHAR(72) NOT NULL, Event CHAR(50) NOT NULL, Attribute CHAR(50) NOT NULL PRIMARY KEY Dialog_, Control_, Event)"
ExecSql $db "CREATE TABLE ComboBox (Property CHAR(72) NOT NULL, ``Order`` SHORT NOT NULL, Value CHAR(64) NOT NULL, Text CHAR(64) PRIMARY KEY Property, ``Order``)"
ExecSql $db "CREATE TABLE Upgrade (UpgradeCode CHAR(38) NOT NULL, VersionMin CHAR(20), VersionMax CHAR(20), Language CHAR(255), Attributes LONG NOT NULL, Remove CHAR(255), ActionProperty CHAR(72) NOT NULL PRIMARY KEY UpgradeCode, VersionMin, VersionMax, Language, Attributes)"
ExecSql $db "CREATE TABLE Property (Property CHAR(72) NOT NULL, Value CHAR(0) NOT NULL PRIMARY KEY Property)"
ExecSql $db "CREATE TABLE Shortcut (Shortcut CHAR(72) NOT NULL, Directory_ CHAR(72) NOT NULL, Name CHAR(128) NOT NULL, Component_ CHAR(72) NOT NULL, Target CHAR(72) NOT NULL, Arguments CHAR(255), Description CHAR(255), Hotkey SHORT, Icon_ CHAR(72), IconIndex SHORT, ShowCmd SHORT, WkDir CHAR(72) PRIMARY KEY Shortcut)"
ExecSql $db "CREATE TABLE Icon (Name CHAR(72) NOT NULL, Data OBJECT NOT NULL PRIMARY KEY Name)"
ExecSql $db "CREATE TABLE InstallExecuteSequence (Action CHAR(72) NOT NULL, Condition CHAR(255), Sequence SHORT PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE AdminExecuteSequence (Action CHAR(72) NOT NULL, Condition CHAR(255), Sequence SHORT PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE AdvtExecuteSequence (Action CHAR(72) NOT NULL, Condition CHAR(255), Sequence SHORT PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE InstallUISequence (Action CHAR(72) NOT NULL, Condition CHAR(255), Sequence SHORT PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE AdminUISequence (Action CHAR(72) NOT NULL, Condition CHAR(255), Sequence SHORT PRIMARY KEY Action)"
ExecSql $db "CREATE TABLE CreateFolder (Directory_ CHAR(72) NOT NULL, Component_ CHAR(72) NOT NULL PRIMARY KEY Directory_, Component_)"
ExecSql $db "CREATE TABLE RemoveFile (FileKey CHAR(72) NOT NULL, Component_ CHAR(72) NOT NULL, FileName CHAR(255), DirProperty CHAR(72) NOT NULL, InstallMode SHORT NOT NULL PRIMARY KEY FileKey)"

# 5. Klasorler
$progFiles = if ($arch -eq "x64") { "ProgramFiles64Folder" } else { "ProgramFilesFolder" }
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('TARGETDIR', '', 'SourceDir')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('$progFiles', 'TARGETDIR', '.')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('INSTALLDIR', '$progFiles', 'Kaiseki')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('RESDIR', 'INSTALLDIR', 'res')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('LANGDIR', 'INSTALLDIR', 'lang')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('ProgramMenuFolder', 'TARGETDIR', '.')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('MenuDir', 'ProgramMenuFolder', 'Kaiseki')"
ExecSql $db "INSERT INTO Directory (Directory, Directory_Parent, DefaultDir) VALUES ('DesktopFolder', 'TARGETDIR', '.')"

# 6. Feature & Components
ExecParam $db $wi "INSERT INTO Feature (Feature, Feature_Parent, Title, Description, Display, Level, Directory_, Attributes) VALUES (?, ?, ?, ?, ?, ?, ?, ?)" @("MainFeature", "", "Kaiseki", "Kaiseki APK Analysis & Reverse Engineering Suite", 1, 1, "INSTALLDIR", 0)

$compAttr = if ($arch -eq "x64") { 256 } else { 0 }
$seq = 1
foreach ($f in $files) {
    $cId = $f.Comp
    $dir = $f.Dir
    $guid = [System.Guid]::NewGuid().ToString("B").ToUpper()
    $fName = $f.Name
    $fId = $f.Id
    $fSize = [int](Get-Item $f.Path).Length

    ExecParam $db $wi "INSERT INTO Component (Component, ComponentId, Directory_, Attributes, Condition, KeyPath) VALUES (?, ?, ?, ?, ?, ?)" @($cId, $guid, $dir, $compAttr, "", $fId)
    ExecParam $db $wi "INSERT INTO FeatureComponents (Feature_, Component_) VALUES (?, ?)" @("MainFeature", $cId)
    ExecParam $db $wi "INSERT INTO File (File, Component_, FileName, FileSize, Version, Language, Attributes, Sequence) VALUES (?, ?, ?, ?, ?, ?, ?, ?)" @($fId, $cId, $fName, $fSize, "", "", 512, $seq)
    $seq++
}

# Registry & Kisayollar (Root = -1: Adaptive Per-Machine / Per-User)
$regGuid = [System.Guid]::NewGuid().ToString("B").ToUpper()
ExecParam $db $wi "INSERT INTO Component (Component, ComponentId, Directory_, Attributes, Condition, KeyPath) VALUES (?, ?, ?, ?, ?, ?)" @("c_reg_lang", $regGuid, "INSTALLDIR", ($compAttr + 4), "", "reg_uilang")
ExecParam $db $wi "INSERT INTO FeatureComponents (Feature_, Component_) VALUES (?, ?)" @("MainFeature", "c_reg_lang")
ExecParam $db $wi "INSERT INTO Registry (Registry, Root, ``Key``, Name, Value, Component_) VALUES (?, ?, ?, ?, ?, ?)" @("reg_uilang", -1, "Software\Kaiseki", "Language", "[UILANG]", "c_reg_lang")
ExecParam $db $wi "INSERT INTO Registry (Registry, Root, ``Key``, Name, Value, Component_) VALUES (?, ?, ?, ?, ?, ?)" @("reg_instdir", -1, "Software\Kaiseki", "InstallDir", "[INSTALLDIR]", "c_reg_lang")

ExecSql $db "INSERT INTO CreateFolder (Directory_, Component_) VALUES ('MenuDir', 'c_exe')"
ExecSql $db "INSERT INTO RemoveFile (FileKey, Component_, FileName, DirProperty, InstallMode) VALUES ('rem_menudir', 'c_exe', '', 'MenuDir', 2)"
ExecParam $db $wi "INSERT INTO Shortcut (Shortcut, Directory_, Name, Component_, Target, Arguments, Description, Hotkey, Icon_, IconIndex, ShowCmd, WkDir) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" @("sc_menu", "MenuDir", "Kaiseki", "c_exe", "[#f_exe]", "", "Kaiseki APK Analysis Suite", 0, "KaisekiIco", 0, 1, "INSTALLDIR")

$deskGuid = [System.Guid]::NewGuid().ToString("B").ToUpper()
ExecParam $db $wi "INSERT INTO Component (Component, ComponentId, Directory_, Attributes, Condition, KeyPath) VALUES (?, ?, ?, ?, ?, ?)" @("c_desk_sc", $deskGuid, "DesktopFolder", ($compAttr + 4), "DESKTOP_SHORTCUT = `"1`"", "reg_desk_sc")
ExecParam $db $wi "INSERT INTO FeatureComponents (Feature_, Component_) VALUES (?, ?)" @("MainFeature", "c_desk_sc")
ExecParam $db $wi "INSERT INTO Registry (Registry, Root, ``Key``, Name, Value, Component_) VALUES (?, ?, ?, ?, ?, ?)" @("reg_desk_sc", -1, "Software\Kaiseki", "DesktopShortcut", "1", "c_desk_sc")
ExecParam $db $wi "INSERT INTO Shortcut (Shortcut, Directory_, Name, Component_, Target, Arguments, Description, Hotkey, Icon_, IconIndex, ShowCmd, WkDir) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" @("sc_desk", "DesktopFolder", "Kaiseki", "c_desk_sc", "[#f_exe]", "", "Kaiseki APK Analysis Suite", 0, "KaisekiIco", 0, 1, "INSTALLDIR")

$totalFiles = $files.Count
ExecSql $db "INSERT INTO Media (DiskId, LastSequence, DiskPrompt, Cabinet, VolumeLabel, Source) VALUES (1, $totalFiles, '1', '#Data1.cab', 'DISK1', '')"

# 7. Coklu Dil Sozlugu (EN = Default, TR, JA, DE)
$i18n = @{
    "EN" = @{
        "TXT_BTN_NEXT"       = "Next >"
        "TXT_BTN_BACK"       = "< Back"
        "TXT_BTN_CANCEL"     = "Cancel"
        "TXT_BTN_INSTALL"    = "Install"
        "TXT_BTN_REMOVE"     = "Uninstall"
        "TXT_BTN_REPAIR"     = "Repair"
        "TXT_BTN_FINISH"     = "Finish"
        "TXT_BTN_BROWSE"     = "Browse..."
        "TXT_BTN_OK"         = "OK"
        "TXT_BTN_UP"         = "Up"
        "TXT_BTN_YES"        = "Yes"
        "TXT_BTN_NO"         = "No"
        "TXT_WELCOME_TITLE"  = "Welcome to the Kaiseki v$version Setup Wizard"
        "TXT_WELCOME_BODY"   = "This wizard will install the Kaiseki APK Analysis & Reverse Engineering Suite on your computer.`r`n`r`nIt is recommended that you close all other applications before continuing.`r`n`r`nClick Next to continue."
        "TXT_LICENSE_TITLE"  = "End-User License Agreement"
        "TXT_LICENSE_SUB"    = "Please review the license terms before installing Kaiseki."
        "TXT_LICENSE_ACCEPT" = "I accept the terms in the License Agreement (GNU GPLv3)"
        "TXT_DIR_TITLE"      = "Destination Folder"
        "TXT_DIR_SUB"        = "Select the installation folder and shortcuts for Kaiseki."
        "TXT_DIR_PROMPT"     = "Kaiseki will be installed in the following folder. To install in a different folder, click Browse or type a new path:"
        "TXT_DESK_SC"        = "Create a desktop shortcut"
        "TXT_BROWSE_TITLE"   = "Browse Destination Folder"
        "TXT_BROWSE_SUB"     = "Select the target folder where Kaiseki will be installed."
        "TXT_BROWSE_LOOKIN"  = "Look in:"
        "TXT_READY_TITLE"    = "Ready to Install Kaiseki"
        "TXT_READY_SUB"      = "The wizard is ready to begin the installation."
        "TXT_READY_BODY"     = "Click Install to begin the installation.`r`n`r`nDestination Folder:`r`n[INSTALLDIR]`r`n`r`nClick Back to review or change any settings."
        "TXT_MAINT_TITLE"    = "Kaiseki Maintenance & Setup"
        "TXT_MAINT_SUB"      = "Modify, repair, or remove the existing Kaiseki installation."
        "TXT_MAINT_BODY"     = "Kaiseki v$version is already installed on your computer. Select the operation you wish to perform:"
        "TXT_MAINT_REPAIR_D" = "Repairs missing or corrupted files, shortcuts, and registry entries."
        "TXT_MAINT_REMOVE_D" = "Completely removes Kaiseki from your computer."
        "TXT_VREM_TITLE"     = "Uninstall Kaiseki"
        "TXT_VREM_SUB"       = "You have chosen to remove the program from your system."
        "TXT_VREM_BODY"      = "Click Uninstall to remove Kaiseki v$version from your computer. If you want to review or change any settings, click Back."
        "TXT_PROG_TITLE"     = "Installing Kaiseki..."
        "TXT_PROG_SUB"       = "Please wait while the Setup Wizard configures Kaiseki."
        "TXT_PROG_BODY"      = "Copying files, creating shortcuts, and configuring system settings. This may take a few moments."
        "TXT_EXIT_TITLE"     = "Completing the Kaiseki Setup Wizard"
        "TXT_EXIT_BODY"      = "The Setup Wizard has successfully completed the operation on your computer.`r`n`r`nClick Finish to exit the wizard."
        "TXT_LAUNCH_APP"     = "Launch Kaiseki when setup exits"
        "TXT_CANCEL_PROMPT"  = "Are you sure you want to cancel the Kaiseki setup?"
    }
    "TR" = @{
        "TXT_BTN_NEXT"       = "İleri >"
        "TXT_BTN_BACK"       = "< Geri"
        "TXT_BTN_CANCEL"     = "İptal"
        "TXT_BTN_INSTALL"    = "Yükle"
        "TXT_BTN_REMOVE"     = "Kaldır"
        "TXT_BTN_REPAIR"     = "Onar"
        "TXT_BTN_FINISH"     = "Son"
        "TXT_BTN_BROWSE"     = "Gözat..."
        "TXT_BTN_OK"         = "Tamam"
        "TXT_BTN_UP"         = "Yukarı"
        "TXT_BTN_YES"        = "Evet"
        "TXT_BTN_NO"         = "Hayır"
        "TXT_WELCOME_TITLE"  = "Kaiseki v$version Kurulum Sihirbazına Hoş Geldiniz"
        "TXT_WELCOME_BODY"   = "Bu sihirbaz Kaiseki APK Çözümleme ve Tersine Mühendislik Paketini bilgisayarınıza kuracaktır.`r`n`r`nDevam etmeden önce diğer tüm uygulamaları kapatmanız önerilir.`r`n`r`nDevam etmek için İleri'ye tıklayın."
        "TXT_LICENSE_TITLE"  = "Son Kullanıcı Lisans Sözleşmesi"
        "TXT_LICENSE_SUB"    = "Kaiseki kurulumuna geçmeden önce lütfen lisans koşullarını inceleyiniz."
        "TXT_LICENSE_ACCEPT" = "Lisans Sözleşmesi koşullarını kabul ediyorum (GNU GPLv3)"
        "TXT_DIR_TITLE"      = "Hedef Klasör Seçimi"
        "TXT_DIR_SUB"        = "Kaiseki için kurulum klasörünü ve kısayol tercihlerini belirleyin."
        "TXT_DIR_PROMPT"     = "Kaiseki aşağıdaki klasöre kurulacaktır. Farklı bir klasöre kurmak için Gözat'a tıklayın veya yeni bir yol girin:"
        "TXT_DESK_SC"        = "Masaüstü kısayolu oluştur"
        "TXT_BROWSE_TITLE"   = "Hedef Klasöre Gözat"
        "TXT_BROWSE_SUB"     = "Kaiseki uygulamasının kurulacağı hedef klasörü seçin."
        "TXT_BROWSE_LOOKIN"  = "Bakılacak yer:"
        "TXT_READY_TITLE"    = "Kaiseki Kurulumuna Hazır"
        "TXT_READY_SUB"      = "Sihirbaz kurulumu başlatmaya hazır."
        "TXT_READY_BODY"     = "Kurulumu başlatmak için Yükle butonuna tıklayın.`r`n`r`nHedef Klasör:`r`n[INSTALLDIR]`r`n`r`nAyarları gözden geçirmek veya değiştirmek için Geri'ye tıklayın."
        "TXT_MAINT_TITLE"    = "Kaiseki Bakım ve Kaldırma"
        "TXT_MAINT_SUB"      = "Mevcut Kaiseki kurulumunu onarın veya bilgisayarınızdan kaldırın."
        "TXT_MAINT_BODY"     = "Kaiseki v$version bilgisayarınızda zaten kurulu. Yapmak istediğiniz işlemi seçin:"
        "TXT_MAINT_REPAIR_D" = "Eksik veya bozulmuş dosyaları, kısayolları ve kayıt defteri girdilerini onarır."
        "TXT_MAINT_REMOVE_D" = "Kaiseki uygulamasını bilgisayarınızdan tamamen kaldırır."
        "TXT_VREM_TITLE"     = "Kaiseki Uygulamasını Kaldır"
        "TXT_VREM_SUB"       = "Programı sisteminizden kaldırmayı seçtiniz."
        "TXT_VREM_BODY"      = "Kaiseki v$version paketini bilgisayarınızdan kaldırmak için Kaldır butonuna tıklayın."
        "TXT_PROG_TITLE"     = "Kaiseki Kuruluyor..."
        "TXT_PROG_SUB"       = "Lütfen Kurulum Sihirbazı Kaiseki uygulamasını yapılandırırken bekleyin."
        "TXT_PROG_BODY"      = "Dosyalar kopyalanıyor, kısayollar oluşturuluyor ve sistem ayarları yapılıyor. Bu işlem biraz zaman alabilir."
        "TXT_EXIT_TITLE"     = "Kaiseki Kurulum Sihirbazı Tamamlandı"
        "TXT_EXIT_BODY"      = "Kurulum sihirbazı işlemi başarıyla tamamladı.`r`n`r`nSihirbazdan çıkmak için Son butonuna tıklayın."
        "TXT_LAUNCH_APP"     = "Kurulum tamamlandığında Kaiseki'yi başlat"
        "TXT_CANCEL_PROMPT"  = "Kaiseki kurulumunu iptal etmek istediğinizden emin misiniz?"
    }
    "JA" = @{
        "TXT_BTN_NEXT"       = "次へ >"
        "TXT_BTN_BACK"       = "< 戻る"
        "TXT_BTN_CANCEL"     = "キャンセル"
        "TXT_BTN_INSTALL"    = "インストール"
        "TXT_BTN_REMOVE"     = "アンインストール"
        "TXT_BTN_REPAIR"     = "修復"
        "TXT_BTN_FINISH"     = "完了"
        "TXT_BTN_BROWSE"     = "参照..."
        "TXT_BTN_OK"         = "OK"
        "TXT_BTN_UP"         = "上へ"
        "TXT_BTN_YES"        = "はい"
        "TXT_BTN_NO"         = "いいえ"
        "TXT_WELCOME_TITLE"  = "Kaiseki v$version セットアップへようこそ"
        "TXT_WELCOME_BODY"   = "このウィザードは、Kaiseki APK 解析スイートをお使いのコンピューターにインストールします。`r`n`r`n続行する前に他のすべてのアプリケーションを閉じることをお勧めします。`r`n`r`n[次へ] をクリックして続行してください。"
        "TXT_LICENSE_TITLE"  = "使用許諾契約書"
        "TXT_LICENSE_SUB"    = "Kaiseki をインストールする前に、ライセンス条項をご確認ください。"
        "TXT_LICENSE_ACCEPT" = "使用許諾契約（GNU GPLv3）の条項に同意します"
        "TXT_DIR_TITLE"      = "インストール先フォルダーの選択"
        "TXT_DIR_SUB"        = "Kaiseki のインストール先とショートカットを設定します。"
        "TXT_DIR_PROMPT"     = "Kaiseki は次のフォルダーにインストールされます。別のフォルダーにインストールする場合は [参照] をクリックしてください:"
        "TXT_DESK_SC"        = "デスクトップ ショートカットを作成する"
        "TXT_BROWSE_TITLE"   = "フォルダーの参照"
        "TXT_BROWSE_SUB"     = "インストール先のフォルダーを選択してください。"
        "TXT_BROWSE_LOOKIN"  = "検索先:"
        "TXT_READY_TITLE"    = "インストールの準備完了"
        "TXT_READY_SUB"      = "ウィザードはインストールの開始準備が整いました。"
        "TXT_READY_BODY"     = "[インストール] をクリックして開始してください。`r`n`r`nインストール先:`r`n[INSTALLDIR]`r`n`r`n設定を変更する場合は [戻る] をクリックしてください。"
        "TXT_MAINT_TITLE"    = "Kaiseki の保守とセットアップ"
        "TXT_MAINT_SUB"      = "既存の Kaiseki インストールを修復または削除します。"
        "TXT_MAINT_BODY"     = "Kaiseki v$version は既にインストールされています。実行する操作を選択してください:"
        "TXT_MAINT_REPAIR_D" = "破損したファイル、ショートカット、レジストリを修復します。"
        "TXT_MAINT_REMOVE_D" = "コンピューターから Kaiseki を完全に削除します。"
        "TXT_VREM_TITLE"     = "Kaiseki のアンインストール"
        "TXT_VREM_SUB"       = "プログラムをシステムから削除します。"
        "TXT_VREM_BODY"      = "Kaiseki v$version を削除するには [アンインストール] をクリックしてください。"
        "TXT_PROG_TITLE"     = "Kaiseki をインストール中..."
        "TXT_PROG_SUB"       = "セットアップの構成が完了するまでお待ちください。"
        "TXT_PROG_BODY"      = "ファイルをコピーし、ショートカットと設定を構成しています。"
        "TXT_EXIT_TITLE"     = "Kaiseki セットアップ完了"
        "TXT_EXIT_BODY"      = "セットアップ ウィザードが正常に完了しました。`r`n`r`n[完了] をクリックして終了してください。"
        "TXT_LAUNCH_APP"     = "セットアップ終了時に Kaiseki を起動する"
        "TXT_CANCEL_PROMPT"  = "Kaiseki セットアップを中止してもよろしいですか？"
    }
    "DE" = @{
        "TXT_BTN_NEXT"       = "Weiter >"
        "TXT_BTN_BACK"       = "< Zurück"
        "TXT_BTN_CANCEL"     = "Abbrechen"
        "TXT_BTN_INSTALL"    = "Installieren"
        "TXT_BTN_REMOVE"     = "Entfernen"
        "TXT_BTN_REPAIR"     = "Reparieren"
        "TXT_BTN_FINISH"     = "Fertig"
        "TXT_BTN_BROWSE"     = "Durchsuchen..."
        "TXT_BTN_OK"         = "OK"
        "TXT_BTN_UP"         = "Aufwärts"
        "TXT_BTN_YES"        = "Ja"
        "TXT_BTN_NO"         = "Nein"
        "TXT_WELCOME_TITLE"  = "Willkommen beim Kaiseki v$version Setup-Assistenten"
        "TXT_WELCOME_BODY"   = "Dieser Assistent installiert die Kaiseki APK-Analyse- & Reverse-Engineering-Suite auf Ihrem Computer.`r`n`r`nEs wird empfohlen, alle anderen Anwendungen zu schließen, bevor Sie fortfahren.`r`n`r`nKlicken Sie auf Weiter, um fortzufahren."
        "TXT_LICENSE_TITLE"  = "Endbenutzer-Lizenzvereinbarung"
        "TXT_LICENSE_SUB"    = "Bitte lesen Sie die Lizenzbedingungen vor der Installation."
        "TXT_LICENSE_ACCEPT" = "Ich akzeptiere die Bedingungen der Lizenzvereinbarung (GNU GPLv3)"
        "TXT_DIR_TITLE"      = "Zielordner auswählen"
        "TXT_DIR_SUB"        = "Wählen Sie den Installationsordner und die Verknüpfungen für Kaiseki."
        "TXT_DIR_PROMPT"     = "Kaiseki wird im folgenden Ordner installiert. Klicken Sie auf Durchsuchen, um einen anderen Ordner auszuwählen:"
        "TXT_DESK_SC"        = "Desktop-Verknüpfung erstellen"
        "TXT_BROWSE_TITLE"   = "Zielordner durchsuchen"
        "TXT_BROWSE_SUB"     = "Wählen Sie den Zielordner für die Installation."
        "TXT_BROWSE_LOOKIN"  = "Suchen in:"
        "TXT_READY_TITLE"    = "Bereit zur Installation von Kaiseki"
        "TXT_READY_SUB"      = "Der Assistent ist bereit, mit der Installation zu beginnen."
        "TXT_READY_BODY"     = "Klicken Sie auf Installieren, um zu beginnen.`r`n`r`nZielordner:`r`n[INSTALLDIR]`r`n`r`nKlicken Sie auf Zurück, um Ihre Einstellungen zu überprüfen."
        "TXT_MAINT_TITLE"    = "Kaiseki Wartung & Deinstallation"
        "TXT_MAINT_SUB"      = "Bestehende Kaiseki-Installation reparieren oder entfernen."
        "TXT_MAINT_BODY"     = "Kaiseki v$version ist bereits installiert. Wählen Sie die gewünschte Aktion:"
        "TXT_MAINT_REPAIR_D" = "Repariert fehlende oder beschädigte Dateien, Verknüpfungen und Registrierungseinträge."
        "TXT_MAINT_REMOVE_D" = "Entfernt Kaiseki vollständig von Ihrem Computer."
        "TXT_VREM_TITLE"     = "Kaiseki deinstallieren"
        "TXT_VREM_SUB"       = "Sie haben gewählt, das Programm von Ihrem System zu entfernen."
        "TXT_VREM_BODY"      = "Klicken Sie auf Entfernen, um Kaiseki v$version von Ihrem Computer zu deinstallieren."
        "TXT_PROG_TITLE"     = "Kaiseki wird installiert..."
        "TXT_PROG_SUB"       = "Bitte warten Sie, während Kaiseki konfiguriert wird."
        "TXT_PROG_BODY"      = "Dateien werden kopiert, Verknüpfungen erstellt und Systemeinstellungen konfiguriert."
        "TXT_EXIT_TITLE"     = "Kaiseki Setup-Assistent abgeschlossen"
        "TXT_EXIT_BODY"      = "Der Setup-Assistent wurde erfolgreich abgeschlossen.`r`n`r`nKlicken Sie auf Fertig, um das Setup zu beenden."
        "TXT_LAUNCH_APP"     = "Kaiseki nach dem Beenden starten"
        "TXT_CANCEL_PROMPT"  = "Möchten Sie das Kaiseki-Setup wirklich abbrechen?"
    }
}

$numericVersion = ($version -replace '-.*$', '')
if ($numericVersion -notmatch '^\d+(\.\d+){1,3}$') { $numericVersion = "1.0.0" }

$prodCode = [System.Guid]::NewGuid().ToString("B").ToUpper()
$upgCode  = "{B987C341-8A10-4F29-8752-1983EF0192A4}"
ExecParam $db $wi "INSERT INTO Upgrade (UpgradeCode, VersionMin, VersionMax, Language, Attributes, Remove, ActionProperty) VALUES (?, ?, ?, ?, ?, ?, ?)" @($upgCode, "", $numericVersion, "", 257, "", "PREVIOUSVERSIONSINSTALLED")

$baseProps = @{
    "ProductName"            = "Kaiseki (Beta)"
    "ProductVersion"         = $numericVersion
    "DisplayVersion"         = $version
    "ProductCode"            = $prodCode
    "UpgradeCode"            = $upgCode
    "ProductLanguage"        = "1033"
    "Manufacturer"           = "anonimbiri"
    "ALLUSERS"               = "1"
    "MSIINSTALLPERUSER"      = "0"
    "ARPPRODUCTICON"         = "KaisekiIco"
    "ARPNOMODIFY"            = "1"
    "ARPHELPLINK"            = "https://github.com/GameSketchers/Kaiseki"
    "DefaultUIFont"          = "DlgFontNormal"
    "LAUNCHAPP"              = "1"
    "DESKTOP_SHORTCUT"       = "1"
    "AGREELICENSE"           = "1"
    "UILANG"                 = "EN"
    "WIXUI_INSTALLDIR"       = "INSTALLDIR"
    "BrowseProperty"         = "INSTALLDIR"
    "SecureCustomProperties" = "PREVIOUSVERSIONSINSTALLED"
}
foreach ($kv in $baseProps.GetEnumerator()) {
    ExecParam $db $wi "INSERT INTO Property (Property, Value) VALUES (?, ?)" @($kv.Key, $kv.Value)
}

# Varsayilan olarak Ingilizce (EN) metinlerini Property tablosuna yukle
foreach ($kv in $i18n["EN"].GetEnumerator()) {
    ExecParam $db $wi "INSERT INTO Property (Property, Value) VALUES (?, ?)" @($kv.Key, $kv.Value)
}

# Her dil icin Type 51 CustomAction (Property Guncelleyici) olustur
foreach ($langKey in @("EN", "TR", "JA", "DE")) {
    $dict = $i18n[$langKey]
    foreach ($kv in $dict.GetEnumerator()) {
        $caName = "CA_$($langKey)_$($kv.Key)"
        ExecParam $db $wi "INSERT INTO CustomAction (Action, Type, Source, Target, ExtendedType) VALUES (?, ?, ?, ?, ?)" @($caName, 51, $kv.Key, $kv.Value, 0)
    }
}

ExecSql $db "INSERT INTO CustomAction (Action, Type, Source, Target, ExtendedType) VALUES ('LaunchApp', 210, 'f_exe', '', 0)"

# ComboBox Dil Secenekleri (EN Default)
ExecParam $db $wi "INSERT INTO ComboBox (Property, ``Order``, Value, Text) VALUES (?, ?, ?, ?)" @("UILANG", 1, "EN", "English")
ExecParam $db $wi "INSERT INTO ComboBox (Property, ``Order``, Value, Text) VALUES (?, ?, ?, ?)" @("UILANG", 2, "TR", "Türkçe")
ExecParam $db $wi "INSERT INTO ComboBox (Property, ``Order``, Value, Text) VALUES (?, ?, ?, ?)" @("UILANG", 3, "JA", "日本語")
ExecParam $db $wi "INSERT INTO ComboBox (Property, ``Order``, Value, Text) VALUES (?, ?, ?, ?)" @("UILANG", 4, "DE", "Deutsch")

# Yazi Tipleri
ExecSql $db "INSERT INTO TextStyle (TextStyle, FaceName, Size, Color, StyleBits) VALUES ('DlgFontNormal', 'Segoe UI', 9, 1973790, 0)"
ExecSql $db "INSERT INTO TextStyle (TextStyle, FaceName, Size, Color, StyleBits) VALUES ('DlgFontBold', 'Segoe UI', 9, 1973790, 1)"
ExecSql $db "INSERT INTO TextStyle (TextStyle, FaceName, Size, Color, StyleBits) VALUES ('DlgFontSub', 'Segoe UI', 8, 6710886, 0)"
ExecSql $db "INSERT INTO TextStyle (TextStyle, FaceName, Size, Color, StyleBits) VALUES ('DlgFontHeader', 'Segoe UI', 10, 1381653, 1)"
ExecSql $db "INSERT INTO TextStyle (TextStyle, FaceName, Size, Color, StyleBits) VALUES ('DlgFontTitle', 'Segoe UI', 12, 1381653, 1)"

# 8. Kurulum & UI Siralamasi
$seqs = @(
    @("FindRelatedProducts", "", 200),
    @("CostInitialize", "", 800),
    @("FileCost", "", 900),
    @("CostFinalize", "", 1000),
    @("InstallValidate", "", 1400),
    @("RemoveExistingProducts", "", 1425),
    @("InstallInitialize", "", 1500),
    @("ProcessComponents", "", 1600),
    @("UnpublishFeatures", "", 1800),
    @("RemoveRegistryValues", "", 2600),
    @("RemoveShortcuts", "", 3200),
    @("RemoveFiles", "", 3500),
    @("RemoveFolders", "", 3600),
    @("CreateFolders", "", 3700),
    @("InstallFiles", "", 4000),
    @("CreateShortcuts", "", 4500),
    @("WriteRegistryValues", "", 5000),
    @("RegisterUser", "", 6000),
    @("RegisterProduct", "", 6100),
    @("PublishFeatures", "", 6300),
    @("PublishProduct", "", 6400),
    @("InstallFinalize", "", 6600)
)
foreach ($s in $seqs) {
    ExecParam $db $wi "INSERT INTO InstallExecuteSequence (Action, Condition, Sequence) VALUES (?, ?, ?)" @($s[0], $s[1], [int]$s[2])
}

$uiSeqs = @(
    @("FindRelatedProducts", "", 200),
    @("CostInitialize", "", 800),
    @("FileCost", "", 900),
    @("CostFinalize", "", 1000),
    @("LangDlg", "", 1050),
    @("ProgressDlg", "", 1150),
    @("ExecuteAction", "", 1200),
    @("ExitDialog", "", -1)
)
foreach ($s in $uiSeqs) {
    ExecParam $db $wi "INSERT INTO InstallUISequence (Action, Condition, Sequence) VALUES (?, ?, ?)" @($s[0], $s[1], [int]$s[2])
}

# 9. Diyaloglar (Tam Tab Ring Dogrulamali)
function Add-DialogWithRing($db, $wi, $dlgName, $w, $h, $attr, $title, $firstCtrl, $defCtrl, $cancelCtrl, [array]$ctrls) {
    ExecParam $db $wi "INSERT INTO Dialog (Dialog, HCentering, VCentering, Width, Height, Attributes, Title, Control_First, Control_Default, Control_Cancel) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" @(
        $dlgName, 50, 50, $w, $h, $attr, $title, $firstCtrl, $defCtrl, $cancelCtrl
    )
    for ($i = 0; $i -lt $ctrls.Count; $i++) {
        $c = $ctrls[$i]
        $nextCtrl = $ctrls[($i + 1) % $ctrls.Count].Id
        ExecParam $db $wi "INSERT INTO Control (Dialog_, Control, Type, X, Y, Width, Height, Attributes, Property, Text, Control_Next, Help) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" @(
            $dlgName, $c.Id, $c.Type, [int]$c.X, [int]$c.Y, [int]$c.W, [int]$c.H, [int]$c.Attr, [string]$c.Prop, [string]$c.Text, $nextCtrl, ""
        )
    }
}

# 1. LangDlg (Klasik Dil Secim Penceresi)
Add-DialogWithRing $db $wi "LangDlg" 260 100 3 "Select Setup Language" "LangCombo" "OkBtn" "OkBtn" @(
    @{ Id="LangCombo"; Type="ComboBox";   X=18; Y=38; W=224; H=80; Attr=131075; Prop="UILANG"; Text="" },
    @{ Id="OkBtn";     Type="PushButton"; X=95; Y=70; W=70;  H=17; Attr=3;      Prop="";       Text="OK" },
    @{ Id="PromptTxt"; Type="Text";       X=18; Y=14; W=224; H=20; Attr=3;      Prop="";       Text="Select the language to use during the installation:" }
)

# 2. WelcomeDlg
Add-DialogWithRing $db $wi "WelcomeDlg" 370 270 3 "Kaiseki v$version Setup" "NextBtn" "NextBtn" "CancelBtn" @(
    @{ Id="Backdrop";  Type="Bitmap";     X=0;   Y=0;   W=370; H=234; Attr=1;     Prop=""; Text="DialogBmp" },
    @{ Id="TitleTxt";  Type="Text";       X=135; Y=20;  W=220; H=42;  Attr=65539; Prop=""; Text="{\DlgFontTitle}[TXT_WELCOME_TITLE]" },
    @{ Id="BodyTxt";   Type="Text";       X=135; Y=70;  W=220; H=120; Attr=65539; Prop=""; Text="[TXT_WELCOME_BODY]" },
    @{ Id="DivLine";   Type="Line";       X=0;   Y=234; W=370; H=2;   Attr=1;     Prop=""; Text="" },
    @{ Id="BackBtn";   Type="PushButton"; X=180; Y=243; W=56;  H=17;  Attr=1;     Prop=""; Text="[TXT_BTN_BACK]" },
    @{ Id="NextBtn";   Type="PushButton"; X=236; Y=243; W=56;  H=17;  Attr=3;     Prop=""; Text="[TXT_BTN_NEXT]" },
    @{ Id="CancelBtn"; Type="PushButton"; X=304; Y=243; W=56;  H=17;  Attr=3;     Prop=""; Text="[TXT_BTN_CANCEL]" }
)

# 3. LicenseDlg (GPLv3 Lisans Sozlesmesi ve Onayi)
Add-DialogWithRing $db $wi "LicenseDlg" 370 270 3 "Kaiseki v$version Setup" "NextBtn" "NextBtn" "CancelBtn" @(
    @{ Id="Banner";      Type="Bitmap";         X=0;   Y=0;   W=370; H=44;  Attr=1;      Prop="";             Text="BannerBmp" },
    @{ Id="TitleTxt";    Type="Text";           X=15;  Y=8;   W=295; H=16;  Attr=65539;  Prop="";             Text="{\DlgFontHeader}[TXT_LICENSE_TITLE]" },
    @{ Id="SubTxt";      Type="Text";           X=15;  Y=23;  W=295; H=15;  Attr=65539;  Prop="";             Text="{\DlgFontSub}[TXT_LICENSE_SUB]" },
    @{ Id="TopLine";     Type="Line";           X=0;   Y=44;  W=370; H=2;   Attr=1;      Prop="";             Text="" },
    @{ Id="LicenseText"; Type="ScrollableText"; X=15;  Y=52;  W=340; H=146; Attr=65539;  Prop="";             Text="LicenseRtf" },
    @{ Id="AcceptCheck"; Type="CheckBox";       X=15;  Y=206; W=340; H=18;  Attr=3;      Prop="AGREELICENSE"; Text="[TXT_LICENSE_ACCEPT]" },
    @{ Id="DivLine";     Type="Line";           X=0;   Y=234; W=370; H=2;   Attr=1;      Prop="";             Text="" },
    @{ Id="BackBtn";     Type="PushButton";     X=180; Y=243; W=56;  H=17;  Attr=3;      Prop="";             Text="[TXT_BTN_BACK]" },
    @{ Id="NextBtn";     Type="PushButton";     X=236; Y=243; W=56;  H=17;  Attr=3;      Prop="";             Text="[TXT_BTN_NEXT]" },
    @{ Id="CancelBtn";   Type="PushButton";     X=304; Y=243; W=56;  H=17;  Attr=3;      Prop="";             Text="[TXT_BTN_CANCEL]" }
)

# 4. InstallDirDlg
Add-DialogWithRing $db $wi "InstallDirDlg" 370 270 3 "Kaiseki v$version Setup" "NextBtn" "NextBtn" "CancelBtn" @(
    @{ Id="Banner";       Type="Bitmap";     X=0;   Y=0;   W=370; H=44;  Attr=1;     Prop="";                 Text="BannerBmp" },
    @{ Id="TitleTxt";     Type="Text";       X=15;  Y=8;   W=295; H=16;  Attr=65539; Prop="";                 Text="{\DlgFontHeader}[TXT_DIR_TITLE]" },
    @{ Id="SubTxt";       Type="Text";       X=15;  Y=23;  W=295; H=15;  Attr=65539; Prop="";                 Text="{\DlgFontSub}[TXT_DIR_SUB]" },
    @{ Id="TopLine";      Type="Line";       X=0;   Y=44;  W=370; H=2;   Attr=1;     Prop="";                 Text="" },
    @{ Id="PromptTxt";    Type="Text";       X=20;  Y=60;  W=330; H=38;  Attr=3;     Prop="";                 Text="[TXT_DIR_PROMPT]" },
    @{ Id="PathEditCtrl"; Type="PathEdit";   X=20;  Y=105; W=255; H=18;  Attr=3;     Prop="INSTALLDIR";       Text="" },
    @{ Id="BrowseBtn";    Type="PushButton"; X=280; Y=105; W=70;  H=18;  Attr=3;     Prop="";                 Text="[TXT_BTN_BROWSE]" },
    @{ Id="DeskCheck";    Type="CheckBox";   X=20;  Y=145; W=320; H=18;  Attr=3;     Prop="DESKTOP_SHORTCUT"; Text="[TXT_DESK_SC]" },
    @{ Id="DivLine";      Type="Line";       X=0;   Y=234; W=370; H=2;   Attr=1;     Prop="";                 Text="" },
    @{ Id="BackBtn";      Type="PushButton"; X=180; Y=243; W=56;  H=17;  Attr=3;     Prop="";                 Text="[TXT_BTN_BACK]" },
    @{ Id="NextBtn";      Type="PushButton"; X=236; Y=243; W=56;  H=17;  Attr=3;     Prop="";                 Text="[TXT_BTN_NEXT]" },
    @{ Id="CancelBtn";    Type="PushButton"; X=304; Y=243; W=56;  H=17;  Attr=3;     Prop="";                 Text="[TXT_BTN_CANCEL]" }
)

# 5. BrowseDlg
Add-DialogWithRing $db $wi "BrowseDlg" 370 270 3 "Kaiseki v$version Setup" "DirList" "OkBtn" "CancelBtn" @(
    @{ Id="Banner";    Type="Bitmap";         X=0;   Y=0;   W=370; H=44;  Attr=1;      Prop="";           Text="BannerBmp" },
    @{ Id="TitleTxt";  Type="Text";           X=15;  Y=8;   W=295; H=16;  Attr=65539;  Prop="";           Text="{\DlgFontHeader}[TXT_BROWSE_TITLE]" },
    @{ Id="SubTxt";    Type="Text";           X=15;  Y=23;  W=295; H=15;  Attr=65539;  Prop="";           Text="{\DlgFontSub}[TXT_BROWSE_SUB]" },
    @{ Id="TopLine";   Type="Line";           X=0;   Y=44;  W=370; H=2;   Attr=1;      Prop="";           Text="" },
    @{ Id="LookInTxt"; Type="Text";           X=20;  Y=58;  W=52;  H=15;  Attr=3;      Prop="";           Text="[TXT_BROWSE_LOOKIN]" },
    @{ Id="DirCombo";  Type="DirectoryCombo"; X=75;  Y=55;  W=205; H=80;  Attr=458755; Prop="INSTALLDIR"; Text="" },
    @{ Id="UpBtn";     Type="PushButton";     X=285; Y=55;  W=65;  H=18;  Attr=3;      Prop="";           Text="[TXT_BTN_UP]" },
    @{ Id="DirList";   Type="DirectoryList";  X=20;  Y=85;  W=330; H=110; Attr=7;      Prop="INSTALLDIR"; Text="" },
    @{ Id="PathEdit";  Type="PathEdit";       X=20;  Y=205; W=330; H=18;  Attr=3;      Prop="INSTALLDIR"; Text="" },
    @{ Id="DivLine";   Type="Line";           X=0;   Y=234; W=370; H=2;   Attr=1;      Prop="";           Text="" },
    @{ Id="OkBtn";     Type="PushButton";     X=236; Y=243; W=56;  H=17;  Attr=3;      Prop="";           Text="[TXT_BTN_OK]" },
    @{ Id="CancelBtn"; Type="PushButton";     X=304; Y=243; W=56;  H=17;  Attr=3;      Prop="";           Text="[TXT_BTN_CANCEL]" }
)

# 6. VerifyReadyDlg
Add-DialogWithRing $db $wi "VerifyReadyDlg" 370 270 3 "Kaiseki v$version Setup" "InstallBtn" "InstallBtn" "CancelBtn" @(
    @{ Id="InstallBtn"; Type="PushButton"; X=230; Y=243; W=64;  H=17; Attr=8388611; Prop=""; Text="[TXT_BTN_INSTALL]" },
    @{ Id="BackBtn";    Type="PushButton"; X=168; Y=243; W=56;  H=17; Attr=3;     Prop=""; Text="[TXT_BTN_BACK]" },
    @{ Id="CancelBtn";  Type="PushButton"; X=304; Y=243; W=56;  H=17; Attr=3;     Prop=""; Text="[TXT_BTN_CANCEL]" },
    @{ Id="TitleTxt";   Type="Text";       X=15;  Y=8;   W=295; H=16; Attr=65539; Prop=""; Text="{\DlgFontHeader}[TXT_READY_TITLE]" },
    @{ Id="SubTxt";     Type="Text";       X=15;  Y=23;  W=295; H=15; Attr=65539; Prop=""; Text="{\DlgFontSub}[TXT_READY_SUB]" },
    @{ Id="ReadyTxt";   Type="Text";       X=20;  Y=65;  W=330; H=95; Attr=3;     Prop=""; Text="[TXT_READY_BODY]" },
    @{ Id="TopLine";    Type="Line";       X=0;   Y=44;  W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="DivLine";    Type="Line";       X=0;   Y=234; W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="Banner";     Type="Bitmap";     X=0;   Y=0;   W=370; H=44;  Attr=1;     Prop=""; Text="BannerBmp" }
)

# 7. MaintenanceDlg (Kaldirma & Onarma Ekrani)
Add-DialogWithRing $db $wi "MaintenanceDlg" 370 270 3 "Kaiseki v$version Setup" "RemoveBtn" "RemoveBtn" "CancelBtn" @(
    @{ Id="RemoveBtn"; Type="PushButton"; X=25;  Y=150; W=85;  H=20; Attr=3;     Prop=""; Text="[TXT_BTN_REMOVE]" },
    @{ Id="RepairBtn"; Type="PushButton"; X=25;  Y=95;  W=85;  H=20; Attr=8388611; Prop=""; Text="[TXT_BTN_REPAIR]" },
    @{ Id="BackBtn";   Type="PushButton"; X=236; Y=243; W=56;  H=17; Attr=1;     Prop=""; Text="[TXT_BTN_BACK]" },
    @{ Id="CancelBtn"; Type="PushButton"; X=304; Y=243; W=56;  H=17; Attr=3;     Prop=""; Text="[TXT_BTN_CANCEL]" },
    @{ Id="TitleTxt";  Type="Text";       X=15;  Y=8;   W=295; H=16; Attr=65539; Prop=""; Text="{\DlgFontHeader}[TXT_MAINT_TITLE]" },
    @{ Id="SubTxt";    Type="Text";       X=15;  Y=23;  W=295; H=15; Attr=65539; Prop=""; Text="{\DlgFontSub}[TXT_MAINT_SUB]" },
    @{ Id="BodyTxt";   Type="Text";       X=20;  Y=55;  W=330; H=30; Attr=3;     Prop=""; Text="[TXT_MAINT_BODY]" },
    @{ Id="RepairTxt"; Type="Text";       X=120; Y=95;  W=230; H=35; Attr=3;     Prop=""; Text="[TXT_MAINT_REPAIR_D]" },
    @{ Id="RemoveTxt"; Type="Text";       X=120; Y=150; W=230; H=35; Attr=3;     Prop=""; Text="[TXT_MAINT_REMOVE_D]" },
    @{ Id="TopLine";   Type="Line";       X=0;   Y=44;  W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="DivLine";   Type="Line";       X=0;   Y=234; W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="Banner";    Type="Bitmap";     X=0;   Y=0;   W=370; H=44;  Attr=1;     Prop=""; Text="BannerBmp" }
)

# 8. VerifyRemoveDlg (Kaldirma Onay Ekrani)
Add-DialogWithRing $db $wi "VerifyRemoveDlg" 370 270 3 "Kaiseki v$version Setup" "RemoveConfirmBtn" "RemoveConfirmBtn" "CancelBtn" @(
    @{ Id="RemoveConfirmBtn"; Type="PushButton"; X=225; Y=243; W=70;  H=17; Attr=8388611; Prop=""; Text="[TXT_BTN_REMOVE]" },
    @{ Id="BackBtn";          Type="PushButton"; X=160; Y=243; W=56;  H=17; Attr=3;     Prop=""; Text="[TXT_BTN_BACK]" },
    @{ Id="CancelBtn";        Type="PushButton"; X=304; Y=243; W=56;  H=17; Attr=3;     Prop=""; Text="[TXT_BTN_CANCEL]" },
    @{ Id="TitleTxt";         Type="Text";       X=15;  Y=8;   W=295; H=16; Attr=65539; Prop=""; Text="{\DlgFontHeader}[TXT_VREM_TITLE]" },
    @{ Id="SubTxt";           Type="Text";       X=15;  Y=23;  W=295; H=15; Attr=65539; Prop=""; Text="{\DlgFontSub}[TXT_VREM_SUB]" },
    @{ Id="BodyTxt";          Type="Text";       X=20;  Y=65;  W=330; H=80; Attr=3;     Prop=""; Text="[TXT_VREM_BODY]" },
    @{ Id="TopLine";          Type="Line";       X=0;   Y=44;  W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="DivLine";          Type="Line";       X=0;   Y=234; W=370; H=2;  Attr=1;     Prop=""; Text="" },
    @{ Id="Banner";           Type="Bitmap";     X=0;   Y=0;   W=370; H=44;  Attr=1;     Prop=""; Text="BannerBmp" }
)

# 9. ProgressDlg (Modeless Ilerleme Cubugu: Attr 17 = Visible [1] + KeepModeless [16], Modal [2] = 0)
Add-DialogWithRing $db $wi "ProgressDlg" 370 270 17 "Kaiseki v$version Setup" "CancelBtn" "CancelBtn" "CancelBtn" @(
    @{ Id="CancelBtn"; Type="PushButton";  X=304; Y=243; W=56;  H=17; Attr=3;      Prop=""; Text="[TXT_BTN_CANCEL]" },
    @{ Id="ProgBar";   Type="ProgressBar"; X=20;  Y=115; W=330; H=16; Attr=65537;  Prop=""; Text="" },
    @{ Id="TitleTxt";  Type="Text";        X=15;  Y=8;   W=295; H=16; Attr=65539;  Prop=""; Text="{\DlgFontHeader}[TXT_PROG_TITLE]" },
    @{ Id="SubTxt";    Type="Text";        X=15;  Y=23;  W=295; H=15; Attr=65539;  Prop=""; Text="{\DlgFontSub}[TXT_PROG_SUB]" },
    @{ Id="BodyTxt";   Type="Text";        X=20;  Y=68;  W=330; H=32; Attr=3;      Prop=""; Text="[TXT_PROG_BODY]" },
    @{ Id="ActionTxt"; Type="Text";        X=20;  Y=140; W=330; H=20; Attr=196611; Prop=""; Text="" },
    @{ Id="TopLine";   Type="Line";        X=0;   Y=44;  W=370; H=2;  Attr=1;      Prop=""; Text="" },
    @{ Id="DivLine";   Type="Line";        X=0;   Y=234; W=370; H=2;  Attr=1;      Prop=""; Text="" },
    @{ Id="Banner";    Type="Bitmap";      X=0;   Y=0;   W=370; H=44;  Attr=1;      Prop=""; Text="BannerBmp" }
)
ExecSql $db "INSERT INTO EventMapping (Dialog_, Control_, Event, Attribute) VALUES ('ProgressDlg', 'ProgBar', 'SetProgress', 'Progress')"
ExecSql $db "INSERT INTO EventMapping (Dialog_, Control_, Event, Attribute) VALUES ('ProgressDlg', 'ActionTxt', 'ActionData', 'Text')"

# 10. ExitDialog
Add-DialogWithRing $db $wi "ExitDialog" 370 270 3 "Kaiseki v$version Setup" "FinishBtn" "FinishBtn" "FinishBtn" @(
    @{ Id="Backdrop";    Type="Bitmap";     X=0;   Y=0;   W=370; H=234; Attr=1;     Prop="";          Text="DialogBmp" },
    @{ Id="TitleTxt";    Type="Text";       X=135; Y=20;  W=220; H=42;  Attr=65539; Prop="";          Text="{\DlgFontTitle}[TXT_EXIT_TITLE]" },
    @{ Id="BodyTxt";     Type="Text";       X=135; Y=70;  W=220; H=95;  Attr=65539; Prop="";          Text="[TXT_EXIT_BODY]" },
    @{ Id="LaunchCheck"; Type="CheckBox";   X=135; Y=185; W=215; H=18;  Attr=3;     Prop="LAUNCHAPP"; Text="[TXT_LAUNCH_APP]" },
    @{ Id="DivLine";     Type="Line";       X=0;   Y=234; W=370; H=2;   Attr=1;     Prop="";          Text="" },
    @{ Id="FinishBtn";   Type="PushButton"; X=304; Y=243; W=56;  H=17;  Attr=3;     Prop="";          Text="[TXT_BTN_FINISH]" }
)

# 11. CancelDlg
Add-DialogWithRing $db $wi "CancelDlg" 260 90 3 "Kaiseki Setup" "NoBtn" "NoBtn" "NoBtn" @(
    @{ Id="NoBtn";     Type="PushButton"; X=138; Y=60; W=62;  H=18; Attr=3; Prop=""; Text="[TXT_BTN_NO]" },
    @{ Id="YesBtn";    Type="PushButton"; X=65;  Y=60; W=62;  H=18; Attr=3; Prop=""; Text="[TXT_BTN_YES]" },
    @{ Id="PromptTxt"; Type="Text";       X=18;  Y=15; W=224; H=36; Attr=3; Prop=""; Text="[TXT_CANCEL_PROMPT]" }
)

# ControlCondition: LicenseDlg NextBtn etkinlestirme / devre disi birakma
ExecSql $db "INSERT INTO ControlCondition (Dialog_, Control_, Action, Condition) VALUES ('LicenseDlg', 'NextBtn', 'Enable', 'AGREELICENSE = `"1`"')"
ExecSql $db "INSERT INTO ControlCondition (Dialog_, Control_, Action, Condition) VALUES ('LicenseDlg', 'NextBtn', 'Disable', 'AGREELICENSE <> `"1`"')"

# 12. ControlEvents (Dil Yukleme + Kurulum/Kaldirma Akisi)
$ord = 1
foreach ($langKey in @("EN", "TR", "JA", "DE")) {
    foreach ($propKey in $i18n[$langKey].Keys) {
        $caName = "CA_$($langKey)_$propKey"
        ExecParam $db $wi "INSERT INTO ControlEvent (Dialog_, Control_, Event, Argument, Condition, Ordering) VALUES (?, ?, ?, ?, ?, ?)" @(
            "LangDlg", "OkBtn", "DoAction", $caName, "UILANG = `"$langKey`"", $ord
        )
        $ord++
    }
}
ExecParam $db $wi "INSERT INTO ControlEvent (Dialog_, Control_, Event, Argument, Condition, Ordering) VALUES (?, ?, ?, ?, ?, ?)" @("LangDlg", "OkBtn", "NewDialog", "WelcomeDlg", "NOT Installed", ($ord + 1))
ExecParam $db $wi "INSERT INTO ControlEvent (Dialog_, Control_, Event, Argument, Condition, Ordering) VALUES (?, ?, ?, ?, ?, ?)" @("LangDlg", "OkBtn", "NewDialog", "MaintenanceDlg", "Installed", ($ord + 2))

$events = @(
    # WelcomeDlg -> LicenseDlg
    @("WelcomeDlg", "NextBtn",   "NewDialog",   "LicenseDlg",    "1", 1),
    @("WelcomeDlg", "CancelBtn", "SpawnDialog", "CancelDlg",     "1", 1),

    # LicenseDlg -> WelcomeDlg veya InstallDirDlg
    @("LicenseDlg", "BackBtn",   "NewDialog",   "WelcomeDlg",    "1", 1),
    @("LicenseDlg", "NextBtn",   "NewDialog",   "InstallDirDlg", "AGREELICENSE = `"1`"", 1),
    @("LicenseDlg", "CancelBtn", "SpawnDialog", "CancelDlg",     "1", 1),

    # InstallDirDlg -> LicenseDlg veya VerifyReadyDlg
    @("InstallDirDlg", "BackBtn",   "NewDialog",     "LicenseDlg",     "1", 1),
    @("InstallDirDlg", "BrowseBtn", "SpawnDialog",   "BrowseDlg",      "1", 1),
    @("InstallDirDlg", "NextBtn",   "SetTargetPath", "INSTALLDIR",     "1", 1),
    @("InstallDirDlg", "NextBtn",   "NewDialog",     "VerifyReadyDlg", "1", 2),
    @("InstallDirDlg", "CancelBtn", "SpawnDialog",   "CancelDlg",      "1", 1),

    # BrowseDlg
    @("BrowseDlg", "UpBtn",     "DirectoryListUp", "0",          "1", 1),
    @("BrowseDlg", "OkBtn",     "SetTargetPath",   "INSTALLDIR", "1", 1),
    @("BrowseDlg", "OkBtn",     "EndDialog",       "Return",     "1", 2),
    @("BrowseDlg", "CancelBtn", "Reset",           "0",          "1", 1),
    @("BrowseDlg", "CancelBtn", "EndDialog",       "Return",     "1", 2),

    # VerifyReadyDlg -> Kurulum Baslatma
    @("VerifyReadyDlg", "BackBtn",    "NewDialog",   "InstallDirDlg", "1", 1),
    @("VerifyReadyDlg", "InstallBtn", "EndDialog",   "Return",        "1", 1),
    @("VerifyReadyDlg", "CancelBtn",  "SpawnDialog", "CancelDlg",     "1", 1),

    # MaintenanceDlg (Kaldir / Onar)
    @("MaintenanceDlg", "RepairBtn", "Reinstall",   "ALL",             "1", 1),
    @("MaintenanceDlg", "RepairBtn", "EndDialog",   "Return",          "1", 2),
    @("MaintenanceDlg", "RemoveBtn", "NewDialog",   "VerifyRemoveDlg", "1", 1),
    @("MaintenanceDlg", "CancelBtn", "SpawnDialog", "CancelDlg",       "1", 1),

    # VerifyRemoveDlg (Tam Kaldirma)
    @("VerifyRemoveDlg", "BackBtn",          "NewDialog",   "MaintenanceDlg", "1", 1),
    @("VerifyRemoveDlg", "RemoveConfirmBtn", "Remove",      "ALL",            "1", 1),
    @("VerifyRemoveDlg", "RemoveConfirmBtn", "EndDialog",   "Return",         "1", 2),
    @("VerifyRemoveDlg", "CancelBtn",        "SpawnDialog", "CancelDlg",      "1", 1),

    # ProgressDlg
    @("ProgressDlg", "CancelBtn", "SpawnDialog", "CancelDlg", "1", 1),

    # ExitDialog
    @("ExitDialog", "FinishBtn", "DoAction",  "LaunchApp", "LAUNCHAPP = `"1`" AND REMOVE <> `"ALL`"", 1),
    @("ExitDialog", "FinishBtn", "EndDialog", "Return",    "1",                                       2),

    # CancelDlg
    @("CancelDlg", "YesBtn", "EndDialog", "Exit",   "1", 1),
    @("CancelDlg", "NoBtn",  "EndDialog", "Return", "1", 1)
)
foreach ($ev in $events) {
    ExecParam $db $wi "INSERT INTO ControlEvent (Dialog_, Control_, Event, Argument, Condition, Ordering) VALUES (?, ?, ?, ?, ?, ?)" @(
        $ev[0], $ev[1], $ev[2], $ev[3], $ev[4], [int]$ev[5]
    )
}

# 13. Binary, Icon & Cab Gomme
function EmbedBinaryData($db, $wi, $table, $name, $path) {
    $full = [System.IO.Path]::GetFullPath($path)
    $view = $db.OpenView("SELECT Name, Data FROM $table WHERE Name = '$name'")
    $view.Execute()
    $rec = $view.Fetch()
    if (-not $rec) {
        $rec = $wi.CreateRecord(2)
        $rec.StringData(1) = $name
    }
    $rec.SetStream(2, $full)
    $view.Modify(3, $rec) # 3 = MSIMODIFY_ASSIGN (Kalici Kayit)
    $view.Close()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($rec) | Out-Null
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($view) | Out-Null
}

EmbedBinaryData $db $wi "Icon" "KaisekiIco" "Kaiseki\res\Kaiseki.ico"
EmbedBinaryData $db $wi "Binary" "BannerBmp" $bannerBmpPath
EmbedBinaryData $db $wi "Binary" "DialogBmp" $dialogBmpPath
EmbedBinaryData $db $wi "Binary" "LicenseRtf" $rtfPath
EmbedBinaryData $db $wi "_Streams" "Data1.cab" $cabPath

# 14. Summary Information (Compressed Bit 1: 2, Bit 3: 0 allows UAC elevation prompt)
$sumInfo = $db.SummaryInformation(20)
$sumInfo.Property(1)  = 1252
$sumInfo.Property(2)  = "Installation Database"
$sumInfo.Property(3)  = "Kaiseki v$version Setup"
$templateStr = if ($arch -eq "x64") { "x64;1033" } else { "Intel;1033" }
$sumInfo.Property(7)  = $templateStr
$sumInfo.Property(9)  = [System.Guid]::NewGuid().ToString("B").ToUpper()
$sumInfo.Property(14) = 200
$sumInfo.Property(15) = 2 # Compressed = 2, Bit 3 = 0 (Privileges CAN be required -> enables UAC elevation)
$sumInfo.Property(18) = "Windows Installer"
$sumInfo.Persist()

$db.Commit()

[System.Runtime.InteropServices.Marshal]::ReleaseComObject($sumInfo) | Out-Null
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($db) | Out-Null
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($wi) | Out-Null

# Temizlik
Remove-Item -Path $tempCabDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "MSI Basariyla Olusturuldu: $outFullPath" -ForegroundColor Green
