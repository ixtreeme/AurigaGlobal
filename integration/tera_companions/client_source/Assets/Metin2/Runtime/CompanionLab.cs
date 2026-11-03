using System;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEngine;
using Metin2.Runtime.Network;
namespace TeraCompanionLab
{
 public class CompanionLab : MonoBehaviour
 {
  [Serializable] public class Entry { public string name,kind,saddle,prefabResource; public string[] clipResources,clipNames; public GameObject prefab; public AnimationClip[] clips; public float scale=1; }
  [Serializable] public class SocketData { public string name,bone; public float[] position,scale; }
  [Serializable] public class SocketModel { public string key; public SocketData[] sockets; }
  [Serializable] public class SocketCatalog { public SocketModel[] models; }
  static SocketCatalog sockets;
  public Entry[] entries; public GameObject warriorPrefab; public Camera view;
  GameObject actor,rider,dummy; Transform seat; Vector3 riderScale,sourceScale;
  float sizeFactor=1,cameraZoom=.45f,attackUntil; int selected,hits,listCategory; bool mounted,moving;
  Vector2 scroll; Animation anim; Metin2CharacterMotionSet motion; CompanionLabEffects fx;
  string settingStatus="",currentMountClip=""; const float RiderYaw=-90f;
  void Start()
  {
   rider=Instantiate(warriorPrefab);Metin2.Runtime.Metin2Units.ApplyCharacterScale(rider);
   riderScale=rider.transform.localScale;motion=rider.GetComponent<Metin2CharacterMotionSet>();int initial=Array.FindIndex(entries,e=>e.name=="Vehicle_BoneDragon_C");Select(initial>=0?initial:0);
   dummy=GameObject.CreatePrimitive(PrimitiveType.Capsule);dummy.name="Training target";dummy.transform.position=new Vector3(0,1,6);
  }
  void Select(int index)
  {
   if(entries==null||entries.Length==0)return;Dismount();if(actor){actor.SetActive(false);Destroy(actor);}
   selected=(index+entries.Length)%entries.Length;var e=entries[selected];listCategory=e.kind=="mount"?0:e.kind=="pet"?1:2;
   var prefab=e.prefab?e.prefab:CompanionLabAssets.Load<GameObject>(e.prefabResource);
   if(!prefab)throw new InvalidOperationException("Missing test model: "+e.name);
   actor=Instantiate(prefab);actor.transform.localScale*=e.scale;sourceScale=actor.transform.localScale;sizeFactor=1;
   try {var saved=CompanionLabSettings.Get(e);if(saved!=null)sizeFactor=saved.sizeMultiplier;settingStatus=saved!=null?"Mentett méret betöltve":"Nincs mentett méret";}
   catch(Exception error){settingStatus="Betöltési hiba: "+error.Message;}
   actor.transform.localScale=sourceScale*sizeFactor;moving=false;
   anim=actor.GetComponent<Animation>();if(!anim)anim=actor.AddComponent<Animation>();
   var clips=e.clips!=null&&e.clips.Length>0?e.clips:Array.ConvertAll(e.clipResources,p=>CompanionLabAssets.Load<AnimationClip>(p));
   for(int i=0;i<clips.Length;i++)if(clips[i])anim.AddClip(clips[i],e.clipNames!=null&&i<e.clipNames.Length?e.clipNames[i]:clips[i].name);
   anim.cullingType=AnimationCullingType.AlwaysAnimate;
   if(sockets==null){var source=CompanionLabAssets.Load<TextAsset>("sockets");if(source)sockets=JsonUtility.FromJson<SocketCatalog>(source.text);}
   var socketModel=Array.Find(sockets?.models??new SocketModel[0],s=>s.key==CompanionLabSettings.Key(e));
   if(socketModel!=null)foreach(var socket in socketModel.sockets)
   {
    var bone=Array.Find(actor.GetComponentsInChildren<Transform>(),t=>t.name==socket.bone);if(!bone)continue;
    var t=new GameObject(socket.name).transform;t.SetParent(bone,false);var p=socket.position;t.localPosition=new Vector3(-p[1],p[2],p[0])*.01f;
    var s=socket.scale;t.localScale=new Vector3(s[1],s[2],s[0]);
   }
   seat=Array.Find(actor.GetComponentsInChildren<Transform>(),t=>t.name.Equals(e.saddle,StringComparison.OrdinalIgnoreCase));
   fx=actor.AddComponent<CompanionLabEffects>();fx.Configure(CompanionLabSettings.Key(e));currentMountClip="";Loop(false);
   Ground();rider.transform.position=new Vector3(actor.transform.position.x,0,actor.transform.position.z)+ComparisonSide()*3;GroundRider();
  }
#if UNITY_EDITOR
  public void PreparePoseReadbackInEditor(){CompanionLabPose.PrepareInEditor(actor);CompanionLabPose.PrepareInEditor(rider);}
#endif
  void Ground()
  {
   if(actor&&CompanionLabPose.TryBounds(actor,rider?rider.transform:null,out Bounds bounds))actor.transform.position-=Vector3.up*bounds.min.y;
  }
  static Vector3 ComparisonSide()=>Vector3.Cross(Vector3.up,new Vector3(-7,0,9)).normalized;
  void GroundRider()
  {
   if(!mounted&&rider&&CompanionLabPose.TryBounds(rider,null,out Bounds bounds))rider.transform.position-=Vector3.up*bounds.min.y;
  }
  void Dismount()
  {
   mounted=false;if(!rider)return;rider.transform.SetParent(null,true);
   rider.transform.position=actor?new Vector3(actor.transform.position.x,0,actor.transform.position.z)+ComparisonSide()*3:ComparisonSide()*3;
   rider.transform.rotation=Quaternion.Euler(0,RiderYaw,0);rider.transform.localScale=riderScale;if(motion)motion.PlayIdle(false);
  }
  void Toggle()
  {
   if(mounted){Dismount();return;}if(!seat||entries[selected].kind!="mount")return;
   mounted=true;rider.transform.SetParent(seat,true);rider.transform.position=seat.position;
   rider.transform.rotation=actor.transform.rotation*Quaternion.Euler(0,RiderYaw,0);
   PreserveRiderSize();if(motion)motion.PlayIdle(true);
  }
  void PreserveRiderSize(){var s=seat.lossyScale;rider.transform.localScale=new Vector3(riderScale.x/s.x,riderScale.y/s.y,riderScale.z/s.z);}
  void PlayMount(string name)
  {
   if(anim[name]==null)return;currentMountClip=name;anim[name].wrapMode=WrapMode.Loop;
   if(anim.isPlaying)anim.CrossFade(name,.15f);else anim.Play(name);anim.Sample();if(fx)fx.Play(name);
  }
  void Loop(bool move)
  {
   if(!anim)return;foreach(AnimationState state in anim)
    if(move?state.name.IndexOf("run",StringComparison.OrdinalIgnoreCase)>=0:
       state.name.IndexOf("idle",StringComparison.OrdinalIgnoreCase)>=0||state.name.IndexOf("wait",StringComparison.OrdinalIgnoreCase)>=0)
    {PlayMount(state.name);break;}
  }
  void Update()
  {
   if(!actor||!rider)return;if(Input.GetKeyDown(KeyCode.E))Toggle();
   if(Input.GetKeyDown(KeyCode.PageDown))SelectAdjacent(1);if(Input.GetKeyDown(KeyCode.PageUp))SelectAdjacent(-1);
   Vector3 direction=new Vector3(Input.GetAxisRaw("Horizontal"),0,Input.GetAxisRaw("Vertical")).normalized;
   var root=mounted?actor.transform:rider.transform;
   if(direction.sqrMagnitude>0){root.position+=direction*Time.deltaTime*(mounted?6:3);root.rotation=Quaternion.Slerp(root.rotation,Quaternion.LookRotation(direction)*(mounted?Quaternion.identity:Quaternion.Euler(0,RiderYaw,0)),Time.deltaTime*12);}
   bool pointerOverPanel=Input.mousePosition.x>=12&&Input.mousePosition.x<=352&&Input.mousePosition.y>=12&&Input.mousePosition.y<=Screen.height-12;
   if(Input.GetMouseButtonDown(0)&&!pointerOverPanel&&Time.time>=attackUntil){attackUntil=Time.time+.8f;if(motion)motion.PlayAttack();if(dummy&&Vector3.Distance(root.position,dummy.transform.position)<3.5f)hits++;}
   bool isMoving=direction.sqrMagnitude>0;
   if(Time.time>=attackUntil){if(motion){if(isMoving)motion.PlayMove(mounted);else motion.PlayIdle(mounted);}if(isMoving!=moving){Loop(isMoving&&(mounted||entries[selected].kind=="pet"));moving=isMoving;}}
   if(!mounted&&entries[selected].kind=="pet"){var target=rider.transform.position-ComparisonSide()*3;target.y=actor.transform.position.y;actor.transform.position=Vector3.MoveTowards(actor.transform.position,target,Time.deltaTime*3);}
   if(!pointerOverPanel)cameraZoom=Mathf.Clamp(cameraZoom-Input.mouseScrollDelta.y*.04f,.1f,2);
   view.transform.position=Vector3.Lerp(view.transform.position,root.position+new Vector3(7,5,-9)*cameraZoom,Time.deltaTime*5);view.transform.LookAt(root.position+Vector3.up*1.5f);
  }
  void LateUpdate()
  {
   if(actor&&!Regex.IsMatch(currentMountClip??"","fly|jump|leap|takeoff",RegexOptions.IgnoreCase))Ground();
   if(!mounted){GroundRider();return;}
   if(!seat||!rider)return;
   rider.transform.rotation=actor.transform.rotation*Quaternion.Euler(0,RiderYaw,0);PreserveRiderSize();
   var pivot=Array.Find(rider.GetComponentsInChildren<Transform>(),t=>t.name.Equals("Bip01",StringComparison.OrdinalIgnoreCase));
   if(pivot)rider.transform.position+=seat.position-pivot.position;
  }
  void SetSize(float factor){sizeFactor=factor;actor.transform.localScale=sourceScale*factor;Ground();LateUpdate();settingStatus="Nem mentett méretmódosítás";}
  int[] VisibleIndices()
  {
   return Enumerable.Range(0,entries.Length).Where(i=>listCategory==0?entries[i].kind=="mount":listCategory==1?entries[i].kind=="pet":entries[i].kind!="mount"&&entries[i].kind!="pet").OrderBy(i=>entries[i].name,StringComparer.OrdinalIgnoreCase).ToArray();
  }
  void ChooseCategory(int category)
  {
   listCategory=category;scroll=Vector2.zero;var indices=VisibleIndices();if(indices.Length>0&&Array.IndexOf(indices,selected)<0)Select(indices[0]);
  }
  void SelectAdjacent(int direction)
  {
   var indices=VisibleIndices();if(indices.Length==0)return;int position=Array.IndexOf(indices,selected);Select(indices[(Mathf.Max(0,position)+direction+indices.Length)%indices.Length]);
  }
  void OnGUI()
  {
   GUILayout.BeginArea(new Rect(12,12,340,Screen.height-24),GUI.skin.box);
   GUILayout.Label("TERA mount / pet teszt · találatok: "+hits);
   GUILayout.Label("WASD: mozgás · E: fel/le · bal egér: ütés\nGörgő: közelítés · Page Up/Down: modellváltás");
   if(entries==null||entries.Length==0){GUILayout.EndArea();return;}
   int category=GUILayout.Toolbar(listCategory,new[]{"Mountok","Petek","Modellrészek"});if(category!=listCategory)ChooseCategory(category);
   GUILayout.Label(entries[selected].name+" · "+entries[selected].kind);GUILayout.Label(seat?"Nyereg: "+seat.name:"Nincs eredeti nyeregkapcsolat");
   GUILayout.Label("Méret: "+sizeFactor.ToString("F2")+"×");float factor=GUILayout.HorizontalSlider(sizeFactor,.05f,10);
   if(Mathf.Abs(factor-sizeFactor)>.001f)SetSize(factor);
   if(GUILayout.Button("Méret mentése ehhez a modellhez"))try{CompanionLabSettings.Save(entries[selected],sizeFactor);settingStatus="Méret elmentve · újraindítás után is megmarad";}catch(Exception error){settingStatus="Mentési hiba: "+error.Message;}
   GUILayout.Label(settingStatus);if(GUILayout.Button("Forrásméret visszaállítása"))SetSize(1);
   GUILayout.Label(fx?fx.Status:"Effektek betöltése");if(fx)GUILayout.Label("Aktív kibocsátók: "+fx.ActiveEmitters+" · summon nélkül");
   GUILayout.BeginHorizontal();if(GUILayout.Button("Effektek be/ki")&&fx)fx.Toggle();if(GUILayout.Button("Külön effekt")&&fx)fx.PreviewNext();GUILayout.EndHorizontal();
   GUILayout.Label("Animáció: "+currentMountClip);if(GUILayout.Button("Következő tesztanimáció")){var names=entries[selected].clipNames;int i=Array.IndexOf(names,currentMountClip);if(names.Length>0)PlayMount(names[(i+1)%names.Length]);}
   scroll=GUILayout.BeginScrollView(scroll);foreach(int i in VisibleIndices())if(GUILayout.Button(entries[i].name))Select(i);GUILayout.EndScrollView();GUILayout.EndArea();
  }
 }
}
