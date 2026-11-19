using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Metin2.Runtime.Patching;
using UnityEditor;
using UnityEditor.AddressableAssets;
using UnityEditor.AddressableAssets.Build;
using UnityEditor.AddressableAssets.Build.DataBuilders;
using UnityEditor.AddressableAssets.Settings;
using UnityEditor.AddressableAssets.Settings.GroupSchemas;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;
using UnityEngine.AddressableAssets.ResourceLocators;
using UnityEngine.ResourceManagement.ResourceLocations;
using UnityEngine.ResourceManagement.ResourceProviders;

namespace Metin2.Editor.Patching
{
    [Serializable] internal sealed class PatchRelease
    {
        public int schemaVersion = 1;
        public string platform, clientVersion, minimumClientVersion, latestClientVersion, contentVersion, releaseId, addressablesPlayerVersion;
        public string windowsUpdateUrl, androidStoreUrl, androidDirectApkUrl, iosStoreUrl;
        public string publishedUtc, requiredLabel, buildType, gitCommit;
        public string patchNotesTitle,patchNotesAuthor,patchNotesText;
        public long totalRemoteContentBytes, patchBytes;
    }

    [Serializable] internal sealed class PatchFile { public string path, sha256; public long size; }
    [Serializable] internal sealed class PatchFileManifest { public PatchFile[] files; }
    [Serializable] internal sealed class PatchUpdatePolicy
    {
        public string minimumClientVersion;
        public string latestClientVersion;
        public string windowsUpdateUrl;
        public string androidStoreUrl;
        public string androidDirectApkUrl;
        public string iosStoreUrl;
    }

    [InitializeOnLoad]
    public sealed class Metin2PatchManager : EditorWindow
    {
        // The shipped 1.0.3 content was built before Cloud association. This is a
        // published storage namespace, not the mutable Unity/UVCS project identity.
        internal const string PublishedBundleProjectNamespace = "";
        static Metin2PatchManager() => EnsureBundleIdentity();
        internal static void EnsureBundleIdentity() =>
            BuildScriptPackedMode.BundleProjectIdOverride = PublishedBundleProjectNamespace;

        private static readonly string[] RemoteGroups = {
            "Metin2-Audio", "Metin2-Effects", "Metin2-Eclipse-Effects", "Metin2-Equipment", "Metin2-Maps",
            "Metin2-Maps-Shared-Effects", "Metin2-Maps-Shared-Environment",
            "Metin2-Maps-Shared-Trees", "Metin2-Maps-Shared-Zones", "Metin2-Races", "Metin2-Shared-Characters"
        };

        internal static string ProjectRoot => Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        internal static string StateRoot => Path.Combine(ProjectRoot, "PatchReleaseState");
        internal static string OutputRoot => Path.Combine(ProjectRoot, "PatchOutput");
        internal static string UpdatePolicyPath => Path.Combine(ProjectRoot, "Assets", "Metin2", "Editor", "Patching", "Metin2PatchUpdatePolicy.json");
        private PatchUpdatePolicy updatePolicy;

        [MenuItem("Metin2/Patch eszközök (haladó)/Régi Patch Manager")]
        public static void Open() => GetWindow<Metin2PatchManager>("Metin2 Patch");

        private void OnEnable() => updatePolicy = LoadUpdatePolicy();

        private void OnGUI()
        {
            EditorGUILayout.LabelField("Platform", EditorUserBuildSettings.activeBuildTarget.ToString());
            EditorGUILayout.LabelField("URL", Metin2PatchConfig.BaseUrl + "/" + Folder(EditorUserBuildSettings.activeBuildTarget) + "/");
            EditorGUILayout.LabelField("Client version", PlayerSettings.bundleVersion);
            EditorGUILayout.LabelField("Publisher", File.Exists(Path.Combine(ProjectRoot, "PatchPublisher.local.json")) ? "Local config present" : "Not configured");
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Client update policy", EditorStyles.boldLabel);
            if (updatePolicy == null) updatePolicy = LoadUpdatePolicy();
            updatePolicy.minimumClientVersion = EditorGUILayout.TextField("Minimum compatible", updatePolicy.minimumClientVersion);
            updatePolicy.latestClientVersion = EditorGUILayout.TextField("Latest available", updatePolicy.latestClientVersion);
            updatePolicy.windowsUpdateUrl = EditorGUILayout.TextField("Windows update URL", updatePolicy.windowsUpdateUrl);
            updatePolicy.androidStoreUrl = EditorGUILayout.TextField("Android store URL", updatePolicy.androidStoreUrl);
            updatePolicy.androidDirectApkUrl = EditorGUILayout.TextField("Android direct APK URL", updatePolicy.androidDirectApkUrl);
            updatePolicy.iosStoreUrl = EditorGUILayout.TextField("iOS App Store URL", updatePolicy.iosStoreUrl);
            if (GUILayout.Button("Save client update policy")) SaveUpdatePolicy(updatePolicy);
            EditorGUILayout.HelpBox("Clients below Minimum compatible are blocked. Clients below Latest available may choose Update or Later. Leave the direct APK URL empty for a Google Play-only build.", MessageType.Info);
            EditorGUILayout.Space();
            if (GUILayout.Button("Validate / configure production")) Validate();
            if (GUILayout.Button("Build full release - current platform")) FullRelease();
            if (GUILayout.Button("Build patch-capable player - current platform")) BuildPlayerCurrent();
            if (GUILayout.Button("Build content update - current platform")) ContentUpdate();
            if (GUILayout.Button("Build + publish content update - all platforms")) Metin2PatchPublisher.ContentUpdateAll();
            if (GUILayout.Button("Dry-run publish plan - current platform")) DryRunPublish();
            EditorGUILayout.HelpBox("The first patch-capable player needs a full release and new app build. Publishing is separate from building. Local publisher credentials are not stored in Unity assets.", MessageType.Info);
        }

