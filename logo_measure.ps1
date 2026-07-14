Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile("C:\Users\davis\Desktop\Claude\StageLink\images\ReelWorks Logo.png")

# Find the white shape's bounding box and center
$minX=9999;$maxX=-1;$minY=9999;$maxY=-1
for ($y=0; $y -lt $src.Height; $y++) {
  for ($x=0; $x -lt $src.Width; $x++) {
    $p = $src.GetPixel($x,$y)
    if ($p.A -gt 128 -and (0.299*$p.R+0.587*$p.G+0.114*$p.B) -gt 110) {
      if ($x -lt $minX){$minX=$x}; if ($x -gt $maxX){$maxX=$x}
      if ($y -lt $minY){$minY=$y}; if ($y -gt $maxY){$maxY=$y}
    }
  }
}
$cx = ($minX+$maxX)/2.0; $cy = ($minY+$maxY)/2.0
$R = (($maxX-$minX)+($maxY-$minY))/4.0
Write-Output "white bbox: x $minX..$maxX  y $minY..$maxY  center ($cx,$cy)  outerR=$R"

# Radial profile: walk outward along several angles, record white/black runs
foreach ($angDeg in 0, 45, 90, 180, 270) {
  $a = $angDeg * [math]::PI / 180.0
  $profile = ""
  $lastWhite = $null
  for ($r = 0; $r -le [int]($R+2); $r++) {
    $x = [int]($cx + $r * [math]::Cos($a)); $y = [int]($cy + $r * [math]::Sin($a))
    if ($x -lt 0 -or $y -lt 0 -or $x -ge $src.Width -or $y -ge $src.Height) { break }
    $p = $src.GetPixel($x,$y)
    $w = ($p.A -gt 128 -and (0.299*$p.R+0.587*$p.G+0.114*$p.B) -gt 110)
    if ($w -ne $lastWhite) { $profile += "$r$(if($w){'W'}else{'B'}) "; $lastWhite = $w }
  }
  Write-Output "angle $angDeg : $profile"
}

# scan the center region for the small-dot cluster: list black blobs within r<30 of center
Write-Output "--- center 60x60 map (B=black, .=white) around center ---"
for ($y=[int]$cy-15; $y -le [int]$cy+15; $y+=1) {
  $row = ""
  for ($x=[int]$cx-15; $x -le [int]$cx+15; $x+=1) {
    $p = $src.GetPixel($x,$y)
    $w = ($p.A -gt 128 -and (0.299*$p.R+0.587*$p.G+0.114*$p.B) -gt 110)
    $row += $(if($w){"."}else{"B"})
  }
  Write-Output $row
}
$src.Dispose()
