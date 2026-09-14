# make-icon.ps1 - generates the Resonux brand asset set with no binary inputs.
#
# Concept "Luminous Node at Resonance":
#   A resonant ring holds a standing-wave filament that winds to the cavity
#   wall and ignites into a single point of warm light. Resonance becomes
#   visible as light. The tile is dark graphite hardware; the mark is the
#   light.
#
# Outputs (desktop/build/):
#   icon.ico                multi-size Windows icon (16-256, PNG entries)
#   icon.png                1024px tile master
#   logo.svg                primary lockup (tile + mark)
#   logo-mark.svg           standalone full-colour mark (transparent)
#   logo-mark-dark.svg      monochrome mark for dark surfaces
#   logo-mark-light.svg     monochrome mark for light surfaces
#   wordmark.svg            RESONUX wordmark (graphite)
#   wordmark-light.svg      RESONUX wordmark (off-white)
#   logo-lockup.svg         tile mark + RESONUX (graphite)
#   logo-lockup-light.svg   tile mark + RESONUX (off-white)
#   logo-mark-128.png       transparent PNG of the mark (128)
#   favicon.png             transparent PNG of the mark (64)
#
# Run from desktop/:  powershell -ExecutionPolicy Bypass -File build\make-icon.ps1

Add-Type -AssemblyName System.Drawing

$outDir = Join-Path $PSScriptRoot "."

# ---- palette ---------------------------------------------------------------
$graphiteTop = [System.Drawing.Color]::FromArgb(255, 35, 40, 49)
$graphiteBot = [System.Drawing.Color]::FromArgb(255, 20, 23, 27)
$amber       = [System.Drawing.Color]::FromArgb(255, 245, 154, 62)   # filament + ring
$ignite      = [System.Drawing.Color]::FromArgb(255, 255, 233, 207)  # the point of light
$darkInk     = [System.Drawing.Color]::FromArgb(255, 28, 32, 38)     # monochrome on light
$lightInk    = [System.Drawing.Color]::FromArgb(255, 238, 240, 244)  # monochrome on dark

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

# Draw the mark (ring + filament + node) onto a fresh bitmap. Design space 256.
# NOTE: no [switch] params here - a function with a switch parameter that also
# returns .NET draw objects mis-binds a call-operator result in PS 5.1.
function Draw-MarkFrame {
  param([int]$size)
  $s = $size / 256.0
  $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::Transparent)

  $pen = [System.Drawing.Pen]::new($amber, [float](10.0 * $s))
  $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $g.DrawEllipse($pen, (128.0 - 86.0) * $s, (130.0 - 86.0) * $s, 172.0 * $s, 172.0 * $s)

  $fil = [System.Drawing.Drawing2D.GraphicsPath]::new()
  $fil.AddBezier(63 * $s, 192 * $s, 100 * $s, 192 * $s, 98 * $s, 136 * $s, 128 * $s, 130 * $s)
  $fil.AddBezier(128 * $s, 130 * $s, 158 * $s, 124 * $s, 162 * $s, 84 * $s, 192 * $s, 74 * $s)
  $g.DrawPath($pen, $fil)

  $node = [System.Drawing.SolidBrush]::new($ignite)
  $g.FillEllipse($node, (192.0 - 17.0) * $s, (74.0 - 17.0) * $s, 34.0 * $s, 34.0 * $s)

  $g.Dispose()
  return $bmp
}

# Draw-IconFrame = graphite tile + the mark (the Windows app icon).
function Draw-IconFrame {
  param([int]$size)
  $s = $size / 256.0
  $bmp = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::Transparent)

  $Rf = { param([float]$x, [float]$y, [float]$w, [float]$h, [float]$radius) return Get-RoundRect ([System.Drawing.RectangleF]::new($x * $s, $y * $s, $w * $s, $h * $s)) ($radius * $s) }

  $tile = & $Rf 8 8 240 240 56
  $grad = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
    [System.Drawing.RectangleF]::new(8 * $s, 8 * $s, 240 * $s, 240 * $s), $graphiteTop, $graphiteBot, [float]90.0)
  $g.FillPath($grad, $tile)
  $border = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 46, 53, 64), [float](1.5 * $s))
  $g.DrawPath($border, $tile)

  $pen = [System.Drawing.Pen]::new($amber, [float](10.0 * $s))
  $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $g.DrawEllipse($pen, (128.0 - 86.0) * $s, (130.0 - 86.0) * $s, 172.0 * $s, 172.0 * $s)

  $fil = [System.Drawing.Drawing2D.GraphicsPath]::new()
  $fil.AddBezier(63 * $s, 192 * $s, 100 * $s, 192 * $s, 98 * $s, 136 * $s, 128 * $s, 130 * $s)
  $fil.AddBezier(128 * $s, 130 * $s, 158 * $s, 124 * $s, 162 * $s, 84 * $s, 192 * $s, 74 * $s)
  $g.DrawPath($pen, $fil)

  $node = [System.Drawing.SolidBrush]::new($ignite)
  $g.FillEllipse($node, (192.0 - 17.0) * $s, (74.0 - 17.0) * $s, 34.0 * $s, 34.0 * $s)

  $g.Dispose()
  return $bmp
}

# ---- SVG emission ----------------------------------------------------------
$svgHeader = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">'

function Out-Svg([string]$name, [string]$body) {
  $content = "$svgHeader`n$body`n</svg>"
  [System.IO.File]::WriteAllText((Join-Path $outDir $name), $content, (New-Object System.Text.UTF8Encoding($false)))
}

