# Stops the bo2-dev game (only processes running from bo2-dev\run), waits until the files are free,
# copies the built exe + DLLs into run\ and checks the hashes. Never touches bo2-bisect.
# -Prof deploys the profiling build (build_prof.cmd) to run-prof\ instead, seeding its shader cache
# from run\cache_mp on first use (a new folder otherwise starts with a cold pipeline cache).
param([switch]$Prof)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'app\out\build\win-amd64\Release'
$dst = Join-Path $root 'run'
if ($Prof) {
  $src = Join-Path $root 'app\out\build\win-amd64-prof\RelWithDebInfo'
  $dst = Join-Path $root 'run-prof'
  New-Item -ItemType Directory -Force $dst | Out-Null
  $cache = Join-Path $root 'run\cache_mp'
  if ((Test-Path $cache) -and -not (Test-Path (Join-Path $dst 'cache_mp'))) {
    Copy-Item $cache (Join-Path $dst 'cache_mp') -Recurse
  }
}
Get-Process defaultmp -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dst\*" } |
  Stop-Process -Force -Confirm:$false
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Process defaultmp -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dst\*" }) -and
       (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
$files = Get-ChildItem $src -File | Where-Object { $_.Extension -in '.exe', '.dll' }
foreach ($f in $files) {
  for ($try = 0; $try -lt 40; $try++) {
    try { Copy-Item $f.FullName $dst -Force; break } catch { Start-Sleep -Milliseconds 250 }
  }
}
$bad = $files | Where-Object { (Get-FileHash $_.FullName).Hash -ne (Get-FileHash (Join-Path $dst $_.Name)).Hash }
if ($bad) { Write-Output ("MISMATCH: " + ($bad.Name -join ', ')); exit 1 }
Write-Output ("deployed {0} files, exe {1}" -f $files.Count, (Get-FileHash (Join-Path $dst 'defaultmp.exe')).Hash.Substring(0, 12))
