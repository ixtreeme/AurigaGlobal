using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Reflection;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using Metin2.Runtime;
using Metin2.Runtime.Network;
[InitializeOnLoad] public static class Metin2MountFlightQA
{
    const string Request="Migration/MountFlight/run-qa.request";
    static int checks;
    static Metin2MountFlightQA(){EditorApplication.update+=Poll;}
    static void Poll()
    {
        if(EditorApplication.isCompiling||EditorApplication.isUpdating||!File.Exists(Request))return;
        string revision=File.GetLastWriteTimeUtc(Request).Ticks.ToString()+"|local-route-v2";
        if(SessionState.GetString("MountFlight.QARevision","")==revision)return;
        if(EditorApplication.isPlaying){EditorApplication.isPlaying=false;return;}
        if(EditorApplication.isPlayingOrWillChangePlaymode)return;
        SessionState.SetString("MountFlight.QARevision",revision);Run();
        if(File.ReadAllText(Request).Contains("OPEN_MAIN")&&File.ReadAllText("Migration/MountFlight/client-qa.txt").StartsWith("PASS "))TeraCompanionReleaseContent.OpenMainForTesting();
    }
    static void Check(bool ok,string message){checks++;if(!ok)throw new Exception(message);}
    static void Set(object target,string name,object value)=>target.GetType().GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).SetValue(target,value);
    static byte[] Packet(uint vid,byte flags,int altitude,sbyte axis=0)
    {var p=new byte[11];p[0]=225;Buffer.BlockCopy(BitConverter.GetBytes(vid),0,p,1,4);p[5]=flags;Buffer.BlockCopy(BitConverter.GetBytes(altitude),0,p,6,4);p[10]=unchecked((byte)axis);return p;}
    [MenuItem("Metin2/Tests/Mount Flight")]
    public static void Run()
    {
        if(EditorApplication.isPlaying)throw new InvalidOperationException("Stop Play before flight QA");
        var manifest=JsonUtility.FromJson<TeraCompanionReleaseBuilder.Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));
        var flyers=new HashSet<uint>(manifest.entries.Where(e=>e.kind=="mount"&&e.clips.Any(c=>c.Equals("FlyForward",StringComparison.OrdinalIgnoreCase))).Select(e=>e.item));
        var itemCatalog=AssetDatabase.LoadAssetAtPath<Metin2ItemCatalog>("Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset");
        for(int i=0;i<itemCatalog.entries.Length;i++)if(flyers.Contains(itemCatalog.entries[i].vnum)){var item=itemCatalog.entries[i];item.value5=1;itemCatalog.entries[i]=item;}
        EditorUtility.SetDirty(itemCatalog);AssetDatabase.SaveAssets();
        var previous=SceneManager.GetActiveScene();var scene=EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Additive);SceneManager.SetActiveScene(scene);
        var assets=new List<UnityEngine.Object>();checks=0;
        try
        {
            var wire=Metin2ProtocolCodec.TrimPacketSequence(Metin2ProtocolCodec.WriteMountFlight(1,2600,1234,99));
            Check(wire.Length==10&&wire[0]==225&&BitConverter.ToInt32(wire,2)==2600,"CG wire layout");
            var framer=Metin2PacketFramer.CreateGameCoreFramer();var snapshot=Packet(200,3,5000);
            Check(framer.Feed(snapshot.AsSpan(0,4)).Count==0,"Fragment prematurely decoded");
            Check(framer.Feed(snapshot.AsSpan(4)).Single().SequenceEqual(snapshot),"Fragmented flight state did not roundtrip");
            var data=new TerrainData{heightmapResolution=33,size=new Vector3(256,100,256)};assets.Add(data);
            Action<float> height=h=>{var values=new float[33,33];for(int y=0;y<33;y++)for(int x=0;x<33;x++)values[y,x]=h/100;data.SetHeights(0,0,values);};height(20);
            var terrainRoot=Terrain.CreateTerrainGameObject(data);terrainRoot.transform.position=new Vector3(0,0,-256);
            var definition=ScriptableObject.CreateInstance<Metin2MapDefinition>();assets.Add(definition);definition.width=definition.height=1;
            var world=new GameObject("Flight terrain").AddComponent<Metin2WorldRuntime>();
            Set(world,"currentMap",terrainRoot);Set(world,"terrains",new[]{terrainRoot.GetComponent<Terrain>()});
            typeof(Metin2WorldRuntime).GetProperty("Current").SetValue(world,new Metin2MapCatalog.Entry{mapName="FlightQA",width=1,height=1,definition=definition,prefab=terrainRoot});
            var local=new GameObject("Local flyer");var controller=local.AddComponent<Metin2PlayerController>();controller.Initialize(new Vector3(10,20,-10),null);Set(controller,"footOffset",.6f);controller.PlaceOnGround(new Vector3(10,20,-10));controller.SetMounted(true);
            var localWorld=new GameObject("Local receiver").AddComponent<Metin2NetworkEntityWorld>();Set(localWorld,"worldRuntime",world);localWorld.LocalVid=100;localWorld.LocalPlayerRoot=local.transform;
            var requests=new List<byte>();localWorld.FlightRequested+=(action,z,t)=>requests.Add(action);
            var flight=localWorld.AttachFlight(local,100,controller);
            localWorld.HandlePacket(Packet(100,0,0));flight.Tick(1,.1f,0);Check(!flight.Flying,"Unflagged mount took off");
            localWorld.HandlePacket(Packet(100,1,0));Check(flight.Capable,"Local-player snapshot did not grant capability");for(int i=0;i<10;i++)flight.Tick(1,.1f,0);
            Check(flight.Flying&&Mathf.Abs(flight.Altitude-26)<.01f,"Shift ascent speed");float hover=flight.Altitude;
            height(22);local.transform.position+=Vector3.right*20;flight.Tick(0,.1f,0);
            Check(Mathf.Abs(flight.Altitude-hover)<.001f&&Mathf.Abs(controller.LogicalGroundPosition.y-hover)<.001f,"Hill changed hover altitude");
            height(0);local.transform.position+=Vector3.right*20;flight.Tick(0,.1f,0);
            Check(Mathf.Abs(flight.Altitude-hover)<.001f,"Cliff lowered hover altitude");
            height(30);Check(!flight.CanAdvance(local.transform.position+Vector3.right),"Flight tunnelled through higher terrain");height(0);
            for(int i=0;i<100;i++)flight.Tick(-1,.1f,0);
            Check(!flight.Flying&&Mathf.Abs(controller.LogicalGroundPosition.y)<.01f&&requests.Contains((byte)2),"Ctrl landing failed");
            height(20);
            var observers=new List<Metin2MountFlight>();
            for(int i=0;i<2;i++)
            {
                var receiver=new GameObject("Receiver "+i).AddComponent<Metin2NetworkEntityWorld>();Set(receiver,"worldRuntime",world);
                Check(receiver.HandlePacket(snapshot),"GC flight routing"); // Packet before asynchronous actor creation.
                var root=new GameObject("Observed rider "+i);root.transform.position=new Vector3(10,20,-10);
                var entity=root.AddComponent<Metin2NetworkEntity>();
                var dict=(Dictionary<uint,Metin2NetworkEntity>)typeof(Metin2NetworkEntityWorld).GetField("entities",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(receiver);dict[200]=entity;
                var remote=receiver.AttachFlight(root,200,null);observers.Add(remote);
                Check(remote.Flying&&Mathf.Abs(root.transform.position.y-50)<.001f,"Late actor lost flight snapshot");
                receiver.HandlePacket(Packet(200,1,5000));Check(!remote.Flying&&Mathf.Abs(root.transform.position.y-20)<.01f,"Remote landing/revocation did not ground actor");
                receiver.HandlePacket(snapshot);Check(remote.Flying&&Mathf.Abs(root.transform.position.y-50)<.001f,"Remote airborne state replay");
            }
            Check(observers[0].Altitude==observers[1].Altitude,"Observers disagree on altitude");
            var release=JsonUtility.FromJson<TeraCompanionReleaseBuilder.Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));var entry=release.entries.First(e=>e.item==630090u);
            var prefab=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabAsset>("Assets/Metin2/AddressableContent/Races/Prefabs/"+entry.mob+".asset").prefab;
            var mount=UnityEngine.Object.Instantiate(prefab);var entityForAnimation=observers[0].GetComponent<Metin2NetworkEntity>();typeof(Metin2NetworkEntity).GetProperty("MountInstance").SetValue(entityForAnimation,mount);
            var animator=mount.GetComponentInChildren<Animator>();animator.Rebind();animator.Update(0);observers[0].ApplyAnimation(true);animator.Update(0);
            var clip=animator.GetCurrentAnimatorClipInfo(0).First().clip;
            Check(clip.name.Equals("FlyForward",StringComparison.OrdinalIgnoreCase),"Moving flight did not select original FlyForward");
            observers[0].ApplyAnimation(false);animator.Update(0);clip=animator.GetCurrentAnimatorClipInfo(0).First().clip;
            Check(clip.name.Equals("FlyWait",StringComparison.OrdinalIgnoreCase),"Hover did not select original FlyWait");
            Directory.CreateDirectory("Migration/MountFlight");File.WriteAllText("Migration/MountFlight/client-qa.txt","PASS "+checks+" checks: CG/GC wire and fragmentation, permission, ascent, absolute hover over hills/cliffs, terrain obstruction, Ctrl landing, two receivers, late actor snapshots, revoke, original Gemfly 630090 fly/hover animations.\n");Debug.Log("MOUNT_FLIGHT_CLIENT_QA_SUCCESS checks="+checks);
        }
        catch(Exception e){Directory.CreateDirectory("Migration/MountFlight");File.WriteAllText("Migration/MountFlight/client-qa.txt","FAIL "+e);Debug.LogException(e);}
        finally{EditorSceneManager.CloseScene(scene,true);if(previous.IsValid()&&previous.isLoaded)SceneManager.SetActiveScene(previous);foreach(var asset in assets)if(asset)UnityEngine.Object.DestroyImmediate(asset);}
    }
}
