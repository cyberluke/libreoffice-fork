# run_smoke.ps1 - Windows UI smoke-test / crash-catcher harness for the built LibreOffice.
#
# Usage:
#   pwsh -NoProfile -File v271/smoke/run_smoke.ps1 -Scenario tree          # dump UIA tree (learning run)
#   pwsh -NoProfile -File v271/smoke/run_smoke.ps1 -Scenario font          # open font picker (crash repro)
#   pwsh -NoProfile -File v271/smoke/run_smoke.ps1 -Scenario managechanges # open Manage Changes deck (crash repro)
#   pwsh -NoProfile -File v271/smoke/run_smoke.ps1 -Scenario styles        # count Styles gallery cards
#
# Flow per scenario: ensure initialized profile -> launch soffice.bin (no debugger)
# -> wait for main window -> attach cdb with crash capture -> run UIA action(s)
# -> liveness check -> on crash: analyze dump with cdb -> write result file.

param(
    [ValidateSet('tree','font','managechanges','styles')]
    [string]$Scenario = 'tree'
)

$ErrorActionPreference = 'Stop'

$cdb = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
$sofficeExe = 'D:\_SATIN_AI\LibreOffice\instdir\program\soffice.exe'
$instDir = 'D:\_SATIN_AI\LibreOffice\instdir\program'
$repoSmoke = 'D:\_SATIN_AI\LibreOffice\v271\smoke'
$workDir = 'C:\Users\LUKES~1.COR\AppData\Local\Temp\kilo\smoke'
$dumpDir = Join-Path $workDir 'dumps'
$profileDir = Join-Path $workDir 'profile\setup'
$resultsDir = Join-Path $repoSmoke 'results'
New-Item -ItemType Directory -Force -Path $dumpDir, $resultsDir | Out-Null

$profileUrl = "file:///$($profileDir -replace '\\','/')"
$stamp = Get-Date -Format 'yyyyMMddHHmmss'
$dumpFile = Join-Path $dumpDir "crash_$($Scenario)_$stamp.dmp"
$cdbLog = Join-Path $dumpDir "cdb_$($Scenario)_$stamp.log"
$resultFile = Join-Path $resultsDir "$($Scenario)_$stamp.txt"
$uiaTreeFile = Join-Path $dumpDir "uia_tree_$($Scenario)_$stamp.txt"
$logLines = New-Object System.Collections.Generic.List[string]
function Log([string]$m) { $logLines.Add($m); Write-Output $m }
function FlushResult([string]$status, [string]$detail) {
    Log "RESULT: $status"
    Log "DETAIL: $detail"
    $logLines | Set-Content -LiteralPath $resultFile -Encoding Utf8
    Log "result file: $resultFile"
}

# ---------------------------------------------------------------- helpers
function Get-Uia {
    if (-not $script:uia) { $script:uia = New-Object -ComObject UIAutomationClient.CUIAutomation }
    return $script:uia
}

function Get-SofficePid {
    $sb = Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($sb) { return $sb.Id }
    return 0
}

function Wait-MainWindow([int]$timeoutSec = 240) {
    # The wrapper (soffice.exe, ProcessName 'soffice') spawns soffice.bin
    # (ProcessName 'soffice.bin') asynchronously; the window belongs to
    # soffice.bin. Match BOTH names and use MainWindowHandle.
    for ($i = 0; $i -lt [math]::Ceiling($timeoutSec / 2); $i++) {
        Start-Sleep -Seconds 2
        $sb = Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
        if ($sb) { return $sb }
    }
    return $null
}

function Find-ByName($root, [string]$name) {
    $uia = Get-Uia
    $cond = $uia.CreatePropertyCondition(30005, $name)  # NameProperty
    return $root.FindFirst(4, $cond)  # Descendants
}

function Find-ByControlTypeAndName($root, [int]$ctrlType, [string]$name) {
    $uia = Get-Uia
    $c1 = $uia.CreatePropertyCondition(30003, $ctrlType)   # ControlType
    $c2 = $uia.CreatePropertyCondition(30005, $name)       # Name
    $and = $uia.CreateAndCondition($c1, $c2)
    return $root.FindFirst(4, $and)
}

function Send-Keys([string]$keys) {
    $ws = New-Object -ComObject WScript.Shell
    Start-Sleep -Milliseconds 300
    $ws.SendKeys($keys)
    Start-Sleep -Milliseconds 500
}

