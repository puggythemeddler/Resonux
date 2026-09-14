# make-icon.ps1 — generates the Resonux brand icon set without any assets.
#
# The mark: a warm amber tile (the app's "filament" accent) with a dark
# "R" built from equalizer bars and a pale pulse dot — sound bars that form
# the letter, because Resonux is a music-reactive light.
#
# Outputs (desktop/build/):
#   icon.ico   multi-size Windows icon (16–256, PNG-compressed entries)
#   icon.png   512px transparent PNG master
#   logo.svg   the same mark as vector source
#
# Run from the desktop/ dir:  powershell -ExecutionPolicy Bypass -File build\make-icon.ps1

Add-Type -AssemblyName System.Drawing

$dark  = [System.Drawing.Color]::FromArgb(255, 42, 20, 6)   # accent-ink
$dot   = [System.Drawing.Color]::FromArgb(255, 255, 233, 207) # warm pale
$amberTop = [System.Drawing.Color]::FromArgb(255, 255, 177, 92)
$amberBot = [System.Drawing.Color]::FromArgb(255, 232, 89, 12)
$outDir = Join-Path $PSScriptRoot "."

function Get-RoundRect {
  param([System.Drawing.RectangleF]$r, [float]$rad)
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = 2 * $rad
  $p.AddArc($r.X, $r.Y, $d, $d, 180, 90)
  $p.AddArc($r.X + $r.Width - $d, $r.Y, $d, $d, 270, 90)
  $p.AddArc($r.X + $r.Width - $d, $r.Y + $r.Height - $d, $d, $d, 0, 90)
  $p.AddArc($r.X, $r.Y + $r.Height - $d, $d, $d, 90, 90)
  $p.CloseFigure()
  return $p
}

# Draw one frame at an arbitrary pixel size. All geometry lives in a 256-unit
# design space and is scaled by $s = $size / 256.
function Draw-Frame {
  param([int]$size)
  $s = $size / 256.0
  $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::Transparent)

  $r = [float]$s
  $Rf = { param([float]$x, [float]$y, [float]$w, [float]$h, [float]$radius) return Get-RoundRect ([System.Drawing.RectangleF]::new($x * $s, $y * $s, $w * $s, $h * $s)) ($radius * $s) }

  # Tile.
  $tile = & $Rf 8 8 240 240 56
  $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    [System.Drawing.RectangleF]::new(8 * $s, 8 * $s, 240 * $s, 240 * $s),
    $amberTop, $amberBot, 90.0)
  $g.FillPath($grad, $tile)

  # Soft top sheen (gloss) clipped to the tile.
  $sheen = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    [System.Drawing.RectangleF]::new(8 * $s, 8 * $s, 240 * $s, 120 * $s),
    [System.Drawing.Color]::FromArgb(90, 255, 255, 255),
    [System.Drawing.Color]::FromArgb(0, 255, 255, 255), 90.0)
  $g.FillPath($sheen, $tile)

  # The equalizer "R".
  $ink = New-Object System.Drawing.SolidBrush($dark)
  $g.FillPath($ink, (& $Rf 84 60 28 124 12))    # stem
  $g.FillPath($ink, (& $Rf 84 60 88 28 12))     # roof
  $g.FillPath($ink, (& $Rf 148 60 24 80 12))    # bowl wall
  $legPen = New-Object System.Drawing.Pen($dark, (24 * $s))
  $legPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $legPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $g.DrawLine($legPen, 124 * $s, 162 * $s, 184 * $s, 184 * $s) # leg

  # Pulse dot.
  $g.FillEllipse((New-Object System.Drawing.SolidBrush($dot)), 198 * $s, 54 * $s, 26 * $s, 26 * $s)

  $g.Dispose()
  return $bmp
}

# ---- vector source ---------------------------------------------------------
$svg = @'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffb15c"/>
      <stop offset="1" stop-color="#e8590c"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="56" fill="url(#tile)"/>
  <g fill="#2a1406">
    <rect x="84" y="60" width="28" height="124" rx="12"/>
    <rect x="84" y="60" width="88" height="28" rx="12"/>
    <rect x="148" y="60" width="24" height="80" rx="12"/>
    <line x1="124" y1="162" x2="184" y2="184" stroke="#2a1406" stroke-width="24" stroke-linecap="round"/>
  </g>
  <circle cx="211" cy="67" r="13" fill="#ffe9cf"/>
</svg>
'@
[System.IO.File]::WriteAllText((Join-Path $outDir "logo.svg"), $svg, (New-Object System.Text.UTF8Encoding($false)))

# ---- PNG master ------------------------------------------------------------
$bmp512 = Draw-Frame 512
$bmp512.Save((Join-Path $outDir "icon.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp512.Dispose()

# ---- ICO (multi-size, PNG entries) ----------------------------------------
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$blobs = @()
foreach ($sz in $sizes) {
  $b = Draw-Frame $sz
  $ms = New-Object System.IO.MemoryStream
  $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  $blobs += , @($sz, $ms.ToArray())
  $b.Dispose()
}

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($out)
$w.Write([UInt16]0)          # reserved
$w.Write([UInt16]1)          # type: icon
$w.Write([UInt16]$blobs.Count)
$offset = 6 + 16 * $blobs.Count
foreach ($b in $blobs) {
  $sz = $b[0]; $data = $b[1]
  $w.Write([Byte]($(if ($sz -ge 256) { 0 } else { $sz })))
  $w.Write([Byte]($(if ($sz -ge 256) { 0 } else { $sz })))
  $w.Write([Byte]0)          # palette
  $w.Write([Byte]0)          # reserved
  $w.Write([UInt16]1)        # planes
  $w.Write([UInt16]32)       # bpp
  $w.Write([UInt32]$data.Length)
  $w.Write([UInt32]$offset)
  $offset += $data.Length
}
foreach ($b in $blobs) { $w.Write($b[1]) }
$w.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $outDir "icon.ico"), $out.ToArray())
$w.Dispose(); $out.Dispose()

Write-Output "wrote:"
Get-ChildItem -Path $outDir -Filter "icon.*" | ForEach-Object { "  $($_.Name) - $($_.Length) bytes" }
Get-ChildItem -Path $outDir -Filter "logo.svg" | ForEach-Object { "  $($_.Name) - $($_.Length) bytes" }