// Build with the installed .NET Framework csc /platform:x86. Loads the user's
// existing Granny 2.11 reader; no downloaded code or binary patching is needed.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;
public static class SourceRigidMesh {
    private const string Dll="F:/SF_V3_5/CLIENT/granny2.dll";
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern IntPtr GrannyReadEntireFile(string path);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern IntPtr GrannyGetFileInfo(IntPtr file);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern void GrannyFreeFile(IntPtr file);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern int GrannyGetMeshVertexCount(IntPtr mesh);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern int GrannyGetMeshIndexCount(IntPtr mesh);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern int GrannyGetMeshTriangleGroupCount(IntPtr mesh);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern IntPtr GrannyGetMeshTriangleGroups(IntPtr mesh);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern void GrannyCopyMeshVertices(IntPtr mesh,IntPtr type,IntPtr output);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern void GrannyCopyMeshIndices(IntPtr mesh,int size,IntPtr output);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern bool GrannyMeshIsRigid(IntPtr mesh);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern void GrannyGetModelInitialPlacement4x4(IntPtr model,[Out] float[] matrix);
    [DllImport(Dll,CallingConvention=CallingConvention.StdCall)] private static extern IntPtr GrannyGetMaterialTextureByType(IntPtr material,int type);
    [DllImport("kernel32",CharSet=CharSet.Ansi,SetLastError=true)] private static extern IntPtr LoadLibrary(string name);
    [DllImport("kernel32",CharSet=CharSet.Ansi,ExactSpelling=true)] private static extern IntPtr GetProcAddress(IntPtr module,string name);
    private static int Number(IntPtr p,int offset){return Marshal.ReadInt32(p,offset);}
    private static IntPtr Pointer(IntPtr p,int offset){return Marshal.ReadIntPtr(p,offset);}
    private static string Text(IntPtr p){return p==IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(p);}
    public static int Main(string[] args){
        IntPtr file=IntPtr.Zero;
        try{
            if(IntPtr.Size!=4 || args.Length!=2)throw new Exception("Use x86 SDK reader: source.gr2 output.json");
            file=GrannyReadEntireFile(args[0]);if(file==IntPtr.Zero)throw new Exception("Cannot read original GR2 with Granny 2.11");
            IntPtr info=GrannyGetFileInfo(file);
            // granny_file_info: 3 initial pointers, then 11 count/pointer pairs.
            int models=Number(info,60),animations=Number(info,76);
            if(models!=1 || animations!=0)throw new Exception("Expected one static source model; models="+models+" animations="+animations);
            IntPtr model=Pointer(Pointer(info,64),0);var placement=new float[16];GrannyGetModelInitialPlacement4x4(model,placement);
            // granny_model: Name, Skeleton, granny_transform (68 bytes), count, bindings.
            int count=Number(model,76);IntPtr bindings=Pointer(model,80);
            var meshes=new List<object>();IntPtr module=LoadLibrary(Dll),vertexType=Marshal.ReadIntPtr(GetProcAddress(module,"GrannyPNT332VertexType"));
            for(int i=0;i<count;++i){
                IntPtr mesh=Pointer(bindings,i*4);if(!GrannyMeshIsRigid(mesh))throw new Exception("Refuse skinned source geometry");
                int vertices=GrannyGetMeshVertexCount(mesh),indexCount=GrannyGetMeshIndexCount(mesh);if(vertices<=0 || indexCount<=0 || indexCount%3!=0)throw new Exception("Empty source geometry");
                IntPtr buffer=Marshal.AllocHGlobal(vertices*32),indexBuffer=Marshal.AllocHGlobal(indexCount*4);
                var positions=new List<float[]>();var normals=new List<float[]>();var uv=new List<float[]>();var indices=new int[indexCount];
                try{
                    GrannyCopyMeshVertices(mesh,vertexType,buffer);var values=new float[vertices*8];Marshal.Copy(buffer,values,0,values.Length);
                    GrannyCopyMeshIndices(mesh,4,indexBuffer);Marshal.Copy(indexBuffer,indices,0,indices.Length);
                    for(int j=0;j<vertices;++j){int at=j*8;positions.Add(new[]{values[at],values[at+1],values[at+2]});normals.Add(new[]{values[at+3],values[at+4],values[at+5]});uv.Add(new[]{values[at+6],values[at+7]});}
                }finally{Marshal.FreeHGlobal(buffer);Marshal.FreeHGlobal(indexBuffer);}
                var groups=new List<object>();IntPtr group=GrannyGetMeshTriangleGroups(mesh);
                for(int j=0;j<GrannyGetMeshTriangleGroupCount(mesh);++j)groups.Add(new{material=Number(group,j*12),first=Number(group,j*12+4)*3,count=Number(group,j*12+8)*3});
                var materials=new List<object>();int materialCount=Number(mesh,20);IntPtr materialBindings=Pointer(mesh,24);
                for(int j=0;j<materialCount;++j){IntPtr material=Pointer(materialBindings,j*4),texture=GrannyGetMaterialTextureByType(material,2);materials.Add(new{name=Text(Pointer(material,0)),texture=texture==IntPtr.Zero ? "" : Text(Pointer(texture,0))});}
                meshes.Add(new{name=Text(Pointer(mesh,0)),positions=positions,normals=normals,uv=uv,indices=indices,groups=groups,materials=materials});
            }
            if(meshes.Count==0)throw new Exception("No source model bindings");
            var serializer=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};File.WriteAllText(args[1],serializer.Serialize(new{source=args[0],sdk="2.11.8.0",placement=placement,meshes=meshes})+"\n");
            Console.WriteLine("SOURCE_RIGID_MESH=SUCCESS meshes="+meshes.Count);return 0;
        }catch(Exception e){Console.Error.WriteLine(e);return 1;}finally{if(file!=IntPtr.Zero)GrannyFreeFile(file);}
    }
}
