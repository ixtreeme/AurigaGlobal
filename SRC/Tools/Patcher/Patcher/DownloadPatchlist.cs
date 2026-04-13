using System;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using System.Windows;

namespace AurigaPatcher
{
    class DownloadPatchlist
    {
        public static void WC_DownloadPatchlist()
        {
            DeveloperConsole.Write.Download("Download actual: Patchlist.raiin");
            Application.Current.Dispatcher.Invoke(() =>
            {
               
                    MainWindow.WPF.btn_start.IsEnabled = false;

            });
            var WC_NewDownload = new WebClient();
            WC_NewDownload.DownloadProgressChanged += WC_NewDownload_DownloadProgressChanged;
            WC_NewDownload.DownloadFileCompleted += WC_NewDownload_DownloadFileCompleted;
            WC_NewDownload.Proxy = null;
            WC_NewDownload.DownloadFileAsync(new Uri(Config.Patcher.Links.Patchlist), Functions.GetCurrentFolder() + "Patchlist.raiin");
        }

        private static void WC_NewDownload_DownloadProgressChanged(object sender, DownloadProgressChangedEventArgs e)
        {
            //Laden
        }

        private static async void WC_NewDownload_DownloadFileCompleted(object sender, System.ComponentModel.AsyncCompletedEventArgs e)
        {
            DeveloperConsole.Write.Download("Download finished: Patchlist.raiin");
            MainWindow.BytesHave = 0;
            MainWindow.BytesNeed = 0;
            MainWindow.FilesHave = 0;
            MainWindow.FilesNeed = 0;
            DeveloperConsole.Write.Log("Checking Patchlist now: Patchlist.raiin");
            var Time1 = DateTime.Now;

            var patchlistPath = Path.Combine(Functions.GetCurrentFolder(), "Patchlist.raiin");
            var patchlistContent = File.ReadAllText(patchlistPath);

            // 1) NORMÁL PATCHLIST (nem "update")
            if (patchlistContent != "update")
            {
                MainWindow.Patchlist = Functions.GetPatchlist(patchlistPath);
                var Time2 = DateTime.Now;
                DeveloperConsole.Write.Log("Time elapsed: " + (Time2.TimeOfDay - Time1.TimeOfDay));
                DeveloperConsole.Write.Log("Patchlist checked successfully: Patchlist.raiin");

                if (MainWindow.Patchlist.Count > 0)
                {
                    MainWindow.BytesNeed = Functions.GetBytesNeed(MainWindow.Patchlist);
                    MainWindow.FilesNeed = Functions.GetFilesNeed(MainWindow.Patchlist);

                    // **Itt most VÁRJUK a Files_Thread-et, benne a SelfUpdater-rel**
                    await Start.Files_Thread();
                }
                else
                {
                    // **NINCS MIT PATCH-ELNI -> ITT IS SELFUPDATE**
                    DeveloperConsole.Write.Log("Nothing to patch");

                    if (await SelfUpdater.TryUpdateAsync())
                    {
                        // Ha volt új patcher, SelfUpdater már elindította az updatet és Exit-et hívott.
                        return;
                    }

                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        MainWindow.WPF.btn_start.IsEnabled = true;
                    });

                    Start.Start_EXE();
                }
            }
            // 2) "update" - szerver jelzi, hogy kliens/patcher frissítés alatt
            else
            {
                Application.Current.Dispatcher.Invoke(() =>
                {
                    MainWindow.WPF.btn_start.IsEnabled = true;
                });

                File.Delete(patchlistPath);

                DeveloperConsole.Write.Log("Client will be updated...");
                Application.Current.Dispatcher.Invoke(() =>
                {
                    MainWindow.WPF.tb_pb_speed.Text =
                        "Client is currently being modified! Please try again in 5 minutes.";
                });

                // **ITT IS KÖZVETLENÜL, NEM Task.Run-ból hívjuk a SelfUpdater-t**
                if (!await SelfUpdater.TryUpdateAsync())
                {
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        MessageBox.Show(
                            "The patcher will retry once a new version is available. Please restart the patcher in a few minutes.",
                            "Patcher Update",
                            MessageBoxButton.OK,
                            MessageBoxImage.Information);
                    });
                }
            }
        }

    }
}
