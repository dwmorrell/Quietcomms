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

function Circ($g, $b, [double]$cx, [double]$cy, [double]$r) {
    $g.FillEllipse($b, [float]($cx - $r), [float]($cy - $r), [float]($r * 2), [float]($r * 2))
}

# v3 thumbs-up: geometry measured directly off images/thumbs up.jpg (row-scan
# silhouette bounding boxes, see thumb_measure.ps1/thumb_measure2.ps1), not
# freehand. Coordinate frame: cx,cy center, u = unit, glyph spans a 16u x 16u
# box (ox,oy = cx-8u, cy-8u), ox/oy treated as the reference bbox top-left.
function DrawThumbV3 {
    param($g, [double]$cx, [double]$cy, [double]$u, $fill)
    $ox = $cx - 8 * $u; $oy = $cy - 8 * $u

    # cuff
    RR $g $fill ($ox + 0.0*$u) ($oy + 8.0*$u) (3.0*$u) (7.8*$u) (0.9*$u)

    # palm
    RR $g $fill ($ox + 3.5*$u) ($oy + 6.8*$u) (10.7*$u) (9.6*$u) (1.8*$u)

    # two knuckle bumps on the palm's right edge
    Circ $g $fill ($ox + 14.3*$u) ($oy + 10.5*$u) (1.5*$u)
    Circ $g $fill ($ox + 14.1*$u) ($oy + 12.8*$u) (1.3*$u)

    # thumb: quadratic-bezier circle chain fitted to the measured centerline
    $p0x = 7.7; $p0y = 6.1
    $p1x = 10.4; $p1y = 3.4
    $p2x = 10.0; $p2y = 0.3
    $r0 = 2.6; $r1 = 0.8
    $steps = 16
    for ($i = 0; $i -le $steps; $i++) {
        $t = $i / [double]$steps
        $mt = 1 - $t
        $x = $mt*$mt*$p0x + 2*$mt*$t*$p1x + $t*$t*$p2x
        $y = $mt*$mt*$p0y + 2*$mt*$t*$p1y + $t*$t*$p2y
        $r = $r0 + ($r1 - $r0) * $t
        Circ $g $fill ($ox + $x*$u) ($oy + $y*$u) ($r*$u)
    }
}

$u = 14
$size = [int](16 * $u + 40)
$bmp = New-Object System.Drawing.Bitmap ($size * 2 + 60), ($size + 20)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

$bgDark = [System.Drawing.Color]::FromArgb(255,10,10,18)
$bgPanel = [System.Drawing.Color]::FromArgb(255,28,42,62)
$g.FillRectangle((New-Object System.Drawing.SolidBrush($bgDark)), 0, 0, $size, $size)
$g.FillRectangle((New-Object System.Drawing.SolidBrush($bgPanel)), $size + 60, 0, $size, $size)

$green = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,63,191,143))
DrawThumbV3 $g ($size/2) ($size/2 + 10) $u $green
DrawThumbV3 $g ($size + 60 + $size/2) ($size/2 + 10) $u $green

$g.Dispose()
$bmp.Save("C:\Users\davis\Desktop\Claude\StageLink\images\thumb-v2-preview.png", [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved"
