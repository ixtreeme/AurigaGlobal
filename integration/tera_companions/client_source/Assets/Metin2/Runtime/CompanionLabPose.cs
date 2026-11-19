using UnityEngine;
using System.Collections.Generic;
namespace TeraCompanionLab {
public static class CompanionLabPose {
 sealed class SkinData {public Vector3[] vertices;public BoneWeight[] weights;public Matrix4x4[] bind;}
 static readonly Dictionary<Mesh,SkinData> cache=new Dictionary<Mesh,SkinData>();
 static SkinData Data(Mesh mesh,bool editorPreparation=false){if(cache.TryGetValue(mesh,out var existing)&&existing.vertices.Length==mesh.vertexCount)return existing;if(!mesh.isReadable&&!editorPreparation)return null;SkinData data;{data=new SkinData{vertices=mesh.vertices,weights=mesh.boneWeights,bind=mesh.bindposes};cache[mesh]=data;}return data;}

#if UNITY_EDITOR
 public static void PrepareInEditor(GameObject root){if(!root)return;foreach(var renderer in root.GetComponentsInChildren<SkinnedMeshRenderer>())if(renderer.sharedMesh)Data(renderer.sharedMesh,true);}
#endif
 public static bool TryBounds(GameObject root,Transform exclude,out Bounds bounds){bounds=new Bounds();bool found=false;foreach(var renderer in root.GetComponentsInChildren<SkinnedMeshRenderer>()){if(exclude&&renderer.transform.IsChildOf(exclude))continue;var mesh=renderer.sharedMesh;if(!mesh)continue;var data=Data(mesh);if(data==null)continue;var vertices=data.vertices;var weights=data.weights;var bind=data.bind;var bones=renderer.bones;var matrices=new Matrix4x4[bones.Length];for(int i=0;i<bones.Length;i++)matrices[i]=bones[i]&&i<bind.Length?bones[i].localToWorldMatrix*bind[i]:renderer.transform.localToWorldMatrix;for(int i=0;i<vertices.Length;i++){Vector3 point;if(i<weights.Length){var w=weights[i];point=Vector3.zero;Add(ref point,vertices[i],matrices,w.boneIndex0,w.weight0);Add(ref point,vertices[i],matrices,w.boneIndex1,w.weight1);Add(ref point,vertices[i],matrices,w.boneIndex2,w.weight2);Add(ref point,vertices[i],matrices,w.boneIndex3,w.weight3);}else point=renderer.transform.TransformPoint(vertices[i]);if(!found){bounds=new Bounds(point,Vector3.zero);found=true;}else bounds.Encapsulate(point);}}return found;}
 static void Add(ref Vector3 result,Vector3 point,Matrix4x4[] matrices,int index,float weight){if(weight>0&&index>=0&&index<matrices.Length)result+=matrices[index].MultiplyPoint3x4(point)*weight;}
}}
