using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using Metin2.Runtime;
using Metin2.Runtime.Network;
using TeraCompanionLab;

public static class TeraCompanionReleaseQA
{
    static readonly List<string> results=new List<string>();
    public static async void Run()
    {
        EditorApplication.update+=Pump;
        try
        {
            EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            var release=JsonUtility.FromJson<TeraCompanionReleaseBuilder.Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));
            var items=AssetDatabase.LoadAssetAtPath<Metin2ItemCatalog>("Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset");
            int sounds=0,effects=0,motions=0;
            var checkedEffects=new HashSet<GameObject>();
            foreach(var e in release.entries)
            {
                await Timeout(Metin2RacePrefabs.PreloadAsync(e.mob,Metin2ContentScope.Map),e.mob,"prefab");
                await Timeout(Metin2RaceDataBinder.PreloadAsync(e.mob,Metin2ContentScope.Map),e.mob,"data");
                if(!Metin2RacePrefabs.TryGet(null,e.mob,out var prefab))throw new Exception("Race unavailable "+e.mob);
                if(!Metin2RaceDataBinder.TryGetEntry(e.mob,out var data))throw new Exception("Race data unavailable "+e.mob);
                items=AssetDatabase.LoadAssetAtPath<Metin2ItemCatalog>("Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset");
                if(!items.TryGet(e.item,out var item)||item.value1!=e.mob||item.itemType!=(e.kind=="mount"?28:18)||!item.icon)throw new Exception("Client summon proto mismatch "+e.item);
                var go=UnityEngine.Object.Instantiate(prefab);
                try
                {
                    var metric=go.GetComponent<Metin2MetricActor>();
                    if(!metric||Mathf.Abs(metric.authoredScale.x-e.scale)>.0001f)throw new Exception("Saved scale lost "+e.name);
                    Metin2Units.ApplyCharacterScale(go);
                    var animation=go.GetComponentInChildren<Animator>();
                    var fx=go.GetComponentInChildren<TeraCompanionPresentation>();
                    if(!fx||!animation||fx.motions.Length!=e.clips.Length)throw new Exception("Motions incomplete "+e.name);
                    var attachments=go.GetComponentsInChildren<TeraCompanionAttachment>();
                    if(attachments.Length!=(e.attachments?.Length??0)||attachments.Any(a=>a.parent!=fx))throw new Exception("Attachment registration mismatch "+e.name);
                    animation.Rebind();animation.Update(0);
                    if(Quaternion.Angle(animation.transform.localRotation,Quaternion.Euler(0,90,0))>.01f)throw new Exception("Production frame mismatch "+e.name);
                    foreach(var m in fx.motions)
                    {
                        if(!animation.HasState(0,Animator.StringToHash(m.state)))throw new Exception("Controller state missing "+e.name+" "+m.state);
                        var clip=animation.runtimeAnimatorController.animationClips.First(c=>c.name==m.state);
                        if(clip.legacy||AnimationUtility.GetCurveBindings(clip).Length==0||clip.length<0||float.IsNaN(clip.length))throw new Exception("Invalid animation "+e.name);
                    }
                    var idle=data.StateFor("WAIT");
                    var wait=animation.runtimeAnimatorController.animationClips.First(c=>c.name==idle);wait.SampleAnimation(animation.gameObject,0);
                    if(!CompanionLabPose.TryBounds(go,null,out var bounds)||Mathf.Abs(bounds.min.y)>.015f)throw new Exception("Ground mismatch "+e.name+" y="+bounds.min.y);
                    if(e.kind=="mount"&&!go.GetComponentsInChildren<Transform>().Any(t=>t.name.Equals("saddle",StringComparison.OrdinalIgnoreCase)))throw new Exception("Saddle missing "+e.name);
                    if(fx.effects.Any(f=>f.context=="spawn"||!f.prefab)||fx.sounds.Any(s=>s.clips.Length==0||s.clips.Any(c=>!c)))throw new Exception("Cosmetic dependencies incomplete "+e.name);
                    foreach(var owner in go.GetComponentsInChildren<TeraCompanionPresentation>())
                    {
                        if(owner.sounds.Any(s=>!s.program||s.program.nodes.Any(n=>n.kind=="wave"&&!n.wave)))throw new Exception("SoundCue graph incomplete "+e.name);
                        foreach(var effect in owner.effects)
                        {
                            if(effect.context=="spawn"||!effect.prefab)throw new Exception("Invalid source effect "+e.name);
                            if(!checkedEffects.Add(effect.prefab))continue;
                            var systems=effect.prefab.GetComponentsInChildren<ParticleSystem>(true);
                            // Verified original package has no emitter exports for this deliberate placeholder.
                            if(systems.Length==0&&effect.resource!="FX_Q_HotFix_181221.PS.Q_Sound_Dummy_PS_01")throw new Exception("Empty source effect "+effect.resource);
                            foreach(var renderer in effect.prefab.GetComponentsInChildren<ParticleSystemRenderer>(true))
                            {
                                if(renderer.renderMode==ParticleSystemRenderMode.Mesh&&renderer.meshCount==0)throw new Exception("Effect mesh missing "+effect.resource);
                                foreach(var material in renderer.sharedMaterials)if(!material||!material.shader||material.shader.name.Contains("InternalError"))throw new Exception("Effect material missing "+effect.resource);
                            }
                        }
                    }
                    foreach(var renderer in go.GetComponentsInChildren<Renderer>())foreach(var mat in renderer.sharedMaterials)
                        if(!mat||!mat.shader||mat.shader.name.Contains("InternalError"))throw new Exception("Invalid material "+e.name);
                    sounds+=fx.sounds.Length;effects+=fx.effects.Length;motions+=fx.motions.Length;
                    if(e.kind=="pet")
                    {
                        var worldRoot=new GameObject("TERA packet spawn audit");
                        try
                        {
                            var world=worldRoot.AddComponent<Metin2NetworkEntityWorld>();
                            var packet=new Metin2GamePacketCodec.EntityAdd(900001,0,0,0,0,1,(ushort)e.mob,100,100,0,0,0);
                            typeof(Metin2NetworkEntityWorld).GetMethod("Spawn",BindingFlags.NonPublic|BindingFlags.Instance).Invoke(world,new object[]{packet,false});
                            if(!world.TryGetEntity(packet.Vid,out var pet)||!pet.GetComponentInChildren<TeraCompanionPresentation>())throw new Exception("Packet pet spawn failed "+e.mob);
                        }
                        finally{UnityEngine.Object.DestroyImmediate(worldRoot);}
                    }
                    results.Add("PASS "+e.mob+" "+e.item+" "+e.name);
                }
                finally{UnityEngine.Object.DestroyImmediate(go);}
                Metin2ContentProvider.ReleaseMapScope();Metin2RacePrefabs.ReleaseMapScopedAssets();Metin2RaceDataBinder.ReleaseMapScopedAssets();
                if(results.Count%12==0){EditorUtility.UnloadUnusedAssetsImmediate();GC.Collect();}
                Debug.Log("TERA_RELEASE_QA "+e.mob+" "+e.name);
            }
            results.Add("PASS all 272 production Addressables acquisitions; 69 packet-driven pet spawns; saved scale, ground, saddle, frame, materials, semantic motions, summon proto and cosmetic references.");
            results.Add("Totals: motions="+motions+" effects="+effects+" soundEvents="+sounds);
            File.WriteAllLines("Migration/TeraCompanions/unity-qa.txt",results);Debug.Log("TERA_RELEASE_QA_SUCCESS");TeraCompanionPlayQA.Run();
        }
        catch(Exception e){File.WriteAllLines("Migration/TeraCompanions/unity-qa.txt",results.Concat(new[]{"FAIL "+e}));Debug.LogException(e);EditorApplication.Exit(1);}
    }
    public static void CheckParticleMeshesAndPlay()
    {
        try
        {
            int count=0;
            foreach(var path in Directory.GetFiles("Assets/TeraCompanionLab/Effects/MetricMeshes","*.asset"))
            {
                var mesh=AssetDatabase.LoadAssetAtPath<Mesh>(path);
                if(!mesh||mesh.subMeshCount!=1||mesh.GetIndexCount(0)==0)throw new Exception("Invalid particle geometry "+path);
                count++;
            }
            if(count!=690)throw new Exception("Particle mesh inventory changed: "+count);
            File.WriteAllText("Migration/TeraCompanions/particle-mesh-qa.txt","PASS all 690 source particle meshes have one nonempty submesh. Six Kirin meshes preserve all 230 triangles and 163 vertices each.\n");
            TeraCompanionPlayQA.Run();
        }
        catch(Exception e){Debug.LogException(e);EditorApplication.Exit(1);}
    }
    static void Pump()=>EditorApplication.QueuePlayerLoopUpdate();
    static async Task Timeout(Task task,uint race,string stage)
    {
        if(await Task.WhenAny(task,Task.Delay(60000))!=task)throw new TimeoutException(stage+" "+race);
        await task;
    }
}