        internal static PatchUpdatePolicy LoadUpdatePolicy()
        {
            PatchUpdatePolicy policy = File.Exists(UpdatePolicyPath)
                ? JsonUtility.FromJson<PatchUpdatePolicy>(File.ReadAllText(UpdatePolicyPath))
                : null;
            if (policy == null) policy = new PatchUpdatePolicy();
            if (string.IsNullOrWhiteSpace(policy.minimumClientVersion)) policy.minimumClientVersion = PlayerSettings.bundleVersion;
            if (string.IsNullOrWhiteSpace(policy.latestClientVersion)) policy.latestClientVersion = PlayerSettings.bundleVersion;
            if (string.IsNullOrWhiteSpace(policy.windowsUpdateUrl)) policy.windowsUpdateUrl = Metin2PatchConfig.WebsiteUrl;
            if (string.IsNullOrWhiteSpace(policy.androidStoreUrl)) policy.androidStoreUrl = Metin2PatchConfig.AndroidStoreUrl;
            if (string.IsNullOrWhiteSpace(policy.iosStoreUrl)) policy.iosStoreUrl = Metin2PatchConfig.WebsiteUrl;
            return policy;
        }

        internal static void SaveUpdatePolicy(PatchUpdatePolicy policy)
        {
            ValidateUpdatePolicy(policy);
            Directory.CreateDirectory(Path.GetDirectoryName(UpdatePolicyPath));
            File.WriteAllText(UpdatePolicyPath, JsonUtility.ToJson(policy, true) + Environment.NewLine);
            AssetDatabase.ImportAsset("Assets/Metin2/Editor/Patching/Metin2PatchUpdatePolicy.json", ImportAssetOptions.ForceUpdate);
            Debug.Log("METIN2_PATCH_UPDATE_POLICY saved=" + UpdatePolicyPath);
        }

        private static void ValidateUpdatePolicy(PatchUpdatePolicy policy)
        {
            if (policy == null || !Version.TryParse(policy.minimumClientVersion, out _) || !Version.TryParse(policy.latestClientVersion, out _))
                throw new BuildFailedException("Client update policy versions must be numeric dotted versions.");
            if (Metin2PatchReleaseInfo.CompareVersions(policy.latestClientVersion, policy.minimumClientVersion) < 0)
                throw new BuildFailedException("Latest client version cannot be older than the minimum compatible version.");
            foreach (string url in new[] { policy.windowsUpdateUrl, policy.androidStoreUrl, policy.iosStoreUrl })
                if (!Uri.TryCreate(url, UriKind.Absolute, out Uri parsed) || parsed.Scheme != Uri.UriSchemeHttps)
                    throw new BuildFailedException("Store and update URLs must use HTTPS: " + url);
            if (!string.IsNullOrWhiteSpace(policy.androidDirectApkUrl) &&
                (!Uri.TryCreate(policy.androidDirectApkUrl, UriKind.Absolute, out Uri apk) || apk.Scheme != Uri.UriSchemeHttps))
                throw new BuildFailedException("Direct APK URL must use HTTPS.");
        }

        public static string Folder(BuildTarget target)
        {
            switch (target)
            {
                case BuildTarget.StandaloneWindows64: return "windows";
                case BuildTarget.Android: return "android";
                case BuildTarget.iOS: return "ios";
                default: throw new BuildFailedException("Unsupported patch platform: " + target);
            }
        }

