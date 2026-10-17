$ErrorActionPreference='Stop'
. "$PSScriptRoot\desktop-ui.ps1"
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class NoticePopup {
 [DllImport("user32.dll")] public static extern IntPtr GetLastActivePopup(IntPtr window);
}
'@
$root=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$compiler=Join-Path $root '.tools\llvm-mingw-20261006-ucrt-x86_64\bin\x86_64-w64-mingw32-clang++.exe'
if(-not (Test-Path -LiteralPath $compiler)){throw 'Prepare the pinned compiler with scripts/bootstrap.ps1 first.'}
if(-not (Test-Path -LiteralPath 'build\Atypoi.TestIME.dll')){throw 'Run scripts/build.ps1 first.'}
if([DesktopUi]::Find('Atypoi.StaleFixture') -ne [IntPtr]::Zero){throw 'Another restart-notice fixture is already running.'}
New-Item -ItemType Directory -Force build\stale-fixture | Out-Null
Copy-Item -LiteralPath build\Atypoi.TestIME.dll -Destination build\stale-fixture\Atypoi.IME.dll -Force
& $compiler -std=c++20 -O2 -DUNICODE -D_UNICODE -static -municode -mwindows tests/stale-host.cpp -luser32 -lgdi32 -o build/stale-host.exe
if($LASTEXITCODE -ne 0){throw 'Restart-notice fixture compilation failed.'}
$installed=(Get-Process Atypoi -ErrorAction SilentlyContinue | Select-Object -First 1).Path
if($installed){[DesktopUi]::Send([DesktopUi]::Find('Atypoi.Tray.1'),0x111,116);Start-Sleep -Milliseconds 400}
$tray=$null;$fixture=$null
try{
 $tray=Start-Process -FilePath (Join-Path $root 'dist\Atypoi.exe') -ArgumentList '--background' -WindowStyle Hidden -PassThru
 [void]$tray.WaitForInputIdle(10000)
 $main=[DesktopUi]::Find('Atypoi.Tray.1')
 $fixture=Start-Process -FilePath (Join-Path $root 'build\stale-host.exe') -WindowStyle Hidden -PassThru
 [void]$fixture.WaitForInputIdle(10000)
 $h=[DesktopUi]::Find('Atypoi.StaleFixture')
 [DesktopUi]::Foreground($h);[DesktopUi]::Press(27)
 Start-Sleep -Milliseconds 1100
 $status=[DesktopUi]::Read([DesktopUi]::GetDlgItem($main,123))
 if($status -notlike 'stale-host.exe*'){throw 'Status did not identify the stale host.'}
 [DesktopUi]::Send($main,0x8001,0,((1 -shl 16) -bor 0x405))
 if(-not [DesktopUi]::IsWindowVisible($main)){throw 'Restart balloon did not open settings.'}
 if([NoticePopup]::GetLastActivePopup($main) -ne $main){throw 'Restart balloon unexpectedly opened an update dialog.'}
 [DesktopUi]::Capture($main,(Join-Path $root 'build\restart-notice-settings.png'))
 'Actual stale module detected; app name displayed; restart notification opens settings without invoking updates.'
}finally{
 $main=[DesktopUi]::Find('Atypoi.Tray.1')
 if($main -ne [IntPtr]::Zero){[DesktopUi]::Send($main,0x111,116)}
 $h=[DesktopUi]::Find('Atypoi.StaleFixture')
 if($h -ne [IntPtr]::Zero){[DesktopUi]::Send($h,0x10)}
 if($tray){[void]$tray.WaitForExit(5000)}
 if($fixture){[void]$fixture.WaitForExit(5000)}
 if($installed){Start-Process -FilePath $installed -ArgumentList '--background' -WindowStyle Hidden}
}
