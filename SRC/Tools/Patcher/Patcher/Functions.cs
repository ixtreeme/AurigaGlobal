using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Windows;

namespace AurigaPatcher
{
    class Functions
    {
        public static string GetHashRaw(string File_Name)
        {
            try
            {
                using (var md5 = MD5.Create())
                {
                    const int bufferSize = 1024 * 64;
                    using (var stream = new FileStream(
                               File_Name,
                               FileMode.Open,
                               FileAccess.Read,
                               FileShare.Read,
                               bufferSize,
                               FileOptions.SequentialScan))
                    {
                        var bufferedStream = new BufferedStream(stream, bufferSize);
                        var hash = md5.ComputeHash(bufferedStream);
                        return BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetHashRaw Error", MessageBoxButton.OK,MessageBoxImage.Error);
                Environment.Exit(0);
                return "";
            }           
        }

        public static string GetName(string Line)
        {
            try
            {
                return Line.Split('|')[0];

            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetName Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return "";
            }
        }

        public static string GetHash(string Line)
        {
            try
            {
                return Line.Split('|')[1];

            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetHash Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return "";
            }
        }

        public static long GetSize(string Line)
        {
            try
            {
                return long.Parse(Line.Split('|')[2]);

            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetSize Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return 0;
            }
        }

        public static long GetBytesNeed(List<string> Patchlist)
        {
            try
            {
                return Patchlist.Sum(GetSize);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetBytesNeed Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return 0;
            }
        }

        public static int GetFilesNeed(List<string> Patchlist)
        {
            try
            {
                return Patchlist.Count;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetFilesNeed Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return 0;
            }
        }

        public static string GetFolder(string Line)
        {
            try
            {
                var Result = "";
                for (var i = 0; i <= Line.Split('\\').Count() - 2; i++)
                {
                    Result += Line.Split('\\')[i] + "\\";
                }
                return Result.Remove(Result.Length - 1);
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetFolder Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return "";
            }
        }
        

        public static List<string> GetPatchlist(string Patchlist_Name)
        {
            try
            {
                var Result = new List<string>();
                var TMP_Patchlist = File.ReadAllLines(Patchlist_Name);
                File.Delete(Patchlist_Name);
                if (!TMP_Patchlist.Any()) return Result;
                double iHave = 0;
                double iNeed = TMP_Patchlist.Count();

                Application.Current.Dispatcher.Invoke(() => 
                {
                        
                    MainWindow.WPF.IsEnabled = false;
                       

                    Application.Current.Dispatcher.Invoke(() =>
                    {
                            
                        MainWindow.WPF.img_pb_full.Width = 0;
                            

                    });
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                            
                        MainWindow.WPF.tb_pb_speed.Text = "Checking local...";
                           
                    });
                });

                foreach (var i in TMP_Patchlist)
                {
                    if (GetSize(i) != 0)
                    {
                        
                        if (File.Exists(GetCurrentFolder() + GetName(i)))
                        {
                            var expectedSize = GetSize(i);
                            var localPath = GetCurrentFolder() + GetName(i);
                            var localFile = new FileInfo(localPath);
                            if (localFile.Length != expectedSize)
                            {
                                Result.Add(i);
                            }
                            else
                            {
                                var localHash = GetHashRaw(localPath);
                                DeveloperConsole.Write.Log(localHash);
                                if (localHash != GetHash(i))
                                {
                                    Result.Add(i);
                                }
                            }
                        }
                        else
                        {
                            Result.Add(i);

                        }
                    }
                    iHave++;
                    Application.Current.Dispatcher.Invoke(() => 
                    {
                           
                        MainWindow.WPF.img_pb_full.Width = iHave / iNeed * DownloadFiles.dPWidth;
                        MainWindow.WPF.tb_pb_full.Text =
                            GetName(i) + " (" + Math.Round(iHave / iNeed * 100, 0) + "%)";
                            


                    });
                }
                Application.Current.Dispatcher.Invoke(() => {
                        
                    MainWindow.WPF.IsEnabled = true;
                    Application.Current.Dispatcher.Invoke(() => { MainWindow.WPF.img_pb_full.Width = DownloadFiles.dPWidth; });
                    Application.Current.Dispatcher.Invoke(() => { MainWindow.WPF.tb_pb_speed.Text = "Finished"; });
                        
                });
                return Result;
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetPatchlist Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return new List<string>();
            }
        }

        public static string GetCurrentFolder()
        {
            try
            {
                return Directory.GetCurrentDirectory() + "\\";
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.ToString(), "GetCurrentFolder Error", MessageBoxButton.OK, MessageBoxImage.Error);
                Environment.Exit(0);
                return "";
            }
        }
    }
}
