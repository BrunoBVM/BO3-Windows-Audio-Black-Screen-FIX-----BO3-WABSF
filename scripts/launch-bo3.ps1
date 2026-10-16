<#
.SYNOPSIS
  Launch Black Ops III with the winmm clamp proxy installed for this session only.

.DESCRIPTION
  Default mode (LAN / solo): copies dsound.dll and a one-line dsound_proxy.cfg into the game
  folder, starts the game through Steam, waits for it to exit, then removes both again.
  -Online: removes any leftover proxy and starts the game normally (no proxy, no clamp).

  Do NOT connect to any online servers while a game started in the default mode is
  running. If the script is killed mid-game, run it once with -Online to clean up.

  This script has not been tested widely. Read it before running it.

.EXAMPLE
  .\launch-bo3.ps1 -GameDir "D:\SteamLibrary\steamapps\common\Call of Duty Black Ops III"
  .\launch-bo3.ps1 -GameDir "D:\SteamLibrary\steamapps\common\Call of Duty Black Ops III" -Online
#>
param(
    [Parameter(Mandatory = $true)][string]$GameDir,
    [string]$ProxyDll = (Join-Path $PSScriptRoot "..\dsound.dll"),
    [int]$Cap = 16,
    [switch]$Online
)

$ErrorActionPreference = "Stop"
$dst   = Join-Path $GameDir "dsound.dll"
$cfg   = Join-Path $GameDir "dsound_proxy.cfg"
$appId = 311210

function Remove-Proxy {
    foreach ($f in @($dst, $cfg)) {
        if (Test-Path $f) { Remove-Item $f -Force -ErrorAction SilentlyContinue }
    }
    if ((Test-Path $dst) -or (Test-Path $cfg)) {
        Write-Warning "Could not remove the proxy files from $GameDir. Delete dsound.dll and dsound_proxy.cfg manually before playing online."
    }
}

if (-not (Test-Path (Join-Path $GameDir "BlackOps3.exe"))) {
    throw "BlackOps3.exe not found in $GameDir"
}
if (Get-Process BlackOps3 -ErrorAction SilentlyContinue) {
    throw "BlackOps3 is already running. Close it first."
}

if ($Online) {
    Remove-Proxy
    Write-Host "Proxy removed. Starting the game normally (the black screen bug may return)."
    Start-Process "steam://rungameid/$appId"
    return
}

if (-not (Test-Path $ProxyDll)) { throw "Proxy DLL not found: $ProxyDll" }

try {
    Copy-Item $ProxyDll $dst -Force
    Set-Content -Path $cfg -Value $Cap -Encoding ASCII
    Write-Host "Proxy installed (cap $Cap). LAN / solo only: do not join online servers."
    Start-Process "steam://rungameid/$appId"

    $deadline = (Get-Date).AddMinutes(3)
    while (-not (Get-Process BlackOps3 -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $deadline) { throw "The game did not start within 3 minutes." }
        Start-Sleep -Seconds 2
    }
    Wait-Process -Name BlackOps3
}
finally {
    Remove-Proxy
    Write-Host "Proxy removed."
}
