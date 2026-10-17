# Requires an installed and selected Atypoi profile and the isolated browser opened
# by test-browser-input.ps1 -Launch. Sends real VK + scan-code events, not WM_CHAR.
. "$PSScriptRoot\desktop-ui.ps1"
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$projectRoot=Split-Path -Parent $PSScriptRoot
$p=Get-Process -Id ([int](Get-Content "$projectRoot\build\test-browser-pid.txt"))
if($p.MainWindowTitle -notlike 'Atypoi Windows keyboard verification*'){throw 'Unexpected test browser'}
$root=[System.Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
$results=[Collections.Generic.List[object]]::new()
function U([string]$codes){ -join ($codes.Split(' ') | ForEach-Object {[char][Convert]::ToInt32($_,16)}) }
function Field([string]$id){$root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty,$id))}
function Focus([string]$id){
 [DesktopUi]::Foreground($p.MainWindowHandle);[DesktopUi]::Press(27)
 $element=Field $id;if(-not $element){throw "Missing $id"}
 $b=$element.Current.BoundingRectangle
 [DesktopUi]::ClickAt([int]($b.Left+30),[int]($b.Top+15))
}
function ClearField([string]$id){Focus $id;[DesktopUi]::Chord(17,65);[DesktopUi]::Press(8)}
function TypeKeys([string]$s){[DesktopUi]::Type($p.MainWindowHandle,$s)}
function Value([string]$id){$e=Field $id;([System.Windows.Automation.ValuePattern]$e.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)).Current.Value.Replace("`r`n","`n")}
function Check([string]$name,[string]$actual,[string]$expected){
 $pass=$actual -ceq $expected;$results.Add([pscustomobject]@{test=$name;passed=$pass;actual=$actual;expected=$expected})
 if(-not $pass){throw "$name failed: expected [$expected], got [$actual]"}
 Write-Host "PASS $name"
}
$hello=U 'c548 b155';$hangul=U 'd55c ae00'
$sample=[IO.File]::ReadAllText((Join-Path $projectRoot 'tests\input-sample.json')) | ConvertFrom-Json
try {
 $main=[DesktopUi]::Find('Atypoi.Tray.1')
 [DesktopUi]::Send([DesktopUi]::GetDlgItem($main,105),0x14e,1)
 [DesktopUi]::Send($main,0x111,(105 -bor (1 -shl 16)))
 ClearField plain;TypeKeys $sample.keys;Check 'requested mixed sentence' (Value plain) $sample.text
 ClearField plain;TypeKeys 'dlTdjT';Check 'double final live syllable' (Value plain) (U 'c788 c5c8')
 TypeKeys 'e';Check 'next initial stays Korean' (Value plain) (U 'c788 c5c8 3137')
 TypeKeys 'k';Check 'double final completed word' (Value plain) (U 'c788 c5c8 b2e4')
 [DesktopUi]::Press(8);Check 'Backspace keeps Korean initial' (Value plain) (U 'c788 c5c8 3137')
 ClearField plain;TypeKeys 'dlT';[DesktopUi]::Press(8);Check 'Backspace removes double final' (Value plain) (U 'c774')
 [DesktopUi]::Press(8);Check 'Backspace keeps standalone jamo' (Value plain) (U '3147')
 ClearField plain;TypeKeys 'hello goT';Check 'double final after English' (Value plain) ('hello '+(U 'd588'))
 ClearField plain;TypeKeys 'TjTek qhkTek rkTek gkfxrh';Check 'shift and compound finals' (Value plain) (U 'c37c b2e4 0020 bd24 b2e4 0020 ac14 b2e4 0020 d565 ace0')
 ClearField plain;TypeKeys 'the The got GoT TOP API sunlight leaf ';Check 'English casing preserved' (Value plain) 'the The got GoT TOP API sunlight leaf '
 ClearField plain;TypeKeys 'dkssud';[DesktopUi]::Press(120)
 [DesktopUi]::Press(32);Check 'manual candidate shortcut' (Value plain) 'dkssud '
 ClearField plain;TypeKeys 'gksrnr';[DesktopUi]::Press(8);[DesktopUi]::Press(32);Check 'jamo backspace' (Value plain) ((U 'd55c ad6c')+' ')
 ClearField plain;TypeKeys 'dkssud';[DesktopUi]::Press(13);Check 'first Enter commits only' (Value plain) $hello
 [DesktopUi]::Press(13);Check 'second Enter passes through' (Value plain) ($hello+"`n")
 ClearField plain;TypeKeys 'githubdp APIfmf ';Check 'English with Korean particles' (Value plain) ('github'+(U 'c5d0')+' API'+(U 'b97c')+' ')
 ClearField second;ClearField plain;TypeKeys 'hello';Focus second;TypeKeys 'dkssud ';Check 'focus switch old field' (Value plain) 'hello';Check 'focus switch new field' (Value second) ($hello+' ')
 ClearField plain;TypeKeys 'gksrmf ';Check 'selection replacement' (Value plain) ($hangul+' ')
 ClearField email;TypeKeys 'dkssud';Check 'email field bypass' (Value email) 'dkssud'
 ClearField password;TypeKeys 'dkssud'
 $pass=$root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty,'PASS: synthetic password unchanged'))
 Check 'password bypass (synthetic fixture)' ([string]($null -ne $pass)) 'True'
 $main=[DesktopUi]::Find('Atypoi.Tray.1')
 [DesktopUi]::Send([DesktopUi]::GetDlgItem($main,101),0x14e,3)
 [DesktopUi]::Send($main,0x111,(101 -bor (1 -shl 16)))
 ClearField plain;TypeKeys 'dkssud ';Check 'pause passes original keys' (Value plain) 'dkssud '
 [DesktopUi]::Send([DesktopUi]::GetDlgItem($main,101),0x14e,0)
 [DesktopUi]::Send($main,0x111,(101 -bor (1 -shl 16)))
 Start-Sleep -Milliseconds 600
 ClearField plain;TypeKeys $sample.keys;Check 'resume automatic input' (Value plain) $sample.text
 [DesktopUi]::Capture($p.MainWindowHandle,"$projectRoot\build\verified-browser.png")
} finally {
 $main=[DesktopUi]::Find('Atypoi.Tray.1')
 if($main -ne [IntPtr]::Zero){[DesktopUi]::Send([DesktopUi]::GetDlgItem($main,101),0x14e,0);[DesktopUi]::Send($main,0x111,(101 -bor (1 -shl 16)))}
 $results | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 "$projectRoot\build\installed-input-results.json"
}
