using System;
using System.IO;
using UnityEngine;
using UnityEditor;
using UnityEditor.Build.Reporting;
using Metin2.Editor.Addressables;

public static class TeraCompanionReleaseContent
{
    public static void Run()
    {
        try
        {
            if(EditorUserBuildSettings.activeBuildTarget!=BuildTarget.StandaloneWindows64)
                throw new InvalidOperationException("Windows64 target required; do not silently retarget the main project.");
            TeraCompanionReleaseBuilder.RefreshCueDefaults();
            Metin2AddressablesBuildMenu.SyncSharedDependencies();
            Metin2AddressablesBuildMenu.BuildIncrementalDevContent();
            File.WriteAllText("Migration/TeraCompanions/content-build.txt","PASS Windows64 Addressables content build, including production race registrations and shared dependencies.\n");
            Debug.Log("TERA_WINDOWS_CONTENT_SUCCESS");
            EditorApplication.Exit(0);
        }
        catch(Exception e){Debug.LogException(e);EditorApplication.Exit(1);}
    }
    public static void ResumeContent()
    {
        try
        {
            if(EditorUserBuildSettings.activeBuildTarget!=BuildTarget.StandaloneWindows64)throw new InvalidOperationException("Windows64 target required");
            if(!File.Exists("Migration/ADDRESSABLES_SHARED_DEPENDENCIES.csv"))throw new InvalidOperationException("Completed shared dependency manifest required");
            Metin2AddressablesBuildMenu.BuildIncrementalDevContent();
            File.WriteAllText("Migration/TeraCompanions/content-build.txt","PASS Windows64 Addressables content build, including production race registrations and shared dependencies.\n");
            Debug.Log("TERA_WINDOWS_CONTENT_SUCCESS");EditorApplication.Exit(0);
        }
        catch(Exception e){Debug.LogException(e);EditorApplication.Exit(1);}
    }
    public static void OpenMainForTesting()
    {
        var settings=UnityEditor.AddressableAssets.AddressableAssetSettingsDefaultObject.Settings;
        int fast=settings.DataBuilders.FindIndex(x=>x&&x.GetType().Name=="BuildScriptFastMode");
        if(fast<0)throw new InvalidOperationException("Supported Asset Database play mode is unavailable");
        settings.ActivePlayModeDataBuilderIndex=fast;
        AssetDatabase.SaveAssets();
        UnityEditor.SceneManagement.EditorSceneManager.OpenScene("Assets/Metin2/Generated/Scenes/Metin2Client.unity");
        EditorApplication.delayCall+=()=>{EditorApplication.isPlaying=true;var type=typeof(EditorWindow).Assembly.GetType("UnityEditor.GameView");if(type!=null)EditorWindow.GetWindow(type).Focus();};
        Debug.Log("TERA_MAIN_CLIENT_OPENED_FOR_LIVE_TESTING");
    }
    public static void Player()
    {
        try
        {
            if(!File.Exists("Migration/TeraCompanions/content-build.txt")||!File.Exists("Migration/TeraCompanions/play-qa.txt"))
                throw new InvalidOperationException("Content and production Play checks must precede player packaging");
            if(EditorUserBuildSettings.activeBuildTarget!=BuildTarget.StandaloneWindows64)throw new InvalidOperationException("Windows64 target required");
            var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
                scenes=Array.ConvertAll(Array.FindAll(EditorBuildSettings.scenes,s=>s.enabled),s=>s.path),
                locationPathName="Build/Windows/Metin2/Metin2Unity.exe",target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
            if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Player build failed: "+report.summary.result);
            File.WriteAllText("Migration/TeraCompanions/player-build.txt","PASS "+Path.GetFullPath("Build/Windows/Metin2/Metin2Unity.exe")+" bytes="+report.summary.totalSize+"\n");
            Debug.Log("TERA_WINDOWS_PLAYER_SUCCESS");EditorApplication.Exit(0);
        }
        catch(Exception e){Debug.LogException(e);EditorApplication.Exit(1);}
    }
}
