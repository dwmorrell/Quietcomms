Add-Type -AssemblyName System.Drawing

function RR($g, $b, [double]$x, [double]$y, [double]$w, [double]$h, [double]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc([float]$x, [float]$y, [float]$d, [float]$d, 180, 90)
    $p.AddArc([float]($x + $w - $d), [float]$y, [float]$d, [float]$d, 270, 90)
    $p.AddArc([float]($x + $w - $d), [float]($y + $h - $d), [float]$d, [float]$d, 0, 90)
    $p.AddArc([float]$x, [float]($y + $h - $d), [float]$d, [float]$d, 90, 90)
    $p.CloseFigure()
    $g.FillPath($b, $p)
    $p.Dispose()
}

# Solid fist + thin bg-color creases + overlapping thumb column.
function DrawThumb {
    param($g, [double]$cx, [double]$cy, [double]$u, $fill, $bg)
    $ox = $cx - 8 * $u; $oy = $cy - 8 * $u
    RR $g $fill ($ox + 2.6 * $u) ($oy + 6.4 * $u) (12.0 * $u) (9.2 * $u) (2.4 * $u)    # fist
    for ($i = 1; $i -le 3; $i++) {
        $y = $oy + (6.4 + $i * 2.3) * $u
        RR $g $bg ($ox + 7.0 * $u) $y (8.2 * $u) (0.55 * $u) (0.27 * $u)               # creases
    }
    RR $g $fill ($ox + 2.0 * $u) ($oy + 1.0 * $u) (4.2 * $u) (8.0 * $u) (2.1 * $u)     # thumb
}

$u = 9
$size = 16 * $u + 40
$bmp = New-Object System.Drawing.Bitmap ($size * 2 + 60), ($size + 20)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$panel = [System.Drawing.Color]::FromArgb(255,28,42,62)
$g.Clear($panel)
$green = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,63,191,143))
$bgBrush = New-Object System.Drawing.SolidBrush($panel)
DrawThumb $g ($size/2 + 10) ($size/2 + 10) $u $green $bgBrush
# small button-scale render alongside (u=2.5 like on-device buttons)
$accent = [System.Drawing.Color]::FromArgb(255,38,64,92)
$acBrush = New-Object System.Drawing.SolidBrush($accent)
$g.FillRectangle($acBrush, $size + 30, 40, 180, 40)
$text = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,216,236,255))
DrawThumb $g ($size + 30 + 90) 60 2.2 $text $acBrush
$g.Dispose()
$bmp.Save("C:\Users\davis\Desktop\Claude\StageLink\images\thumb-candidates.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved"
