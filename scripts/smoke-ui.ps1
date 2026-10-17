$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class AtypoiUi {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int command);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] private static extern bool PrintWindow(IntPtr window,IntPtr dc,uint flags);
 [DllImport("user32.dll")] private static extern bool RedrawWindow(IntPtr window,IntPtr rect,IntPtr region,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr SendMessage(IntPtr window,uint msg,IntPtr w,StringBuilder l);
 [DllImport("user32.dll")] private static extern IntPtr SendMessageTimeout(IntPtr window,uint msg,IntPtr w,IntPtr l,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string text);
 public static IntPtr Find(string cls) { return FindWindow(cls,null); }
 public static void Send(IntPtr window,uint msg,int w=0) { IntPtr result; if(SendMessageTimeout(window,msg,(IntPtr)w,IntPtr.Zero,2,3000,out result)==IntPtr.Zero)throw new Exception("Window did not respond"); }
 public static string Read(IntPtr window) {var text=new StringBuilder(4096);SendMessage(window,13,(IntPtr)4096,text);return text.ToString();}
 public static void Capture(IntPtr window,string path) {Rect r;GetWindowRect(window,out r);RedrawWindow(window,IntPtr.Zero,IntPtr.Zero,0x185);using(var b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(var g=Graphics.FromImage(b)){IntPtr dc=g.GetHdc();try{if(!PrintWindow(window,dc,2))throw new Exception("PrintWindow failed");}finally{g.ReleaseHdc(dc);}}b.Save(path,ImageFormat.Png);}}
}
'@
if ([AtypoiUi]::Find('Atypoi.Tray.1') -ne [IntPtr]::Zero) { throw 'Close the running Atypoi instance before this isolated smoke test.' }
$beforeStartup = [Microsoft.Win32.Registry]::GetValue('HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run','Atypoi',$null)
$process = Start-Process -FilePath (Join-Path $projectRoot 'dist\Atypoi.exe') -ArgumentList '--demo' -WindowStyle Hidden -PassThru
try {
    [void]$process.WaitForInputIdle(10000)
    $main = [AtypoiUi]::Find('Atypoi.Tray.1')
    if ($main -eq [IntPtr]::Zero) { throw 'Tray settings window was not created.' }
    $link=[AtypoiUi]::GetDlgItem($main,122)
    if($link -eq [IntPtr]::Zero -or [AtypoiUi]::Read($link) -notlike '*https://github.com/ssrihappy/atypoi*'){throw 'Repository link missing'}
    [void][AtypoiUi]::ShowWindow($main,5)
    [AtypoiUi]::Send($main,0)
    [AtypoiUi]::Capture($main,(Join-Path $projectRoot 'build\settings.png'))
    [AtypoiUi]::Send($main,0x111,121)
    if (-not [AtypoiUi]::IsWindowVisible([AtypoiUi]::GetDlgItem($main,106)) -or [AtypoiUi]::IsWindowVisible([AtypoiUi]::GetDlgItem($main,101))) { throw 'App settings navigation failed.' }
    [AtypoiUi]::Capture($main,(Join-Path $projectRoot 'build\apps.png'))
    [AtypoiUi]::Send($main,0x111,120)
    if (-not [AtypoiUi]::IsWindowVisible([AtypoiUi]::GetDlgItem($main,101)) -or [AtypoiUi]::IsWindowVisible([AtypoiUi]::GetDlgItem($main,106))) { throw 'General settings navigation failed.' }
    [AtypoiUi]::Send($main,0x111,113)
    $test = [AtypoiUi]::Find('Atypoi.Test')
    if ($test -eq [IntPtr]::Zero) { throw 'Input test window was not created.' }
    $output = [AtypoiUi]::Read([AtypoiUi]::GetDlgItem($test,202))
    $expected = ([IO.File]::ReadAllText((Join-Path $projectRoot 'tests\input-sample.json')) | ConvertFrom-Json).text
    if ($output -ne $expected) { throw "Incorrect preview: $output" }
    [AtypoiUi]::Capture($test,(Join-Path $projectRoot 'build\input-test.png'))
    [AtypoiUi]::Send($test,0x10)
    [AtypoiUi]::Send($main,[AtypoiUi]::RegisterWindowMessage('TaskbarCreated'))
    $second = Start-Process -FilePath (Join-Path $projectRoot 'dist\Atypoi.exe') -ArgumentList '--demo' -WindowStyle Hidden -PassThru
    if (-not $second.WaitForExit(5000)) { throw 'Second instance did not exit.' }
    if ($second.ExitCode -ne 0) { throw 'Second instance failed.' }
    [AtypoiUi]::Send($main,0x10)
    if ([AtypoiUi]::IsWindowVisible($main) -or $process.HasExited) { throw 'Close-to-tray behavior failed.' }
    $stop = Start-Process -FilePath (Join-Path $projectRoot 'dist\Atypoi.exe') -ArgumentList '--quit' -WindowStyle Hidden -PassThru
    if (-not $stop.WaitForExit(5000) -or -not $process.WaitForExit(5000)) { throw 'Quit command did not stop tray.' }
    $afterStartup = [Microsoft.Win32.Registry]::GetValue('HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run','Atypoi',$null)
    if ($beforeStartup -ne $afterStartup) { throw 'Demo changed startup setting.' }
    $setup = Start-Process -FilePath (Join-Path $projectRoot 'dist\Atypoi.Setup.exe') -WindowStyle Hidden -PassThru
    [void]$setup.WaitForInputIdle(10000)
    $setupWindow = [AtypoiUi]::Find('Atypoi.Setup')
    if ($setupWindow -eq [IntPtr]::Zero) { throw 'Setup window missing.' }
    [void][AtypoiUi]::ShowWindow($setupWindow,5)
    [AtypoiUi]::Send($setupWindow,0)
    [AtypoiUi]::Capture($setupWindow,(Join-Path $projectRoot 'build\setup.png'))
    [AtypoiUi]::Send($setupWindow,0x10)
    if (-not $setup.WaitForExit(5000)) { throw 'Setup did not close.' }
    Write-Host 'UI smoke passed: preview, single instance, taskbar restore, close-to-tray, quit, no startup mutation.'
} finally {
    $window = [AtypoiUi]::Find('Atypoi.Tray.1')
    if ($window -ne [IntPtr]::Zero) { [AtypoiUi]::Send($window,0x111,116) }
    $setupWindow = [AtypoiUi]::Find('Atypoi.Setup')
    if ($setupWindow -ne [IntPtr]::Zero) { [AtypoiUi]::Send($setupWindow,0x10) }
}