function Dump-UiaTree($root, [string]$file) {
    $uia = Get-Uia
    $trueCond = $uia.CreateTrueCondition()
    $all = $root.FindAll(4, $trueCond)
    $out = New-Object System.Collections.Generic.List[string]
    $ct = @{ 50000='Button'; 50001='Calendar'; 50002='CheckBox'; 50003='ComboBox'; 50004='Edit'; 50005='Hyperlink'; 50006='Image'; 50007='ListItem'; 50008='List'; 50009='Menu'; 50010='MenuBar'; 50011='MenuItem'; 50012='ProgressBar'; 50013='RadioButton'; 50014='ScrollBar'; 50015='Slider'; 50016='Spinner'; 50017='StatusBar'; 50018='Tab'; 50019='TabItem'; 50020='Text'; 50021='ToolBar'; 50022='ToolTip'; 50023='Tree'; 50024='TreeItem'; 50025='Custom'; 50026='Group'; 50027='Thumb'; 50028='DataGrid'; 50029='DataItem'; 50030='Document'; 50031='SplitButton'; 50032='Window'; 50033='Pane'; 50034='Header'; 50035='HeaderItem'; 50036='Table'; 50037='TitleBar'; 50038='Separator'; 50039='SemanticZoom'; 50040='AppBar' }
    for ($i = 0; $i -lt $all.Length; $i++) {
        $e = $all.GetElement($i)
        $tn = if ($ct.ContainsKey([int]$e.CurrentControlType)) { $ct[[int]$e.CurrentControlType] } else { "CT$($e.CurrentControlType)" }
        $out.Add("[$i] $tn | name='$($e.CurrentName)' | aid='$($e.CurrentAutomationId)' | enabled=$($e.CurrentIsEnabled)")
    }
    $out | Set-Content -LiteralPath $file -Encoding Utf8
    return $out
}

# ---------------------------------------------------------------- profile setup
$setupDone = Join-Path $profileDir '.smoke_ready'
if (-not (Test-Path $setupDone)) {
    Log "== profile not ready; running --terminate_after_init setup =="
    Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $profileDir -Recurse -Force -ErrorAction SilentlyContinue
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $sofficeExe
    $psi.WorkingDirectory = $instDir
    $psi.UseShellExecute = $false
    $psi.ArgumentList.Add('--terminate_after_init')
    $psi.ArgumentList.Add('--norestore')
    $psi.ArgumentList.Add("-env:UserInstallation=$profileUrl")
    $p = [System.Diagnostics.Process]::Start($psi)
    if (-not $p.WaitForExit(600000)) { Log "setup timed out"; $p.Kill(); FlushResult 'ERROR' 'profile setup timed out'; exit 1 }
    Log "setup exit code: $($p.ExitCode)"
    if ($p.ExitCode -ne 0) { FlushResult 'ERROR' "profile setup exit code $($p.ExitCode)"; exit 1 }
    New-Item -ItemType File -Path $setupDone -Force | Out-Null
    Log "profile ready"
} else {
    Log "== profile already initialized =="
}

# ---------------------------------------------------------------- launch (wrapper handles the 81-restart)
Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Remove-Item -LiteralPath (Join-Path $profileDir '.lock') -Force -ErrorAction SilentlyContinue

Log "== launching soffice.exe (scenario: $Scenario) =="
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $sofficeExe
$psi.WorkingDirectory = $instDir
$psi.UseShellExecute = $false
$psi.ArgumentList.Add('--writer')
$psi.ArgumentList.Add('--norestore')
$psi.ArgumentList.Add("-env:UserInstallation=$profileUrl")
$p = [System.Diagnostics.Process]::Start($psi)
Log "launcher pid: $($p.Id)"

$sb = Wait-MainWindow 300
if (-not $sb) {
    Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    FlushResult 'ERROR' 'main window never appeared (300s)'
    exit 1
}
$pidMain = $sb.Id
Log "main window: pid=$pidMain title='$($sb.MainWindowTitle)'"
Start-Sleep -Seconds 12  # let the notebookbar fully render

