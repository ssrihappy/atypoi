param([string]$Toolchain='', [string]$InputService='')
# Requires the installed/enabled Atypoi profile and running tray in Auto mode.
# Only the synthetic fixture receives input. No messenger is opened or used.
$ErrorActionPreference='Stop'
. "$PSScriptRoot\desktop-ui.ps1"
$projectRoot=Split-Path -Parent $PSScriptRoot
if(-not $Toolchain){$Toolchain=Join-Path $projectRoot '.tools\llvm-mingw-20261006-ucrt-x86_64'}
if(-not $InputService){$InputService=Join-Path $projectRoot 'dist\Atypoi.IME.dll'}
$fixtureDirectory=Join-Path $projectRoot 'build\imm-fixture'
New-Item -ItemType Directory -Force -Path $fixtureDirectory | Out-Null
$executable=Join-Path $fixtureDirectory 'Atypoi-host.exe'
& (Join-Path $Toolchain 'bin\x86_64-w64-mingw32-clang++.exe') -std=c++20 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -static -municode -mwindows (Join-Path $projectRoot 'tests\imm-host.cpp') -lole32 -loleaut32 -loleacc -luuid -ladvapi32 -lshell32 -luser32 -lgdi32 -limm32 -o $executable
if($LASTEXITCODE -ne 0){throw 'IMM fixture build failed.'}
if(-not ('ImmFixtureMessages' -as [type])){Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class ImmFixtureMessages {
 [DllImport("user32.dll",SetLastError=true)]static extern IntPtr SendMessageTimeout(IntPtr h,uint m,IntPtr w,IntPtr l,uint flags,uint timeout,out IntPtr result);
 public static long Send(IntPtr h,uint m,int w=0){IntPtr r;if(SendMessageTimeout(h,m,(IntPtr)w,IntPtr.Zero,2,3000,out r)==IntPtr.Zero)throw new Exception("Fixture did not respond");return r.ToInt64();}
}
'@}
function U([string]$codes){-join($codes.Split(' ')|ForEach-Object{[char][Convert]::ToInt32($_,16)})}
$process=Start-Process -FilePath $executable -ArgumentList ('"'+[IO.Path]::GetFullPath($InputService)+'"') -PassThru -WindowStyle Normal
try{
 [void]$process.WaitForInputIdle(10000)
 $limit=[DateTime]::UtcNow.AddSeconds(5)
 do{$process.Refresh();$window=$process.MainWindowHandle;if($window -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $limit)
 if($window -eq [IntPtr]::Zero){throw 'IMM fixture window was not created.'}
 [DesktopUi]::Foreground($window)
 $bounds=[DesktopUi+Rect]::new()
 if(-not [DesktopUi]::GetWindowRect($window,[ref]$bounds)){throw 'IMM fixture bounds are unavailable.'}
 [DesktopUi]::ClickAt($bounds.Left+80,$bounds.Top+80)
 if([ImmFixtureMessages]::Send($window,0x8001) -ne 0){throw 'Install and enable the current Atypoi profile before running this test.'}
 function AssertFocus {if([DesktopUi]::GetForegroundWindow() -ne $window){throw 'Focus changed. Physical input stopped.'}}
 function TypeKeys([string]$keys){AssertFocus;[DesktopUi]::Type($window,$keys,25)}
 function PressKey([int]$key){AssertFocus;[DesktopUi]::Press($key)}
 function Check([string]$name,[string]$expected){$actual=[DesktopUi]::Read($window);if($actual -cne $expected){throw "IMM $name failed: expected [$expected], got [$actual]"};Write-Host "PASS IMM $name"}
 function ClearFixture {PressKey 39;if([ImmFixtureMessages]::Send($window,0x8002) -ne 1){throw 'Synthetic composition did not finish.'}}
 TypeKeys 'gksrmf';Check 'live token composition' (U 'd55c ae00');ClearFixture
 TypeKeys 'aksemfdjTek';Check 'Shift double final' (U 'b9cc b4e4 c5c8 b2e4')
 PressKey 8;Check 'Backspace in double final word' (U 'b9cc b4e4 c5c8 3137')
 TypeKeys 'k';Check 'resume after Backspace' (U 'b9cc b4e4 c5c8 b2e4');ClearFixture
 $sample=[IO.File]::ReadAllText((Join-Path $projectRoot 'tests\input-sample.json'))|ConvertFrom-Json
 TypeKeys $sample.keys;Check 'requested mixed sentence' $sample.text
 PressKey 39;Check 'committed mixed sentence' $sample.text
 [DesktopUi]::Capture($window,(Join-Path $projectRoot 'build\imm-input.png'));ClearFixture
 TypeKeys 'the The got GoT TOP API sunlight leaf ';Check 'English casing and spaces' 'the The got GoT TOP API sunlight leaf ';ClearFixture
 TypeKeys 'gksrmf! hello, sunlight. leaf? ';Check 'punctuation and boundaries' ((U 'd55c ae00')+'! hello, sunlight. leaf? ');ClearFixture
 [void][ImmFixtureMessages]::Send($window,0x8003,0x20000000)
 TypeKeys 'gksrmf';Check 'protected field bypass' 'gksrmf';ClearFixture
 [void][ImmFixtureMessages]::Send($window,0x8003,0)
 TypeKeys 'gksrmf ';Check 'recovery after protected field' ((U 'd55c ae00')+' ')
}finally{
 if(-not $process.HasExited){[void]$process.CloseMainWindow();[void]$process.WaitForExit(5000)}
}
if(-not $process.HasExited -or $process.ExitCode -ne 0){throw 'IMM fixture failed its accessibility privacy check or did not exit.'}
