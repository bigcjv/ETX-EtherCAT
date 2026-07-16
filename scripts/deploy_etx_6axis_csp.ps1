param(
    [string]$HostName = "192.168.0.10",
    [string]$UserName = "tpm",
    [string]$Password = "",
    [string]$RemoteDir = "/home/tpm/etx_6axis_csp",
    [string]$HostKey = "SHA256:xBzCH8w5lTi3ExQakSZm9lXGXlcDDXBhhNumThbSNGE",
    [switch]$Build,
    [switch]$RunDryCheck,
    [switch]$RunMotion
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

Write-Host "Installing ENI to /usr/lib/ECPL/ENI/ENI.xml ..."
if ($Password -ne "") {
    $quotedPassword = Quote-BashSingle $Password
    Invoke-Plink "printf '%s\n' $quotedPassword | sudo -S mkdir -p /usr/lib/ECPL/ENI && printf '%s\n' $quotedPassword | sudo -S cp '$RemoteDir/6axis_eni/ENI.xml' /usr/lib/ECPL/ENI/ENI.xml && printf '%s\n' $quotedPassword | sudo -S chmod 644 /usr/lib/ECPL/ENI/ENI.xml"
} else {
    Invoke-Plink "sudo mkdir -p /usr/lib/ECPL/ENI && sudo cp '$RemoteDir/6axis_eni/ENI.xml' /usr/lib/ECPL/ENI/ENI.xml && sudo chmod 644 /usr/lib/ECPL/ENI/ENI.xml"
}

if ($Build) {
    Write-Host "Building on ETX ..."
    Invoke-Plink "cd '$RemoteDir' && make clean && make"
}

if ($RunDryCheck) {
    Write-Host "Running dry check on ETX ..."
    Invoke-RemoteSudo "cd '$RemoteDir' && ./etx_6axis_csp_demo --eni ENI.xml --axes 6"
}

if ($RunMotion) {
    Write-Host "Running conservative 6-axis CSP motion on ETX ..."
    Invoke-RemoteSudo "cd '$RemoteDir' && ./etx_6axis_csp_demo --eni ENI.xml --axes 6 --enable-motion --cycles 1 --distance 1000 --feed 500 --accel 1000 --decel 1000"
}

Write-Host "Done."
