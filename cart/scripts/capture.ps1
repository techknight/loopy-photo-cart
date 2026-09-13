# Launch LoopyMSE with a ROM, optionally drive it with the keyboard, and
# screenshot the emulator window. Useful for checking rendering changes
# without having to flash a cart.
#
# LoopyMSE and the Casio Loopy BIOS are not part of this repository. Point
# -LoopyMSEPath at your own copy, or set the LOOPYMSE_PATH environment
# variable. The folder is expected to contain LoopyMSE.exe, its SDL DLLs, a
# loopymse.ini naming the BIOS files, and the BIOS images themselves
# (bios.bin + soundbios.bin).
#
# Usage:
#   cart\scripts\capture.ps1 -Rom cart\build\fixture-demo.bin -Out grid.png
#   cart\scripts\capture.ps1 -Rom cart\build\fixture-demo.bin -Keys "Z,RIGHT,RIGHT" -Shots 3
#
# Keys are comma separated, from: ENTER UP DOWN LEFT RIGHT Z X C V Q W
# (per the [keyboard-map] in the emulator's loopymse.ini: Enter=Start, Z=A,
# X=B, C=C, V=D, Q=L1, W=R1). Those letters belong to that file, not to this
# script: rebind a pad button there and the $vk table below has to follow, or
# the old letter is still accepted here and then does nothing in the
# emulator.
# Append ":milliseconds" to hold a key longer, e.g. "RIGHT:2000".

param(
    [Parameter(Mandatory = $true)]
    [string]$Rom,
    [string]$LoopyMSEPath = $env:LOOPYMSE_PATH,
    [int]$WaitSeconds = 8,
    [string]$Out = "shot.png",
    [string]$Keys = "",
    # Take a burst of frames instead of one, so motion is visible. Files are
    # named <Out-basename>_1.png, _2.png, ...
    [int]$Shots = 1,
    [int]$ShotIntervalMs = 700,
    # A key to hold *down across the screenshots* and release afterwards.
    # -Keys runs to completion before any shot is taken, so it cannot capture
    # anything that only exists while a button is held, such as the grid
    # cursor repeating across a page.
    [string]$HoldKey = "",
    # Shoot the raw VDP raster instead of the window as the user sees it.
    #
    # The default emulator settings are for looking, not for measuring: with
    # correct_aspect_ratio the horizontal scale is 4.57x against a vertical 4x,
    # antialias blends the seams, and crop_overscan shows only the 256x224
    # window of the 280x240 raster starting at (12, 8). Coordinates read off
    # such a shot are wrong by (12, 8) and drift by ~20px across the screen.
    #
    # -Native runs the emulator with those three off at int_scale = -NativeScale
    # and grabs the *client* rect (no title bar, no invisible Win11 border), so
    # at the default scale of 1 the PNG is the 280x240 raster pixel for pixel,
    # with the 256-pixel-wide picture starting 12 pixels in.
    # loopymse.ini is restored afterwards.
    [switch]$Native,
    [int]$NativeScale = 1
)

Add-Type -AssemblyName System.Drawing

