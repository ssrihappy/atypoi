param([switch]$Launch,[string]$Field='plain',[string]$Text='')
. "$PSScriptRoot\desktop-ui.ps1"
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$projectRoot=Split-Path -Parent $PSScriptRoot
$sample=[IO.File]::ReadAllText((Join-Path $projectRoot 'tests\input-sample.json')) | ConvertFrom-Json
if(-not $Text){$Text=$sample.keys}
if($Launch){
 $p=Start-Process 'C:\Program Files\Google\Chrome\Application\chrome.exe' -ArgumentList @('--user-data-dir="'+$projectRoot+'\build\chrome-verification"','--no-first-run','--no-default-browser-check','--force-renderer-accessibility','--new-window','"'+$projectRoot+'\tests\browser-input.html"') -PassThru
 $p.Id | Set-Content "$projectRoot\build\test-browser-pid.txt"
 [void]$p.WaitForInputIdle(10000)
 Start-Sleep -Milliseconds 800
} else {$p=Get-Process -Id ([int](Get-Content "$projectRoot\build\test-browser-pid.txt"))}
$p.Refresh()
if($p.MainWindowTitle -notlike 'Atypoi Windows keyboard verification*'){throw 'Unexpected browser page. No input sent.'}
[DesktopUi]::Foreground($p.MainWindowHandle)
[DesktopUi]::Press(27)
$root=[System.Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
$input=$root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty,$Field))
if(-not $input){throw 'Test field missing'}
$bounds=$input.Current.BoundingRectangle
[DesktopUi]::ClickAt([int]($bounds.Left+30),[int]($bounds.Top+15))
[DesktopUi]::Type($p.MainWindowHandle,$Text)
if($Field -ne 'password'){
 $actual=([System.Windows.Automation.ValuePattern]$input.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value
 Write-Output $actual
 if($Field -eq 'plain' -and $Text -eq $sample.keys){
  $expected=$sample.text
  if($actual -cne $expected){throw "Physical browser input failed: [$actual]"}
 }
}
& "$projectRoot\build\profile-diagnostic.exe" | Select-Object -Last 3
[DesktopUi]::Capture($p.MainWindowHandle,"$projectRoot\build\chrome-$Field.png")
