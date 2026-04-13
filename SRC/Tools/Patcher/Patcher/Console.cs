using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace AurigaPatcher
{
    class DeveloperConsole
    {
        #region Variables

        [DllImport("kernel32")]
        static extern bool AllocConsole();

        private static List<string> lLog = new List<string>();
        #endregion

        public static void Create()
        {
            AllocConsole();
        }

        public static class Write
        {
            public static void Download(string Text)
            {
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine(@" -> " + Text);
            }

            public static void Log(string Text)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine(@" -> " + Text);
            }
        }      
    }
}
