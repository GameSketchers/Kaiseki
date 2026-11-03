param([string]$msiPath = "")
if ([string]::IsNullOrWhiteSpace($msiPath)) {
    $msiFiles = Get-ChildItem -Path "dist" -Filter "*.msi" | Sort-Object LastWriteTime -Descending
    if ($msiFiles.Count -gt 0) { $msiPath = $msiFiles[0].FullName }
    else { $msiPath = "dist\Kaiseki-v1.0.0-beta-Setup-x64.msi" }
}
$wi = New-Object -ComObject WindowsInstaller.Installer
$db = $wi.OpenDatabase((Resolve-Path $msiPath).Path, 0)
$view = $db.OpenView("SELECT Dialog, Control_First, Control_Default, Control_Cancel FROM Dialog")
$view.Execute()

$hasError = $false

while ($rec = $view.Fetch()) {
    $d = $rec.StringData(1)
    $cFirst = $rec.StringData(2)
    $cDefault = $rec.StringData(3)
    $cCancel = $rec.StringData(4)
    Write-Host "Dialog: $d [First: $cFirst, Default: $cDefault, Cancel: $cCancel]"

    $cview = $db.OpenView("SELECT Control, Type, Control_Next FROM Control WHERE Dialog_ = '$d'")
    $cview.Execute()
    
    $controls = @{}
    $incoming = @{}
    while ($crec = $cview.Fetch()) {
        $cName = $crec.StringData(1)
        $cType = $crec.StringData(2)
        $cNext = $crec.StringData(3)
        $controls[$cName] = @{ Type = $cType; Next = $cNext }
        $incoming[$cNext] = ($incoming[$cNext] -as [int]) + 1
        Write-Host "   $cName ($cType) -> $cNext"
        [System.Runtime.InteropServices.Marshal]::ReleaseComObject($crec) | Out-Null
    }
    $cview.Close()
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($cview) | Out-Null

    # Check incoming count
    foreach ($cName in $controls.Keys) {
        $inc = $incoming[$cName]
        if ($inc -ne 1) {
            Write-Host "   [ERROR] Control $cName has incoming count $inc (must be exactly 1)!" -ForegroundColor Red
            $hasError = $true
        }
        $target = $controls[$cName].Next
        if (-not $controls.ContainsKey($target)) {
            Write-Host "   [ERROR] Control $cName points to non-existent control '$target'!" -ForegroundColor Red
            $hasError = $true
        }
    }

    # Verify cycle contains all controls
    $curr = $cFirst
    $visited = @{}
    while ($curr -and -not $visited.ContainsKey($curr)) {
        $visited[$curr] = $true
        $curr = $controls[$curr].Next
    }
    if ($visited.Count -ne $controls.Count) {
        Write-Host "   [ERROR] Cycle starting at $cFirst visits $($visited.Count)/$($controls.Count) controls!" -ForegroundColor Red
        $hasError = $true
    } else {
        Write-Host "   [OK] Cycle is complete and covers all $($controls.Count) controls." -ForegroundColor Green
    }

    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($rec) | Out-Null
}
$view.Close()
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($view) | Out-Null
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($db) | Out-Null
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($wi) | Out-Null

if (-not $hasError) {
    Write-Host "`nALL DIALOGS VALIDATED PERFECTLY! NO ERROR 2810 POSSIBLE." -ForegroundColor Green
} else {
    Write-Host "`nVALIDATION FAILED!" -ForegroundColor Red
}
