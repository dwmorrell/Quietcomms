Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile("C:\Users\davis\Desktop\Claude\StageLink\images\thumbs up.jpg")

# bbox measured in thumb_measure.ps1, with a little breathing room
$pad = 6
$minX = 132 - $pad; $maxX = 478 + $pad
$minY = 84 - $pad;  $maxY = 448 + $pad
$w = $maxX - $minX; $h = $maxY - $minY

function Luma($p) { return 0.299*$p.R + 0.587*$p.G + 0.114*$p.B }

# Cut the silhouette out as a straight alpha mask (accent color, alpha ramps
# smoothly across the jpeg's anti-aliased/compressed edge instead of a hard
# threshold, so it doesn't look jagged at small sizes).
$accent = [System.Drawing.Color]::FromArgb(255,63,191,143)
$cutout = New-Object System.Drawing.Bitmap $w, $h
for ($y = 0; $y -lt $h; $y++) {
  for ($x = 0; $x -lt $w; $x++) {
    $p = $src.GetPixel($minX + $x, $minY + $y)
    $l = Luma $p
    $alpha = 0
    if ($l -le 90) { $alpha = 255 }
    elseif ($l -ge 170) { $alpha = 0 }
    else { $alpha = [int](255 * (170 - $l) / (170 - 90)) }
    $cutout.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, $accent.R, $accent.G, $accent.B))
  }
}
$cutout.Save("C:\Users\davis\Desktop\Claude\StageLink\images\thumb-cutout.png", [System.Drawing.Imaging.ImageFormat]::Png)
$src.Dispose()

# Preview composited over two theme backgrounds, scaled down to icon size
$iconSize = 200
$bgDark = [System.Drawing.Color]::FromArgb(255,10,10,18)
$bgPanel = [System.Drawing.Color]::FromArgb(255,28,42,62)

$preview = New-Object System.Drawing.Bitmap ($iconSize*2 + 60), ($iconSize + 20)
$g = [System.Drawing.Graphics]::FromImage($preview)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.FillRectangle((New-Object System.Drawing.SolidBrush($bgDark)), 0, 0, $iconSize, $iconSize+20)
$g.FillRectangle((New-Object System.Drawing.SolidBrush($bgPanel)), $iconSize+60, 0, $iconSize, $iconSize+20)
$g.DrawImage($cutout, [System.Drawing.Rectangle]::new(10,10,$iconSize-20,$iconSize-20))
$g.DrawImage($cutout, [System.Drawing.Rectangle]::new($iconSize+70,10,$iconSize-20,$iconSize-20))
$g.Dispose()
$preview.Save("C:\Users\davis\Desktop\Claude\StageLink\images\thumb-cutout-preview.png", [System.Drawing.Imaging.ImageFormat]::Png)
$cutout.Dispose()
$preview.Dispose()
Write-Output "saved cutout + preview, size ${w}x${h}"
