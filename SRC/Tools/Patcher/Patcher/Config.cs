//Copyright © 2016 by Raiin
//Alle Rechte vorbehalten.Hiermit verbiete ich die Weitergabe, das Veröffentlichen und den sonstigen Handel des Codes.

namespace AurigaPatcher
{
    class Config
    {
        public static class Patcher
        {
            // Ezt frissítsd, ha akarsz verzió alapu update-checket is.
            public static string Version = "1.0.0.6";

            public static class Links
            {
                // FONTOS: használj HTTPS-t
                public static string Client = "https://bwmt2global.eu/PATCHER/client/";
                public static string Patchlist = "https://bwmt2global.eu/PATCHER/Patchlist.txt";
                public static string VersionFile = "https://bwmt2global.eu/PATCHER/SelfUpdate2/Version.txt";

                // Self-update exe (amit letölt)
                public static string UpdaterExe = "https://bwmt2global.eu/PATCHER/SelfUpdate2/AurigaPatcher.exe";

                // A hash fájl URL-je (egy sorban a SHA-256, hex)
                // Pl: 2d0aa136c2fcdc30...
                public static string UpdaterExeSha256 = "https://bwmt2global.eu/PATCHER/SelfUpdate2/AurigaPatcher.exe.sha256";
            }

            internal class Settings
            {
                public static string ServerUrl = "https://www.auriga-global.eu/";
                public static string ServerUrlWiki = "https://auror2.com.br/wiki/";
                public static string ServerUrlSupport = "https://auror2.com.br/register";
                public static string ServerUrlFacebook = "https://www.facebook.com/profile.php?id=100074562205846";
                public static string ServerUrlDiscord = "https://discord.gg/JrCtYN9cAW";
                public static string ServerUrlYoutube = "https://discord.gg/Mr3fWCPXZn";
            }

            public static class Names
            {
                public static string Start = "Userinterface.exe";
                public static string Config = "config.exe";
                public static string DX = "Required/dxwebsetup.exe";
                public static string vc_redsist = "Required/VC_redist.x64.exe";
            }
        }
    }
}