        internal static AddressableAssetSettings Configure()
        {
            EnsureBundleIdentity();
            BuildTarget target = EditorUserBuildSettings.activeBuildTarget;
            string folder = Folder(target);
            AddressableAssetSettings settings = AddressableAssetSettingsDefaultObject.GetSettings(false);
            if (settings == null) throw new BuildFailedException("Addressables settings are missing.");
            string profile = settings.profileSettings.GetProfileId("Production");
            if (string.IsNullOrEmpty(profile)) profile = settings.profileSettings.AddProfile("Production", settings.activeProfileId);
            settings.activeProfileId = profile;
            settings.profileSettings.SetValue(profile, AddressableAssetSettings.kRemoteBuildPath, "PatchOutput/" + folder);
            settings.profileSettings.SetValue(profile, AddressableAssetSettings.kRemoteLoadPath, Metin2PatchConfig.BaseUrl + "/" + folder + "/");
            settings.BuildRemoteCatalog = true;
            settings.RemoteCatalogBuildPath.SetVariableByName(settings, AddressableAssetSettings.kRemoteBuildPath);
            settings.RemoteCatalogLoadPath.SetVariableByName(settings, AddressableAssetSettings.kRemoteLoadPath);
            settings.BuildAddressablesWithPlayerBuild = AddressableAssetSettings.PlayerBuildOption.DoNotBuildWithPlayer;
            // The full object-level report is hundreds of MB in this project.
            // Keep it opt-in; it does not participate in bundle/catalog correctness.
            ProjectConfigData.GenerateBuildLayout = Environment.GetEnvironmentVariable("METIN2_PATCH_BUILD_REPORT") == "1";
            settings.AddLabel(Metin2PatchConfig.RequiredLabel);
            foreach (string name in RemoteGroups)
            {
                AddressableAssetGroup group = settings.FindGroup(name);
                if (group == null) throw new BuildFailedException("Required group missing: " + name);
                BundledAssetGroupSchema bundle = group.GetSchema<BundledAssetGroupSchema>();
                if (bundle == null) throw new BuildFailedException("Bundle schema missing: " + name);
                bundle.BuildPath.SetVariableByName(settings, AddressableAssetSettings.kRemoteBuildPath);
                bundle.LoadPath.SetVariableByName(settings, AddressableAssetSettings.kRemoteLoadPath);
                // Public runtime roots must remain loadable by their stable address.
                // Shared dependency groups intentionally publish GUIDs only.
                if (!name.Contains("Shared"))
                {
                    bundle.IncludeAddressInCatalog = true;
                    bundle.IncludeGUIDInCatalog = true;
                    bundle.IncludeLabelsInCatalog = true;
                }
                EditorUtility.SetDirty(bundle);
                foreach (AddressableAssetEntry entry in group.entries)
                    if (!entry.labels.Contains(Metin2PatchConfig.RequiredLabel)) entry.SetLabel(Metin2PatchConfig.RequiredLabel, true);
                EditorUtility.SetDirty(group);
            }
            foreach (string name in new[] { "Metin2-Core", "Metin2-Shared-Shaders" })
            {
                AddressableAssetGroup group = settings.FindGroup(name);
                if (group == null) throw new BuildFailedException("Local bootstrap group missing: " + name);
                BundledAssetGroupSchema bundle = group.GetSchema<BundledAssetGroupSchema>();
                bundle.BuildPath.SetVariableByName(settings, AddressableAssetSettings.kLocalBuildPath);
                bundle.LoadPath.SetVariableByName(settings, AddressableAssetSettings.kLocalLoadPath);
                EditorUtility.SetDirty(bundle);
            }
            EditorUtility.SetDirty(settings);
            AssetDatabase.SaveAssets();
            ValidateConfigured(settings, folder);
            return settings;
        }

