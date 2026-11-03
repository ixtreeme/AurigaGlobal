using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.AddressableAssets.ResourceLocators;

namespace Metin2.Editor.Patching
{
    [InitializeOnLoad] internal static class Metin2CatalogRecovery
    {
        [Serializable] sealed class Request { public int owner;public string version; }
        const string RequestPath="Migration/PatchPublisher/catalog-rebuild.request.json";
        static Metin2CatalogRecovery(){EditorApplication.update+=Tick;}
        static void Tick()
        {
            if(!File.Exists(RequestPath)||EditorApplication.isCompiling||EditorApplication.isUpdating||Metin2ReleaseCoordinator.Busy)return;
            string revision=File.GetLastWriteTimeUtc(RequestPath).Ticks.ToString();
            if(SessionState.GetString("CatalogRecovery.Request","")==revision)return;
            var request=JsonUtility.FromJson<Request>(File.ReadAllText(RequestPath));
            if(request.owner!=System.Diagnostics.Process.GetCurrentProcess().Id)return;
            if(EditorApplication.isPlayingOrWillChangePlaymode){EditorApplication.isPlaying=false;return;}
            SessionState.SetString("CatalogRecovery.Request",revision);
            try
            {
                // Regression: the deployed catalog must fail validation before recovery.
                string previous="PatchOutput/windows/catalog_1.0.10.json";
                bool rejected=false;
                try{Metin2CatalogAddressValidator.Validate(JsonUtility.FromJson<ContentCatalogData>(File.ReadAllText(previous)));}
                catch(UnityEditor.Build.BuildFailedException error){rejected=true;Debug.Log("METIN2_CATALOG_REGRESSION_REJECTED "+error.Message);}
                if(!rejected)throw new InvalidOperationException("Reported missing-address regression did not reproduce.");
                if(request.version!="1.0.11"||Directory.Exists("PatchReleaseState/client-"+request.version+"/windows"))throw new InvalidOperationException("Recovery baseline already exists or unexpected version.");
                PlayerSettings.bundleVersion=request.version;
                AssetDatabase.SaveAssets();
                Metin2PatchManager.Build(false,true);
                Metin2ReleasePlanner.Write("windows","content-built",Metin2ReleasePlanner.Capture("windows"));
                Metin2ReleaseCoordinator.Start(new[]{"windows"},ReleaseAction.BuildAndPublish,true,false);
                Debug.Log("METIN2_CATALOG_RECOVERY content built; preparing matching client and publication");
            }
            catch(Exception error){Debug.LogException(error);File.WriteAllText("Migration/PatchPublisher/catalog-rebuild-error.txt",error.ToString());}
        }
    }
}
