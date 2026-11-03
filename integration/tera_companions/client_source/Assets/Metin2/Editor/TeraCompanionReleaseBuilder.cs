using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEditor.Animations;
using UnityEditor.AddressableAssets;
using UnityEditor.AddressableAssets.Settings;
using UnityEngine;
using Metin2.Runtime;
using Metin2.Runtime.Network;
using TeraCompanionLab;

public static class TeraCompanionReleaseBuilder
{
    const string Root = "Assets/Metin2/Generated/TeraCompanions";
    const string Source = Root + "/Source";
    [Serializable] public class Release { public Entry[] entries,parts; }
    [Serializable] public class Attachment { public string key,bone;public float[] rotation,scale;public float distance,height,mountAngle; }
    [Serializable] public class Entry
    {
        public string key,name,kind,saddle; public uint item,mob; public float scale;
        public string[] clips,clipResources; public Sound[] sounds; public Attachment[] attachments;
    }
    [Serializable] public class Sound { public string state,cue; public float time,volume; public string[] files; }
    [Serializable] public class CueCatalog { public Cue[] programs; }
    [Serializable] public class Cue { public string cue,key;public int first,maxConcurrent;public float volume,pitch;public CueNode[] nodes; }
    [Serializable] public class CueNode
    {
        public string kind,algorithm,file;public int[] children;public float[] weights,volumes;
        public float volumeMin,volumeMax,pitchMin,pitchMax,radiusMin,radiusMax,lpfMin,lpfMax;public bool lowpass;
    }
    static readonly Dictionary<string, AnimationClip> clipCache = new Dictionary<string, AnimationClip>();
    static readonly HashSet<string> preparedClips = new HashSet<string>();
    static readonly List<string> report = new List<string>();
    public static void Run()
    {
        var mode=EditorSettings.serializationMode;int result=0;
        try { Debug.Log("TERA_RELEASE_START serialization="+mode);Build(); }
        catch(Exception e) { Debug.LogException(e); result=1; }
        finally { EditorSettings.serializationMode=mode;AssetDatabase.SaveAssets(); }
        EditorApplication.Exit(result);
    }
    static void Folder(string path)
    {
        path=path.Replace('\\','/');if(AssetDatabase.IsValidFolder(path))return;
        string parent=Path.GetDirectoryName(path).Replace('\\','/');Folder(parent);
        AssetDatabase.CreateFolder(parent,Path.GetFileName(path));
    }
    static void Move(string from,string to)
    {
        if(!AssetDatabase.IsValidFolder(from)) return;
        Folder(Path.GetDirectoryName(to));
        string error=AssetDatabase.MoveAsset(from,to);
        if(error!="")throw new InvalidOperationException(error);
    }
    static float F(string value,float fallback=0)=>float.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out float result)?result:fallback;
    static string Pick(Entry e,string pattern,string fallback=null)=>e.clips.FirstOrDefault(c=>Regex.IsMatch(c,pattern,RegexOptions.IgnoreCase))??fallback;
    public static void RefreshCueDefaults()
    {
        foreach(var c in JsonUtility.FromJson<CueCatalog>(File.ReadAllText("Migration/TeraCompanions/audio-cues.json")).programs)
        {
            var asset=AssetDatabase.LoadAssetAtPath<TeraSoundCue>(Root+"/SoundCues/"+c.key+".asset");
            if(!asset)throw new InvalidOperationException("Cue asset missing "+c.cue);
            asset.volume=c.volume;asset.pitch=c.pitch;asset.maxConcurrent=c.maxConcurrent;EditorUtility.SetDirty(asset);
        }
        AssetDatabase.SaveAssets();Debug.Log("TERA_CUE_DEFAULTS_REFRESHED");
    }
    static void Build()
    {
        var release=JsonUtility.FromJson<Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));
        Debug.Log("TERA_RELEASE_MANIFEST "+release.entries.Length);
        if(release.entries.Length!=272)throw new InvalidOperationException("Unexpected release entry count");
        Move("Assets/TeraCompanionLab/Editor/Resources/Native",Source+"/Native");
        Move("Assets/TeraCompanionLab/Editor/Resources/TeraFX",Source+"/TeraFX");
        Folder(Root+"/Actors");Folder(Root+"/Clips");Folder(Root+"/Controllers");
        Folder("Assets/Metin2/AddressableContent/Races/Prefabs");Folder("Assets/Metin2/AddressableContent/Races/Data");
        Debug.Log("TERA_RELEASE_FOLDERS_READY");
        var socketCatalog=JsonUtility.FromJson<CompanionLab.SocketCatalog>(File.ReadAllText("Assets/TeraCompanionLab/Editor/Resources/sockets.json"));
        var metadata=JsonUtility.FromJson<CompanionLabEffects.Metadata>(File.ReadAllText(Source+"/TeraFX/metadata.json"));
        var prefabIndex=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabIndex>("Assets/Metin2/Resources/Metin2RacePrefabIndex.asset");
        var dataIndex=AssetDatabase.LoadAssetAtPath<Metin2RaceDataIndex>("Assets/Metin2/Resources/Metin2RaceDataIndex.asset");
        Debug.Log("TERA_RELEASE_METADATA_READY");
        var settings=AddressableAssetSettingsDefaultObject.Settings;
        Debug.Log("TERA_RELEASE_SETTINGS_READY");
        var group=settings.FindGroup("Metin2-Races")??throw new InvalidOperationException("Race Addressables group missing");
        Folder(Root+"/SoundCues");var cues=new Dictionary<string,TeraSoundCue>();
        AssetDatabase.StartAssetEditing();try{
        foreach(var c in JsonUtility.FromJson<CueCatalog>(File.ReadAllText("Migration/TeraCompanions/audio-cues.json")).programs)
        {
            string path=Root+"/SoundCues/"+c.key+".asset";
            var asset=AssetDatabase.LoadAssetAtPath<TeraSoundCue>(path);
            if(!asset){asset=ScriptableObject.CreateInstance<TeraSoundCue>();AssetDatabase.CreateAsset(asset,path);}
            asset.source=c.cue;asset.first=c.first;asset.maxConcurrent=c.maxConcurrent;asset.volume=c.volume;asset.pitch=c.pitch;
            asset.nodes=c.nodes.Select(n=>new TeraSoundCue.Node{kind=n.kind,algorithm=n.algorithm,children=n.children,weights=n.weights,volumes=n.volumes,
                volumeMin=n.volumeMin,volumeMax=n.volumeMax,pitchMin=n.pitchMin,pitchMax=n.pitchMax,radiusMin=n.radiusMin,radiusMax=n.radiusMax,lpfMin=n.lpfMin,lpfMax=n.lpfMax,lowpass=n.lowpass,
                wave=string.IsNullOrEmpty(n.file)?null:AssetDatabase.LoadAssetAtPath<AudioClip>(Root+"/Audio/"+Path.GetFileName(n.file))}).ToArray();
            if(asset.nodes.Any(n=>n.kind=="wave"&&!n.wave))throw new InvalidOperationException("Cue waveform missing "+c.cue);
            EditorUtility.SetDirty(asset);cues[c.cue]=asset;
        }
        }finally{AssetDatabase.StopAssetEditing();}
        AssetDatabase.SaveAssets();Debug.Log("TERA_CUE_PROGRAMS "+cues.Count);
        foreach(var e in (release.parts??Array.Empty<Entry>()).Concat(release.entries))
        {
            Debug.Log("TERA_RELEASE_BEGIN_MODEL "+e.key+" "+e.name);
            string sourcePath=Source+"/Native/"+e.key+".prefab";
            var original=AssetDatabase.LoadAssetAtPath<GameObject>(sourcePath);
            if(!original)throw new InvalidOperationException("Missing model "+sourcePath);
            var root=new GameObject("TERA_"+e.mob+"_"+e.name);
            try
            {
                var model=UnityEngine.Object.Instantiate(original,root.transform,false);model.name="Model";
                // Lab models face +Z. Production native actors face +X beneath the rider's -90 wrapper.
                model.transform.localRotation=Quaternion.Euler(0,90,0);
                var legacy=model.GetComponent<Animation>();if(legacy)UnityEngine.Object.DestroyImmediate(legacy);
                var metric=root.AddComponent<Metin2MetricActor>();metric.authoredScale=Vector3.one*e.scale;
                root.AddComponent<Metin2NativeActor>().sourceModel="TERA/"+e.key;
                var socketModel=Array.Find(socketCatalog.models,s=>s.key==e.key);
                if(socketModel!=null)foreach(var s in socketModel.sockets)
                {
                    var bone=Array.Find(model.GetComponentsInChildren<Transform>(),t=>t.name==s.bone);if(!bone)throw new InvalidOperationException("Socket bone missing "+e.name+" "+s.bone);
                    var t=new GameObject(s.name).transform;t.SetParent(bone,false);
                    t.localPosition=new Vector3(-s.position[1],s.position[2],s.position[0])*.01f;
                    t.localScale=new Vector3(s.scale[1],s.scale[2],s.scale[0]);
                }
                var seat=model.GetComponentsInChildren<Transform>().FirstOrDefault(t=>t.name.Equals(e.saddle??"",StringComparison.OrdinalIgnoreCase));
                if(e.kind=="mount"&&seat==null)throw new InvalidOperationException("No saddle for "+e.name);
                if(seat!=null&&!seat.name.Equals("saddle",StringComparison.OrdinalIgnoreCase))
                { var alias=new GameObject("saddle").transform;alias.SetParent(seat,false); }
                string controllerPath=Root+"/Controllers/"+e.key+".controller";
                var controller=AssetDatabase.LoadAssetAtPath<AnimatorController>(controllerPath)??AnimatorController.CreateAnimatorControllerAtPath(controllerPath);
                var sm=controller.layers[0].stateMachine;
                foreach(var state in sm.states)sm.RemoveState(state.state);
                var animationClips=new Dictionary<string,AnimationClip>();
                for(int i=0;i<e.clips.Length;i++)
                {
                    string path=Source+"/"+e.clipResources[i]+".anim";
                    if(!clipCache.TryGetValue(path,out var clip))
                    {
                        var src=AssetDatabase.LoadAssetAtPath<AnimationClip>(path);if(!src)throw new InvalidOperationException("Missing clip "+path);
                        string target=Root+"/Clips/"+Path.GetFileName(path);
                        clip=AssetDatabase.LoadAssetAtPath<AnimationClip>(target);
                        if(!clip){clip=UnityEngine.Object.Instantiate(src);clip.legacy=false;AssetDatabase.CreateAsset(clip,target);}
                        else if(!preparedClips.Contains(path))EditorUtility.CopySerialized(src,clip);
                        preparedClips.Add(path);
                        clipCache[path]=clip;
                    }
                    // Several models share the same source clip but semantic state names are authoritative.
                    clip.name=e.clips[i];clip.legacy=false;
                    // Logical movement belongs to Auriga's actor controller; retain vertical jump motion.
                    foreach(var binding in AnimationUtility.GetCurveBindings(clip))
                    {
                        string bone=Path.GetFileName(binding.path);
                        if(!Regex.IsMatch(bone,"^(Rig|Dummy_root)$",RegexOptions.IgnoreCase))continue;
                        if(!binding.propertyName.EndsWith("localPosition.x",StringComparison.OrdinalIgnoreCase)&&!binding.propertyName.EndsWith("localPosition.z",StringComparison.OrdinalIgnoreCase))continue;
                        var curve=AnimationUtility.GetEditorCurve(clip,binding);
                        if(curve!=null&&curve.length>0)AnimationUtility.SetEditorCurve(clip,binding,clip.length>0?AnimationCurve.Constant(0,clip.length,curve.Evaluate(0)):new AnimationCurve(new Keyframe(0,curve.Evaluate(0))));
                    }
                    var cs=AnimationUtility.GetAnimationClipSettings(clip);
                    cs.loopTime=!Regex.IsMatch(e.clips[i],"death|die|jumpstart|jumpend|attack|damage|hit|spawn",RegexOptions.IgnoreCase);
                    AnimationUtility.SetAnimationClipSettings(clip,cs);EditorUtility.SetDirty(clip);
                    var state=sm.AddState(e.clips[i]);state.motion=clip;animationClips[e.clips[i]]=clip;
                }
                string idle=Pick(e,"^(unarmedwait|idle0?1|idle|wait)$",Pick(e,"wait|idle",e.clips[0]));
                string run=Pick(e,"^run$",Pick(e,"run|forward|walk",idle));
                string walk=Pick(e,"^walk$",run),dead=Pick(e,"death|die|dead",idle),attack=Pick(e,"attack|atk",idle),damage=Pick(e,"damage|hit",idle);
                sm.defaultState=sm.states.First(s=>s.state.name==idle).state;
                var animator=model.AddComponent<Animator>();animator.runtimeAnimatorController=controller;animator.cullingMode=AnimatorCullingMode.AlwaysAnimate;animator.applyRootMotion=false;
                var presentation=model.AddComponent<TeraCompanionPresentation>();presentation.modelKey=e.key;
                presentation.motions=e.clips.Select(c=>new TeraCompanionPresentation.Motion{state=c,source=c,duration=animationClips[c].length}).ToArray();
                presentation.sounds=e.sounds.Select(s=>new TeraCompanionPresentation.Sound{state=s.state,cue=s.cue,time=s.time,volume=s.volume,program=cues[s.cue],clips=s.files.Select(f=>AssetDatabase.LoadAssetAtPath<AudioClip>(Root+"/Audio/"+Path.GetFileName(f))).ToArray()}).ToArray();
                if(presentation.sounds.Any(s=>s.clips.Any(c=>!c)))throw new InvalidOperationException("Missing waveform "+e.name);
                string fxPath=Source+"/TeraFX/Models/"+e.key+".json";
                var fx=File.Exists(fxPath)?JsonUtility.FromJson<CompanionLabEffects.Definition>(File.ReadAllText(fxPath)):new CompanionLabEffects.Definition{entries=new CompanionLabEffects.Entry[0]};
                presentation.effects=fx.entries.Where(f=>f.context!="spawn"&&!string.IsNullOrEmpty(f.prefab)).Select(f=>
                {
                    var a=f.attributes??new CompanionLabEffects.Attributes();
                    string key=Path.GetFileName(f.prefab);
                    var meta=Array.Find(metadata.effects,m=>m.key==key);
                    return new TeraCompanionPresentation.Effect{
                        context=f.context,condition=f.condition,clip=f.clip,resource=f.resource,bone=a.boneName,
                        prefab=AssetDatabase.LoadAssetAtPath<GameObject>(Source+"/"+f.prefab+".prefab"),
                        position=new Vector3(-F(a.posY),F(a.posZ),F(a.posX))*.01f,
                        rotation=new Vector3(-F(a.rotY),-F(a.rotZ),F(a.rotX)),
                        scale=new Vector3(F(a.scaleY,1),F(a.scaleZ,1),F(a.scaleX,1))*F(a.scale,1),
                        delay=F(a.startTime)*.001f,time=F(a.time,F(a.startTime))*.001f,rate=F(a.playRate,1),
                        velocityEmitters=meta?.emitters?.Where(m=>m.kind=="sprite"&&m.alignment=="PSA_Velocity").Select(m=>m.name).ToArray()??Array.Empty<string>()};
                }).ToArray();
                if(presentation.effects.Any(f=>!f.prefab))throw new InvalidOperationException("Missing effect "+e.name);
                foreach(var attachment in e.attachments??Array.Empty<Attachment>())
                {
                    var anchor=model.GetComponentsInChildren<Transform>().FirstOrDefault(t=>t.name.Equals(attachment.bone,StringComparison.OrdinalIgnoreCase));
                    if(!anchor)throw new InvalidOperationException("Attachment bone missing "+e.name+" "+attachment.bone);
                    var part=AssetDatabase.LoadAssetAtPath<GameObject>(Root+"/Actors/"+attachment.key+".prefab");
                    if(!part)throw new InvalidOperationException("Attachment model missing "+attachment.key);
                    var attached=UnityEngine.Object.Instantiate(part.transform.Find("Model").gameObject,anchor,false);
                    attached.name="Attachment_"+attachment.key;
                    attached.transform.localPosition=new Vector3(0,attachment.height,attachment.distance);
                    var rot=attachment.rotation??new float[3];attached.transform.localRotation=Quaternion.Euler(-rot[1],-rot[2],rot[0]+attachment.mountAngle);
                    var sc=attachment.scale??new[]{1f,1f,1f};attached.transform.localScale=new Vector3(sc[1],sc[2],sc[0]);
                    attached.AddComponent<TeraCompanionAttachment>().parent=presentation;
                    var ac=attached.GetComponent<Animator>().runtimeAnimatorController.animationClips;
                    var initial=ac.FirstOrDefault(c=>c.name.Equals(idle,StringComparison.OrdinalIgnoreCase))??ac.First();initial.SampleAnimation(attached,0);
                }
                animationClips[idle].SampleAnimation(model,0);
                if(!CompanionLabPose.TryBounds(model,null,out var bounds))throw new InvalidOperationException("No visible geometry "+e.name);
                model.transform.localPosition-=Vector3.up*bounds.min.y;
                root.transform.localScale=metric.authoredScale;
                string prefabPath=Root+"/Actors/"+e.key+".prefab";
                var prefab=PrefabUtility.SaveAsPrefabAsset(root,prefabPath);
                EditorUtility.SetDirty(controller);
                if(e.mob==0){AssetDatabase.SaveAssets();Debug.Log("TERA_RELEASE_PART "+e.name);continue;}
                var states=new Dictionary<string,string>{{"WAIT",idle},{"WALK",walk},{"RUN",run},{"NORMAL_ATTACK",attack},{"DAMAGE",damage},{"DEAD",dead}};
                var data=new Metin2RaceDataCatalog.Entry{race=e.mob,descriptor=(e.kind=="pet"?"npc_pet/":"tera_mount/")+e.key,motions=states.Select(p=>new Metin2RaceDataCatalog.MotionState{type=p.Key,state=p.Value,percent=100}).ToArray()};
                string pp="Assets/Metin2/AddressableContent/Races/Prefabs/"+e.mob+".asset",dp="Assets/Metin2/AddressableContent/Races/Data/"+e.mob+".asset";
                var pa=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabAsset>(pp);if(!pa){pa=ScriptableObject.CreateInstance<Metin2RacePrefabAsset>();AssetDatabase.CreateAsset(pa,pp);}pa.race=e.mob;pa.folder="tera/"+e.key;pa.prefab=prefab;EditorUtility.SetDirty(pa);
                var da=AssetDatabase.LoadAssetAtPath<Metin2RaceDataAsset>(dp);if(!da){da=ScriptableObject.CreateInstance<Metin2RaceDataAsset>();AssetDatabase.CreateAsset(da,dp);}da.race=e.mob;da.entry=data;EditorUtility.SetDirty(da);
                foreach(var pair in new[]{new[]{pp,Metin2RacePrefabAsset.ResourcePathFor(e.mob)},new[]{dp,Metin2RaceDataAsset.ResourcePathFor(e.mob)}})
                {
                    var registration=settings.CreateOrMoveEntry(AssetDatabase.AssetPathToGUID(pair[0]),group);registration.address=pair[1];
                    foreach(var old in registration.labels.Where(l=>l.StartsWith("race-family-")||l.StartsWith("race-data-")).ToArray())registration.SetLabel(old,false);
                    registration.SetLabel("race-family-"+AssetDatabase.AssetPathToGUID(prefabPath),true,true);
                }
                var deps=AssetDatabase.GetDependencies(prefabPath,true);
                if(deps.Any(p=>p.Contains("/Editor/")))throw new InvalidOperationException("Editor-only dependency "+e.name+": "+string.Join(",",deps.Where(p=>p.Contains("/Editor/"))));
                report.Add(e.mob+"\t"+e.item+"\t"+e.name+"\t"+e.kind+"\t"+e.scale.ToString(CultureInfo.InvariantCulture)+"\t"+e.clips.Length+"\t"+presentation.effects.Length+"\t"+presentation.sounds.Length);
                AssetDatabase.SaveAssets();
                Debug.Log("TERA_RELEASE_ACTOR "+e.mob+" "+e.name);
            }
            finally{UnityEngine.Object.DestroyImmediate(root);clipCache.Clear();EditorUtility.UnloadUnusedAssetsImmediate();GC.Collect();}
        }
        FinalizeMetadata(release);
    }
    public static void FinalizeFromCommandLine()
    {
        try
        {
            var release=JsonUtility.FromJson<Release>(File.ReadAllText("Migration/TeraCompanions/release.json"));
            RefreshCueDefaults();FinalizeMetadata(release);EditorApplication.Exit(0);
        }
        catch(Exception e){Debug.LogException(e);EditorApplication.Exit(1);}
    }
    static void FinalizeMetadata(Release release)
    {
        // Per-model memory cleanup unloads assets referenced only by editor locals; reacquire publication roots.
        var prefabIndex=AssetDatabase.LoadAssetAtPath<Metin2RacePrefabIndex>("Assets/Metin2/Resources/Metin2RacePrefabIndex.asset");
        var dataIndex=AssetDatabase.LoadAssetAtPath<Metin2RaceDataIndex>("Assets/Metin2/Resources/Metin2RaceDataIndex.asset");
        var settings=AddressableAssetSettingsDefaultObject.Settings;var group=settings.FindGroup("Metin2-Races");
        if(!prefabIndex||!dataIndex||!settings||!group||prefabIndex.races==null||dataIndex.races==null)throw new InvalidOperationException("Publication root missing");
        foreach(var e in release.entries)
        {
            string actor=Root+"/Actors/"+e.key+".prefab";
            if(!File.Exists(actor))throw new InvalidOperationException("Actor build incomplete "+e.name);
            foreach(var pair in new[]{new[]{"Assets/Metin2/AddressableContent/Races/Prefabs/"+e.mob+".asset",Metin2RacePrefabAsset.ResourcePathFor(e.mob)},new[]{"Assets/Metin2/AddressableContent/Races/Data/"+e.mob+".asset",Metin2RaceDataAsset.ResourcePathFor(e.mob)}})
            {
                if(!File.Exists(pair[0]))throw new InvalidOperationException("Race build incomplete "+e.name);
                var registration=settings.CreateOrMoveEntry(AssetDatabase.AssetPathToGUID(pair[0]),group);registration.address=pair[1];
                registration.SetLabel("race-family-"+AssetDatabase.AssetPathToGUID(actor),true,true);
            }
        }
        prefabIndex.races=prefabIndex.races.Concat(release.entries.Select(e=>e.mob)).Distinct().OrderBy(x=>x).ToArray();
        dataIndex.races=dataIndex.races.Concat(release.entries.Select(e=>e.mob)).Distinct().OrderBy(x=>x).ToArray();
        EditorUtility.SetDirty(prefabIndex);EditorUtility.SetDirty(dataIndex);EditorUtility.SetDirty(settings);EditorUtility.SetDirty(group);
        var items=AssetDatabase.LoadAssetAtPath<Metin2ItemCatalog>("Assets/Metin2/GeneratedData/Metin2ItemCatalog.asset");
        if(!items)throw new InvalidOperationException("Item catalog missing");
        var itemEntries=items.entries.ToDictionary(x=>x.vnum);
        foreach(var e in release.entries)
        {
            var item=itemEntries[e.kind=="mount"?611500u:48321u];
            if(!item.icon){var fallback=itemEntries[14590u];item.icon=fallback.icon;item.iconPath=fallback.iconPath;}
            item.vnum=e.item;item.name="TERA "+e.name;item.description=e.kind=="mount"?"TERA mount summon seal":"TERA pet summon seal";
            item.itemType=e.kind=="mount"?28:18;item.subType=e.kind=="mount"?2:0;item.size=1;item.itemFlag=8192;item.antiFlag=0;item.wearFlag=0;
            item.limitType0=e.kind=="mount"?10:0;item.limitValue0=e.kind=="mount"?1:0;item.limitType1=item.limitValue1=0;
            item.applyType0=item.applyType1=item.applyType2=item.applyType3=item.applyType4=0;
            item.applyValue0=item.applyValue1=item.applyValue2=item.applyValue3=item.applyValue4=0;
            item.rarity=item.extraApplyType0=item.extraApplyValue0=item.extraApplyType1=item.extraApplyValue1=0;
            item.value0=item.value2=item.value3=item.value4=item.value5=0;item.value1=(int)e.mob;
            item.buyItemPrice=item.sellItemPrice=0;itemEntries[e.item]=item;
        }
        items.entries=itemEntries.Values.OrderBy(x=>x.vnum).ToArray();EditorUtility.SetDirty(items);
        AssetDatabase.SaveAssets();
        if(report.Count!=release.entries.Length)
        {
            report.Clear();foreach(var e in release.entries)
            {
                string path=Source+"/TeraFX/Models/"+e.key+".json";
                int count=File.Exists(path)?JsonUtility.FromJson<CompanionLabEffects.Definition>(File.ReadAllText(path)).entries.Count(f=>f.context!="spawn"&&!string.IsNullOrEmpty(f.prefab)):0;
                report.Add(e.mob+"\t"+e.item+"\t"+e.name+"\t"+e.kind+"\t"+e.scale.ToString(CultureInfo.InvariantCulture)+"\t"+e.clips.Length+"\t"+count+"\t"+e.sounds.Length);
            }
        }
        File.WriteAllLines("Migration/TeraCompanions/unity-release-report.tsv",new[]{"mob\titem\tmodel\tkind\tscale\tmotions\teffects\tsoundEvents"}.Concat(report));
        Debug.Log("TERA_RELEASE_BUILD_SUCCESS actors="+release.entries.Length);
    }
}
