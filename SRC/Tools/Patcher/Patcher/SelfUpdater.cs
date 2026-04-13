using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace AurigaPatcher
{
    internal static class SelfUpdater
    {
        private static readonly HttpClient Http = new HttpClient();

        private const string SelfUpdateArgument = "--self-update";
        private const string SelfUpdateTargetArgument = "--update-target";
        private const string SelfUpdateParentPidArgument = "--update-parent-pid";

        // Hivd a Main elején: ha self-update worker, akkor lefut és kilép.
        public static bool TryHandleSelfUpdate(string[] args)
        {
            if (!TryParseSelfUpdateArgs(args, out var options))
                return false;

            try
            {
                RunSelfUpdateWorkerAsync(options).GetAwaiter().GetResult();
            }
            catch (Exception ex)
            {
                DeveloperConsole.Write.Log($"Self-update worker crashed: {ex}");
            }

            Environment.Exit(0);
            return true;
        }

        // Ezt hívd a patcher indulásakor (pl. window load után): ha van új verzió, letölti és újraindit.
        public static async Task<bool> TryUpdateAsync()
        {
            Version latest = await GetLatestVersionAsync().ConfigureAwait(false);
            if (latest == null)
                return false;

            Version current;
            if (!Version.TryParse(Config.Patcher.Version, out current))
            {
                DeveloperConsole.Write.Log($"Invalid current version in Config: {Config.Patcher.Version}");
                return false;
            }

            if (latest <= current)
            {
                DeveloperConsole.Write.Log($"Patcher is up-to-date (current {current}, latest {latest}).");
                return false;
            }

            DeveloperConsole.Write.Log($"New patcher version detected: {latest}");
            await DownloadAndSwapAsync(latest).ConfigureAwait(false);
            return true;
        }

        private static async Task<Version> GetLatestVersionAsync()
        {
            try
            {
                EnsureHttpsEndpoint(Config.Patcher.Links.VersionFile, "VersionFile");

                string s = (await Http.GetStringAsync(Config.Patcher.Links.VersionFile).ConfigureAwait(false)).Trim();
                DeveloperConsole.Write.Log($"Latest version string: \"{s}\"");

                Version v;
                if (Version.TryParse(s, out v))
                    return v;

                DeveloperConsole.Write.Log($"Invalid version string received: {s}");
            }
            catch (Exception ex)
            {
                DeveloperConsole.Write.Log($"Version check failed: {ex.Message}");
            }
            return null;
        }

        private static async Task DownloadAndSwapAsync(Version latestVersion)
        {
            EnsureHttpsEndpoint(Config.Patcher.Links.UpdaterExe, "UpdaterExe");
            EnsureHttpsEndpoint(Config.Patcher.Links.UpdaterExeSha256, "UpdaterExeSha256");

            string currentExePath = Assembly.GetExecutingAssembly().Location;
            string currentExeName = Path.GetFileName(currentExePath);

            string updatesDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "AurigaGlobal");

            Directory.CreateDirectory(updatesDir);

            string tempExePath = Path.Combine(updatesDir, $"{currentExeName}.{latestVersion}.new");

            try
            {
                DeveloperConsole.Write.Log("Downloading latest patcher EXE...");
                await DownloadFileAsync(Config.Patcher.Links.UpdaterExe, tempExePath).ConfigureAwait(false);

                DeveloperConsole.Write.Log("Downloading SHA256 for updater...");
                string expectedSha = await DownloadSha256Async(Config.Patcher.Links.UpdaterExeSha256).ConfigureAwait(false);

                DeveloperConsole.Write.Log("Verifying updater SHA256...");
                string actualSha = ComputeSha256Hex(tempExePath);

                if (!actualSha.Equals(expectedSha, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidOperationException(
                        $"Updater hash mismatch! Expected {expectedSha}, got {actualSha}.");
                }

                DeveloperConsole.Write.Log("Updater verified OK. Launching self-update worker...");

                int parentPid = Process.GetCurrentProcess().Id;

                // Ne rejtve inditsd (AV heurisztika miatt is jobb)
                Process.Start(new ProcessStartInfo
                {
                    FileName = tempExePath,
                    Arguments = $"{SelfUpdateArgument} {SelfUpdateTargetArgument} \"{currentExePath}\" {SelfUpdateParentPidArgument} {parentPid}",
                    UseShellExecute = true,
                });

                // Opcionális: üzenet a usernek
                try
                {
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        MessageBox.Show(
                            "A patcher frissítést talált és újraindul a frissítés befejezéséhez.",
                            "Patcher Update",
                            MessageBoxButton.OK,
                            MessageBoxImage.Information);
                    });
                }
                catch { /* ha nincs UI ready, ignore */ }

                Environment.Exit(0);
            }
            catch (Exception ex)
            {
                DeveloperConsole.Write.Log($"Self-update failed: {ex}");

                try
                {
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        MessageBox.Show(
                            "A patcher nem tudta letölteni/ellenõrizni az új verziót. Próbáld újra késõbb.",
                            "Patcher Update",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    });
                }
                catch { }
            }
        }

        private static async Task DownloadFileAsync(string url, string toPath)
        {
            using (var resp = await Http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead).ConfigureAwait(false))
            {
                resp.EnsureSuccessStatusCode();

                using (var inStream = await resp.Content.ReadAsStreamAsync().ConfigureAwait(false))
                using (var outStream = new FileStream(toPath, FileMode.Create, FileAccess.Write, FileShare.None, 81920, true))
                {
                    byte[] buf = new byte[81920];
                    int read;
                    while ((read = await inStream.ReadAsync(buf, 0, buf.Length).ConfigureAwait(false)) > 0)
                        await outStream.WriteAsync(buf, 0, read).ConfigureAwait(false);
                }
            }
        }

        private static async Task<string> DownloadSha256Async(string url)
        {
            string s = (await Http.GetStringAsync(url).ConfigureAwait(false)) ?? "";
            s = s.Trim();

            // engedjük, ha valaki így menti: "SHA256=xxxx" vagy "xxxx  filename"
            // -> kiszedjük belõle az elsõ 64 hex karaktert
            string hex = ExtractFirstSha256Hex(s);
            if (hex == null)
                throw new InvalidOperationException($"Invalid SHA256 file content: \"{s}\"");

            return hex.ToUpperInvariant();
        }

        private static string ExtractFirstSha256Hex(string s)
        {
            // keressünk 64 db hex karaktert
            if (string.IsNullOrEmpty(s))
                return null;

            StringBuilder current = new StringBuilder(64);
            for (int i = 0; i < s.Length; i++)
            {
                char c = s[i];
                bool isHex =
                    (c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F');

                if (isHex)
                {
                    current.Append(c);
                    if (current.Length == 64)
                        return current.ToString();
                }
                else
                {
                    current.Clear();
                }
            }
            return null;
        }

        private static string ComputeSha256Hex(string filePath)
        {
            using (var sha = SHA256.Create())
            using (var fs = File.OpenRead(filePath))
            {
                var hash = sha.ComputeHash(fs);
                return BitConverter.ToString(hash).Replace("-", "");
            }
        }

        private static void EnsureHttpsEndpoint(string url, string name)
        {
            if (!Uri.TryCreate(url, UriKind.Absolute, out var uri))
                throw new InvalidOperationException($"Invalid {name} URL: {url}");

            if (!string.Equals(uri.Scheme, Uri.UriSchemeHttps, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException($"{name} must use HTTPS: {url}");
        }

        private static bool TryParseSelfUpdateArgs(string[] args, out SelfUpdateOptions options)
        {
            options = null;
            if (args == null || args.Length == 0)
                return false;

            bool hasSelfUpdate = false;
            string targetPath = null;
            int parentPid = 0;

            for (int i = 0; i < args.Length; i++)
            {
                string a = args[i];

                if (string.Equals(a, SelfUpdateArgument, StringComparison.OrdinalIgnoreCase))
                {
                    hasSelfUpdate = true;
                    continue;
                }

                if (string.Equals(a, SelfUpdateTargetArgument, StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    targetPath = args[i + 1];
                    i++;
                    continue;
                }

                if (string.Equals(a, SelfUpdateParentPidArgument, StringComparison.OrdinalIgnoreCase) && i + 1 < args.Length)
                {
                    int pid;
                    if (int.TryParse(args[i + 1], out pid))
                        parentPid = pid;
                    i++;
                    continue;
                }
            }

            if (!hasSelfUpdate || string.IsNullOrWhiteSpace(targetPath) || parentPid <= 0)
                return false;

            options = new SelfUpdateOptions(targetPath, parentPid);
            return true;
        }

        private static async Task RunSelfUpdateWorkerAsync(SelfUpdateOptions options)
        {
            DeveloperConsole.Write.Log("Self-update worker started.");

            // várjuk meg a parent kilépését
            try
            {
                var parent = Process.GetProcessById(options.ParentPid);
                await Task.Run(() => parent.WaitForExit()).ConfigureAwait(false);
            }
            catch { /* már nincs */ }

            string selfPath = Assembly.GetExecutingAssembly().Location;

            bool swapped = await TryReplaceExecutableAsync(selfPath, options.TargetPath).ConfigureAwait(false);
            if (!swapped)
            {
                DeveloperConsole.Write.Log("Self-update worker: replace failed.");
                return;
            }

            // inditsuk az új patchert
            Process.Start(new ProcessStartInfo
            {
                FileName = options.TargetPath,
                UseShellExecute = true
            });
        }

        private static async Task<bool> TryReplaceExecutableAsync(string sourcePath, string targetPath)
        {
            const int maxAttempts = 40;

            string targetDir = Path.GetDirectoryName(targetPath) ?? Environment.CurrentDirectory;
            string staging = Path.Combine(targetDir, Path.GetFileName(targetPath) + ".staging");
            string backup = targetPath + ".bak";

            for (int attempt = 1; attempt <= maxAttempts; attempt++)
            {
                try
                {
                    File.Copy(sourcePath, staging, true);

                    if (File.Exists(targetPath))
                        File.Replace(staging, targetPath, backup, true);
                    else
                        File.Move(staging, targetPath);

                    try { if (File.Exists(staging)) File.Delete(staging); } catch { }
                    return true;
                }
                catch (Exception ex)
                {
                    DeveloperConsole.Write.Log($"Self-update swap retry {attempt}/{maxAttempts}: {ex.Message}");
                    try { if (File.Exists(staging)) File.Delete(staging); } catch { }
                    await Task.Delay(400).ConfigureAwait(false);
                }
            }

            return false;
        }

        private sealed class SelfUpdateOptions
        {
            public SelfUpdateOptions(string targetPath, int parentPid)
            {
                TargetPath = targetPath;
                ParentPid = parentPid;
            }

            public string TargetPath { get; }
            public int ParentPid { get; }
        }
    }
}
