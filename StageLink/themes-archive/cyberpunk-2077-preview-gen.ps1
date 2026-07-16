Add-Type -AssemblyName System.Drawing

function C($hex) {
  $v = [Convert]::ToInt32($hex, 16)
  return [System.Drawing.Color]::FromArgb(255, ($v -shr 16) -band 0xFF, ($v -shr 8) -band 0xFF, $v -band 0xFF)
}

$bg     = C "000000"
$panel  = C "16161F"
$text   = C "F2F2F0"
$textDim= C "8A8A99"
$yellow = C "F4E10A"
$cyan   = C "00E5FF"
$magenta= C "FF2E9A"
$alert  = C "FF3B3B"
$border = C "2A2A38"

$scale = 2
$W = 240 * $scale; $H = 320 * $scale
$font   = New-Object -TypeName System.Drawing.Font -ArgumentList "Consolas", ([float](9*$scale)), ([System.Drawing.FontStyle]::Bold)
$fontSm = New-Object -TypeName System.Drawing.Font -ArgumentList "Consolas", ([float](6*$scale)), ([System.Drawing.FontStyle]::Bold)
$fontLg = New-Object -TypeName System.Drawing.Font -ArgumentList "Consolas", ([float](15*$scale)), ([System.Drawing.FontStyle]::Bold)

function DrawStatusBar($g) {
  $g.FillRectangle((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $bg), 0, 0, $W, 12*$scale)
  $pen = New-Object -TypeName System.Drawing.Pen -ArgumentList $border, 1.0
  $g.DrawLine($pen, 0, 12*$scale, $W, 12*$scale)
  $g.FillEllipse((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $cyan), 3*$scale, 3.5*$scale, 5*$scale, 5*$scale)
}

function RoundRectPath($x,$y,$w,$h,$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r*2
  $p.AddArc($x,$y,$d,$d,180,90); $p.AddArc($x+$w-$d,$y,$d,$d,270,90)
  $p.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90); $p.AddArc($x,$y+$h-$d,$d,$d,90,90)
  $p.CloseFigure()
  return $p
}

# --- Panel 1: Home screen, single-button carousel ---
$bmp1 = New-Object -TypeName System.Drawing.Bitmap -ArgumentList $W, $H
$g1 = [System.Drawing.Graphics]::FromImage($bmp1)
$g1.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g1.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
$g1.FillRectangle((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $bg), 0, 0, $W, $H)
DrawStatusBar $g1

$bx = [int](20*$scale); $by = [int](60*$scale); $bw = [int](200*$scale); $bh = [int](150*$scale)
$path = RoundRectPath $bx $by $bw $bh (4*$scale)
$g1.FillPath((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $yellow), $path)
$sf = New-Object System.Drawing.StringFormat
$sf.Alignment = [System.Drawing.StringAlignment]::Center
$sf.LineAlignment = [System.Drawing.StringAlignment]::Center
$rectF = New-Object -TypeName System.Drawing.RectangleF -ArgumentList $bx, $by, $bw, $bh
$g1.DrawString("SOUND", $fontLg, (New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $bg), $rectF, $sf)

$cy = $by + $bh/2
$chevBrush = New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $textDim
$pt1 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](10*$scale)), ([float]($cy-10*$scale))
$pt2 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](10*$scale)), ([float]($cy+10*$scale))
$pt3 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](2*$scale)), ([float]$cy)
$g1.FillPolygon($chevBrush, @($pt1,$pt2,$pt3))
$pt4 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](230*$scale)), ([float]($cy-10*$scale))
$pt5 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](230*$scale)), ([float]($cy+10*$scale))
$pt6 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float](238*$scale)), ([float]$cy)
$g1.FillPolygon($chevBrush, @($pt4,$pt5,$pt6))

$dotY = 230*$scale
for ($i=0; $i -lt 5; $i++) {
  $dx = (120 + ($i-2)*14) * $scale
  $r = if ($i -eq 0) { 3.5*$scale } else { 2.5*$scale }
  $b = if ($i -eq 0) { New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $yellow } else { New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $border }
  $g1.FillEllipse($b, $dx-$r, $dotY-$r, $r*2, $r*2)
}
$rectF2 = New-Object -TypeName System.Drawing.RectangleF -ArgumentList 0, ([float](250*$scale)), ([float]$W), ([float](20*$scale))
$g1.DrawString("<< SWIPE >>", $fontSm, (New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $textDim), $rectF2, $sf)
$g1.Dispose()

# --- Panel 2: Category screen, list style ---
$bmp2 = New-Object -TypeName System.Drawing.Bitmap -ArgumentList $W, $H
$g2 = [System.Drawing.Graphics]::FromImage($bmp2)
$g2.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g2.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
$g2.FillRectangle((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $bg), 0, 0, $W, $H)
DrawStatusBar $g2

$items = @(
  @{t="More Volume"; c=$yellow},
  @{t="More Bass"; c=$cyan},
  @{t="Less Bass"; c=$magenta},
  @{t="Monitors cut out"; c=$alert}
)
$y = 30*$scale
$sfL = New-Object System.Drawing.StringFormat
$sfL.Alignment = [System.Drawing.StringAlignment]::Near
$sfL.LineAlignment = [System.Drawing.StringAlignment]::Center
foreach ($it in $items) {
  $rowH = 42*$scale
  $rx = [int](10*$scale); $ry = [int]$y; $rw = [int](220*$scale); $rh = [int]($rowH-8*$scale)
  $p2 = RoundRectPath $rx $ry $rw $rh (3*$scale)
  $g2.FillPath((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $panel), $p2)
  $pen2 = New-Object -TypeName System.Drawing.Pen -ArgumentList $it.c, ([float](2*$scale))
  $g2.DrawPath($pen2, $p2)
  $rectF3 = New-Object -TypeName System.Drawing.RectangleF -ArgumentList ([float]($rx+16*$scale)), ([float]$ry), ([float]($rw-20*$scale)), ([float]$rh)
  $g2.DrawString($it.t, $font, (New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $text), $rectF3, $sfL)
  $ccy = $ry + $rh/2
  $cp1 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float]($rx+4*$scale)), ([float]($ccy-4*$scale))
  $cp2 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float]($rx+4*$scale)), ([float]($ccy+4*$scale))
  $cp3 = New-Object -TypeName System.Drawing.PointF -ArgumentList ([float]($rx+9*$scale)), ([float]$ccy)
  $g2.FillPolygon((New-Object -TypeName System.Drawing.SolidBrush -ArgumentList $it.c), @($cp1,$cp2,$cp3))
  $y += $rowH
}
$g2.Dispose()

$pad = 30
$out = New-Object -TypeName System.Drawing.Bitmap -ArgumentList ($W*2+$pad*3), ($H+$pad*2)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(255,40,40,45))
$g.DrawImage($bmp1, $pad, $pad)
$g.DrawImage($bmp2, $W+$pad*2, $pad)
$g.Dispose()
$out.Save("C:\Users\davis\Desktop\Claude\StageLink\StageLink\themes-archive\cyberpunk-2077-preview.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved"

