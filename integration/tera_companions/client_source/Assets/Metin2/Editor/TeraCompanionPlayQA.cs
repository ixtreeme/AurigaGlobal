using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using UnityEditor.SceneManagement;
using Metin2.Runtime;
using Metin2.Runtime.Network;
using TeraCompanionLab;

[InitializeOnLoad] public static class TeraCompanionPlayQA
{
    const string Flag="TeraProductionPlayQA";
    static readonly string[] Cases={"Trans_Lion_Black_01","Ice_Horse","Vehicle_BoneDragon_C","Vehicle_Pegasus","Vehicle_Mechanic_A","Kart_A","Vehicle_Kirin","StoneHead","Pet_Animal_Panda"};
    static readonly List<string> report=new List<string>();
    static int index;static double deadline,started;static GameObject current;static Camera camera;
    static TeraCompanionReleaseBuilder.Entry entry;static Metin2NetworkEntity riderEntity;
    static GameObject riderVisual;static Metin2RaceCatalog catalog;static Metin2RaceDataCatalog dataCatalog;
    static TeraCompanionPresentation presentation;static bool error;
    static readonly BindingFlags Private=BindingFlags.NonPublic|BindingFlags.Instance;
    static TeraCompanionPlayQA(){if(SessionState.GetBool(Flag,false))Attach();}
    static void Attach()
    {
        started=EditorApplication.timeSinceStartup;
        Application.logMessageReceived+=(m,s,t)=>{if(t==LogType.Error||t==LogType.Exception)error=true;};
        EditorApplication.update+=Tick;
    }
    public static void Run()
    {
        EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
        SessionState.SetBool(Flag,true);EditorApplication.isPlaying=true;
    }
    static void Tick()
    {
        if(!EditorApplication.isPlaying)return;
        try
        {
            if(EditorApplication.timeSinceStartup-started>180)throw new Exception("Play validation timeout");
            if(!camera)
            {
                camera=new GameObject("Proof camera").AddComponent<Camera>();camera.gameObject.AddComponent<AudioListener>();
                camera.backgroundColor=new Color(.12f,.16f,.22f);camera.clearFlags=CameraClearFlags.SolidColor;
                var light=new GameObject("Light").AddComponent<Light>();light.type=LightType.Directional;light.intensity=1.1f;light.transform.rotation=Quaternion.Euler(45,-35,0);
                RenderSettings.ambientLight=new Color(.5f,.5f,.5f);
                var floor=GameObject.CreatePrimitive(PrimitiveType.Plane);floor.name="Ground";floor.transform.localScale=Vector3.one*20;
                floor.GetComponent<Renderer>().material.color=new Color(.24f,.27f,.3f);
            }
            if(current==null)
            {
                var release=JsonUtility.FromJson<TeraCompanionReleaseBuilder.Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));
                entry=release.entries.First(e=>e.name==Cases[index]);
                var prefab=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabAsset>("Assets/Metin2/AddressableContent/Races/Prefabs/"+entry.mob+".asset").prefab;
                var data=AssetDatabase.LoadAssetAtPath<Metin2RaceDataAsset>("Assets/Metin2/AddressableContent/Races/Data/"+entry.mob+".asset").entry;
                dataCatalog=ScriptableObject.CreateInstance<Metin2RaceDataCatalog>();dataCatalog.entries=new[]{data};Metin2RaceDataBinder.Catalog=dataCatalog;
                current=new GameObject("Production proof "+entry.name);
                if(entry.kind=="mount")
                {
                    catalog=ScriptableObject.CreateInstance<Metin2RaceCatalog>();catalog.entries=new[]{new Metin2RaceCatalog.Entry{race=entry.mob,prefab=prefab}};
                    riderVisual=UnityEngine.Object.Instantiate(AssetDatabase.LoadAssetAtPath<GameObject>("Assets/Metin2/Generated/Players/Prefabs/pc_warrior.prefab"),current.transform,false);
                    Metin2Units.ApplyCharacterScale(riderVisual);
                    riderEntity=current.AddComponent<Metin2NetworkEntity>();riderEntity.ModelYawOffset=-90;
                    riderEntity.Initialize(991001,0,6,100,100,null,catalog,riderVisual.transform);riderEntity.SetMount(entry.mob);
                    if(!riderEntity.MountInstance)throw new Exception("Production SetMount failed "+entry.name);
                    presentation=riderEntity.MountInstance.GetComponentInChildren<TeraCompanionPresentation>();
                    riderEntity.ApplyMountLocomotion(true,false);
                    if(entry.name=="Vehicle_Pegasus")presentation.GetComponent<Animator>().Play("FlyForward",0,0);
                }
                else
                {
                    var pet=UnityEngine.Object.Instantiate(prefab,current.transform,false);Metin2Units.ApplyCharacterScale(pet);Metin2ActorOrientation.Apply(pet.transform,entry.mob);
                    presentation=pet.GetComponentInChildren<TeraCompanionPresentation>();
                    var state=presentation.motions.FirstOrDefault(m=>m.source.Equals("run",StringComparison.OrdinalIgnoreCase))??presentation.motions[0];
                    presentation.GetComponent<Animator>().Play(state.state,0,0);
                }
                deadline=EditorApplication.timeSinceStartup+3.5;return;
            }
            if(EditorApplication.timeSinceStartup<deadline)return;
            if(riderEntity)
            {
                var seat=riderEntity.MountInstance.GetComponentsInChildren<Transform>().First(t=>t.name.Equals("saddle",StringComparison.OrdinalIgnoreCase));
                var pivot=riderVisual.GetComponentsInChildren<Transform>().First(t=>t.name.Equals("Bip01",StringComparison.OrdinalIgnoreCase));
                foreach(float yaw in new[]{0f,37f,180f,275f})
                {
                    current.transform.rotation=Quaternion.Euler(0,yaw,0);
                    typeof(Metin2NetworkEntity).GetMethod("UpdateMountedRiderFrame",Private).Invoke(riderEntity,null);
                    if(Vector3.Distance(seat.position,pivot.position)>.02f)throw new Exception("Rider detached from saddle "+entry.name);
                    if(Vector3.Dot(riderVisual.transform.up,Vector3.up)<.999f)throw new Exception("Rider tilted "+entry.name);
                    if(Mathf.Abs(riderVisual.transform.lossyScale.x-.01f)>.0001f)throw new Exception("Rider scale changed "+entry.name);
                    if(Vector3.Dot(riderVisual.transform.right,presentation.transform.forward)<.98f)throw new Exception("Rider facing mismatch "+entry.name);
                }
                current.transform.rotation=Quaternion.Euler(0,35,0);
                typeof(Metin2NetworkEntity).GetMethod("UpdateMountedRiderFrame",Private).Invoke(riderEntity,null);
            }
            int particles=current.GetComponentsInChildren<ParticleSystem>().Sum(p=>p.particleCount);
            int soundCount=current.GetComponentsInChildren<TeraCompanionPresentation>().Sum(p=>p.PlayedSounds);
            int systems=current.GetComponentsInChildren<ParticleSystem>().Length;
            if((entry.name=="Vehicle_BoneDragon_C"||entry.name=="Vehicle_Pegasus")&&(systems==0||particles==0))throw new Exception("No emitted production particles "+entry.name);
            if(entry.name=="Vehicle_Mechanic_A"||entry.name=="Kart_A")
            {
                var loops=current.GetComponentsInChildren<TeraCuePlayback>().Where(v=>v.looping&&!v.stopping).ToArray();
                if(loops.Length==0||loops.GroupBy(v=>v.cue).Any(g=>g.Count()>1))throw new Exception("Engine sound loop missing or duplicated "+entry.name);
            }
            if(entry.sounds.Any(s=>s.state.Equals(presentation.CurrentMotion,StringComparison.OrdinalIgnoreCase)&&s.time<3)&&soundCount==0)throw new Exception("No motion sound playback "+entry.name);
            Capture(entry.name);
            report.Add("PASS "+entry.name+" rider="+(riderEntity!=null)+" particles="+particles+" systems="+systems+" sounds="+soundCount);
            Debug.Log("TERA_PRODUCTION_PLAY "+report.Last());
            UnityEngine.Object.DestroyImmediate(current);current=null;riderEntity=null;riderVisual=null;
            if(catalog)UnityEngine.Object.DestroyImmediate(catalog);if(dataCatalog)UnityEngine.Object.DestroyImmediate(dataCatalog);catalog=null;dataCatalog=null;
            index++;
            if(index==Cases.Length)
            {
                if(error)throw new Exception("Runtime error logged during production play proof");
                report.Add("PASS production SetMount, four headings, upright rider, saddle pivot, warrior metric scale, body attachments, emitted effects and motion-linked audio.");
                File.WriteAllLines("Migration/TeraCompanions/play-qa.txt",report);End(0);
            }
        }
        catch(Exception e){report.Add("FAIL "+e);File.WriteAllLines("Migration/TeraCompanions/play-qa.txt",report);Debug.LogException(e);End(1);}
    }
    static void Capture(string name)
    {
        if(riderVisual)CompanionLabPose.PrepareInEditor(riderVisual);
        if(!CompanionLabPose.TryBounds(current,null,out var bounds))throw new Exception("No proof geometry");
        float distance=Mathf.Max(3,bounds.size.magnitude*1.2f);
        camera.transform.position=bounds.center+new Vector3(1,.65f,-1).normalized*distance;camera.transform.LookAt(bounds.center);
        var rt=new RenderTexture(1200,900,24);var old=RenderTexture.active;camera.targetTexture=rt;camera.Render();RenderTexture.active=rt;
        var tex=new Texture2D(1200,900,TextureFormat.RGB24,false);tex.ReadPixels(new Rect(0,0,1200,900),0,0);tex.Apply();
        string folder="Migration/TeraCompanions/Proof";Directory.CreateDirectory(folder);File.WriteAllBytes(folder+"/"+name+".png",tex.EncodeToPNG());
        camera.targetTexture=null;RenderTexture.active=old;UnityEngine.Object.DestroyImmediate(tex);UnityEngine.Object.DestroyImmediate(rt);
    }
    static void End(int result){SessionState.SetBool(Flag,false);EditorApplication.update-=Tick;EditorApplication.Exit(result);}
}
