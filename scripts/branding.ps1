# Rebuild vector originals and multi-resolution Windows artwork. No external tools.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $root 'assets\brand'
New-Item -ItemType Directory -Force $dest | Out-Null
Add-Type -AssemblyName System.Drawing
# The same geometry is used for SVG and raster exports: an a + input caret + dot.
$symbol = '<rect width="128" height="128" rx="30" fill="#6750e4"/><circle cx="51" cy="73" r="22" fill="none" stroke="white" stroke-width="12"/><path d="M73 51v44" stroke="white" stroke-width="12" stroke-linecap="round"/><path d="M96 54v39" stroke="#baf2df" stroke-width="10" stroke-linecap="round"/><circle cx="96" cy="34" r="6" fill="#baf2df"/>'
[IO.File]::WriteAllText((Join-Path $dest 'icon.svg'),'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128" role="img" aria-label="Atypoi">'+$symbol+'</svg>')
[IO.File]::WriteAllText((Join-Path $dest 'wordmark.svg'),'<svg xmlns="http://www.w3.org/2000/svg" width="430" height="104" viewBox="0 0 430 104" role="img" aria-label="atypoi: AI + typo"><g transform="translate(4 4) scale(.75)">'+$symbol+'</g><text x="124" y="72" font-family="Segoe UI, sans-serif" font-size="68" font-weight="600" letter-spacing="-3" fill="#1d2131">atypoi</text></svg>')
function Draw-Mark($g,[float]$size) {
    $g.SmoothingMode = 'AntiAlias'
    $scale = $size / 128
    $g.ScaleTransform($scale,$scale)
    $path = New-Object Drawing.Drawing2D.GraphicsPath
    foreach ($arc in @(@(0,0,180),@(68,0,270),@(68,68,0),@(0,68,90))) { $path.AddArc($arc[0],$arc[1],60,60,$arc[2],90) }
    $path.CloseFigure()
    $purple = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#6750e4'))
    $mint = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#baf2df'))
    $white = New-Object Drawing.Pen ([Drawing.Color]::White),12
    $caret = New-Object Drawing.Pen ($mint.Color),10
    $white.StartCap=$white.EndCap=$caret.StartCap=$caret.EndCap='Round'
    $g.FillPath($purple,$path); $g.DrawEllipse($white,29,51,44,44); $g.DrawLine($white,73,51,73,95)
    $g.DrawLine($caret,96,54,96,93); $g.FillEllipse($mint,90,28,12,12)
    $g.ResetTransform()
    $path.Dispose(); $purple.Dispose(); $mint.Dispose(); $white.Dispose(); $caret.Dispose()
}
$images = @()
foreach($size in @(16,20,24,32,40,48,64,128,256)) {
    # Supersample for clean small-size edges and transparent corners.
    $large = New-Object Drawing.Bitmap 512,512
    $g=[Drawing.Graphics]::FromImage($large); Draw-Mark $g 512; $g.Dispose()
    $bitmap=New-Object Drawing.Bitmap $size,$size
    $g=[Drawing.Graphics]::FromImage($bitmap); $g.InterpolationMode='HighQualityBicubic'; $g.DrawImage($large,0,0,$size,$size);$g.Dispose();$large.Dispose()
    $stream=New-Object IO.MemoryStream; $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
    $images+=,@{Size=$size;Bytes=$stream.ToArray()}
    if($size -eq 256){$bitmap.Save((Join-Path $dest 'icon.png'),[Drawing.Imaging.ImageFormat]::Png)}
    $bitmap.Dispose();$stream.Dispose()
}
$file=[IO.File]::Create((Join-Path $root 'resources\atypoi.ico'));$writer=New-Object IO.BinaryWriter $file
try {
    $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$images.Count)
    $offset=6+16*$images.Count
    foreach($item in $images){$side=if($item.Size -eq 256){0}else{$item.Size};$writer.Write([byte]$side);$writer.Write([byte]$side);$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$item.Bytes.Length);$writer.Write([uint32]$offset);$offset+=$item.Bytes.Length}
    foreach($item in $images){$writer.Write([byte[]]$item.Bytes)}
} finally {$writer.Dispose();$file.Dispose()}
# GitHub social preview, 1280 x 640. Text stays editable in the vector original.
$banner='<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="640" viewBox="0 0 1280 640"><rect width="1280" height="640" fill="#f6f7fb"/><rect x="744" y="64" width="472" height="512" rx="32" fill="#eeebfc"/><g transform="translate(824 132) scale(2.4)">'+$symbol+'</g><text x="72" y="176" font-family="Segoe UI, sans-serif" font-size="80" font-weight="600" letter-spacing="-4" fill="#1d2131">atypoi</text><text x="76" y="222" font-family="Segoe UI, sans-serif" font-size="20" fill="#6750e4">AI + TYPO · MADE FOR WINDOWS</text><text x="72" y="324" font-family="Malgun Gothic, sans-serif" font-size="42" font-weight="700" fill="#1d2131">한영 전환 없이,</text><text x="72" y="386" font-family="Malgun Gothic, sans-serif" font-size="42" font-weight="700" fill="#1d2131">생각의 흐름대로.</text><text x="76" y="474" font-family="Segoe UI, sans-serif" font-size="24" fill="#686f82">dkssud hello → 안녕 hello</text><text x="76" y="556" font-family="Segoe UI, sans-serif" font-size="18" fill="#686f82">LOCAL PROCESSING / OPEN SOURCE</text></svg>'
[IO.File]::WriteAllText((Join-Path $dest 'social-preview.svg'),$banner,[Text.UTF8Encoding]::new($false))
$canvas=New-Object Drawing.Bitmap 1280,640
$g=[Drawing.Graphics]::FromImage($canvas)
$g.Clear([Drawing.ColorTranslator]::FromHtml('#f6f7fb'))
$g.SmoothingMode='AntiAlias';$g.TextRenderingHint='AntiAliasGridFit'
$ink=New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#1d2131'))
$slate=New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#686f82'))
$violet=New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#6750e4'))
$panel=New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#eeebfc'))
$g.FillRectangle($panel,744,64,472,512)
$mark=New-Object Drawing.Bitmap 512,512
$mg=[Drawing.Graphics]::FromImage($mark);Draw-Mark $mg 512;$mg.Dispose()
$g.InterpolationMode='HighQualityBicubic';$g.DrawImage($mark,824,132,307,307);$mark.Dispose()
function Banner-Text([string]$text,[float]$x,[float]$y,[float]$size,$brush,[bool]$bold=$false,[string]$face='Segoe UI') {
    $style=if($bold){[Drawing.FontStyle]::Bold}else{[Drawing.FontStyle]::Regular}
    $f=New-Object Drawing.Font $face,$size,$style,([Drawing.GraphicsUnit]::Pixel)
    $g.DrawString($text,$f,$brush,$x,$y);$f.Dispose()
}
Banner-Text 'atypoi' 68 80 80 $ink $true
Banner-Text 'AI + TYPO / MADE FOR WINDOWS' 76 198 20 $violet
Banner-Text '한영 전환 없이,' 68 268 42 $ink $true '맑은 고딕'
Banner-Text '생각의 흐름대로.' 68 330 42 $ink $true '맑은 고딕'
Banner-Text 'dkssud hello → 안녕 hello' 72 440 24 $slate $false '맑은 고딕'
Banner-Text 'LOCAL PROCESSING / OPEN SOURCE' 76 534 18 $slate
$canvas.Save((Join-Path $dest 'social-preview.png'),[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$canvas.Dispose();$ink.Dispose();$slate.Dispose();$violet.Dispose();$panel.Dispose()
Write-Host 'Brand artwork rebuilt: assets/brand and resources/atypoi.ico'
