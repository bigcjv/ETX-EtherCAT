param(
    [string]$HostName = "192.168.0.10",
    [string]$UserName = "tpm",
    [string]$Password = "",
    [string]$RemoteDir = "/home/tpm/etx_6axis_csp",
    [string]$HostKey = "SHA256:xBzCH8w5lTi3ExQakSZm9lXGXlcDDXBhhNumThbSNGE",
    [ValidateSet(125, 250, 500, 1000, 2000, 4000, 8000, 10000)]
    [int]$CycleTimeUs = 500,
    [switch]$PrepareEniExport,
    [switch]$ConfirmAxesStoppedAndServoOff,
    [switch]$ConfigureCycleTime,
    [switch]$ActivateScenario1,
    [switch]$Build,
    [switch]$RunDryCheck,
    [switch]$RunMotion,
    [switch]$ConfirmMotionSafety
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$projectDir = Join-Path $repoRoot "src\etx_6axis_csp"
$eniFile = Join-Path $repoRoot "6axis_eni\ENI.xml"
$puttyDir = "C:\Program Files\PuTTY"
$plink = Join-Path $puttyDir "plink.exe"
$pscp = Join-Path $puttyDir "pscp.exe"

if (!(Test-Path $plink) -or !(Test-Path $pscp)) {
    throw "PuTTY plink/pscp not found under $puttyDir. Install PuTTY or edit this script."
}
if (!(Test-Path $eniFile)) {
    throw "ENI file not found: $eniFile"
}
if ($ConfigureCycleTime -and !$ActivateScenario1) {
    throw "-ConfigureCycleTime requires -ActivateScenario1 so no other master owns EtherCAT."
}
if ($RunMotion -and (!$RunDryCheck -or !$ConfirmMotionSafety)) {
    throw "-RunMotion requires -RunDryCheck and -ConfirmMotionSafety."
}

function Get-LittleEndianCycleData {
    param([int]$Microseconds)
    $bytes = [BitConverter]::GetBytes([uint64]$Microseconds * 1000)
    return (($bytes | ForEach-Object { $_.ToString("X2") }) -join "")
}

function Assert-EniCycleConfiguration {
    param([string]$Path, [int]$Microseconds, [int]$AxisCount = 6)

    [xml]$eni = Get-Content -LiteralPath $Path -Raw
    $slaves = @($eni.SelectNodes("//Config/Slave"))
    if ($slaves.Count -ne $AxisCount -or
        @($slaves | Where-Object { $_.Info.Name -notmatch "MADLT05BF" }).Count -ne 0) {
        throw "ENI must contain exactly $AxisCount MADLT05BF drives."
    }

    $dcNodes = @($eni.SelectNodes("//Slave/DC"))
    if ($dcNodes.Count -ne $AxisCount) {
        throw "ENI must contain $AxisCount DC nodes; found $($dcNodes.Count). Re-export it from ECATNavi."
    }

    $expectedNs = [string]([uint64]$Microseconds * 1000)
    foreach ($dc in $dcNodes) {
        if ($dc.CycleTime0 -ne $expectedNs -or $dc.CycleTime1 -ne $expectedNs) {
            throw "ENI DC cycle must be $expectedNs ns on all axes. Re-export it for $Microseconds us."
        }
    }

    $referenceClocks = @($dcNodes | Where-Object {
        $_.ReferenceClock.Trim().ToLowerInvariant() -eq "true"
    })
    if ($referenceClocks.Count -ne 1) {
        throw "ENI must contain exactly one DC reference clock; found $($referenceClocks.Count)."
    }

    $expectedData = Get-LittleEndianCycleData $Microseconds
    $cycleWrites = @($eni.SelectNodes(
        "//Slave/InitCmds/InitCmd[normalize-space(Comment)='set DC cycle time']/Data"))
    if ($cycleWrites.Count -ne $AxisCount -or
        @($cycleWrites | Where-Object { $_.InnerText.Trim().ToUpperInvariant() -ne $expectedData }).Count -ne 0) {
        throw "ENI DC register writes do not all encode $Microseconds us ($expectedData). Re-export it; do not edit XML bytes manually."
    }

    $dcActivationWrites = @($eni.SelectNodes(
        "//Slave/InitCmds/InitCmd[normalize-space(Comment)='set DC activation']/Data"))
    if ($dcActivationWrites.Count -ne $AxisCount -or
        @($dcActivationWrites | Where-Object { $_.InnerText.Trim().ToUpperInvariant() -ne "0003" }).Count -ne 0) {
        throw "ENI must enable DC SYNC0 (activation data 0003) on all $AxisCount drives."
    }

    Write-Host "Validated ENI: $AxisCount DC axes, one reference clock, cycle $Microseconds us."
}

function Invoke-Plink {
    param([string]$Command)
    $args = @("-ssh", "$UserName@$HostName", "-hostkey", $HostKey)
    if ($Password -ne "") {
        $args += @("-pw", $Password, "-batch")
    }
    $args += $Command
    & $plink @args
    if ($LASTEXITCODE -ne 0) {
        throw "plink failed with exit code $LASTEXITCODE"
    }
}

function Quote-BashSingle {
    param([string]$Value)
    return "'" + $Value.Replace("'", "'\''") + "'"
}

function Invoke-RemoteSudo {
    param([string]$Command)
    if ($Password -ne "") {
        $quotedPassword = Quote-BashSingle $Password
        Invoke-Plink "printf '%s\n' $quotedPassword | sudo -S sh -c $(Quote-BashSingle $Command)"
    } else {
        Invoke-Plink "sudo sh -c $(Quote-BashSingle $Command)"
    }
}

function Set-RemoteCycleTime {
    param([int]$Microseconds)

    $backupSetting = "import shutil,time; p='/usr/lib/ECPL/Config/Setting.json'; shutil.copy2(p,p+'.bak.'+time.strftime('%Y%m%d_%H%M%S'))"
    Invoke-RemoteSudo "python3 -c $(Quote-BashSingle $backupSetting)"

    $updateSetting = "import json; p='/usr/lib/ECPL/Config/Setting.json'; d=json.load(open(p)); d['CycleTime']=$Microseconds; d['ENABLE_DC']=True; f=open(p,'w'); json.dump(d,f,indent=2); f.write('\\n'); f.close()"
    Invoke-RemoteSudo "python3 -c $(Quote-BashSingle $updateSetting)"
}

if ($PrepareEniExport) {
    if (!$ConfirmAxesStoppedAndServoOff) {
        throw "-PrepareEniExport requires -ConfirmAxesStoppedAndServoOff."
    }
    if ($Build -or $RunDryCheck -or $RunMotion -or $ConfigureCycleTime -or $ActivateScenario1) {
        throw "-PrepareEniExport cannot be combined with deployment or Scenario 1 switches."
    }

    Write-Host "Preparing Scenario 2 for a new $CycleTimeUs us ENI export ..."
    Invoke-RemoteSudo "systemctl stop etx.service"
    Invoke-Plink "if pgrep -f '[e]tx_6axis_csp_demo' >/dev/null; then echo 'Motion program is active.' >&2; exit 1; fi"
    Set-RemoteCycleTime $CycleTimeUs
    Invoke-RemoteSudo "systemctl enable --now etx.service"
    Invoke-Plink "grep -E 'CycleTime|ENABLE_DC' /usr/lib/ECPL/Config/Setting.json; systemctl is-active etx.service; ss -lnt | grep ':5886'"
    Write-Host "Scenario 2 is ready. Re-open Device Settings -> DC, apply DC SYNC0 x1 to all six axes, then export ENI.xml."
    return
}

Assert-EniCycleConfiguration -Path $eniFile -Microseconds $CycleTimeUs

Write-Host "Preparing remote directory $RemoteDir ..."
Invoke-Plink "mkdir -p '$RemoteDir' '$RemoteDir/6axis_eni'"

Write-Host "Uploading source ..."
if ($Password -ne "") {
    & $pscp -r -pw $Password -batch -hostkey $HostKey "$projectDir\*" "$UserName@${HostName}:$RemoteDir/"
} else {
    & $pscp -r -hostkey $HostKey "$projectDir\*" "$UserName@${HostName}:$RemoteDir/"
}
if ($LASTEXITCODE -ne 0) {
    throw "pscp source upload failed with exit code $LASTEXITCODE"
}

Write-Host "Uploading ENI ..."
if ($Password -ne "") {
    & $pscp -pw $Password -batch -hostkey $HostKey $eniFile "$UserName@${HostName}:$RemoteDir/6axis_eni/ENI.xml"
} else {
    & $pscp -hostkey $HostKey $eniFile "$UserName@${HostName}:$RemoteDir/6axis_eni/ENI.xml"
}
if ($LASTEXITCODE -ne 0) {
    throw "pscp ENI upload failed with exit code $LASTEXITCODE"
}

if ($Build) {
    Write-Host "Building on ETX ..."
    Invoke-Plink "cd '$RemoteDir' && make clean && make"
}

if ($ActivateScenario1) {
    Write-Host "Activating Scenario 1 ownership (disabling etx.service) ..."
    Invoke-RemoteSudo "systemctl disable --now etx.service"
}

Invoke-Plink "if systemctl is-active --quiet etx.service || pgrep -f '[e]tx_6axis_csp_demo' >/dev/null; then echo 'Another EtherCAT master is active.' >&2; exit 1; fi"

if ($ConfigureCycleTime) {
    Write-Host "Backing up and setting ETX master cycle to $CycleTimeUs us ..."
    Set-RemoteCycleTime $CycleTimeUs
}

$verifySetting = "import json,sys; d=json.load(open('/usr/lib/ECPL/Config/Setting.json')); ok=d.get('CycleTime')==$CycleTimeUs and d.get('ENABLE_DC') is True; print('ETX CycleTime=%s us ENABLE_DC=%s' % (d.get('CycleTime'),d.get('ENABLE_DC'))); sys.exit(0 if ok else 1)"
Invoke-Plink "python3 -c $(Quote-BashSingle $verifySetting)"

Write-Host "Backing up and installing ENI to /usr/lib/ECPL/ENI/ENI.xml ..."
$backupEni = "import os,shutil,time; p='/usr/lib/ECPL/ENI/ENI.xml'; os.path.exists(p) and shutil.copy2(p,p+'.bak.'+time.strftime('%Y%m%d_%H%M%S'))"
Invoke-RemoteSudo "python3 -c $(Quote-BashSingle $backupEni)"
if ($Password -ne "") {
    $quotedPassword = Quote-BashSingle $Password
    Invoke-Plink "printf '%s\n' $quotedPassword | sudo -S mkdir -p /usr/lib/ECPL/ENI && printf '%s\n' $quotedPassword | sudo -S cp '$RemoteDir/6axis_eni/ENI.xml' /usr/lib/ECPL/ENI/ENI.xml && printf '%s\n' $quotedPassword | sudo -S chmod 644 /usr/lib/ECPL/ENI/ENI.xml"
} else {
    Invoke-Plink "sudo mkdir -p /usr/lib/ECPL/ENI && sudo cp '$RemoteDir/6axis_eni/ENI.xml' /usr/lib/ECPL/ENI/ENI.xml && sudo chmod 644 /usr/lib/ECPL/ENI/ENI.xml"
}

if ($RunDryCheck) {
    Write-Host "Running dry check on ETX ..."
    Invoke-RemoteSudo "cd '$RemoteDir' && ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us $CycleTimeUs"
}

if ($RunMotion) {
    Write-Host "Running conservative 6-axis CSP motion on ETX ..."
    Invoke-RemoteSudo "cd '$RemoteDir' && ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --cycle-us $CycleTimeUs --enable-motion --cycles 1 --distance 1000 --feed 500 --accel 1000 --decel 1000"
}

Write-Host "Done."
