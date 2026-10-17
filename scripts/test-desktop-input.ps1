param([string]$Executable = '', [string]$Text = '')
. "$PSScriptRoot\desktop-ui.ps1"
$projectRoot=Split-Path -Parent $PSScriptRoot
$sample=[IO.File]::ReadAllText((Join-Path $projectRoot 'tests\input-sample.json')) | ConvertFrom-Json
if(-not $Text){$Text=$sample.keys}
if ($Executable) {
 $p=Start-Process -FilePath $Executable -PassThru
 [void]$p.WaitForInputIdle(10000)
}
$main=[DesktopUi]::Find('Atypoi.Tray.1')
[DesktopUi]::Send($main,0x111,113)
$test=[DesktopUi]::Find('Atypoi.Test')
[DesktopUi]::Foreground($test)
[DesktopUi]::Press(27)
$input=[DesktopUi]::GetDlgItem($test,203)
[DesktopUi]::Click($input)
[DesktopUi]::Chord(17,65)
[DesktopUi]::Press(8)
[DesktopUi]::Type($test,$Text)
$actual=[DesktopUi]::Read($input)
[DesktopUi]::Capture($test,(Join-Path $projectRoot 'build\desktop-input.png'))
Write-Output $actual
if($Text -eq $sample.keys){
 $expected=$sample.text
 if($actual -cne $expected){throw "Physical desktop input failed: [$actual]"}
}
if (Test-Path "$projectRoot\build\profile-diagnostic.exe") { & "$projectRoot\build\profile-diagnostic.exe" | Select-Object -Last 3 }