        private static void ValidateConfigured(AddressableAssetSettings settings, string folder)
        {
            if (!settings.BuildRemoteCatalog || settings.profileSettings.GetProfileName(settings.activeProfileId) != "Production")
                throw new BuildFailedException("Production remote catalog is not active.");
            string url = settings.RemoteCatalogLoadPath.GetValue(settings);
            if (url != Metin2PatchConfig.BaseUrl + "/" + folder + "/")
                throw new BuildFailedException("Wrong platform catalog URL: " + url);
            foreach (string name in RemoteGroups)
            {
                AddressableAssetGroup group = settings.FindGroup(name);
                BundledAssetGroupSchema bundle = group.GetSchema<BundledAssetGroupSchema>();
                if (bundle.LoadPath.GetValue(settings) != url || group.entries.Count == 0 ||
                    group.entries.Any(e => !e.labels.Contains(Metin2PatchConfig.RequiredLabel)))
                    throw new BuildFailedException("Remote group or label invalid: " + name);
            }
            Debug.Log("METIN2_PATCH_VERIFY platform=" + folder + " url=" + url);
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Validate")]
        public static void Validate() => Configure();

        [MenuItem("Metin2/Patch eszközök (haladó)/Run Configuration Self-Checks")]
        public static void RunSelfChecks()
        {
            if (Folder(BuildTarget.StandaloneWindows64) != "windows" || Folder(BuildTarget.Android) != "android" || Folder(BuildTarget.iOS) != "ios")
                throw new BuildFailedException("Patch target mapping failed.");
            if (Metin2PatchConfig.PlatformUrl(RuntimePlatform.WindowsPlayer) != "https://www.metin2pegasus.eu/aurigapatch/windows/" ||
                Metin2PatchConfig.PlatformUrl(RuntimePlatform.Android) != "https://www.metin2pegasus.eu/aurigapatch/android/" ||
                Metin2PatchConfig.PlatformUrl(RuntimePlatform.IPhonePlayer) != "https://www.metin2pegasus.eu/aurigapatch/ios/")
                throw new BuildFailedException("Patch URL mapping failed.");
            if (Metin2PatchReleaseInfo.CompareVersions("1.0", "1.0.0") != 0 ||
                Metin2PatchReleaseInfo.CompareVersions("1.0.1", "1.0.0") <= 0 ||
                Metin2PatchReleaseInfo.CompareVersions("1.0.0", "1.1.0") >= 0)
                throw new BuildFailedException("Client version comparison failed.");
            Configure();
            Debug.Log("METIN2_PATCH_VERIFY selfChecks=SUCCESS");
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Audit Startup Scene Dependencies")]
        public static void AuditStartupScene()
        {
            const string scene = "Assets/Metin2/Generated/Scenes/Metin2Client.unity";
            if (!File.Exists(scene)) throw new BuildFailedException("Main scene missing: " + scene);
            string[] dependencies = AssetDatabase.GetDependencies(scene, true);
            string output = Path.Combine(ProjectRoot, "PatchAudit", "startup-scene-dependencies.csv");
            Directory.CreateDirectory(Path.GetDirectoryName(output));
            var rows = dependencies.Select(path => new {
                path, bytes = File.Exists(path) ? new FileInfo(path).Length : 0L
            }).OrderByDescending(x => x.bytes).ToArray();
            using (var writer = new StreamWriter(output, false))
            {
                writer.WriteLine("source_asset,source_bytes");
                foreach (var row in rows)
                    writer.WriteLine("\"" + row.path.Replace("\"", "\"\"") + "\"," + row.bytes);
            }
            Debug.Log("METIN2_PATCH_VERIFY startupSceneDependencies=" + rows.Length + " sourceBytes=" + rows.Sum(x => x.bytes) + " report=" + output);
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Audit Resources Calls")]
        public static void AuditResourcesCalls()
        {
            string[] resources = Directory.GetFiles(Application.dataPath, "*", SearchOption.AllDirectories)
                .Where(p => p.Replace('\\', '/').Contains("/Resources/") && !p.EndsWith(".meta", StringComparison.OrdinalIgnoreCase)).ToArray();
            var byKey = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            foreach (string file in resources)
            {
                string normalized = file.Replace('\\', '/');
                string key = Path.ChangeExtension(normalized.Substring(normalized.LastIndexOf("/Resources/", StringComparison.Ordinal) + 11), null).Replace('\\', '/');
                if (!byKey.TryGetValue(key, out List<string> list)) byKey[key] = list = new List<string>();
                list.Add("Assets" + normalized.Substring(Application.dataPath.Replace('\\', '/').Length));
            }
            string output = Path.Combine(ProjectRoot, "PatchAudit", "resources-calls.csv");
            Directory.CreateDirectory(Path.GetDirectoryName(output));
            using (var writer = new StreamWriter(output, false))
            {
                writer.WriteLine("source_file,line,resources_path,actual_asset,source_bytes,dependency_source_bytes,pre_patch,decision");
                foreach (string source in Directory.GetFiles(Path.Combine(Application.dataPath, "Metin2", "Runtime"), "*.cs", SearchOption.AllDirectories))
                {
                    string[] lines = File.ReadAllLines(source);
                    for (int i = 0; i < lines.Length; i++)
                    {
                        if (!lines[i].Contains("Resources.Load")) continue;
                        var match = System.Text.RegularExpressions.Regex.Match(lines[i], "Resources\\.Load(?:Async)?(?:<[^>]+>)?\\(\\s*\"([^\"]+)\"");
                        string key = match.Success ? match.Groups[1].Value : "<dynamic expression>";
                        if (!byKey.TryGetValue(key, out List<string> matches)) matches = new List<string>();
                        if (matches.Count == 0) WriteResourceRow(writer, source, i + 1, key, "<unresolved>", 0, 0);
                        foreach (string asset in matches)
                        {
                            long bytes = File.Exists(asset) ? new FileInfo(asset).Length : 0;
                            long closure = AssetDatabase.GetDependencies(asset, true).Where(File.Exists).Sum(p => new FileInfo(p).Length);
                            WriteResourceRow(writer, source, i + 1, key, asset, bytes, closure);
                        }
                    }
                }
            }
            Debug.Log("METIN2_PATCH_VERIFY resourcesAudit=" + output);
        }

        private static void WriteResourceRow(StreamWriter writer, string source, int line, string key, string asset, long bytes, long closure)
        {
            bool bootstrap = key.Contains("MobileUI") || key.Contains("Index") || key.Contains("Catalog") || key.Contains("Shader");
            string decision = asset == "<unresolved>" ? "manual-review" : bootstrap ? "remain-local-review-dependencies" :
                closure > 1048576 ? "migrate-review-runtime-call" : "remain-local-review";
            string[] values = { source.Substring(ProjectRoot.Length).TrimStart(Path.DirectorySeparatorChar).Replace('\\', '/'),
                line.ToString(), key, asset, bytes.ToString(), closure.ToString(), bootstrap ? "yes-or-early" : "no-or-unknown", decision };
            writer.WriteLine(string.Join(",", values.Select(v => "\"" + v.Replace("\"", "\"\"") + "\"")));
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Full Release/Build Current Platform")]
        public static void FullRelease() => Build(false);

        [MenuItem("Metin2/Patch eszközök (haladó)/Content Update/Build Current Platform")]
        public static void ContentUpdate() => Build(true);

        [MenuItem("Metin2/Patch eszközök (haladó)/Finalize Unpublished Baseline Current Platform")]
        public static void FinalizeUnpublishedBaseline()
        {
            string folder = Folder(EditorUserBuildSettings.activeBuildTarget);
            string baselineDir = Path.Combine(StateRoot, "client-" + PlayerSettings.bundleVersion, folder);
            string output = Path.Combine(OutputRoot, folder);
            if (!File.Exists(Path.Combine(baselineDir, "addressables_content_state.bin")) ||
                !File.Exists(Path.Combine(baselineDir, "release.json")) || !Directory.Exists(output))
                throw new BuildFailedException("Unpublished full baseline is incomplete for " + folder + ".");
            if (File.Exists(Path.Combine(baselineDir, "published-files.json")))
                throw new BuildFailedException("Published baselines are immutable. Increase the client version instead.");
            File.WriteAllText(Path.Combine(baselineDir, "code-fingerprint.txt"), CodeFingerprint());
            File.WriteAllText(Path.Combine(output, "publish-files.json"), JsonUtility.ToJson(MakeManifest(output), true));
            Debug.Log("METIN2_PATCH_BUILD baseline-finalized platform=" + folder);
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Full Release/Build Patch-Capable Player Current Platform")]
        public static void BuildPlayerCurrent()
            =>BuildPlayerCurrent(false);

        internal static void BuildPlayerCurrent(bool compatibleCode)
        {
            BuildTarget target = EditorUserBuildSettings.activeBuildTarget;
            string folder = Folder(target);
            Configure();
            string baseline = Path.Combine(StateRoot, "client-" + PlayerSettings.bundleVersion, folder, "addressables_content_state.bin");
            if (!File.Exists(baseline)) throw new BuildFailedException("Full content baseline missing for " + folder);
            if (!(compatibleCode?Metin2ReleasePlanner.CanReuseContent(folder):IsCurrentBaselineCompatible(folder)))
                throw new BuildFailedException("Player code differs from its content baseline. Build a new compatible client baseline first.");
            string[] scenes = EditorBuildSettings.scenes.Where(s => s.enabled).Select(s => s.path).ToArray();
            if (scenes.Length == 0) throw new BuildFailedException("No enabled scenes.");
            if (target == BuildTarget.Android) { if(compatibleCode)Metin2AndroidPlayerBuilder.BuildReuseFromMenu();else Metin2AndroidPlayerBuilder.BuildFromMenu(); return; }
            if (target == BuildTarget.iOS) { Metin2IOSBuild.ExportPlayerOnly(); return; }
            string output = Path.Combine(ProjectRoot, "Build", "PatchWindows", "Metin2Unity.exe");
            Directory.CreateDirectory(Path.GetDirectoryName(output));
            BuildReport report = BuildPipeline.BuildPlayer(scenes, output, target, BuildOptions.None);
            if (report.summary.result != BuildResult.Succeeded)
                throw new BuildFailedException("Patch-capable Windows player build failed: " + report.summary.result);
            Debug.Log("METIN2_PATCH_BUILD player platform=" + folder + " bytes=" + report.summary.totalSize);
        }

        internal static void Build(bool update,bool compatibleCode=false)
        {
            BuildTarget target = EditorUserBuildSettings.activeBuildTarget;
            string folder = Folder(target);
            AddressableAssetSettings settings = Configure();
            string baselineDir = Path.Combine(StateRoot, "client-" + PlayerSettings.bundleVersion, folder);
            string baselineState = Path.Combine(baselineDir, "addressables_content_state.bin");
            if (update)
            {
                if (!File.Exists(baselineState) || !File.Exists(Path.Combine(baselineDir, "release.json")))
                    throw new BuildFailedException("Published baseline state missing for " + folder + " client " + PlayerSettings.bundleVersion);
                PatchRelease baseline = JsonUtility.FromJson<PatchRelease>(File.ReadAllText(Path.Combine(baselineDir, "release.json")));
                if (baseline == null || baseline.platform != folder || baseline.clientVersion != PlayerSettings.bundleVersion)
                    throw new BuildFailedException("Content state platform or client version mismatch.");
                // Fingerprint the actual released working tree, including pre-existing local edits.
                string fingerprintPath = Path.Combine(baselineDir, "code-fingerprint.txt");
                if (!(compatibleCode?Metin2ReleasePlanner.CanReuseContent(folder,false):File.Exists(fingerprintPath)&&File.ReadAllText(fingerprintPath)==CodeFingerprint()))
                    throw new BuildFailedException("New client baseline required: code, native plugin, package or project settings changed.");
            }
            else if (File.Exists(baselineState))
                throw new BuildFailedException("Immutable baseline already exists: " + baselineState + ". Increase the client version for a new baseline.");

            Debug.Log("METIN2_PATCH_BUILD target=" + target + " type=" + (update ? "content-update" : "full"));
            AddressablesPlayerBuildResult result = update
                ? UnityEditor.AddressableAssets.Build.ContentUpdateScript.BuildContentUpdate(settings, baselineState)
                : BuildFull();
            if (result == null || !string.IsNullOrEmpty(result.Error))
                throw new BuildFailedException("Addressables build failed: " + result?.Error);
            UnityEditor.Build.Pipeline.Utilities.PersistentBuildObjectTypes.Report();
            string output = Path.Combine(OutputRoot, folder);
            if (!Directory.Exists(output)) throw new BuildFailedException("Remote build output missing: " + output);
            // Addressables 1.21 writes StandaloneWindows64 state under its platform name, "Windows".
            string statePlatform = target == BuildTarget.StandaloneWindows64 ? "Windows" : target.ToString();
            string state = Path.Combine(Application.dataPath, "AddressableAssetsData", statePlatform, "addressables_content_state.bin");
            if (!File.Exists(state)) throw new BuildFailedException("Content state missing: " + state);
            Directory.CreateDirectory(baselineDir);
            if (!update) File.Copy(state, baselineState, false);
            if (!update) File.WriteAllText(Path.Combine(baselineDir, "code-fingerprint.txt"), CodeFingerprint());
            string catalogVersion = update
                ? ContentUpdateScript.LoadContentState(baselineState).playerVersion : PlayerSettings.bundleVersion;
            PatchFileManifest manifest = MakeManifest(output, catalogVersion, folder);
            long total = manifest.files.Sum(f => f.size);
            PatchFileManifest previous = Metin2PatchPublisher.LastPublishedManifest(folder);
            HashSet<string> old = previous == null ? new HashSet<string>() :
                new HashSet<string>(previous.files.Select(f => f.path + ":" + f.sha256));
            long changed = manifest.files.Where(f => !old.Contains(f.path + ":" + f.sha256)).Sum(f => f.size);
            PatchUpdatePolicy policy = LoadUpdatePolicy();
            ValidateUpdatePolicy(policy);
            if (target == BuildTarget.StandaloneWindows64)
            {
                // Windows has an automatic binary updater. Do not raise the mobile
                // store requirements when publishing a Windows-only client version.
                policy.minimumClientVersion = PlayerSettings.bundleVersion;
                policy.latestClientVersion = PlayerSettings.bundleVersion;
            }
            PatchRelease release = new PatchRelease {
                platform = folder, clientVersion = PlayerSettings.bundleVersion, minimumClientVersion = policy.minimumClientVersion,
                latestClientVersion = policy.latestClientVersion,
                windowsUpdateUrl = policy.windowsUpdateUrl,
                androidStoreUrl = policy.androidStoreUrl,
                androidDirectApkUrl = policy.androidDirectApkUrl,
                iosStoreUrl = policy.iosStoreUrl,
                contentVersion = DateTime.UtcNow.ToString("yyyy.MM.dd.HHmmss"), releaseId = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss"),
                addressablesPlayerVersion = update?UnityEditor.AddressableAssets.Build.ContentUpdateScript.LoadContentState(baselineState).playerVersion:PlayerSettings.bundleVersion,
                publishedUtc = DateTime.UtcNow.ToString("o"), requiredLabel = Metin2PatchConfig.RequiredLabel,
                buildType = update ? "content-update" : "full", totalRemoteContentBytes = total, patchBytes = changed,
                gitCommit = Git("rev-parse HEAD").Trim()
            };
            Metin2ReleaseNotesDraft.Apply(folder,release);
            // Include the final metadata in the totals, and compare a new client
            // baseline against the actual published destination, not an empty history.
            bool stable = false;
            for (int pass = 0; pass < 8; ++pass)
            {
                File.WriteAllText(Path.Combine(output, "release.json"), JsonUtility.ToJson(release, true));
                manifest = MakeManifest(output);
                total = manifest.files.Sum(f => f.size);
                changed = manifest.files.Where(f => !old.Contains(f.path + ":" + f.sha256)).Sum(f => f.size);
                if (release.totalRemoteContentBytes == total && release.patchBytes == changed) { stable = true; break; }
                release.totalRemoteContentBytes = total;
                release.patchBytes = changed;
            }
            if (!stable) throw new BuildFailedException("Release metadata byte totals did not converge.");
            File.WriteAllText(Path.Combine(output, "publish-files.json"), JsonUtility.ToJson(manifest, true));
            if (!update) File.WriteAllText(Path.Combine(baselineDir, "release.json"), JsonUtility.ToJson(release, true));
            Debug.Log("METIN2_PATCH_BUILD success platform=" + folder + " remoteBytes=" + total + " changedBytes=" + changed + " state=" + baselineState);
        }

        private static AddressablesPlayerBuildResult BuildFull()
        {
            AddressableAssetSettings.BuildPlayerContent(out AddressablesPlayerBuildResult result);
            return result;
        }

        internal static PatchFileManifest MakeManifest(string root, string catalogVersion = null, string platform = null)
        {
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            try
            {
            int checkedFiles=0;
            var manifest=new PatchFileManifest { files = ManifestPaths(root, catalogVersion, platform)
                .Where(p => !p.EndsWith(".meta", StringComparison.OrdinalIgnoreCase) &&
                    !p.EndsWith("publish-files.json", StringComparison.OrdinalIgnoreCase) &&
                    !p.EndsWith("client-publish-plan.json", StringComparison.OrdinalIgnoreCase) &&
                    !p.EndsWith("addressables_content_state.bin", StringComparison.OrdinalIgnoreCase))
                .Select(p => {
                    if((checkedFiles++ & 63)==0)Metin2ReleasePlanner.Progress("Kimeneti fájlok ellenőrzése: "+checkedFiles);
                    return new PatchFile { path = p.Substring(root.Length).TrimStart(Path.DirectorySeparatorChar).Replace('\\', '/'),
                        size = new FileInfo(p).Length, sha256 = Metin2ReleasePlanner.FileHash(p, p.EndsWith(".bundle", StringComparison.OrdinalIgnoreCase)) };
                }).OrderBy(f => f.path, StringComparer.Ordinal).ToArray() };
            Metin2ReleasePlanner.SaveHashCache();
            return manifest;
            }
            finally { EditorUtility.ClearProgressBar(); }
        }

        // Output directories may contain years of old bundles, including interrupted
        // unpublished builds. Only the active catalog's dependency closure is a release.
        internal static IEnumerable<string> ManifestPaths(string root, string catalogVersion = null, string platform = null)
        {
            root = Path.GetFullPath(root);
            string releasePath = Path.Combine(root, "release.json");
            if (File.Exists(releasePath))
            {
                var release = JsonUtility.FromJson<PatchRelease>(File.ReadAllText(releasePath));
                catalogVersion = catalogVersion ?? release?.addressablesPlayerVersion;
                platform = platform ?? release?.platform;
                if (string.IsNullOrEmpty(catalogVersion) || string.IsNullOrEmpty(platform))
                    throw new BuildFailedException("Release catalog identity missing: " + releasePath);
            }
            // Anonymous fixture/legacy output has no declared active catalog.
            if (string.IsNullOrEmpty(catalogVersion)) return Directory.GetFiles(root, "*", SearchOption.AllDirectories);
            if (!System.Text.RegularExpressions.Regex.IsMatch(catalogVersion, "^[A-Za-z0-9._-]+$") ||
                !new[] { "windows", "android", "ios" }.Contains(platform))
                throw new BuildFailedException("Invalid release catalog identity.");
            string catalog = Path.Combine(root, "catalog_" + catalogVersion + ".json");
            string catalogHash = Path.ChangeExtension(catalog, ".hash");
            if (!File.Exists(catalog) || !File.Exists(catalogHash))
                throw new BuildFailedException("Active catalog/hash missing: " + catalog);
            var paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { catalog, catalogHash };
            if (File.Exists(releasePath)) paths.Add(releasePath);
            var data = JsonUtility.FromJson<ContentCatalogData>(File.ReadAllText(catalog));
            Metin2CatalogAddressValidator.Validate(data);
            var pending = new Stack<IResourceLocation>(data.CreateLocator().Locations.Values.SelectMany(locations => locations));
            var visited = new HashSet<IResourceLocation>();
            var remote = new Uri(Metin2PatchConfig.BaseUrl.TrimEnd('/') + "/" + platform + "/");
            int bundles = 0;
            while (pending.Count != 0)
            {
                var location = pending.Pop();
                if (!visited.Add(location)) continue;
                if (location.HasDependencies)
                    foreach (var dependency in location.Dependencies) pending.Push(dependency);
                if (location.ResourceType != typeof(IAssetBundleResource)) continue;
                if (!Uri.TryCreate(location.InternalId, UriKind.Absolute, out var uri) ||
                    (uri.Scheme != "https" && uri.Scheme != "http")) continue; // Local bootstrap bundle.
                if (!remote.IsBaseOf(uri))
                    throw new BuildFailedException("Remote bundle outside release URL: " + location.InternalId);
                string relative = Uri.UnescapeDataString(remote.MakeRelativeUri(uri).ToString());
                string path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
                if (!path.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) ||
                    !path.EndsWith(".bundle", StringComparison.OrdinalIgnoreCase) || !File.Exists(path))
                    throw new BuildFailedException("Referenced bundle missing/unsafe: " + relative);
                if (paths.Add(path)) bundles++;
            }
            if (bundles == 0) throw new BuildFailedException("Active catalog contains no remote bundles: " + catalog);
            return paths;
        }

        private static string Git(string arguments)
        {
            try
            {
                var process = new System.Diagnostics.Process { StartInfo = new System.Diagnostics.ProcessStartInfo("git", arguments) {
                    WorkingDirectory = ProjectRoot, RedirectStandardOutput = true, UseShellExecute = false, CreateNoWindow = true } };
                process.Start();
                string value = process.StandardOutput.ReadToEnd();
                process.WaitForExit(5000);
                return process.ExitCode == 0 ? value : string.Empty;
            }
            catch { return string.Empty; }
        }

        private static string GitCodeChanges(string baseline)
        {
            if (string.IsNullOrEmpty(baseline)) return "Baseline Git commit unavailable";
            string files = Git("diff --name-only " + baseline + " -- Assets Packages ProjectSettings") +
                Git("ls-files --others --exclude-standard -- Assets Packages ProjectSettings");
            string[] risky = files.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                .Where(p => p.EndsWith(".cs") || p.EndsWith(".asmdef") || p.EndsWith(".dll") || p.EndsWith(".so") ||
                    p.EndsWith(".a") || p.EndsWith(".framework") || p == "Packages/manifest.json" || p.StartsWith("ProjectSettings/"))
                .Take(12).ToArray();
            return string.Join(", ", risky);
        }

        internal static bool IsCurrentBaselineCompatible(string folder)
        {
            string path = Path.Combine(StateRoot, "client-" + PlayerSettings.bundleVersion, folder, "code-fingerprint.txt");
            return File.Exists(path) && File.ReadAllText(path) == CodeFingerprint();
        }

        internal static string CodeFingerprint()
        {
            var builder = new StringBuilder();
            foreach (string root in new[] { "Assets", "Packages", "ProjectSettings" })
            {
                string directory = Path.Combine(ProjectRoot, root);
                if (!Directory.Exists(directory)) continue;
                foreach (string file in Directory.GetFiles(directory, "*", SearchOption.AllDirectories).OrderBy(p => p, StringComparer.Ordinal))
                {
                    string name = file.Substring(ProjectRoot.Length).TrimStart(Path.DirectorySeparatorChar).Replace('\\', '/');
                    // Editor-only release tooling is not compiled into a player and must not force a new client baseline.
                    if (name.IndexOf("/Editor/", StringComparison.OrdinalIgnoreCase) >= 0) continue;
                    bool risky = name.EndsWith(".cs", StringComparison.OrdinalIgnoreCase) ||
                        name.EndsWith(".asmdef", StringComparison.OrdinalIgnoreCase) ||
                        name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
                        name.EndsWith(".so", StringComparison.OrdinalIgnoreCase) ||
                        name.EndsWith(".a", StringComparison.OrdinalIgnoreCase) ||
                        name.EndsWith(".framework", StringComparison.OrdinalIgnoreCase) ||
                        name == "Packages/manifest.json" || name == "ProjectSettings/ProjectSettings.asset";
                    if (!risky) continue;
                    // Reuse the length/mtime keyed release hash cache. Keep the
                    // historical Base64 fingerprint format without rereading
                    // unchanged source/plugin files on every release guard.
                    string hex = Metin2ReleasePlanner.FileHash(file);
                    byte[] digest = new byte[hex.Length / 2];
                    for (int i = 0; i < digest.Length; ++i)
                        digest[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
                    builder.Append(name).Append(':').Append(Convert.ToBase64String(digest)).Append('\n');
                }
            }
            Metin2ReleasePlanner.SaveHashCache();
            return builder.ToString();
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Dry-run Publish Current Platform")]
        public static void DryRunPublish()
        {
            string folder = Folder(EditorUserBuildSettings.activeBuildTarget);
            string root = Path.Combine(OutputRoot, folder);
            if (!Directory.Exists(root)) throw new BuildFailedException("No patch output for " + folder);
            PatchFileManifest current = MakeManifest(root);
            string publishedPath = Path.Combine(StateRoot, "client-" + PlayerSettings.bundleVersion, folder, "published-files.json");
            PatchFileManifest previous = File.Exists(publishedPath) ? JsonUtility.FromJson<PatchFileManifest>(File.ReadAllText(publishedPath)) : null;
            Dictionary<string, string> old = previous == null ? new Dictionary<string, string>() : previous.files.ToDictionary(f => f.path, f => f.sha256);
            PatchFile[] changed = current.files.Where(f => !old.TryGetValue(f.path, out string hash) || hash != f.sha256).ToArray();
            Debug.Log("METIN2_PATCH_PUBLISH dry-run platform=" + folder + " files=" + changed.Length + " bytes=" + changed.Sum(f => f.size) +
                " metadata-last=release.json,catalog.json,catalog.hash");
        }
    }
}
