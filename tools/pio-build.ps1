# Helper to build / test the ESP32 firmware from a shell whose caller
# (e.g. the opencode tool wrapper) kills long-running foreground processes.
#
# It runs `pio` as a DETACHED child process, polls until it exits, and tails
# the log, so an interrupted wrapper cannot abort the build. It also puts the
# winget MinGW toolchain -- required by the `native` test env but absent from
# the default PATH -- on PATH automatically.
#
# Usage:
#   .\tools\pio-build.ps1                       # esp32 build + native tests
#   .\tools\pio-build.ps1 -Target esp32         # firmware build only
#   .\tools\pio-build.ps1 -Target native        # host unit tests only
#   .\tools\pio-build.ps1 -Clean                # clean rebuild (esp32)
#   .\tools\pio-build.ps1 -KillZombies          # kill stale pio/compiler procs first

param(
  [ValidateSet('esp32', 'native', 'both')]
  [string]$Target = 'both',
  [switch]$Clean,
  [switch]$KillZombies
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot    # repo root
$firmware = Join-Path $root 'firmware'
$tmp = Join-Path $env:TEMP 'pio-build'
New-Item -ItemType Directory -Path $tmp -Force | Out-Null

function Write-Step([string]$m) { Write-Host "==> $m" }

# ---- locate `pio` -----------------------------------------------------------
if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  $cands = @(
    Join-Path $env:LOCALAPPDATA 'Programs\Python\Python313\Scripts\pio.exe',
    Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\Scripts\pio.exe',
    Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'
  )
  $pioExe = $cands | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $pioExe) {
    Write-Error 'pio not found; add it to PATH or install PlatformIO.'
  }
  $env:PATH = (Split-Path $pioExe) + ';' + $env:PATH
}

# ---- put winget MinGW (native host compiler) on PATH ------------------------
$mingw = Get-ChildItem (
    Join-Path $env:USERPROFILE 'AppData\Local\Microsoft\WinGet\Packages'
  ) -Directory -Filter '*WinLibs*' -ErrorAction SilentlyContinue |
  Sort-Object Name -Descending | Select-Object -First 1
if ($mingw) {
  $bin = Join-Path $mingw.FullName 'mingw64\bin'
  if ((Test-Path (Join-Path $bin 'g++.exe')) -and ($env:PATH -notlike "*$bin*")) {
    $env:PATH = "$bin;" + $env:PATH
    Write-Step "MinGW for native tests: $bin"
  }
}

# ---- optional zombie cleanup -------------------------------------------------
if ($KillZombies) {
  Write-Step 'Killing stale pio/compiler processes...'
  $zombies = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match 'pio|python|cc1plus|xtensa-|clang|^as$'
  }
  foreach ($z in $zombies) {
    Stop-Process -Id $z.Id -Force -ErrorAction SilentlyContinue
  }
  Start-Sleep -Seconds 2
}

# ---- run one pio command detached and poll ----------------------------------
# NOTE: Start-Process -RedirectStandardOutput makes $p.ExitCode unreadable in
# PowerShell 5.1, so pio is wrapped in `cmd /v:on /c "..." & echo EXITCODE=..`
# and the real exit code is parsed back out of the log.
function Invoke-PioDetached {
  param(
    [string]$Name,
    [string[]]$Cmd,
    [string]$LogBase
  )
  $outLog = Join-Path $tmp "$LogBase.log"
  $errLog = Join-Path $tmp "$LogBase.err.log"
  $pidPath = Join-Path $tmp "$LogBase.pid"
  Remove-Item $outLog, $errLog, $pidPath -ErrorAction SilentlyContinue

  Write-Step "$Name -> pio $($Cmd -join ' ')"
  $cmdLine = "pio $($Cmd -join ' ') > `"$outLog`" 2> `"$errLog`" "
  $cmdLine += "& echo EXITCODE=!ERRORLEVEL! >> `"$errLog`""
  $proc = Start-Process -FilePath 'cmd.exe' `
    -ArgumentList @('/v:on', '/c', $cmdLine) `
    -WorkingDirectory $firmware -PassThru
  Set-Content -Path $pidPath -Value $proc.Id

  while (-not $proc.HasExited) {
    Start-Sleep -Seconds 10
    $proc.Refresh()
  }

  $failed = $true
  if (Test-Path $outLog) {
    Write-Host "--- $Name output (last 40 lines) ---"
    (Get-Content $outLog -Tail 40) | ForEach-Object { Write-Host $_ }
  }
  if (Test-Path $errLog) {
    $marker = Get-Content $errLog | Select-Object -Last 5
    foreach ($m in $marker) {
      if ($m -match 'EXITCODE=(\d+)') {
        $failed = [int]$Matches[1] -ne 0
      }
    }
  }
  if ($failed) { Write-Step "$Name FAILED (exit code unknown or non-zero)" }
  else { Write-Step "$Name SUCCESS" }
  return -not $failed
}

$ok = $true
if ($Target -eq 'esp32' -or $Target -eq 'both') {
  $cmd = @('run', '-e', 'esp32-s3')
  if ($Clean) { $cmd = @('run', '-t', 'clean', '-e', 'esp32-s3') }
  if (-not (Invoke-PioDetached -Name 'esp32-s3' -Cmd $cmd -LogBase 'esp32')) {
    $ok = $false
  }
}
if ($Target -eq 'native' -or $Target -eq 'both') {
  if (-not (Invoke-PioDetached -Name 'native' -Cmd @('test', '-e', 'native') -LogBase 'native')) {
    $ok = $false
  }
}

if (-not $ok) { exit 1 }
Write-Step 'All targets OK'