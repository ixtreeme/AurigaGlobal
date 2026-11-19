using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
namespace TeraCompanionLab {
public static class CompanionLabSettings {
 [Serializable] public class ModelSetting {public string key;public float sourceScale,sizeMultiplier=1,totalScale,riderYaw=-90;}
 [Serializable] public class Settings {public int version=1;public List<ModelSetting> models=new List<ModelSetting>();}
 static Settings data;public static string PathName=>Path.GetFullPath(Path.Combine(Application.dataPath,"../Migration/TeraCompanionLab/settings.json"));
 static Settings Data {get{if(data==null){data=File.Exists(PathName)?JsonUtility.FromJson<Settings>(File.ReadAllText(PathName)):new Settings();if(data==null||data.models==null)throw new InvalidDataException("Invalid companion settings");}return data;}}
 public static string Key(CompanionLab.Entry e)=>string.IsNullOrEmpty(e.prefabResource)?e.name:e.prefabResource.Substring(e.prefabResource.LastIndexOf('/')+1);
 public static ModelSetting Get(CompanionLab.Entry e)=>Data.models.Find(s=>s.key==Key(e));
 public static void Save(CompanionLab.Entry e,float multiplier){if(float.IsNaN(multiplier)||float.IsInfinity(multiplier)||multiplier<=0)throw new ArgumentOutOfRangeException(nameof(multiplier));var setting=Get(e);if(setting==null){setting=new ModelSetting{key=Key(e)};Data.models.Add(setting);}setting.sourceScale=e.scale;setting.sizeMultiplier=multiplier;setting.totalScale=e.scale*multiplier;setting.riderYaw=-90;Directory.CreateDirectory(Path.GetDirectoryName(PathName));string tmp=PathName+".tmp";File.WriteAllText(tmp,JsonUtility.ToJson(Data,true));if(File.Exists(PathName))File.Replace(tmp,PathName,PathName+".bak");else File.Move(tmp,PathName);}
}}
