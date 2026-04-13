using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

namespace AurigaPatcher
{
    class Start
    {
        public static void Start_EXE()
        {
            string exePath = Functions.GetCurrentFolder() + Config.Patcher.Names.Start;
            const string argument = "KC8nRPUPHbL9rMeamQqr";

            if (File.Exists(exePath))
            {
                Process NewProcess = new Process();
                NewProcess.StartInfo = new ProcessStartInfo
                {
                    FileName = exePath,
                    Arguments = argument,
                    UseShellExecute = false
                };

                try
                {
                    NewProcess.Start();
                    DeveloperConsole.Write.Log(Config.Patcher.Names.Start + " started with argument: " + argument );
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Error starting " + Config.Patcher.Names.Start + ": " + ex.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
            else
            {
                MessageBox.Show(Config.Patcher.Names.Start + " not found", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        public static void Config_EXE()
        {
            if (File.Exists(Functions.GetCurrentFolder() + Config.Patcher.Names.Config))
            {
                Process NewProcess = new Process();
                NewProcess.StartInfo.FileName = Functions.GetCurrentFolder() + Config.Patcher.Names.Config;
                NewProcess.Start();
                DeveloperConsole.Write.Log(Config.Patcher.Names.Config + " started");
            }
            else
            {
                MessageBox.Show(Config.Patcher.Names.Config + " not found", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        public static void dx_EXE()
        {
            if (File.Exists(Functions.GetCurrentFolder() + Config.Patcher.Names.DX))
            {
                Process NewProcess = new Process();
                NewProcess.StartInfo.FileName = Functions.GetCurrentFolder() + Config.Patcher.Names.DX;
                NewProcess.Start();
                DeveloperConsole.Write.Log(Config.Patcher.Names.DX + " started");
            }
            else
            {
                MessageBox.Show(Config.Patcher.Names.DX + " not found", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        public static void vc_EXE()
        {
            if (File.Exists(Functions.GetCurrentFolder() + Config.Patcher.Names.vc_redsist))
            {
                Process NewProcess = new Process();
                NewProcess.StartInfo.FileName = Functions.GetCurrentFolder() + Config.Patcher.Names.vc_redsist;
                NewProcess.Start();
                DeveloperConsole.Write.Log(Config.Patcher.Names.vc_redsist + " started");
            }
            else
            {
                MessageBox.Show(Config.Patcher.Names.vc_redsist + " not found", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        public static void Patchlist_Thread()
        {
            Thread NewThread = new Thread(DownloadPatchlist.WC_DownloadPatchlist);
            NewThread.SetApartmentState(ApartmentState.STA);
            NewThread.Start();
            DeveloperConsole.Write.Log("Patchlist_Thread" + " started");
        }

        public static async Task Files_Thread()
        {
            DeveloperConsole.Write.Log("Files_Thread started");

            if (await SelfUpdater.TryUpdateAsync())
            {
                return;
            }

            try
            {
                await DownloadFiles.DownloadAllFilesAsync();
                DeveloperConsole.Write.Log("Download completed successfully!");
            }
            catch (Exception ex)
            {
                DeveloperConsole.Write.Log($"Download failed: {ex.Message}");
            }
        }
    }
}
