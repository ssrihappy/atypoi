# Runs only in the synthetic Chrome fixture launched by test-browser-input.ps1.
. "$PSScriptRoot\desktop-ui.ps1"
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$root=Split-Path -Parent $PSScriptRoot
$cases=[IO.File]::ReadAllText((Join-Path $root 'tests\past-tense-input.json')) | ConvertFrom-Json
$browser=Get-Process -Id ([int](Get-Content (Join-Path $root 'build\test-browser-pid.txt')))
if($browser.MainWindowTitle -notlike 'Atypoi Windows keyboard verification*'){throw 'Unexpected browser; no input sent'}
$element=[Windows.Automation.AutomationElement]::FromHandle($browser.MainWindowHandle)
$field=$element.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'plain'))
$results=[Collections.Generic.List[object]]::new()
function Read-Field {([Windows.Automation.ValuePattern]$field.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern)).Current.Value}
function Prepare-Field {
    [DesktopUi]::Foreground($browser.MainWindowHandle);[DesktopUi]::Press(27)
    $bounds=$field.Current.BoundingRectangle
    [DesktopUi]::ClickAt([int]($bounds.Left+25),[int]($bounds.Top+20));[DesktopUi]::Chord(17,65);[DesktopUi]::Press(8)
}
function Check-Result([string]$name,[string]$expected){$actual=Read-Field;$passed=$actual -ceq $expected;$results.Add([pscustomobject]@{test=$name;passed=$passed;actual=$actual;expected=$expected});if(-not $passed){throw "Failed $name"};Write-Host "PASS $name"}
try {
    foreach($case in $cases){Prepare-Field;[DesktopUi]::Type($browser.MainWindowHandle,$case.keys,25);Check-Result $case.keys $case.text}
    Prepare-Field;[DesktopUi]::Type($browser.MainWindowHandle,$cases[0].keys,25)
    [DesktopUi]::Press(8);Check-Result 'past ending Backspace' ($cases[0].text.Substring(0,$cases[0].text.Length-1)+[char]0x3137)
    [DesktopUi]::Type($browser.MainWindowHandle,'k');Check-Result 'past ending retype' $cases[0].text
    [DesktopUi]::Press(120);Check-Result 'past word raw candidate' $cases[0].keys
    [DesktopUi]::Press(120);Check-Result 'past word Korean candidate' $cases[0].text
    [DesktopUi]::Press(32);Check-Result 'past word Space commit' ($cases[0].text+' ')
    [DesktopUi]::Capture($browser.MainWindowHandle,(Join-Path $root 'build\past-tense-browser.png'))
} finally {$results | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $root 'build\past-tense-results.json') -Encoding UTF8}