$sig = @'
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
[DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
[StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
[StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
'@
if (-not ("Win32Cap" -as [type])) {
    Add-Type -MemberDefinition $sig -Name Win32Cap -Namespace ""
}

# Resolve the ROM before changing directory, so relative paths work.
$romFull = (Resolve-Path -LiteralPath $Rom -ErrorAction Stop).Path

if (-not $LoopyMSEPath) { $LoopyMSEPath = Join-Path $PSScriptRoot "..\..\..\emu" }
if (-not (Test-Path (Join-Path $LoopyMSEPath "LoopyMSE.exe"))) {
    Write-Error "LoopyMSE.exe not found in '$LoopyMSEPath'. Pass -LoopyMSEPath or set LOOPYMSE_PATH."
    exit 1
}

$outDir = Get-Location
Set-Location $LoopyMSEPath

# -Native: swap in measuring settings for the run, restore them afterwards.
$iniPath = Join-Path $LoopyMSEPath "loopymse.ini"
$iniSaved = $null
if ($Native) {
    $iniSaved = Get-Content $iniPath -Raw
    ($iniSaved `
        -replace '(?m)^correct_aspect_ratio\s*=.*$', 'correct_aspect_ratio=false' `
        -replace '(?m)^antialias\s*=.*$', 'antialias=false' `
        -replace '(?m)^crop_overscan\s*=.*$', 'crop_overscan=false' `
        -replace '(?m)^int_scale\s*=.*$', "int_scale=$NativeScale") |
        Set-Content $iniPath -NoNewline
}

try {

$p = Start-Process -FilePath .\LoopyMSE.exe `
    -ArgumentList "`"$romFull`"" `
    -RedirectStandardOutput cap_out.txt -RedirectStandardError cap_err.txt `
    -PassThru -NoNewWindow

Start-Sleep -Seconds 3
try { $p.WaitForInputIdle(3000) | Out-Null } catch {}

# Give the ROM time to boot and draw.
Start-Sleep -Seconds $WaitSeconds

if ($p.HasExited) {
    Write-Output "EMULATOR EXITED EARLY code=$($p.ExitCode)"
    Get-Content cap_err.txt -ErrorAction SilentlyContinue | Select-Object -First 10
    Set-Location $outDir
    exit 1
}

$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) {
    Write-Output "NO WINDOW HANDLE"
    $p.Kill(); Set-Location $outDir; exit 1
}

[Win32Cap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700

# SDL reads the keyboard from the low-level input queue, which SendKeys does
# not reach reliably, so synthesise real key events with keybd_event. Hold
# each key long enough for the cartridge to see it.
$vk = @{
    'ENTER' = 0x0D; 'UP' = 0x26; 'DOWN' = 0x28; 'LEFT' = 0x25; 'RIGHT' = 0x27
    'Z' = 0x5A; 'X' = 0x58; 'C' = 0x43; 'V' = 0x56; 'Q' = 0x51; 'W' = 0x57
}
$KEYEVENTF_KEYUP = 0x2
$KEYEVENTF_EXTENDEDKEY = 0x1

function Send-Key([int]$code, [int]$holdMs) {
    $scan = [Win32Cap]::MapVirtualKey([uint32]$code, 0)
    # Arrow keys need the extended-key flag.
    $ext = 0
    if ($code -in 0x25, 0x26, 0x27, 0x28) { $ext = $KEYEVENTF_EXTENDEDKEY }
    [Win32Cap]::keybd_event([byte]$code, [byte]$scan, [uint32]$ext, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $holdMs
    [Win32Cap]::keybd_event([byte]$code, [byte]$scan, [uint32]($ext -bor $KEYEVENTF_KEYUP), [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}

if ($Keys -ne "") {
    foreach ($k in $Keys.Split(',')) {
        $name = $k.Trim().ToUpper()
        $hold = 200
        if ($name -match '^(.*):(\d+)$') { $name = $Matches[1]; $hold = [int]$Matches[2] }
        if ($vk.ContainsKey($name)) {
            Send-Key $vk[$name] $hold
        } else {
            Write-Output "unknown key: $name"
        }
    }
    # Settle before shooting; kept short when a key is being held for the
    # capture, so the held key's effect is still on screen.
    if ($HoldKey -ne "") { Start-Sleep -Milliseconds 300 }
    else { Start-Sleep -Seconds 2 }
}

# Press and keep pressed. Released after the last shot.
$holdCode = 0
if ($HoldKey -ne "") {
    $hk = $HoldKey.Trim().ToUpper()
    if ($vk.ContainsKey($hk)) {
        $holdCode = $vk[$hk]
        $hscan = [Win32Cap]::MapVirtualKey([uint32]$holdCode, 0)
        $hext = 0
        if ($holdCode -in 0x25, 0x26, 0x27, 0x28) { $hext = $KEYEVENTF_EXTENDEDKEY }
        [Win32Cap]::keybd_event([byte]$holdCode, [byte]$hscan, [uint32]$hext, [UIntPtr]::Zero)
        # Give the cartridge a few frames to see the key and redraw.
        Start-Sleep -Milliseconds 500
    } else {
        Write-Output "unknown hold key: $hk"
    }
}

$r = New-Object Win32Cap+RECT
[Win32Cap]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left
$ht = $r.Bottom - $r.Top
Write-Output "window ${w}x${ht} at $($r.Left),$($r.Top)"

if ($Native) {
    # The window is placed by SDL and has been seen straddling the top of the
    # desktop; CopyFromScreen off the desktop returns garbage, so move it fully
    # into view first (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE).
    if ($r.Left -lt 0 -or $r.Top -lt 0) {
        [Win32Cap]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, 0, 0, 0x15) | Out-Null
        Start-Sleep -Milliseconds 300
    }
    # Client rect, converted to screen coordinates: the raster and nothing else.
    $c = New-Object Win32Cap+RECT
    [Win32Cap]::GetClientRect($h, [ref]$c) | Out-Null
    $o = New-Object Win32Cap+POINT
    [Win32Cap]::ClientToScreen($h, [ref]$o) | Out-Null
    $r.Left = $o.X; $r.Top = $o.Y
    $w = $c.Right - $c.Left
    $ht = $c.Bottom - $c.Top
    Write-Output "client ${w}x${ht} at $($r.Left),$($r.Top) (raster $($w / $NativeScale)x$($ht / $NativeScale) at ${NativeScale}x)"
}

# Whatever came to the front during the wait (a browser, a terminal) is
# what CopyFromScreen would shoot: put the emulator above everything for
# the shots (HWND_TOPMOST, SWP_NOMOVE | SWP_NOSIZE).
[Win32Cap]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x3) | Out-Null
[Win32Cap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400

$base = [System.IO.Path]::GetFileNameWithoutExtension($Out)
$ext = [System.IO.Path]::GetExtension($Out)

for ($s = 1; $s -le $Shots; $s++) {
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)

    $name = if ($Shots -eq 1) { $Out } else { "${base}_${s}${ext}" }
    $bmp.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "saved $name"

    if ($s -lt $Shots) { Start-Sleep -Milliseconds $ShotIntervalMs }
}

if ($holdCode -ne 0) {
    [Win32Cap]::keybd_event([byte]$holdCode, [byte]$hscan,
        [uint32]($hext -bor $KEYEVENTF_KEYUP), [UIntPtr]::Zero)
}

$p.Kill()

} finally {
    if ($iniSaved -ne $null) { Set-Content $iniPath $iniSaved -NoNewline }
    Set-Location $outDir
}
