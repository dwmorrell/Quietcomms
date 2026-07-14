Add-Type -AssemblyName System.Drawing

# Geometry measured from images/ReelWorks Logo.png (logo_measure.ps1):
# outer thin outline 0.955R..1.0R, black gap 0.855R..0.955R, main disc
# to 0.855R, five big holes ring 0.49R radius 0.18R (one at top), center
# cluster of five small dots ring 0.13R radius 0.045R staggered 36 deg
# from the big holes, tiny center dot 0.03R.
$size = 200
$cx = 100.0; $cy = 100.0
$R = 88.0
$white = [System.Drawing.Color]::FromArgb(255, 240, 240, 240)

function Draw-Reel {
    param($g, [double]$angleDeg)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $wb = New-Object System.Drawing.SolidBrush($white)
    $blackB = [System.Drawing.Brushes]::Black

    function Disc($g, $brush, [double]$hx, [double]$hy, [double]$r) {
        $g.FillEllipse($brush, [float]($hx - $r), [float]($hy - $r), [float]($r * 2), [float]($r * 2))
    }

    Disc $g $wb     $cx $cy $R              # thin outer outline
    Disc $g $blackB $cx $cy ($R * 0.955)    # gap ring
    Disc $g $wb     $cx $cy ($R * 0.855)    # main disc

    # five big holes, one at top, spinning with angleDeg
    $holeRing = $R * 0.49; $holeR = $R * 0.18
    for ($i = 0; $i -lt 5; $i++) {
        $a = ([math]::PI / 180.0) * ($angleDeg + $i * 72 - 90)
        Disc $g $blackB ($cx + $holeRing * [math]::Cos($a)) ($cy + $holeRing * [math]::Sin($a)) $holeR
    }

    # center cluster: five small dots staggered 36 deg from the big holes
    $dotRing = $R * 0.13; $dotR = $R * 0.045
    for ($i = 0; $i -lt 5; $i++) {
        $a = ([math]::PI / 180.0) * ($angleDeg + $i * 72 - 90 + 36)
        Disc $g $blackB ($cx + $dotRing * [math]::Cos($a)) ($cy + $dotRing * [math]::Sin($a)) $dotR
    }
    Disc $g $blackB $cx $cy ($R * 0.03)     # tiny center dot

    $wb.Dispose()
}

# --- clean single-frame PNG, transparent background ---
$png = New-Object System.Drawing.Bitmap $size, $size
$g = [System.Drawing.Graphics]::FromImage($png)
$g.Clear([System.Drawing.Color]::Transparent)
Draw-Reel $g 0
$png.Save("C:\Users\davis\Desktop\Claude\StageLink\images\ReelWorks-clean.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $png.Dispose()

# --- spinning frames over a starfield, assembled into an animated GIF ---
$rnd = New-Object System.Random 7
$stars = @()
for ($i = 0; $i -lt 40; $i++) { $stars += ,@($rnd.Next(0, $size), $rnd.Next(0, $size), $rnd.Next(1, 3)) }

$frameCount = 24
$frames = @()
for ($f = 0; $f -lt $frameCount; $f++) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    foreach ($s in $stars) {
        $g.FillRectangle([System.Drawing.Brushes]::DimGray, $s[0], $s[1], $s[2], $s[2])
    }
    Draw-Reel $g ($f * 72.0 / $frameCount)   # one hole-period per loop = seamless
    $g.Dispose()
    $frames += $bmp
}

Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase
$enc = New-Object System.Windows.Media.Imaging.GifBitmapEncoder
foreach ($bmp in $frames) {
    $h = $bmp.GetHbitmap()
    $src = [System.Windows.Interop.Imaging]::CreateBitmapSourceFromHBitmap($h, [System.IntPtr]::Zero, [System.Windows.Int32Rect]::Empty, [System.Windows.Media.Imaging.BitmapSizeOptions]::FromEmptyOptions())
    $enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($src))
}
$ms = New-Object System.IO.MemoryStream
$enc.Save($ms)
$bytes = $ms.ToArray()
$ms.Dispose()
foreach ($bmp in $frames) { $bmp.Dispose() }

$list = New-Object System.Collections.Generic.List[byte]
$list.AddRange($bytes)
$loop = [byte[]](0x21,0xFF,0x0B,0x4E,0x45,0x54,0x53,0x43,0x41,0x50,0x45,0x32,0x2E,0x30,0x03,0x01,0x00,0x00,0x00)
$insertAt = 13
$packed = $bytes[10]
if (($packed -band 0x80) -ne 0) {
    $gctSize = 3 * [math]::Pow(2, ($packed -band 0x07) + 1)
    $insertAt = 13 + [int]$gctSize
}
$list.InsertRange($insertAt, $loop)
$arr = $list.ToArray()
for ($i = 0; $i -lt $arr.Length - 8; $i++) {
    if ($arr[$i] -eq 0x21 -and $arr[$i+1] -eq 0xF9 -and $arr[$i+2] -eq 0x04) {
        $arr[$i+4] = 8; $arr[$i+5] = 0
    }
}
[System.IO.File]::WriteAllBytes("C:\Users\davis\Desktop\Claude\StageLink\images\ReelWorks-spin.gif", $arr)
Write-Output "wrote tightened ReelWorks-clean.png and ReelWorks-spin.gif"
