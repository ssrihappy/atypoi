$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('DesktopUi' -as [type])) { Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading;
public static class DesktopUi {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] struct Input { public uint type; public InputUnion data; }
 [StructLayout(LayoutKind.Explicit)] struct InputUnion { [FieldOffset(0)] public Key key; [FieldOffset(0)] public Mouse mouse; }
 [StructLayout(LayoutKind.Sequential)] struct Key { public ushort vk,scan; public uint flags,time; public UIntPtr extra; }
 [StructLayout(LayoutKind.Sequential)] struct Mouse { public int x,y; public uint data,flags,time; public UIntPtr extra; }
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int cmd);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 public delegate bool EnumProc(IntPtr hwnd,IntPtr param);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc,IntPtr param);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr window,IntPtr dc,uint flags);
 [DllImport("user32.dll")] static extern bool RedrawWindow(IntPtr window,IntPtr rect,IntPtr region,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr window,uint msg,IntPtr w,StringBuilder l);
 [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr window,uint msg,IntPtr w,IntPtr l,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")] static extern uint SendInput(uint n,Input[] inputs,int size);
 [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code,uint map);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr LoadKeyboardLayout(string name,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern short VkKeyScanEx(char c,IntPtr layout);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll")] public static extern IntPtr GetKeyboardLayout(uint thread);
 public static IntPtr Find(string cls) {return FindWindow(cls,null);}
 public static void Send(IntPtr h,uint msg,int w=0,int l=0) {IntPtr r;if(SendMessageTimeout(h,msg,(IntPtr)w,(IntPtr)l,2,3000,out r)==IntPtr.Zero)throw new Exception("Unresponsive window");}
 public static string Read(IntPtr h) {var s=new StringBuilder(8192);SendMessage(h,13,(IntPtr)s.Capacity,s);return s.ToString();}
 public static void KeyEvent(int vk,bool up=false) {var a=new Input[1];a[0].type=1;a[0].data.key.vk=(ushort)vk;a[0].data.key.scan=(ushort)MapVirtualKey((uint)vk,0);a[0].data.key.flags=up?2u:0u;if(SendInput(1,a,Marshal.SizeOf(typeof(Input)))!=1)throw new Exception("SendInput failed");}
 public static void Press(int vk) {KeyEvent(vk);KeyEvent(vk,true);Thread.Sleep(90);}
 public static void Foreground(IntPtr h) {KeyEvent(18);try{SetForegroundWindow(h);}finally{KeyEvent(18,true);}Thread.Sleep(200);if(GetForegroundWindow()!=h)throw new Exception("Cannot focus target");}
 public static void ClickAt(int x,int y) {SetCursorPos(x,y);var a=new Input[2];a[0].data.mouse.flags=2;a[1].data.mouse.flags=4;if(SendInput(2,a,Marshal.SizeOf(typeof(Input)))!=2)throw new Exception("Click failed");Thread.Sleep(200);}
 public static void Click(IntPtr h) {Rect r;if(!GetWindowRect(h,out r))throw new Exception("Missing control");ClickAt(r.Left+30,r.Top+15);}
 public static void Chord(int modifier,int vk) {KeyEvent(modifier);try{Press(vk);}finally{KeyEvent(modifier,true);}Thread.Sleep(150);}
 public static void Type(IntPtr target,string ascii) {Type(target,ascii,90);}
 public static void Type(IntPtr target,string ascii,int delay) {if(delay<10||delay>200)throw new ArgumentOutOfRangeException("delay");if(GetForegroundWindow()!=target)throw new Exception("Target is not foreground");var us=LoadKeyboardLayout("00000409",0x80);foreach(char c in ascii){if(GetForegroundWindow()!=target)throw new Exception("Focus changed; input stopped");short mapped=VkKeyScanEx(c,us);if(c<32||c>126||mapped==-1)throw new Exception("Only ASCII key samples are supported");bool shift=(mapped&0x100)!=0;if(shift)KeyEvent(16);try{KeyEvent(mapped&255);KeyEvent(mapped&255,true);Thread.Sleep(delay);}finally{if(shift)KeyEvent(16,true);}}}
 public static void Capture(IntPtr h,string path) {Rect r;if(!GetWindowRect(h,out r))throw new Exception("Missing window");RedrawWindow(h,IntPtr.Zero,IntPtr.Zero,0x185);using(var b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(var g=Graphics.FromImage(b)){var dc=g.GetHdc();try{if(!PrintWindow(h,dc,2))throw new Exception("Window capture failed");}finally{g.ReleaseHdc(dc);}}b.Save(path,ImageFormat.Png);}}
}
'@ }
