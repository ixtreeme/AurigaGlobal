using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEditor.Build;
using UnityEngine;

namespace Metin2.Editor.Patching
{
    [Serializable] internal sealed class PatchPublisherConfig
    {
        public string protocol, host, username, remoteRoot, publicBaseUrl, privateKeyPath, passwordEnvironmentVariable, credentialFilePath;
        public int port = 22;
    }

    internal static class Metin2PatchPublisher
    {
        private static string ConfigPath => Path.Combine(Metin2PatchManager.ProjectRoot, "PatchPublisher.local.json");

        [MenuItem("Metin2/Patch eszközök (haladó)/Publish Existing Output Current Platform")]
        public static void PublishExistingOutput() => PublishCurrent();

        [MenuItem("Metin2/Patch eszközök (haladó)/Rollback Metadata...")]
        public static void RollbackMetadata()
        {
            string folder = Metin2PatchManager.Folder(EditorUserBuildSettings.activeBuildTarget);
            string history = Path.Combine(Metin2PatchManager.ProjectRoot, "PatchReleaseHistory", folder);
            string selected = EditorUtility.OpenFolderPanel("Select prior patch metadata snapshot", history, "");
            if (string.IsNullOrEmpty(selected)) return;
            selected = Path.GetFullPath(selected);
            if (!selected.StartsWith(Path.GetFullPath(history) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                throw new BuildFailedException("Rollback selection is outside platform history.");
            string releasePath = Path.Combine(selected, "release.json");
            if (!File.Exists(releasePath)) throw new BuildFailedException("Selected release metadata is missing.");
            PatchRelease release = JsonUtility.FromJson<PatchRelease>(File.ReadAllText(releasePath));
            if (release == null || release.platform != folder) throw new BuildFailedException("Rollback platform mismatch.");
            string[] catalog = Directory.GetFiles(selected, "catalog*.json", SearchOption.TopDirectoryOnly);
            string[] hashes = Directory.GetFiles(selected, "catalog*.hash", SearchOption.TopDirectoryOnly);
            if (catalog.Length == 0 || hashes.Length == 0) throw new BuildFailedException("Catalog or hash missing from rollback snapshot.");
            if (!EditorUtility.DisplayDialog("Rollback patch metadata", "Restore " + release.releaseId + " for " + folder + "? Old bundles must still exist on the host.", "Restore", "Cancel"))
                return;
            PatchPublisherConfig config = LoadConfig();
            string remote = config.remoteRoot.TrimEnd('/') + "/" + folder;
            foreach (string file in catalog) Activate(config, selected, remote, Path.GetFileName(file));
            foreach (string file in hashes) Activate(config, selected, remote, Path.GetFileName(file));
            Activate(config, selected, remote, "release.json");
            Verify(folder, "release.json", new FileInfo(releasePath).Length);
            UnityEngine.Debug.Log("METIN2_PATCH_PUBLISH rollback platform=" + folder + " release=" + release.releaseId);
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Full Release/Build + Publish Current Platform")]
        public static void FullAndPublish()
        {
            Metin2PatchManager.Build(false);
            PublishCurrent();
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Content Update/Build + Publish Current Platform")]
        public static void UpdateAndPublish()
        {
            Metin2PatchManager.Build(true);
            PublishCurrent();
        }

        [MenuItem("Metin2/Patch eszközök (haladó)/Content Update/Build + Publish All Platforms")]
        public static void ContentUpdateAll()
        {
            BuildTarget original = EditorUserBuildSettings.activeBuildTarget;
            BuildTarget[] targets = { BuildTarget.StandaloneWindows64, BuildTarget.Android, BuildTarget.iOS };
            try
            {
                // Complete every platform build before any live catalog can be activated.
                foreach (BuildTarget target in targets)
                {
                    if (!EditorUserBuildSettings.SwitchActiveBuildTarget(BuildPipeline.GetBuildTargetGroup(target), target))
                        throw new BuildFailedException("Could not switch build target to " + target);
                    Metin2PatchManager.Build(true);
                }
                foreach (BuildTarget target in targets)
                {
                    if (!EditorUserBuildSettings.SwitchActiveBuildTarget(BuildPipeline.GetBuildTargetGroup(target), target))
                        throw new BuildFailedException("Could not switch build target to " + target);
                    PublishCurrent();
                }
            }
            finally
            {
                if (EditorUserBuildSettings.activeBuildTarget != original)
                    EditorUserBuildSettings.SwitchActiveBuildTarget(BuildPipeline.GetBuildTargetGroup(original), original);
            }
        }

        internal static void PublishCurrent()
        {
            PatchPublisherConfig config = LoadConfig();
            string folder = Metin2PatchManager.Folder(EditorUserBuildSettings.activeBuildTarget);
            string root = Path.Combine(Metin2PatchManager.OutputRoot, folder);
            if (!Directory.Exists(root)) throw new BuildFailedException("Patch output missing: " + root);
            string baseline = Path.Combine(Metin2PatchManager.StateRoot, "client-" + PlayerSettings.bundleVersion, folder);
            string lastManifestPath = Path.Combine(baseline, "published-files.json");
            PatchFileManifest current = Metin2PatchManager.MakeManifest(root);
            if (!current.files.Any(f => f.path.EndsWith(".bundle", StringComparison.OrdinalIgnoreCase)) ||
                !current.files.Any(f => IsCatalog(f.path) && f.path.EndsWith(".json", StringComparison.OrdinalIgnoreCase)) ||
                !current.files.Any(f => IsCatalog(f.path) && f.path.EndsWith(".hash", StringComparison.OrdinalIgnoreCase)))
                throw new BuildFailedException("Patch output lacks bundles, catalog or catalog hash; refusing publish.");
            PatchFileManifest previous = LastPublishedManifest(folder);
            Dictionary<string, string> old = previous == null ? new Dictionary<string, string>() : previous.files.ToDictionary(f => f.path, f => f.sha256);
            PatchFile[] changed = current.files.Where(f => !old.TryGetValue(f.path, out string hash) || hash != f.sha256).ToArray();
            if (string.Equals(config.protocol, "ftps", StringComparison.OrdinalIgnoreCase))
            {
                PublishFtps(config, root, folder, changed);
                ArchiveMetadata(root, folder);
                Directory.CreateDirectory(baseline);
                File.WriteAllText(lastManifestPath, JsonUtility.ToJson(current, true));
                RecordDestination(baseline);
                UnityEngine.Debug.Log("METIN2_PATCH_PUBLISH success platform=" + folder + " changedFiles=" + changed.Length);
                return;
            }
            string remote = config.remoteRoot.TrimEnd('/') + "/" + folder;
            Sftp(config, "-mkdir " + QuoteRemote(remote));
            PatchFile[] bundles = changed.Where(f => f.path.EndsWith(".bundle", StringComparison.OrdinalIgnoreCase)).ToArray();
            PatchFile[] metadata = changed.Except(bundles).Where(f => f.path != "release.json").ToArray();
            foreach (PatchFile file in bundles)
            {
                Upload(config, root, remote, file.path);
                Verify(folder, file.path, file.size);
            }
            // Non-bundle metadata may be required by the new catalog. Upload it before activation.
            foreach (PatchFile file in metadata.Where(f => !IsCatalog(f.path)))
                Upload(config, root, remote, file.path);
            foreach (PatchFile file in metadata.Where(f => IsCatalog(f.path) && !f.path.EndsWith(".hash", StringComparison.OrdinalIgnoreCase)))
            { Activate(config, root, remote, file.path); Verify(folder, file.path, file.size); }
            foreach (PatchFile file in metadata.Where(f => f.path.EndsWith(".hash", StringComparison.OrdinalIgnoreCase)))
            { Activate(config, root, remote, file.path); Verify(folder, file.path, file.size); }
            Activate(config, root, remote, "release.json");
            Verify(folder, "release.json", new FileInfo(Path.Combine(root, "release.json")).Length);
            ArchiveMetadata(root, folder);
            Directory.CreateDirectory(baseline);
            File.WriteAllText(lastManifestPath, JsonUtility.ToJson(current, true));
            RecordDestination(baseline);
            UnityEngine.Debug.Log("METIN2_PATCH_PUBLISH success platform=" + folder + " changedFiles=" + changed.Length +
                " bundleBytes=" + bundles.Sum(f => f.size));
        }

        private static bool IsCatalog(string path) => path.StartsWith("catalog", StringComparison.OrdinalIgnoreCase) &&
            (path.EndsWith(".json", StringComparison.OrdinalIgnoreCase) || path.EndsWith(".hash", StringComparison.OrdinalIgnoreCase));

        internal static void RecordSuccessfulPublication(string folder)
        {
            string root = Path.Combine(Metin2PatchManager.OutputRoot, folder);
            ArchiveMetadata(root, folder);
            string baseline = Path.Combine(Metin2PatchManager.StateRoot, "client-" + PlayerSettings.bundleVersion, folder);
            Directory.CreateDirectory(baseline);
            File.WriteAllText(Path.Combine(baseline, "published-files.json"), JsonUtility.ToJson(Metin2PatchManager.MakeManifest(root), true));
            RecordDestination(baseline);
        }
        [Serializable] sealed class PublishedDestination { public int schema=1;public string identity; }
        internal static string DestinationIdentity(PatchPublisherConfig config)
            =>Metin2ReleasePlanner.Digest((config.protocol??"").ToLowerInvariant()+"|"+(config.host??"").ToLowerInvariant()+"|"+
                config.port+"|"+config.username+"|"+(config.remoteRoot??"").TrimEnd('/')+"|"+(config.publicBaseUrl??"").TrimEnd('/'));
        static void RecordDestination(string baseline)
            =>Metin2ReleaseStateFiles.Write(Path.Combine(baseline,"published-destination.json"),
                JsonUtility.ToJson(new PublishedDestination{identity=DestinationIdentity(LoadConfig())},true));
        internal static PatchFileManifest LastPublishedManifest(string folder)
        {
            if(!Directory.Exists(Metin2PatchManager.StateRoot)||!File.Exists(ConfigPath))return null;
            var config=LoadConfig();string identity=DestinationIdentity(config);
            foreach(string previous in Directory.GetFiles(Metin2PatchManager.StateRoot,"published-files.json",SearchOption.AllDirectories)
                .Where(p=>p.EndsWith(Path.Combine(folder,"published-files.json"),StringComparison.OrdinalIgnoreCase)).OrderByDescending(File.GetLastWriteTimeUtc))
            {
                string baseline=Path.GetDirectoryName(previous),receipt=Path.Combine(baseline,"published-destination.json");
                var manifest=JsonUtility.FromJson<PatchFileManifest>(File.ReadAllText(previous));
                if(manifest?.files==null)continue;
                if(File.Exists(receipt))
                {
                    var destination=JsonUtility.FromJson<PublishedDestination>(File.ReadAllText(receipt));
                    if(destination?.schema==1&&destination.identity==identity)return manifest;
                    continue; // A changed destination must never inherit an old receipt.
                }
                string legacy=Path.Combine(baseline,"published-destination.hash");
                bool unchanged=File.Exists(legacy)?File.ReadAllText(legacy)==Metin2ReleasePlanner.FileHash(ConfigPath,false):
                    File.GetLastWriteTimeUtc(ConfigPath)<=File.GetLastWriteTimeUtc(previous);
                // Migrate old receipts using only published metadata, not thousands of
                // bundle HEAD requests. Missing/mismatching remote metadata fails closed.
                if(!unchanged&&!VerifyLegacyPublishedMetadata(config,folder,manifest))continue;
                RecordDestination(baseline);
                UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_REUSED platform="+folder+" files="+manifest.files.Length);
                return manifest;
            }
            return null;
        }

        static bool VerifyLegacyPublishedMetadata(PatchPublisherConfig config,string folder,PatchFileManifest manifest)
        {
            if(!Uri.TryCreate(config.publicBaseUrl,UriKind.Absolute,out var baseUri)||baseUri.Scheme!="https")return false;
            var metadata=manifest.files.Where(f=>f.path=="release.json"||IsCatalog(f.path)).ToArray();
            if(!metadata.Any(f=>f.path=="release.json")||!metadata.Any(f=>f.path.EndsWith(".hash"))||!metadata.Any(f=>IsCatalog(f.path)&&f.path.EndsWith(".json")))return false;
            try
            {
                foreach(var file in metadata)
                {
                    if(!Regex.IsMatch(file.path,"^[A-Za-z0-9_.-]+$")||!Regex.IsMatch(file.sha256??"","^[a-f0-9]{64}$"))return false;
                    var request=(HttpWebRequest)WebRequest.Create(config.publicBaseUrl.TrimEnd('/')+"/"+folder+"/"+file.path);
                    request.Timeout=15000;request.ReadWriteTimeout=15000;
                    using(var response=(HttpWebResponse)request.GetResponse())
                    using(var sha=System.Security.Cryptography.SHA256.Create())
                    using(var stream=response.GetResponseStream())
                    {
                        if(response.StatusCode!=HttpStatusCode.OK||(response.ContentLength>=0&&response.ContentLength!=file.size))
                        {UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_VERIFY_REFUSED file="+file.path+" reason=response-size");return false;}
                        long bytes=0;var buffer=new byte[65536];int read;
                        while((read=stream.Read(buffer,0,buffer.Length))>0)
                        {
                            bytes+=read;if(bytes>file.size)return false;
                            sha.TransformBlock(buffer,0,read,buffer,0);
                        }
                        sha.TransformFinalBlock(buffer,0,0);
                        string hash=BitConverter.ToString(sha.Hash).Replace("-","").ToLowerInvariant();
                        if(bytes!=file.size||hash!=file.sha256)
                        {UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_VERIFY_REFUSED file="+file.path+" reason=hash-or-size");return false;}
                    }
                }
                UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_METADATA_VERIFIED platform="+folder+" requests="+metadata.Length);
                return true;
            }
            catch(WebException error){UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_VERIFY_REFUSED reason="+error.Status);return false;}
            catch(IOException){UnityEngine.Debug.Log("METIN2_PATCH_HISTORY_VERIFY_REFUSED reason=io");return false;}
        }

        private static void ArchiveMetadata(string root, string folder)
        {
            PatchRelease release = JsonUtility.FromJson<PatchRelease>(File.ReadAllText(Path.Combine(root, "release.json")));
            if (release == null || !Regex.IsMatch(release.releaseId ?? "", "^[0-9-]+$"))
                throw new BuildFailedException("Release ID invalid for history archive.");
            string destination = Path.Combine(Metin2PatchManager.ProjectRoot, "PatchReleaseHistory", folder, release.releaseId);
            Directory.CreateDirectory(destination);
            foreach (string file in Directory.GetFiles(root).Where(p => IsCatalog(Path.GetFileName(p)) || Path.GetFileName(p) == "release.json"))
            {
                string copy = Path.Combine(destination, Path.GetFileName(file));
                if (File.Exists(copy))
                {
                    if (!File.ReadAllBytes(copy).SequenceEqual(File.ReadAllBytes(file)))
                        throw new BuildFailedException("Immutable release history collision: " + copy);
                }
                else File.Copy(file, copy, false);
            }
        }

        private static PatchPublisherConfig LoadConfig()
        {
            if (!File.Exists(ConfigPath)) throw new BuildFailedException("Publisher credentials not configured. Fill PatchPublisher.local.json from the documentation template.");
            PatchPublisherConfig config = JsonUtility.FromJson<PatchPublisherConfig>(File.ReadAllText(ConfigPath));
            if (config == null || !Regex.IsMatch(config.protocol ?? "", "^(sftp|ftps)$", RegexOptions.IgnoreCase) ||
                !Regex.IsMatch(config.host ?? "", "^[A-Za-z0-9.-]+$") ||
                !Regex.IsMatch(config.username ?? "", "^[A-Za-z0-9._-]+$") ||
                !Regex.IsMatch(config.remoteRoot ?? "", "^/[A-Za-z0-9/_.-]+$") ||
                config.port < 1 || config.port > 65535)
                throw new BuildFailedException("Publisher config is incomplete or unsafe. Check protocol, host, user, remoteRoot and port.");
            if (string.Equals(config.protocol, "sftp", StringComparison.OrdinalIgnoreCase) &&
                ((config.privateKeyPath ?? "").IndexOfAny(new[] { '"', '\r', '\n' }) >= 0 ||
                 !File.Exists(config.privateKeyPath ?? "")))
                throw new BuildFailedException("Publisher config is incomplete or unsafe. Check host, user, remoteRoot, port and privateKeyPath.");
            return config;
        }

        private static void PublishFtps(PatchPublisherConfig config, string root, string folder, PatchFile[] changed)
        {
            if (!string.IsNullOrEmpty(config.credentialFilePath))
            {
                string credential = Path.GetFullPath(config.credentialFilePath);
                if (!File.Exists(credential) || credential.StartsWith(Path.Combine(Metin2PatchManager.ProjectRoot, "Assets") + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                    throw new BuildFailedException("FTPS credential file is missing or inside Unity Assets.");
            }
            string script = Path.Combine(Metin2PatchManager.ProjectRoot, "Tools", "Patching", "Publish-Ftps.ps1");
            if (!File.Exists(script)) throw new BuildFailedException("FTPS publisher script is missing.");
            string plan = Path.Combine(Path.GetTempPath(), "metin2-publish-" + Guid.NewGuid().ToString("N") + ".json");
            try
            {
                File.WriteAllText(plan, JsonUtility.ToJson(new PatchFileManifest { files = changed }, true));
                Run("powershell.exe", "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script +
                    "\" -ConfigPath \"" + ConfigPath + "\" -LocalRoot \"" + root + "\" -PlanPath \"" + plan + "\" -Platform " + folder);
            }
            finally { if (File.Exists(plan)) File.Delete(plan); }
        }

        private static string QuoteRemote(string path)
        {
            if (!Regex.IsMatch(path, "^/[A-Za-z0-9/_.,-]+$")) throw new BuildFailedException("Unsafe remote path: " + path);
            return "\"" + path + "\"";
        }

        private static void Upload(PatchPublisherConfig config, string root, string remote, string relative)
        {
            string local = Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar));
            string target = remote + "/" + relative;
            string directory = Path.GetDirectoryName(target).Replace('\\', '/');
            if (directory != remote) Sftp(config, "-mkdir " + QuoteRemote(directory));
            Sftp(config, "put \"" + local.Replace('\\', '/') + "\" " + QuoteRemote(target));
        }

        private static void Activate(PatchPublisherConfig config, string root, string remote, string relative)
        {
            string target = remote + "/" + relative;
            string temporary = target + ".uploading";
            QuoteRemote(target);
            QuoteRemote(temporary);
            string local = Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar));
            Sftp(config, "put \"" + local.Replace('\\', '/') + "\" " + QuoteRemote(temporary));
            // SSH mv replaces one metadata file atomically. Refuse hosts without shell access.
            Run("ssh.exe", "-i \"" + config.privateKeyPath + "\" -p " + config.port +
                " -o BatchMode=yes -o StrictHostKeyChecking=yes " + config.username + "@" + config.host +
                " \"mv -f -- " + temporary + " " + target + "\"");
        }

        private static void Sftp(PatchPublisherConfig config, string command)
        {
            string batch = Path.GetTempFileName();
            try
            {
                File.WriteAllText(batch, command + Environment.NewLine);
                Run("sftp.exe", "-i \"" + config.privateKeyPath + "\" -P " + config.port +
                    " -o BatchMode=yes -o StrictHostKeyChecking=yes -b \"" + batch + "\" " + config.username + "@" + config.host);
            }
            finally { File.Delete(batch); }
        }

        internal static void Run(string executable, string arguments)
        {
            using (var process = new Process { StartInfo = new ProcessStartInfo(executable, arguments) {
                WorkingDirectory = Metin2PatchManager.ProjectRoot, UseShellExecute = false,
                RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true } })
            {
                var watch=Stopwatch.StartNew();
                var activity=new Metin2PublisherActivity(15L*60*1000);
                var stdout = new System.Text.StringBuilder();
                var stderr = new System.Text.StringBuilder();
                process.OutputDataReceived += (_, data) =>
                {
                    if(data.Data==null)return;
                    lock(stdout){stdout.AppendLine(data.Data);if(stdout.Length>65536)stdout.Remove(0,stdout.Length-65536);}
                    activity.Observe(watch.ElapsedMilliseconds);
                    if(data.Data.StartsWith("METIN2_",StringComparison.Ordinal))UnityEngine.Debug.Log(data.Data);
                };
                process.ErrorDataReceived += (_, data) => {if(data.Data!=null){lock(stderr){stderr.AppendLine(data.Data);if(stderr.Length>65536)stderr.Remove(0,stderr.Length-65536);}activity.Observe(watch.ElapsedMilliseconds);}};
                process.Start();
                process.BeginOutputReadLine();process.BeginErrorReadLine();
                try
                {
                    while(!process.WaitForExit(250))
                    {
                        if(activity.IsStalled(watch.ElapsedMilliseconds))
                        {
                            StopProcessTree(process);
                            throw new BuildFailedException("Publisher stalled: no output or transfer progress for 15 minutes: "+executable+" "+OutputTail(stderr)+" "+OutputTail(stdout));
                        }
                        if(!Application.isBatchMode&&EditorUtility.DisplayCancelableProgressBar("Patch kiadása", "Feltöltés / ellenőrzés folyamatban. Eltelt idő: "+watch.Elapsed.ToString(@"hh\:mm\:ss"),0f))
                        {StopProcessTree(process);throw new OperationCanceledException("Kiadás megszakítva; az ellenőrzött feltöltések folytathatók.");}
                    }
                    process.WaitForExit(); // Flush async output after the process exits.
                    if (process.ExitCode != 0)
                        throw new BuildFailedException("Publisher command failed: " + executable + " " + OutputTail(stderr) + " " + OutputTail(stdout));
                }
                finally{if(!Application.isBatchMode)EditorUtility.ClearProgressBar();}
            }
        }

        static void StopProcessTree(Process process)
        {
            if(process.HasExited)return;
            try
            {
                using(var stop=Process.Start(new ProcessStartInfo("taskkill.exe","/PID "+process.Id+" /T /F"){UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true}))
                {stop.BeginOutputReadLine();stop.BeginErrorReadLine();stop.WaitForExit(10000);}
            }
            catch{if(!process.HasExited)process.Kill();}
            if(!process.HasExited)process.Kill();
            process.WaitForExit(10000);
        }

        static string OutputTail(System.Text.StringBuilder output)
        {lock(output){const int limit=4096;return output.Length<=limit?output.ToString():"[earlier process output omitted] "+output.ToString(output.Length-limit,limit);}}

        private static void Verify(string folder, string relative, long expectedBytes)
        {
            string url = Metin2.Runtime.Patching.Metin2PatchConfig.BaseUrl + "/" + folder + "/" + relative;
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.Method = "HEAD";
            request.Timeout = 30000;
            try
            {
                using (var response = (HttpWebResponse)request.GetResponse())
                    CheckResponse(response, url, expectedBytes, false);
            }
            catch (WebException exception) when (exception.Response is HttpWebResponse response &&
                (response.StatusCode == HttpStatusCode.MethodNotAllowed || response.StatusCode == HttpStatusCode.NotImplemented))
            {
                response.Dispose();
                var range = (HttpWebRequest)WebRequest.Create(url);
                range.Method = "GET";
                range.AddRange(0, 0);
                range.Timeout = 30000;
                using (var ranged = (HttpWebResponse)range.GetResponse())
                    CheckResponse(ranged, url, expectedBytes, true);
            }
            UnityEngine.Debug.Log("METIN2_PATCH_VERIFY " + url);
        }

        private static void CheckResponse(HttpWebResponse response, string url, long expectedBytes, bool ranged)
        {
            bool lengthValid = ranged
                ? response.StatusCode == HttpStatusCode.PartialContent &&
                  (response.Headers["Content-Range"] ?? string.Empty).EndsWith("/" + expectedBytes, StringComparison.Ordinal)
                : response.StatusCode == HttpStatusCode.OK &&
                  (response.ContentLength < 0 || response.ContentLength == expectedBytes);
            if (!lengthValid || (response.ContentType ?? string.Empty).StartsWith("text/html", StringComparison.OrdinalIgnoreCase))
                throw new BuildFailedException("HTTPS verification failed: " + url);
        }
    }
}
