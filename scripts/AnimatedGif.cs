using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Text;

// Assembles GIF frames encoded by Windows GDI+. No third-party encoder required.
public static class AtypoiGif {
    static Color[] Palette(Bitmap first) {
        var counts=new Dictionary<int,int>();
        var bits=first.LockBits(new Rectangle(0,0,first.Width,first.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        try {byte[] row=new byte[first.Width*4];for(int y=0;y<first.Height;y++){System.Runtime.InteropServices.Marshal.Copy(IntPtr.Add(bits.Scan0,y*bits.Stride),row,0,row.Length);for(int x=0;x<first.Width;x++){int p=x*4,rgb=row[p]|row[p+1]<<8|row[p+2]<<16,value;counts.TryGetValue(rgb,out value);counts[rgb]=value+1;}}}finally{first.UnlockBits(bits);}
        var entries=new List<KeyValuePair<int,int>>(counts);entries.Sort((a,b)=>b.Value.CompareTo(a.Value));
        var colors=new Color[256];for(int i=0;i<256;i++){int rgb=i<entries.Count?entries[i].Key:0;colors[i]=Color.FromArgb((rgb>>16)&255,(rgb>>8)&255,rgb&255);}return colors;
    }
    static Bitmap Indexed(Bitmap source,Rectangle rect,Color[] colors,Dictionary<int,byte> cache) {
        var result=new Bitmap(rect.Width,rect.Height,PixelFormat.Format8bppIndexed);var palette=result.Palette;for(int i=0;i<256;i++)palette.Entries[i]=colors[i];result.Palette=palette;
        var input=source.LockBits(rect,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        var output=result.LockBits(new Rectangle(0,0,rect.Width,rect.Height),ImageLockMode.WriteOnly,PixelFormat.Format8bppIndexed);
        try {byte[] pixels=new byte[rect.Width*4],row=new byte[rect.Width];for(int y=0;y<rect.Height;y++){
            System.Runtime.InteropServices.Marshal.Copy(IntPtr.Add(input.Scan0,y*input.Stride),pixels,0,pixels.Length);
            for(int x=0;x<rect.Width;x++){int p=x*4,rgb=pixels[p]|pixels[p+1]<<8|pixels[p+2]<<16;byte index;
                if(!cache.TryGetValue(rgb,out index)){int best=int.MaxValue;index=0;for(int i=0;i<256;i++){int r=pixels[p+2]-colors[i].R,g=pixels[p+1]-colors[i].G,b=pixels[p]-colors[i].B,distance=r*r+g*g+b*b;if(distance<best){best=distance;index=(byte)i;if(best==0)break;}}cache[rgb]=index;}row[x]=index;
            }System.Runtime.InteropServices.Marshal.Copy(row,0,IntPtr.Add(output.Scan0,y*output.Stride),row.Length);
        }}finally{source.UnlockBits(input);result.UnlockBits(output);}return result;
    }
    static Rectangle Changes(Bitmap previous,Bitmap next) {
        if(previous==null)return new Rectangle(0,0,next.Width,next.Height);
        int left=next.Width,top=next.Height,right=-1,bottom=-1;
        Rectangle all=new Rectangle(0,0,next.Width,next.Height);
        var a=previous.LockBits(all,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        var b=next.LockBits(all,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        try {
            byte[] ar=new byte[next.Width*4],br=new byte[next.Width*4];
            for(int y=0;y<next.Height;y++) {
                System.Runtime.InteropServices.Marshal.Copy(IntPtr.Add(a.Scan0,y*a.Stride),ar,0,ar.Length);
                System.Runtime.InteropServices.Marshal.Copy(IntPtr.Add(b.Scan0,y*b.Stride),br,0,br.Length);
                for(int x=0;x<next.Width;x++){int p=x*4;if(ar[p]!=br[p]||ar[p+1]!=br[p+1]||ar[p+2]!=br[p+2]){left=Math.Min(left,x);right=Math.Max(right,x);top=Math.Min(top,y);bottom=y;}}
            }
        } finally {previous.UnlockBits(a);next.UnlockBits(b);}
        return right<0?new Rectangle(0,0,1,1):Rectangle.FromLTRB(left,top,right+1,bottom+1);
    }
    static void Frame(BinaryWriter output,Bitmap bitmap,Rectangle rect,int delay,Color[] colors,Dictionary<int,byte> cache) {
        byte[] data;
        using(var indexed=Indexed(bitmap,rect,colors,cache))using(var stream=new MemoryStream()){indexed.Save(stream,ImageFormat.Gif);data=stream.ToArray();}
        int packed=data[10],pos=13;byte[] global=null;
        if((packed&128)!=0){int count=3*(2<<(packed&7));global=new byte[count];Array.Copy(data,pos,global,0,count);pos+=count;}
        while(pos<data.Length && data[pos]!=0x2c) {
            if(data[pos++]!=0x21)throw new InvalidDataException("Unexpected GIF block");pos++;
            int size;while((size=data[pos++])!=0)pos+=size;
        }
        if(pos>=data.Length)throw new InvalidDataException("GIF image missing");
        int imagePacked=data[pos+9];pos+=10;byte[] palette=global;int tableBits=packed&7;
        if((imagePacked&128)!=0){tableBits=imagePacked&7;int count=3*(2<<tableBits);palette=new byte[count];Array.Copy(data,pos,palette,0,count);pos+=count;}
        if(palette==null)throw new InvalidDataException("GIF palette missing");
        output.Write(new byte[]{0x21,0xf9,4,4});output.Write((ushort)delay);output.Write((byte)0);output.Write((byte)0);
        output.Write((byte)0x2c);output.Write((ushort)rect.Left);output.Write((ushort)rect.Top);output.Write((ushort)rect.Width);output.Write((ushort)rect.Height);
        output.Write((byte)(0x80|tableBits|(imagePacked&0x40)));output.Write(palette);
        output.Write(data[pos++]); // LZW minimum code size
        while(true){int size=data[pos++];output.Write((byte)size);if(size==0)break;output.Write(data,pos,size);pos+=size;}
    }
    public static void Encode(string[] paths,string destination,int delay) {
        if(paths.Length==0)throw new ArgumentException("No captured frames");
        Bitmap previous=null;
        try {
            using(var file=File.Create(destination))using(var writer=new BinaryWriter(file)) {
                Color[] colors;var cache=new Dictionary<int,byte>();
                using(var first=new Bitmap(paths[0])){colors=Palette(first);writer.Write(Encoding.ASCII.GetBytes("GIF89a"));writer.Write((ushort)first.Width);writer.Write((ushort)first.Height);writer.Write(new byte[]{0x70,0,0});}
                writer.Write(new byte[]{0x21,0xff,11});writer.Write(Encoding.ASCII.GetBytes("NETSCAPE2.0"));writer.Write(new byte[]{3,1,0,0,0});
                for(int i=0;i<paths.Length;i++) {
                    var bitmap=new Bitmap(paths[i]);
                    if(previous!=null&&(previous.Size!=bitmap.Size))throw new InvalidDataException("Window changed size during capture");
                    var rect=Changes(previous,bitmap);Frame(writer,bitmap,rect,i==0?70:i==paths.Length-1?250:delay,colors,cache);
                    if(previous!=null)previous.Dispose();previous=bitmap;
                }
                writer.Write((byte)0x3b);
            }
        } finally {if(previous!=null)previous.Dispose();}
        using(var check=Image.FromFile(destination)){if(check.GetFrameCount(FrameDimension.Time)!=paths.Length)throw new InvalidDataException("Animation frame count mismatch");}
    }
}
