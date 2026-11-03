using System;

using System.Collections;

using System.Collections.Generic;

using System.Globalization;

using System.Text.RegularExpressions;

using UnityEngine;

namespace TeraCompanionLab {

public class CompanionLabEffects : MonoBehaviour {

 [Serializable] public class Attributes {public string boneName,posX,posY,posZ,rotX,rotY,rotZ,scale,scaleX,scaleY,scaleZ,playRate,startTime,time;}

 [Serializable] public class Entry {public string context,condition,clip,prefab,resource;public Attributes attributes;}

 [Serializable] public class Definition {public Entry[] entries;}

 [Serializable] public class EmitterMeta {public string name,kind,alignment;} [Serializable] public class EffectMeta {public string key;public EmitterMeta[] emitters;} [Serializable] public class Metadata {public EffectMeta[] effects;} static Metadata metadata; readonly List<GameObject> active=new List<GameObject>();Definition data;Animation animationSource;string clip="";float previous=-.001f;bool enabledEffects=true;public int SpawnedSystems{get;private set;}int previewIndex;public int ActiveEmitters=>GetComponentsInChildren<ParticleSystem>().Length;public void PreviewNext(){if(data?.entries==null||data.entries.Length==0)return;Clear();var e=data.entries[previewIndex++%data.entries.Length];Spawn(e);Status="Külön effektteszt: "+e.resource;}public int EntryCount=>data?.entries?.Length??0;public string Status{get;private set;}="";

 static float Number(string value,float fallback=0)=>float.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out var result)?result:fallback;

 public void Configure(string key){var text=CompanionLabAssets.Load<TextAsset>("TeraFX/Models/"+key);data=text?JsonUtility.FromJson<Definition>(text.text):new Definition{entries=new Entry[0]};animationSource=GetComponent<Animation>();if(metadata==null){var meta=CompanionLabAssets.Load<TextAsset>("TeraFX/metadata");if(meta)metadata=JsonUtility.FromJson<Metadata>(meta.text);}Status=EntryCount+" eredeti effektbejegyzés · summon nélkül";}

 public static bool Matches(string condition,string name){string c=(condition??"").ToUpperInvariant(),n=(name??"").ToLowerInvariant();if(c==""||c=="NPC_ALIVE_ON")return !Regex.IsMatch(n,"death|die");if(c=="NPC_ALIVE_START")return true;if(c=="NPC_DEATH_START")return Regex.IsMatch(n,"death|die");if(c=="NPC_ANGRY_START")return Regex.IsMatch(n,"angry|rage|anger");if(c.EndsWith("WALKING_OFF"))return !Regex.IsMatch(n,"run|walk");if(Regex.IsMatch(c,"DEPART|TAKEOFF"))return Regex.IsMatch(n,"jumpstart|flyup");if(c.Contains("LANDING"))return Regex.IsMatch(n,"jumpendland|jumpend");if(c.Contains("BOOST"))return Regex.IsMatch(n,"flyfast|runfast");if(c.Contains("DESCENT"))return n.Contains("flydescent");if(Regex.IsMatch(c,"NORAML|NORMAL"))return n.StartsWith("fly")&&!Regex.IsMatch(n,"fast|descent");if(Regex.IsMatch(c,"MOVE|RUN|WALK"))return Regex.IsMatch(n,"run|walk");return false;}

 public void Play(string name){if(clip==name)return;clip=name;previous=-.001f;Clear();if(!enabledEffects||data?.entries==null)return;foreach(var e in data.entries)if(e.context=="attached"&&Matches(e.condition,name))StartCoroutine(SpawnAfter(e,Number(e.attributes?.startTime)*.001f));}

 IEnumerator SpawnAfter(Entry entry,float delay){if(delay>0)yield return new WaitForSeconds(delay);if(!enabledEffects||!this)yield break;Spawn(entry);}

 void Spawn(Entry e){if(e.context=="spawn"||string.IsNullOrEmpty(e.prefab))return;var prefab=CompanionLabAssets.Load<GameObject>(e.prefab);if(!prefab){Status="Hiányzó effekt prefab: "+e.resource;Debug.LogError(Status);return;}var a=e.attributes??new Attributes();Transform bone=transform;if(!string.IsNullOrEmpty(a.boneName)){foreach(var t in GetComponentsInChildren<Transform>())if(t.name==a.boneName){bone=t;break;}}var go=Instantiate(prefab,bone,false);go.name="SourceFX_"+e.resource;go.transform.localPosition=new Vector3(-Number(a.posY),Number(a.posZ),Number(a.posX))*.01f;go.transform.localRotation=Quaternion.Euler(-Number(a.rotY),-Number(a.rotZ),Number(a.rotX));float scale=Number(a.scale,1);go.transform.localScale=new Vector3(Number(a.scaleY,1),Number(a.scaleZ,1),Number(a.scaleX,1))*scale;var metaFx=metadata==null?null:Array.Find(metadata.effects,f=>f.key==e.prefab.Substring(e.prefab.LastIndexOf("/")+1));foreach(var ps in go.GetComponentsInChildren<ParticleSystem>()){var sourceEmitter=metaFx==null?null:Array.Find(metaFx.emitters,m=>m.name==ps.gameObject.name);if(sourceEmitter!=null&&sourceEmitter.kind=="sprite"&&sourceEmitter.alignment=="PSA_Velocity"&&Enum.TryParse<ParticleSystemRenderSpace>("Velocity",out var alignment))ps.GetComponent<ParticleSystemRenderer>().alignment=alignment;var main=ps.main;main.simulationSpeed=Mathf.Max(.01f,Number(a.playRate,1));ps.Play(false);}active.Add(go);SpawnedSystems++;if(e.context=="animation"){float lifespan=1;foreach(var ps in go.GetComponentsInChildren<ParticleSystem>()){var main=ps.main;main.loop=false;lifespan=Mathf.Max(lifespan,main.duration+main.startLifetime.constantMax+main.startDelay.constantMax);}Destroy(go,lifespan+1);}}

 void Clear(){StopAllCoroutines();foreach(var go in active)if(go)Destroy(go);active.Clear();}

 public void Toggle(){enabledEffects=!enabledEffects;string old=clip;clip="";Play(old);}

 void Update(){if(!enabledEffects||data?.entries==null||!animationSource||string.IsNullOrEmpty(clip))return;var state=animationSource[clip];if(state==null||state.length<=0)return;float now=Mathf.Repeat(state.time,state.length);foreach(var e in data.entries){if(e.context!="animation"||!string.Equals(e.clip,clip,StringComparison.OrdinalIgnoreCase))continue;float at=Number(e.attributes?.time,Number(e.attributes?.startTime))*.001f;if(now>=previous?(at>previous&&at<=now):(at>previous||at<=now))Spawn(e);}previous=now;}

 void OnDestroy(){Clear();}

}}
