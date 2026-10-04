# Genera los iconos del mod (ruleta) con System.Drawing
Add-Type -AssemblyName System.Drawing

function Draw-Wheel([System.Drawing.Graphics]$g, [float]$cx, [float]$cy, [float]$r) {
    $colors = @(
        [System.Drawing.Color]::FromArgb(255, 235, 64, 52),
        [System.Drawing.Color]::FromArgb(255, 255, 196, 0),
        [System.Drawing.Color]::FromArgb(255, 60, 200, 80),
        [System.Drawing.Color]::FromArgb(255, 40, 150, 255),
        [System.Drawing.Color]::FromArgb(255, 235, 64, 52),
        [System.Drawing.Color]::FromArgb(255, 255, 196, 0),
        [System.Drawing.Color]::FromArgb(255, 60, 200, 80),
        [System.Drawing.Color]::FromArgb(255, 40, 150, 255)
    )
    $outline = [float]($r * 0.12)
    $black = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Black)
    $g.FillEllipse($black, $cx - $r - $outline, $cy - $r - $outline, 2 * ($r + $outline), 2 * ($r + $outline))
    for ($i = 0; $i -lt 8; $i++) {
        $b = New-Object System.Drawing.SolidBrush ($colors[$i])
        $g.FillPie($b, $cx - $r, $cy - $r, 2 * $r, 2 * $r, [float](-90 + $i * 45 - 22.5), [float]45)
    }
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200, 0, 0, 0)), ([float]($r * 0.04))
    for ($i = 0; $i -lt 8; $i++) {
        $a = (-90 + $i * 45 - 22.5) * [Math]::PI / 180
        $g.DrawLine($pen, $cx, $cy, [float]($cx + [Math]::Cos($a) * $r), [float]($cy + [Math]::Sin($a) * $r))
    }
    # brillo
    $shine = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(60, 255, 255, 255))
    $g.FillPie($shine, $cx - $r, $cy - $r, 2 * $r, 2 * $r, [float]180, [float]180)
    # centro
    $cr = $r * 0.24
    $g.FillEllipse($black, $cx - $cr - $outline * 0.6, $cy - $cr - $outline * 0.6, 2 * ($cr + $outline * 0.6), 2 * ($cr + $outline * 0.6))
    $gold = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 220, 90))
    $g.FillEllipse($gold, $cx - $cr, $cy - $cr, 2 * $cr, 2 * $cr)
    # puntero
    $pw = $r * 0.32
    $ph = $r * 0.42
    $top = $cy - $r - $outline * 1.6
    $pts = @(
        (New-Object System.Drawing.PointF ([float]($cx - $pw), [float]$top)),
        (New-Object System.Drawing.PointF ([float]($cx + $pw), [float]$top)),
        (New-Object System.Drawing.PointF ([float]$cx, [float]($top + $ph)))
    )
    $penO = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), ([float]($outline * 0.9))
    $penO.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPolygon($penO, $pts)
    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    $g.FillPolygon($white, $pts)
}

function New-Icon([int]$size, [string]$path, [bool]$withBg) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    if ($withBg) {
        $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point 0, $size), ([System.Drawing.Color]::FromArgb(255, 60, 40, 120)), ([System.Drawing.Color]::FromArgb(255, 20, 15, 50))
        $g.FillEllipse($bg, 4, 4, $size - 8, $size - 8)
        Draw-Wheel $g ($size / 2) ($size / 2 + $size * 0.04) ($size * 0.33)
    } else {
        Draw-Wheel $g ($size / 2) ($size / 2 + $size * 0.05) ($size * 0.36)
    }
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function New-Eye([int]$size, [string]$path, [bool]$closed) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $s = [float]$size
    # fondo circular
    $bg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(230, 25, 25, 35))
    $g.FillEllipse($bg, 2, 2, $s - 4, $s - 4)
    $rim = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 255, 255)), ([float]($s * 0.05))
    $g.DrawEllipse($rim, $s * 0.05, $s * 0.05, $s * 0.9, $s * 0.9)
    # ojo (forma de almendra)
    $path2 = New-Object System.Drawing.Drawing2D.GraphicsPath
    $l = New-Object System.Drawing.PointF ([float]($s * 0.18), [float]($s * 0.5))
    $r = New-Object System.Drawing.PointF ([float]($s * 0.82), [float]($s * 0.5))
    $path2.AddBezier($l, (New-Object System.Drawing.PointF ([float]($s * 0.35), [float]($s * 0.22))), (New-Object System.Drawing.PointF ([float]($s * 0.65), [float]($s * 0.22))), $r)
    $path2.AddBezier($r, (New-Object System.Drawing.PointF ([float]($s * 0.65), [float]($s * 0.78))), (New-Object System.Drawing.PointF ([float]($s * 0.35), [float]($s * 0.78))), $l)
    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    $g.FillPath($white, $path2)
    $iris = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 40, 150, 255))
    $g.FillEllipse($iris, $s * 0.38, $s * 0.38, $s * 0.24, $s * 0.24)
    $black = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Black)
    $g.FillEllipse($black, $s * 0.45, $s * 0.45, $s * 0.10, $s * 0.10)
    if ($closed) {
        $slashO = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 25, 25, 35)), ([float]($s * 0.16))
        $g.DrawLine($slashO, $s * 0.22, $s * 0.78, $s * 0.78, $s * 0.22)
        $slash = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 80, 80)), ([float]($s * 0.08))
        $g.DrawLine($slash, $s * 0.22, $s * 0.78, $s * 0.78, $s * 0.22)
    }
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# High quality resize of the new logo artwork (assets/logo_source.png)
function New-FromLogo([string]$src, [int]$size, [string]$path) {
    $img = [System.Drawing.Image]::FromFile($src)
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($img, 0, 0, $size, $size)
    $g.Dispose()
    $img.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force (Join-Path $root "resources") | Out-Null
New-Eye 80 (Join-Path $root "resources\eye_open.png") $false
New-Eye 80 (Join-Path $root "resources\eye_closed.png") $true
New-Item -ItemType Directory -Force (Join-Path $root "resources") | Out-Null
New-Icon 120 (Join-Path $root "resources\roulette.png") $false
$logo = Join-Path $root "assets\logo_source.png"
New-FromLogo $logo 336 (Join-Path $root "logo.png")
New-FromLogo $logo 200 (Join-Path $root "resources\ruleta.png")
Write-Output "Icons generated"