$markFullColour = @'
<g fill="none" stroke="#f59a3e" stroke-width="10" stroke-linecap="round">
  <ellipse cx="128" cy="130" rx="86" ry="86"/>
  <path d="M 63 192 C 100 192 98 136 128 130 S 162 84 192 74"/>
</g>
<circle cx="192" cy="74" r="17" fill="#ffe9cf"/>
'@
$markDark = @'
<g fill="none" stroke="#eef0f4" stroke-width="10" stroke-linecap="round">
  <ellipse cx="128" cy="130" rx="86" ry="86"/>
  <path d="M 63 192 C 100 192 98 136 128 130 S 162 84 192 74"/>
</g>
'@
$markLight = $markDark -replace "#eef0f4", "#1c2026"

$tileDefs = @'
<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#232831"/>
    <stop offset="1" stop-color="#14171b"/>
  </linearGradient>
</defs>
'@

# Primary lockup: graphite tile + amber filament, ignite node.
$tileShape = '<rect x="8" y="8" width="240" height="240" rx="56" fill="url(#tile)" stroke="#2e3540" stroke-width="1.5"/>'
Out-Svg "logo.svg" ($tileDefs + $tileShape + "`n" + $markFullColour)

# Standalone marks (transparent background).
Out-Svg "logo-mark.svg" $markFullColour
Out-Svg "logo-mark-dark.svg" $markDark
Out-Svg "logo-mark-light.svg" $markLight

# Wordmarks.
$word = '<text x="128" y="142" text-anchor="middle" style="font-family:&#39;Segoe UI&#39;,system-ui,sans-serif;font-size:44;font-weight:650;letter-spacing:2">RESONUX</text>'
$wordBlock = "<g fill=`"#1c2026`">`n$word`n</g>"
$wordBlockLight = $wordBlock -replace "#1c2026", "#eef0f4"
Out-Svg "wordmark.svg" $wordBlock
Out-Svg "wordmark-light.svg" $wordBlockLight

# Lockups: small tile mark at left + RESONUX text.
$lockMark = @'
<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#232831"/>
    <stop offset="1" stop-color="#14171b"/>
  </linearGradient>
</defs>
<rect x="48" y="72" width="112" height="112" rx="26" fill="url(#tile)" stroke="#2e3540" stroke-width="1.5"/>
<g transform="translate(48 72) scale(0.4375)">
  <g fill="none" stroke="#f59a3e" stroke-width="10" stroke-linecap="round">
    <ellipse cx="128" cy="130" rx="86" ry="86"/>
    <path d="M 63 192 C 100 192 98 136 128 130 S 162 84 192 74"/>
  </g>
  <circle cx="192" cy="74" r="17" fill="#ffe9cf"/>
</g>
'@
$lockText = '<text x="180" y="134" style="font-family:&#39;Segoe UI&#39;,system-ui,sans-serif;font-size:31;font-weight:650;letter-spacing:5">RESONUX</text>'
$lockBlock = "<g fill=`"#1c2026`">`n$lockText`n</g>"
$lockBlockLight = $lockBlock -replace "#1c2026", "#eef0f4"
Out-Svg "logo-lockup.svg" ($lockMark + $lockBlock)
Out-Svg "logo-lockup-light.svg" ($lockMark + $lockBlockLight)

# ---- PNG masters -------------------------------------------------------------
$b = Draw-IconFrame 1024
$b.Save((Join-Path $outDir "icon.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$b.Dispose()

$b = Draw-MarkFrame 128
$b.Save((Join-Path $outDir "logo-mark-128.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$b.Dispose()

$b = Draw-MarkFrame 64
$b.Save((Join-Path $outDir "favicon.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$b.Dispose()

# ---- ICO (multi-size, PNG entries) -------------------------------------------
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$blobs = @()
foreach ($sz in $sizes) {
  $bm = Draw-IconFrame $sz
  $ms = New-Object System.IO.MemoryStream
  $bm.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  $blobs += , @($sz, $ms.ToArray())
  $bm.Dispose()
}
$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($out)
$w.Write([UInt16]0)
$w.Write([UInt16]1)
$w.Write([UInt16]$blobs.Count)
$offset = 6 + 16 * $blobs.Count
foreach ($blob in $blobs) {
  $sz = $blob[0]; $data = $blob[1]
  $w.Write([Byte]($(if ($sz -ge 256) { 0 } else { $sz })))
  $w.Write([Byte]($(if ($sz -ge 256) { 0 } else { $sz })))
  $w.Write([Byte]0); $w.Write([Byte]0)
  $w.Write([UInt16]1); $w.Write([UInt16]32)
  $w.Write([UInt32]$data.Length)
  $w.Write([UInt32]$offset)
  $offset += $data.Length
}
foreach ($blob in $blobs) { $w.Write($blob[1]) }
$w.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $outDir "icon.ico"), $out.ToArray())
$w.Dispose(); $out.Dispose()

Write-Output "wrote asset set:"
Get-ChildItem -Path $outDir | Where-Object { $_.Name -in @("icon.ico","icon.png","logo.svg","logo-mark.svg","logo-mark-dark.svg","logo-mark-light.svg","wordmark.svg","wordmark-light.svg","logo-lockup.svg","logo-lockup-light.svg","logo-mark-128.png","favicon.png","make-icon.ps1") } | ForEach-Object { "  $($_.Name) - $($_.Length) bytes" }