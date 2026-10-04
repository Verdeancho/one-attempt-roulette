# Generates the credit icons (round avatars) and the video button from assets/
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$assets = Join-Path $root "assets"
$res = Join-Path $root "resources"
New-Item -ItemType Directory -Force $res | Out-Null

# Round avatar with a white border
function New-Avatar([System.Drawing.Image]$src, [int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)
    $border = [float]($size * 0.06)
    $clip = New-Object System.Drawing.Drawing2D.GraphicsPath
    $clip.AddEllipse($border, $border, $size - 2 * $border, $size - 2 * $border)
    $g.SetClip($clip)
    # crop to a centered square
    $m = [Math]::Min($src.Width, $src.Height)
    $srcRect = New-Object System.Drawing.Rectangle ([int](($src.Width - $m) / 2)), ([int](($src.Height - $m) / 2)), $m, $m
    $dstRect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $g.DrawImage($src, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    $g.ResetClip()
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), $border
    $g.DrawEllipse($pen, $border / 2, $border / 2, $size - $border, $size - $border)
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# Red play button (generic, for the video link)
function New-PlayButton([int]$w, [int]$h, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $r = [float]($h * 0.32)
    $rect = New-Object System.Drawing.RectangleF 2, 2, ($w - 4), ($h - 4)
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc($rect.X, $rect.Y, 2 * $r, 2 * $r, 180, 90)
    $p.AddArc($rect.Right - 2 * $r, $rect.Y, 2 * $r, 2 * $r, 270, 90)
    $p.AddArc($rect.Right - 2 * $r, $rect.Bottom - 2 * $r, 2 * $r, 2 * $r, 0, 90)
    $p.AddArc($rect.X, $rect.Bottom - 2 * $r, 2 * $r, 2 * $r, 90, 90)
    $p.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 230, 30, 30))), $p)
    $g.DrawPath((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 60, 0, 0)), ([float]($h * 0.05))), $p)
    $cx = $w / 2; $cy = $h / 2; $t = $h * 0.22
    $tri = @(
        (New-Object System.Drawing.PointF ([float]($cx - $t * 0.8), [float]($cy - $t))),
        (New-Object System.Drawing.PointF ([float]($cx - $t * 0.8), [float]($cy + $t))),
        (New-Object System.Drawing.PointF ([float]($cx + $t * 1.1), [float]$cy))
    )
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)), $tri)
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

$guille = [System.Drawing.Image]::FromFile((Join-Path $assets "srguillester.jpg"))
New-Avatar $guille 120 (Join-Path $res "guillester.png")
$guille.Dispose()

$ico = New-Object System.Drawing.Icon((Join-Path $assets "jenrai.ico"), 256, 256)
$jenrai = $ico.ToBitmap()
New-Avatar $jenrai 120 (Join-Path $res "jenrai.png")
$jenrai.Dispose()

New-PlayButton 140 100 (Join-Path $res "play_video.png")
Write-Output "Credit icons generated"