# ---------------------------------------------------------------- attach cdb
Log "== attaching cdb to pid $pidMain =="
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $cdb
$psi.WorkingDirectory = $instDir
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.ArgumentList.Add('-g')
$psi.ArgumentList.Add('-G')
$psi.ArgumentList.Add('-logo'); $psi.ArgumentList.Add($cdbLog)
$psi.ArgumentList.Add('-c')
$psi.ArgumentList.Add("sxd -c `".dump /ma $dumpFile; q`" av; sxd -c `".dump /ma $dumpFile; q`" 0xc0000409; sxd -c `".dump /ma $dumpFile; q`" ibp; sxd -c `".dump /ma $dumpFile; q`" eh; g")
$psi.ArgumentList.Add('-p'); $psi.ArgumentList.Add([string]$pidMain)
$cdbProc = [System.Diagnostics.Process]::Start($psi)
Start-Sleep -Seconds 5
$cdbRunning = -not $cdbProc.HasExited
Log "cdb attached and running: $cdbRunning"

# ---------------------------------------------------------------- scenario action
$uia = Get-Uia
$win = $uia.ElementFromHandle($sb.MainWindowHandle)
Log "uia root: '$($win.CurrentName)'"

$actionDone = $false
$actionDetail = 'no action performed'
try {
    switch ($Scenario) {
        'tree' {
            $tree = Dump-UiaTree $win $uiaTreeFile
            $actionDetail = "uia tree saved: $uiaTreeFile ($($tree.Count) elements)"
            Log "== $actionDetail =="
            $tree | Select-Object -First 150 | ForEach-Object { Log $_ }
            $actionDone = $true
        }
        'font' {
            # The Writer 2027 font picker opens on Alt+Down / dropdown click.
            # Find the font combo by likely names (EN + CS) and drive it with the keyboard.
            $combo = $null
            foreach ($n in @('Font Name','Název písma','btnHomeCharFontName')) {
                $combo = Find-ByName $win $n
                if ($combo) { break }
            }
            if (-not $combo) {
                # fall back to first ComboBox/Edit in the Home toolbar area
                $all = $win.FindAll(4, $uia.CreateTrueCondition())
                for ($i = 0; $i -lt $all.Length; $i++) {
                    $e = $all.GetElement($i)
                    if ($e.CurrentControlType -eq 50003 -or $e.CurrentControlType -eq 50004) {
                        if ($e.CurrentName -match 'Font|Písma|Char|Znak|písmo') { $combo = $e; break }
                    }
                }
            }
            if (-not $combo) { $actionDetail = 'font combo not found'; Log $actionDetail }
            else {
                Log "font combo found: '$($combo.CurrentName)'"
                $combo.SetFocus()
                Start-Sleep -Seconds 1
                Send-Keys '%{DOWN}'   # Alt+Down -> Writer2027FontPopup open path
                Start-Sleep -Seconds 3
                $actionDetail = 'sent Alt+Down to font combo'
                Log $actionDetail
            }
            $actionDone = $true
        }
        'managechanges' {
            # Sidebar deck tab button "Manage Changes" (localized: "Spravovat změny"?)
            $btn = $null
            foreach ($n in @('Manage Changes','Spravovat změny','ManageChanges','SwManageChangesDeck')) {
                $btn = Find-ByName $win $n
                if ($btn) { break }
            }
            if (-not $btn) {
                # try control-type Button + name containing 'Changes'/'změn'
                $all = $win.FindAll(4, $uia.CreateTrueCondition())
                for ($i = 0; $i -lt $all.Length; $i++) {
                    $e = $all.GetElement($i)
                    if ($e.CurrentControlType -eq 50000 -and $e.CurrentName -match 'Change|změn') { $btn = $e; break }
                }
            }
            if (-not $btn) { $actionDetail = 'Manage Changes button not found'; Log $actionDetail }
            else {
                Log "Manage Changes button found: '$($btn.CurrentName)'"
                $ip = $btn.GetCurrentPattern(10000)   # InvokePattern
                $ip.Invoke()
                Start-Sleep -Seconds 4
                $actionDetail = 'invoked Manage Changes deck button'
                Log $actionDetail
            }
            $actionDone = $true
        }
        'styles' {
            # Count style cards in the Home->Styles gallery. The gallery is an icon view
            # (ListItem children under a Pane/Custom control). Count ListItem descendants.
            $all = $win.FindAll(4, $uia.CreateTrueCondition())
            $cards = @()
            for ($i = 0; $i -lt $all.Length; $i++) {
                $e = $all.GetElement($i)
                if ($e.CurrentControlType -eq 50007) {   # ListItem
                    $cards += $e.CurrentName
                }
            }
            $actionDetail = "style cards found: $($cards.Count): $(($cards | Select-Object -First 30) -join ' | ')"
            Log "== $actionDetail =="
            $actionDone = $true
        }
    }
} catch {
    $actionDetail = "ACTION EXCEPTION: $($_.Exception.Message)"
    Log $actionDetail
}

# ---------------------------------------------------------------- liveness / crash check
Start-Sleep -Seconds 6
$alive = Get-Process -Id $pidMain -ErrorAction SilentlyContinue
$dumped = Test-Path $dumpFile
if (-not $alive) {
    if ($dumped) {
        Log "== CRASH DETECTED (dump: $dumpFile) =="
        # analyze with cdb
        $anOut = "$dumpDir\analyze_$($Scenario)_$stamp.txt"
        $psi = [System.Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $cdb
        $psi.WorkingDirectory = $instDir
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.ArgumentList.Add('-z'); $psi.ArgumentList.Add($dumpFile)
        $psi.ArgumentList.Add('-c'); $psi.ArgumentList.Add('.symfix; !analyze -v; q')
        $ap = [System.Diagnostics.Process]::Start($psi)
        $ao = $ap.StandardOutput.ReadToEnd()
        $ap.WaitForExit(120000) | Out-Null
        $ao | Set-Content -LiteralPath $anOut -Encoding Utf8
        $faultLine = ($ao -split "`n") | Where-Object { $_ -match 'FAULTING_MODULE|FAULTING_IP|FAILURE_BUCKET_ID|PROCESS_NAME|EXCEPTION_CODE|STACK_TEXT|swlo|svxcorelo|tbcontrl|writer2027' } | Select-Object -First 25
        FlushResult 'CRASH' "dump=$dumpFile`n$($faultLine -join "`n")`nfull analyze: $anOut"
    } else {
        FlushResult 'DIED-NO-DUMP' 'process died but no dump captured'
    }
} else {
    FlushResult 'PASS' $actionDetail
}

# ---------------------------------------------------------------- cleanup
Get-Process soffice, soffice.bin -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
Get-Process cdb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $profileDir '.lock') -Force -ErrorAction SilentlyContinue
Log '== cleanup done =='
