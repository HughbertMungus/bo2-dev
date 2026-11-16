# In-match A/B for bo2-dev (P2): alternates two sets of settings into run\live.txt every
# -Seconds seconds for -Cycles cycles, while REX_FPS_LOG tags every second with the switches in
# effect. Analyse afterwards with tools\fps_summary.py / tools\ab_summary.py.
#   powershell -ExecutionPolicy Bypass -File tools\ab_toggle.ps1 -A ab\all_off.txt -B ab\all_on.txt
param(
  [Parameter(Mandatory = $true)][string]$A,
  [Parameter(Mandatory = $true)][string]$B,
  [int]$Seconds = 25,
  [int]$Cycles = 6
)
$root = Split-Path -Parent $PSScriptRoot
$live = Join-Path $root 'run\live.txt'
$a = Get-Content -Raw -Path (Resolve-Path $A)
$b = Get-Content -Raw -Path (Resolve-Path $B)
for ($i = 1; $i -le $Cycles; $i++) {
  foreach ($pair in @(@('A', $a), @('B', $b))) {
    Set-Content -Path $live -Value $pair[1] -NoNewline -Encoding ascii
    Write-Output ("{0:HH:mm:ss} cycle {1}/{2}: {3}" -f (Get-Date), $i, $Cycles, $pair[0])
    Start-Sleep -Seconds $Seconds
  }
}
Set-Content -Path $live -Value $a -NoNewline -Encoding ascii
Write-Output ("{0:HH:mm:ss} done, left on A" -f (Get-Date))
