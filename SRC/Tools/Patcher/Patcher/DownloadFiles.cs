using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;
using System.Windows;

namespace AurigaPatcher
{
    class DownloadFiles
    {
        private static readonly HttpClient httpClient = HttpClientFactory.Shared;
        public const double dPWidth = 526;

        private static async Task DownloadFileAsync(string url, string destinationPath, Stopwatch stopwatch)
        {
            // Fejlécek letöltése, itt még él a HttpClient.Timeout
            using var response = await httpClient.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);

            response.EnsureSuccessStatusCode();

            var totalBytes = response.Content.Headers.ContentLength;

            using Stream contentStream = await response.Content.ReadAsStreamAsync();
            using var fileStream = new FileStream(
                destinationPath,
                FileMode.Create,
                FileAccess.Write,
                FileShare.None,
                bufferSize: 81920,
                useAsync: true);

            // 80 KB buffer
            var buffer = new byte[81920];
            long totalRead = 0;
            int bytesRead;

            // UI frissítés "throttling"
            var uiStopwatch = Stopwatch.StartNew();

            // Stall detektálás – ha X másodpercig nem jön adat, timeout
            var lastProgressBytes = 0L;
            var stallWatch = Stopwatch.StartNew();
            const int stallTimeoutSeconds = 30;

            while (true)
            {
                // Stall guard: ha 30 mp-ig nem tudunk olvasni / nincs adat, kilépünk hibával
                var readTask = contentStream.ReadAsync(buffer, 0, buffer.Length);
                var delayTask = Task.Delay(TimeSpan.FromSeconds(stallTimeoutSeconds));

                var completed = await Task.WhenAny(readTask, delayTask);

                if (completed == delayTask)
                {
                    throw new TimeoutException("Download stalled: no data received for 30 seconds.");
                }

                bytesRead = await readTask;
                if (bytesRead == 0)
                    break; // vége a streamnek

                await fileStream.WriteAsync(buffer, 0, bytesRead);
                totalRead += bytesRead;

                // Stall óra reset, ha halad a letöltés
                if (totalRead != lastProgressBytes)
                {
                    lastProgressBytes = totalRead;
                    stallWatch.Restart();
                }

                // UI-t NE frissítsd minden buffer után, csak ~100ms-ként
                if (uiStopwatch.ElapsedMilliseconds >= 100)
                {
                    uiStopwatch.Restart();

                    _ = Application.Current.Dispatcher.BeginInvoke((Action)(() =>
                    {
                        double progress = 0;
                        double width = 0;

                        if (totalBytes.HasValue && totalBytes.Value > 0)
                        {
                            progress = (double)totalRead / totalBytes.Value * 100.0;
                            width = (double)totalRead / totalBytes.Value * dPWidth;
                        }

                        var seconds = stopwatch.ElapsedMilliseconds / 1000.0;
                        var speedKB = seconds > 0 ? (totalRead / seconds) / 1024.0 : 0.0;

                        string speedText;
                        if (speedKB >= 1024)
                        {
                            var speedMB = speedKB / 1024.0;
                            speedText = $"{speedMB:0.00} MB/s";
                        }
                        else
                        {
                            speedText = $"{speedKB:0} KB/s";
                        }

                        MainWindow.WPF.img_pb_full.Width = width;
                        MainWindow.WPF.tb_pb_full.Text =
                            $"{progress:0}% ({Path.GetFileName(destinationPath)})";
                        MainWindow.WPF.tb_pb_speed.Text = speedText;
                    }));
                }
            }
        }


        public static async Task DownloadAllFilesAsync()
        {
            while (MainWindow.FilesHave < MainWindow.FilesNeed)
            {
                var index = MainWindow.FilesHave;
                var fileName = Functions.GetName(MainWindow.Patchlist[index]);
                var destinationPath = Path.Combine(Functions.GetCurrentFolder(), fileName);
                var downloadUrl = Config.Patcher.Links.Client + fileName;

                if (fileName.Contains("\\"))
                {
                    Directory.CreateDirectory(Functions.GetFolder(destinationPath));
                }

                DeveloperConsole.Write.Download("Downloading: " + fileName);

                var stopwatch = Stopwatch.StartNew();

                const int maxRetry = 3;
                int attempt = 0;
                bool success = false;

                while (attempt < maxRetry && !success)
                {
                    attempt++;
                    try
                    {
                        await DownloadFileAsync(downloadUrl, destinationPath, stopwatch);
                        success = true;
                    }
                    catch (TimeoutException tex)
                    {
                        DeveloperConsole.Write.Log(
                            $"Timeout while downloading {fileName} (attempt {attempt}/{maxRetry}): {tex.Message}");
                        if (attempt >= maxRetry)
                            throw; // vagy itt lehet user felé hibaüzenet
                    }
                    catch (Exception ex)
                    {
                        DeveloperConsole.Write.Log(
                            $"Error while downloading {fileName} (attempt {attempt}/{maxRetry}): {ex.Message}");
                        if (attempt >= maxRetry)
                            throw;
                    }
                }

                // Állapot frissítés *csak akkor*, ha sikerült
                MainWindow.BytesHave += Functions.GetSize(MainWindow.Patchlist[index]);
                MainWindow.FilesHave++;

                if (MainWindow.FilesHave == MainWindow.FilesNeed)
                {
                    _ = Application.Current.Dispatcher.BeginInvoke((Action)(() =>
                    {
                        MainWindow.WPF.btn_start.IsEnabled = true;
                        MainWindow.WPF.img_pb_full.Width = dPWidth;
                        MainWindow.WPF.tb_pb_speed.Text = "Finished";
                    }));

                    DeveloperConsole.Write.Log(
                        "All files have been updated successfully: " +
                        MainWindow.FilesHave + "/" + MainWindow.FilesNeed);

                    Start.Start_EXE();
                }
            }
        }


    }
}
