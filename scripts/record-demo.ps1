# Capture only the dedicated Atypoi test window, with synthetic sample input.
$ErrorActionPreference='Stop'
. "$PSScriptRoot\desktop-ui.ps1"
$root=Split-Path -Parent $PSScriptRoot
Add-Type -ReferencedAssemblies System.Drawing -Path (Join-Path $PSScriptRoot 'AnimatedGif.cs')
$sample=[IO.File]::ReadAllText((Join-Path $root 'tests\input-sample.json')) | ConvertFrom-Json
$main=[DesktopUi]::Find('Atypoi.Tray.1')
if($main -eq [IntPtr]::Zero){throw 'Start the installed Atypoi app first'}
[DesktopUi]::Send($main,0x111,113)
$test=[DesktopUi]::Find('Atypoi.Test')
[DesktopUi]::Foreground($test);[DesktopUi]::Press(27)
$input=[DesktopUi]::GetDlgItem($test,203)
[DesktopUi]::Click($input);[DesktopUi]::Chord(17,65);[DesktopUi]::Press(8)
$frames=Join-Path $root ('build\demo-frames-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $frames,(Join-Path $root 'docs\images') | Out-Null
$paths=[Collections.Generic.List[string]]::new()
function Capture-Frame {
    $path=Join-Path $frames ('{0:D3}.png' -f $paths.Count)
    [DesktopUi]::Capture($test,$path);$paths.Add($path)
}
Capture-Frame
foreach($key in $sample.keys.ToCharArray()) {
    [DesktopUi]::Type($test,[string]$key)
    Capture-Frame
}
$actual=[DesktopUi]::Read($input)
if($actual -cne $sample.text){throw 'Recorded input did not match the synthetic sample. GIF not exported.'}
[DesktopUi]::Capture($test,(Join-Path $root 'docs\images\input-test.png'))
$target=Join-Path $root 'docs\images\typing-demo.gif'
[AtypoiGif]::Encode($paths.ToArray(),$target,12)
Write-Host "Recorded actual Windows input: $($paths.Count) frames"
Get-Item $target | Select-Object Name,Length
