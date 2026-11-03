using System;
using UnityEngine;
namespace TeraCompanionLab {
public static class CompanionLabAssets {
 public static T Load<T>(string resource) where T:UnityEngine.Object {
#if UNITY_EDITOR
  string extension=typeof(T)==typeof(GameObject)?".prefab":typeof(T)==typeof(AnimationClip)?".anim":typeof(T)==typeof(TextAsset)?".json":throw new NotSupportedException(typeof(T).Name);
  string path="Assets/TeraCompanionLab/Editor/Resources/"+resource+extension;
  if(resource.StartsWith("Native/")||resource.StartsWith("TeraFX/"))
   path="Assets/Metin2/Generated/TeraCompanions/Source/"+resource+extension;
  return UnityEditor.AssetDatabase.LoadAssetAtPath<T>(path)??UnityEditor.AssetDatabase.LoadAssetAtPath<T>("Assets/TeraCompanionLab/Editor/Resources/"+resource+extension);
#else
  throw new InvalidOperationException("CompanionLab assets are editor-only; use explicit validated production import.");
#endif
 }
}}
