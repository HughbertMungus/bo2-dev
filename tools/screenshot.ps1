# Saves the game window to a PNG without bringing it to the front (PrintWindow with
# PW_RENDERFULLCONTENT, so a covered window still captures and the player keeps focus).
# Ported from bo2-bisect tools/capture.ps1, which used a screen copy after SetForegroundWindow.
# Usage: powershell -ExecutionPolicy Bypass -File tools\screenshot.ps1 [-Out dumps\shot.png]
param(
    [string]$Out = "",
    [string]$ProcName = "defaultmp"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinShot {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
'@
if (-not $Out) {
    $root = Split-Path -Parent $PSScriptRoot
    New-Item -ItemType Directory -Force (Join-Path $root 'dumps') | Out-Null
    $Out = Join-Path $root ("dumps\shot_{0:yyyyMMdd_HHmmss}.png" -f (Get-Date))
}
$proc = Get-Process -Name $ProcName -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } | Select-Object -First 1
if (-not $proc) { throw "no $ProcName window" }
$h = $proc.MainWindowHandle
$rect = New-Object WinShot+RECT
[void][WinShot]::GetWindowRect($h, [ref]$rect)
$w = $rect.Right - $rect.Left
$hh = $rect.Bottom - $rect.Top
if ($w -le 0 -or $hh -le 0) { throw "bad window rect ${w}x${hh} (minimized?)" }
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [WinShot]::PrintWindow($h, $hdc, 2)
$g.ReleaseHdc($hdc)
$g.Dispose()
if (-not $ok) { $bmp.Dispose(); throw "PrintWindow failed" }
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "captured ${w}x${hh} -> $Out"
