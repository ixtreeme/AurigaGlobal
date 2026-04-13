/**
 * Author: blackdragonx61 / Mali
 * 30.03.2022
 * Modified By TMP4
 * 02.11.2024
**/

#define DARK_MODE

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Metin2Config
{
    public partial class FormConfig : Form
    {
        public FormConfig()
        {
            InitializeComponent();
        }

        [DllImport("user32.dll")]
        public static extern bool EnumDisplaySettings(string deviceName, int modeNum, ref DEVMODE devMode);

        [StructLayout(LayoutKind.Sequential)]
        public struct DEVMODE
        {
            private const int CCHDEVICENAME = 0x20;
            private const int CCHFORMNAME = 0x20;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 0x20)]
            public string dmDeviceName;
            public short dmSpecVersion;
            public short dmDriverVersion;
            public short dmSize;
            public short dmDriverExtra;
            public int dmFields;
            public int dmPositionX;
            public int dmPositionY;
            public ScreenOrientation dmDisplayOrientation;
            public int dmDisplayFixedOutput;
            public short dmColor;
            public short dmDuplex;
            public short dmYResolution;
            public short dmTTOption;
            public short dmCollate;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 0x20)]
            public string dmFormName;
            public short dmLogPixels;
            public int dmBitsPerPel;
            public int dmPelsWidth;
            public int dmPelsHeight;
            public int dmDisplayFlags;
            public int dmDisplayFrequency;
            public int dmICMMethod;
            public int dmICMIntent;
            public int dmMediaType;
            public int dmDitherType;
            public int dmReserved1;
            public int dmReserved2;
            public int dmPanningWidth;
            public int dmPanningHeight;
        }

        public struct SLocale
        {
            public string number;
            public string code;
            public string name;

            public override string ToString()
            {
                return $"{this.number} {this.code} {this.name}";
            }
        }

        public struct SScreenSettings
        {
            public int Width;
            public int Height;
            public List<int> FrequencyList;

            public SScreenSettings(int Width, int Height)
            {
                this.Width = Width;
                this.Height = Height;
                FrequencyList = new List<int>();
            }

            public void AddFrequency(int Frequency)
            {
                if (!FrequencyList.Contains(Frequency))
                    FrequencyList.Add(Frequency);
            }

            public override string ToString()
            {
                return $"{this.Width}x{this.Height}";
            }
        }

        public enum ELocaleType
        {
            LANGUAGE_NUMBER,
            LANGUAGE_CODE,
            LANGUAGE_NAME,

            TYPE_MAX
        }

        private string currentLanguage = "en";

        private List<SScreenSettings> m_ScreenSettingsList = new List<SScreenSettings>();
        private List<SLocale> m_LocaleList = new List<SLocale>();
        private Dictionary<string, string> m_StringDictionary = new Dictionary<string, string>();

        private const string m_ConfigFileName = "metin2.cfg";
        private const string m_LocaleFileName = "locale.cfg";

        private bool m_DarkModeEnabled = false;
        private Color DarkBackColor = Color.FromArgb(22, 22, 24); // Main Form's Dark Back Color

        private string GetString(string key)
        {
#pragma warning disable CS8600 // Converting null literal or possible null value to non-nullable type.
            return m_StringDictionary.TryGetValue(key, out var value) ? value : "";
#pragma warning restore CS8600 // Converting null literal or possible null value to non-nullable type.
        }

        private void InitializeDictionary(string language)
        {
            m_StringDictionary.Clear();

            if (language == "hu")
            {
                m_StringDictionary["CONFIG_STRING_CAPTINO"] = "Metin2 Beállítások";
                m_StringDictionary["CONFIG_STRING_APPLY"] = "OK";
                m_StringDictionary["CONFIG_STRING_CANCLE"] = "Mégsem";
                m_StringDictionary["CONFIG_STRING_GRAPHIC"] = "Kijelző";
                m_StringDictionary["CONFIG_STRING_WINDOWSETTING"] = "Ablak mód";
                m_StringDictionary["CONFIG_STRING_GRAPHICSETTING"] = "Grafikai beállítások";
                m_StringDictionary["CONFIG_STRING_SOUND"] = "Hang";
                m_StringDictionary["CONFIG_STRING_RESOLUTIN"] = "Felbontás";
                m_StringDictionary["CONFIG_STRING_FREQUENCY"] = "Frekvencia";
                m_StringDictionary["CONFIG_STRING_GAMMA"] = "Gamma";
                m_StringDictionary["CONFIG_STRING_WINDOWMODE"] = "Ablakos";
                m_StringDictionary["CONFIG_STRING_FULLSCREEN"] = "Teljes";
                m_StringDictionary["CONFIG_STRING_MUSIC"] = "Zene";
                m_StringDictionary["CONFIG_STRING_SOUNDEFFECT"] = "Effektek";
                m_StringDictionary["CONFIG_STRING_SIGHT"] = "Köd";
                m_StringDictionary["CONFIG_STRING_SHADOW"] = "Árnyékok";
                m_StringDictionary["CONFIG_STRING_NONE"] = "Semmi";
                m_StringDictionary["CONFIG_STRING_BG"] = "Háttér";
                m_StringDictionary["CONFIG_STRING_BGSELF"] = "Háttér + karakter";
                m_StringDictionary["CONFIG_STRING_ALL"] = "Minden";
                m_StringDictionary["CONFIG_STRING_SPEEDLOW"] = "Ez a beállítás lassíthatja a játékot!";
                m_StringDictionary["CONFIG_STRING_NOTIC"] = "Figyelmeztetés";
                m_StringDictionary["CONFIG_STRING_SELECTCPU"] = "A CPU tiling mód lehetővé teszi a játék futtatását alacsonyabb rendszer-összetevőkön. Hiba esetén válaszd a GPU tiling módot.";
                m_StringDictionary["CONFIG_STRING_SELECTGPU"] = "Alacsony rendszerkövetelmények esetén a GPU tiling mód lassítja a számítógépet. Hiba esetén választ a CPU tiling módot.";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE"] = "Éjszakai mód";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_FOG_MODE"] = "Köd";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE"] = "Hóesés";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE"] = "Havas textúra";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_LANGUAGE"] = "Nyelv";
                m_StringDictionary["CONFIG_STRING_EFFECT"] = "Effektek";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL1"] = "Mindent mutat";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL2"] = "Saját & Szörnyeké";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL3"] = "Saját & Másoké";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL4"] = "Saját";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL5"] = "Egyik sem";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP"] = "Magán boltok";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL1"] = "Mindent mutat";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL2"] = "Nagy hatókör";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL3"] = "Közepes hatókör";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL4"] = "Kis hatókör";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL5"] = "Közvetlen közel";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM"] = "Tárgy dobás";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL1"] = "Effekt & Név";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL2"] = "Effekt elrejtése";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL3"] = "Név elrejtése";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL4"] = "Csak kurzorra";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL5"] = "Mindent elrejt";
                m_StringDictionary["CONFIG_STRING_PET"] = "Kedvencek";
                m_StringDictionary["CONFIG_STRING_PET_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_PET_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_NPC_NAME"] = "NPC nevek";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_ON"] = "Be";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_OFF"] = "Ki";
                m_StringDictionary["CONFIG_STRING_SHADOW_QUALITY"] = "Árnyékminőség";
                m_StringDictionary["CONFIG_STRING_SHADOW_TARGET"] = "Árnyék szintje";
                m_StringDictionary["CONFIG_STRING_BAD"] = "Alacsony";
                m_StringDictionary["CONFIG_STRING_AVERAGE"] = "Közepes";
                m_StringDictionary["CONFIG_STRING_GOOD"] = "Magas";
                m_StringDictionary["CONFIG_STRING_EN"] = "Angol";
                m_StringDictionary["CONFIG_STRING_DE"] = "Német";
                m_StringDictionary["CONFIG_STRING_HU"] = "Magyar";
            }
            else if (language == "de")
            {
                m_StringDictionary["CONFIG_STRING_CAPTINO"] = "Metin2 Einstellungen";
                m_StringDictionary["CONFIG_STRING_APPLY"] = "OK";
                m_StringDictionary["CONFIG_STRING_CANCLE"] = "Abbrechen";
                m_StringDictionary["CONFIG_STRING_GRAPHIC"] = "Anzeige";
                m_StringDictionary["CONFIG_STRING_WINDOWSETTING"] = "Fenstermodus";
                m_StringDictionary["CONFIG_STRING_GRAPHICSETTING"] = "Grafikeinstellungen";
                m_StringDictionary["CONFIG_STRING_SOUND"] = "Ton";
                m_StringDictionary["CONFIG_STRING_RESOLUTIN"] = "Auflösung";
                m_StringDictionary["CONFIG_STRING_FREQUENCY"] = "Frequenz";
                m_StringDictionary["CONFIG_STRING_GAMMA"] = "Gamma";
                m_StringDictionary["CONFIG_STRING_WINDOWMODE"] = "Fenster";
                m_StringDictionary["CONFIG_STRING_FULLSCREEN"] = "Vollbild";
                m_StringDictionary["CONFIG_STRING_MUSIC"] = "Musik";
                m_StringDictionary["CONFIG_STRING_SOUNDEFFECT"] = "Effekte";
                m_StringDictionary["CONFIG_STRING_SIGHT"] = "Nebel";
                m_StringDictionary["CONFIG_STRING_SHADOW"] = "Schatten";
                m_StringDictionary["CONFIG_STRING_NONE"] = "Nichts";
                m_StringDictionary["CONFIG_STRING_BG"] = "Hintergrund";
                m_StringDictionary["CONFIG_STRING_BGSELF"] = "Hintergrund + Charakter";
                m_StringDictionary["CONFIG_STRING_ALL"] = "Alles";
                m_StringDictionary["CONFIG_STRING_SPEEDLOW"] = "Diese Einstellung kann das Spiel verlangsamen!";
                m_StringDictionary["CONFIG_STRING_NOTIC"] = "Warnung";
                m_StringDictionary["CONFIG_STRING_SELECTCPU"] = "Der CPU-Tiling-Modus ermöglicht das Spielen auf Systemen mit geringeren Anforderungen. Bei Fehlern wählen Sie den GPU-Tiling-Modus.";
                m_StringDictionary["CONFIG_STRING_SELECTGPU"] = "Bei niedrigeren Systemanforderungen verlangsamt der GPU-Tiling-Modus den Computer. Bei Fehlern wählen Sie den CPU-Tiling-Modus.";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE"] = "Nachtmodus";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_FOG_MODE"] = "Nebel";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE"] = "Schneefall";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE"] = "Schneetextur";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_LANGUAGE"] = "Sprache";
                m_StringDictionary["CONFIG_STRING_EFFECT"] = "Effekte";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL1"] = "Alles anzeigen";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL2"] = "Nur eigene & Monster";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL3"] = "Nur eigene & Andere";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL4"] = "Nur eigene";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL5"] = "Keine";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP"] = "Privatläden";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL1"] = "Alles anzeigen";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL2"] = "Großer Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL3"] = "Mittlerer Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL4"] = "Kleiner Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL5"] = "Nur nah";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM"] = "Gegenstand fallen lassen";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL1"] = "Effekt & Name";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL2"] = "Effekt ausblenden";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL3"] = "Name ausblenden";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL4"] = "Nur bei Mauszeiger";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL5"] = "Alles ausblenden";
                m_StringDictionary["CONFIG_STRING_PET"] = "Haustiere";
                m_StringDictionary["CONFIG_STRING_PET_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_PET_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_NPC_NAME"] = "NPC-Namen";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_ON"] = "Ein";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_OFF"] = "Aus";
                m_StringDictionary["CONFIG_STRING_SHADOW_QUALITY"] = "Schattenqualität";
                m_StringDictionary["CONFIG_STRING_SHADOW_TARGET"] = "Schattenstufe";
                m_StringDictionary["CONFIG_STRING_BAD"] = "Niedrig";
                m_StringDictionary["CONFIG_STRING_AVERAGE"] = "Mittel";
                m_StringDictionary["CONFIG_STRING_GOOD"] = "Hoch";
                m_StringDictionary["CONFIG_STRING_EN"] = "Englisch";
                m_StringDictionary["CONFIG_STRING_DE"] = "Deutsch";
                m_StringDictionary["CONFIG_STRING_HU"] = "Ungarisch";
            }
            else // "en"
            {
                m_StringDictionary["CONFIG_STRING_CAPTINO"] = "Metin2 Settings";
                m_StringDictionary["CONFIG_STRING_APPLY"] = "OK";
                m_StringDictionary["CONFIG_STRING_CANCLE"] = "Cancel";
                m_StringDictionary["CONFIG_STRING_GRAPHIC"] = "Display";
                m_StringDictionary["CONFIG_STRING_WINDOWSETTING"] = "Window Mode";
                m_StringDictionary["CONFIG_STRING_GRAPHICSETTING"] = "Graphic Settings";
                m_StringDictionary["CONFIG_STRING_SOUND"] = "Sound";
                m_StringDictionary["CONFIG_STRING_RESOLUTIN"] = "Resolution";
                m_StringDictionary["CONFIG_STRING_FREQUENCY"] = "Frequency";
                m_StringDictionary["CONFIG_STRING_GAMMA"] = "Gamma";
                m_StringDictionary["CONFIG_STRING_WINDOWMODE"] = "Windowed";
                m_StringDictionary["CONFIG_STRING_FULLSCREEN"] = "Fullscreen";
                m_StringDictionary["CONFIG_STRING_MUSIC"] = "Music";
                m_StringDictionary["CONFIG_STRING_SOUNDEFFECT"] = "Effects";
                m_StringDictionary["CONFIG_STRING_SIGHT"] = "Fog";
                m_StringDictionary["CONFIG_STRING_SHADOW"] = "Shadows";
                m_StringDictionary["CONFIG_STRING_NONE"] = "None";
                m_StringDictionary["CONFIG_STRING_BG"] = "Background";
                m_StringDictionary["CONFIG_STRING_BGSELF"] = "Background + Character";
                m_StringDictionary["CONFIG_STRING_ALL"] = "All";
                m_StringDictionary["CONFIG_STRING_SPEEDLOW"] = "This setting may slow down the game!";
                m_StringDictionary["CONFIG_STRING_NOTIC"] = "Warning";
                m_StringDictionary["CONFIG_STRING_SELECTCPU"] = "CPU tiling mode allows the game to run on lower-end systems. If there is an issue, select GPU tiling mode.";
                m_StringDictionary["CONFIG_STRING_SELECTGPU"] = "On low-end systems, GPU tiling mode may slow down the computer. In case of issues, select CPU tiling mode.";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE"] = "Night Mode";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_NIGHT_MODE_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_FOG_MODE"] = "Fog";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_FOG_MODE_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE"] = "Snowfall";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_SNOW_MODE_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE"] = "Snow Texture";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_SNOW_TEXTURE_MODE_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_LANGUAGE"] = "Language";
                m_StringDictionary["CONFIG_STRING_EFFECT"] = "Effects";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL1"] = "Show All";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL2"] = "Self & Monsters";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL3"] = "Self & Others";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL4"] = "Self";
                m_StringDictionary["CONFIG_STRING_EFFECT_LEVEL5"] = "None";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP"] = "Private Shops";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL1"] = "Show All";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL2"] = "Large Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL3"] = "Medium Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL4"] = "Small Radius";
                m_StringDictionary["CONFIG_STRING_PRIVATE_SHOP_LEVEL5"] = "Close Range";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM"] = "Item Drop";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL1"] = "Effect & Name";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL2"] = "Hide Effect";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL3"] = "Hide Name";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL4"] = "Cursor Only";
                m_StringDictionary["CONFIG_STRING_DROP_ITEM_LEVEL5"] = "Hide All";
                m_StringDictionary["CONFIG_STRING_PET"] = "Pets";
                m_StringDictionary["CONFIG_STRING_PET_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_PET_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_NPC_NAME"] = "NPC Names";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_ON"] = "On";
                m_StringDictionary["CONFIG_STRING_NPC_NAME_OFF"] = "Off";
                m_StringDictionary["CONFIG_STRING_SHADOW_QUALITY"] = "Shadow Quality";
                m_StringDictionary["CONFIG_STRING_SHADOW_TARGET"] = "Shadow Level";
                m_StringDictionary["CONFIG_STRING_BAD"] = "Low";
                m_StringDictionary["CONFIG_STRING_AVERAGE"] = "Medium";
                m_StringDictionary["CONFIG_STRING_GOOD"] = "High";
                m_StringDictionary["CONFIG_STRING_EN"] = "English";
                m_StringDictionary["CONFIG_STRING_DE"] = "German";
                m_StringDictionary["CONFIG_STRING_HU"] = "Hungarian";
            }
        }

        private void SetStrings()
        {
            this.Text = GetString("CONFIG_STRING_CAPTINO");
            if (this.Text == "")
                this.Text = " ";

            btnOK.Text = GetString("CONFIG_STRING_APPLY");
            btnCANCEL.Text = GetString("CONFIG_STRING_CANCLE");

            gboxDisplaySettings.Text = GetString("CONFIG_STRING_GRAPHIC");
            lblResolution.Text = GetString("CONFIG_STRING_RESOLUTIN");
            lblFrequency.Text = GetString("CONFIG_STRING_FREQUENCY");
            lblGamma.Text = GetString("CONFIG_STRING_GAMMA");

            gboxSoundSettings.Text = GetString("CONFIG_STRING_SOUND");
            lblBGM.Text = GetString("CONFIG_STRING_MUSIC");
            lblSFX.Text = GetString("CONFIG_STRING_SOUNDEFFECT");

            gboxNightMode.Text = GetString("CONFIG_STRING_NIGHT_MODE");
            rbtnNightOn.Text = GetString("CONFIG_STRING_NIGHT_MODE_ON");
            rbtnNightOff.Text = GetString("CONFIG_STRING_NIGHT_MODE_OFF");

            gboxFogMode.Text = GetString("CONFIG_STRING_FOG_MODE");
            rbtnFogOn.Text = GetString("CONFIG_STRING_FOG_MODE_ON");
            rbtnFogOff.Text = GetString("CONFIG_STRING_FOG_MODE_OFF");

            gboxSnowMode.Text = GetString("CONFIG_STRING_SNOW_MODE");
            rbtnSnowOn.Text = GetString("CONFIG_STRING_SNOW_MODE_ON");
            rbtnSnowOff.Text = GetString("CONFIG_STRING_SNOW_MODE_OFF");

            gboxSnowTextureMode.Text = GetString("CONFIG_STRING_SNOW_TEXTURE_MODE");
            rbtnSnowTextureOn.Text = GetString("CONFIG_STRING_SNOW_TEXTURE_MODE_ON");
            rbtnSnowTextureOff.Text = GetString("CONFIG_STRING_SNOW_TEXTURE_MODE_OFF");

            gboxPetMode.Text = GetString("CONFIG_STRING_PET");
            rbtnPetOn.Text = GetString("CONFIG_STRING_PET_ON");
            rbtnPetOff.Text = GetString("CONFIG_STRING_PET_OFF");

            gboxLanguage.Text = GetString("CONFIG_STRING_LANGUAGE");

            gboxWindowSettings.Text = GetString("CONFIG_STRING_WINDOWSETTING");
            rbtnWindowMode.Text = GetString("CONFIG_STRING_WINDOWMODE");
            rbtnFullscreen.Text = GetString("CONFIG_STRING_FULLSCREEN");

            gboxGraphicSettings.Text = GetString("CONFIG_STRING_GRAPHICSETTING");
            lblShadowTarget.Text = GetString("CONFIG_STRING_SHADOW_TARGET");
            lblShadowQuality.Text = GetString("CONFIG_STRING_SHADOW_QUALITY");
            lblEffects.Text = GetString("CONFIG_STRING_EFFECT");
            lblPrivateShop.Text = GetString("CONFIG_STRING_PRIVATE_SHOP");
            lblDropItem.Text = GetString("CONFIG_STRING_DROP_ITEM");

            gboxNPCName.Text = GetString("CONFIG_STRING_NPC_NAME");
            rbtnNPCNameOn.Text = GetString("CONFIG_STRING_NPC_NAME_ON");
            rbtnNPCNameOff.Text = GetString("CONFIG_STRING_NPC_NAME_OFF");

            cboxTargetShadow.Items[0] = GetString("CONFIG_STRING_NONE");
            cboxTargetShadow.Items[1] = GetString("CONFIG_STRING_BG");
            cboxTargetShadow.Items[2] = GetString("CONFIG_STRING_BGSELF");
            cboxTargetShadow.Items[3] = GetString("CONFIG_STRING_ALL");

            cboxShadowQuality.Items[0] = GetString("CONFIG_STRING_BAD");
            cboxShadowQuality.Items[1] = GetString("CONFIG_STRING_AVERAGE");
            cboxShadowQuality.Items[2] = GetString("CONFIG_STRING_GOOD");

            for (int i = 0; i < cboxEffect.Items.Count; i++)
                cboxEffect.Items[i] = GetString($"CONFIG_STRING_EFFECT_LEVEL{i + 1}");

            for (int i = 0; i < cboxPrivateShop.Items.Count; i++)
                cboxPrivateShop.Items[i] = GetString($"CONFIG_STRING_PRIVATE_SHOP_LEVEL{i + 1}");

            for (int i = 0; i < cboxDropItem.Items.Count; i++)
                cboxDropItem.Items[i] = GetString($"CONFIG_STRING_DROP_ITEM_LEVEL{i + 1}");
        }

        private void LoadLocaleList()
        {
            var localeObjEN = new SLocale
            {
                number = "10002",
                code = "1252",
                name = "en"
            };
            m_LocaleList.Add(localeObjEN);
            cboxLocale.Items.Add(GetString("CONFIG_STRING_EN"));

            var localeObjDE = new SLocale
            {
                number = "10021",
                code = "1252",
                name = "de"
            };
            m_LocaleList.Add(localeObjDE);
            cboxLocale.Items.Add(GetString("CONFIG_STRING_DE"));

            var localeObjHU = new SLocale
            {
                number = "10000",
                code = "1250",
                name = "hu"
            };
            m_LocaleList.Add(localeObjHU);
            cboxLocale.Items.Add(GetString("CONFIG_STRING_HU"));
        }

        private void SetCurrentLanguage()
        {
            if (File.Exists(m_LocaleFileName))
            {
                var lines = File.ReadAllLines(m_LocaleFileName);
                foreach (var line in lines)
                {
                    var arrLocale = line.Split(' ');
                    if (arrLocale.Length < ((int)ELocaleType.TYPE_MAX - 1))
                        continue;

                    var localeName = arrLocale[(int)ELocaleType.LANGUAGE_NAME];
                    if (!String.IsNullOrEmpty(localeName))
                    {
                        currentLanguage = localeName;
                    }
                }
            }
        }

        private void LoadSettings()
        {
            cboxTargetShadow.SelectedIndex = (cboxTargetShadow.Items.Count - 1);
            cboxShadowQuality.SelectedIndex = (cboxShadowQuality.Items.Count - 1);
            cboxEffect.SelectedIndex = 0;
            cboxPrivateShop.SelectedIndex = 0;
            cboxDropItem.SelectedIndex = 0;
            cboxGamma.SelectedIndex = 2;

            if (File.Exists(m_ConfigFileName))
            {
                try
                {
                    var TempScreenSettings = new SScreenSettings(-1, -1);

                    var lines = File.ReadAllLines(m_ConfigFileName);
                    foreach (var line in lines)
                    {
                        if (line == "")
                            continue;

                        var Key = line.Substring(0, line.IndexOf('\t'));
                        var Value = line.Substring(line.LastIndexOf('\t') + 1);

                        switch (Key)
                        {
                            case "WIDTH":
                                TempScreenSettings.Width = Convert.ToInt32(Value);
                                break;

                            case "HEIGHT":
                                TempScreenSettings.Height = Convert.ToInt32(Value);
                                break;

                            case "MUSIC_VOLUME":
                                if (Value.Contains('.'))
                                    trbarBGM.Value = (int)(decimal.Parse(Value, System.Globalization.CultureInfo.InvariantCulture) * trbarBGM.Maximum);
                                else
                                    trbarBGM.Value = Convert.ToByte(Value) * 200;
                                trbarBGM_Scroll(trbarBGM, null);
                                break;

                            case "VOICE_VOLUME":
                                trbarSFX.Value = Convert.ToByte(Value) * 10;
                                trbarSFX_Scroll(trbarSFX, null);
                                break;

                            case "GAMMA":
                                cboxGamma.SelectedIndex = (Convert.ToByte(Value) - 1);
                                break;

                            case "NIGHT_MODE_ON":
                                rbtnNightOff.Checked = (Value == "0");
                                rbtnNightOn.Checked = (Value == "1");
                                break;

                            case "FOG_MODE_ON":
                                rbtnFogOff.Checked = (Value == "0");
                                rbtnFogOn.Checked = (Value == "1");
                                break;

                            case "SNOW_MODE_ON":
                                rbtnSnowOff.Checked = (Value == "0");
                                rbtnSnowOn.Checked = (Value == "1");
                                break;

                            case "SNOW_TEXTURE_MODE":
                                rbtnSnowTextureOff.Checked = (Value == "0");
                                rbtnSnowTextureOn.Checked = (Value == "1");
                                break;

                            case "WINDOWED":
                                rbtnWindowMode.Checked = (Value == "1");
                                rbtnFullscreen.Checked = (Value == "0");
                                break;

                            case "SHADOW_QUALITY_LEVEL":
                                cboxShadowQuality.SelectedIndex = Convert.ToByte(Value);
                                break;

                            case "SHADOW_TARGET_LEVEL":
                                cboxTargetShadow.SelectedIndex = Convert.ToByte(Value);
                                break;

                            case "EFFECT_LEVEL":
                                cboxEffect.SelectedIndex = Convert.ToByte(Value);
                                break;

                            case "PRIVATE_SHOP_LEVEL":
                                cboxPrivateShop.SelectedIndex = Convert.ToByte(Value);
                                break;

                            case "DROP_ITEM_LEVEL":
                                cboxDropItem.SelectedIndex = Convert.ToByte(Value);
                                break;

                            case "PET_STATUS":
                                // Swap 0,1 if your client handles them in the other way!
                                rbtnPetOff.Checked = (Value == "1");
                                rbtnPetOn.Checked = (Value == "0");
                                break;

                            case "NPC_NAME_STATUS":
                                // Swap 0,1 if your client handles them in the other way!
                                rbtnNPCNameOff.Checked = (Value == "1");
                                rbtnNPCNameOn.Checked = (Value == "0");
                                break;

#if DARK_MODE
                            case "CONFIG_DARK_MODE":
                                if (Value == "1")
                                    pboxDarkMode_Click(pboxDarkMode, null);
                                break;
#endif
                        }
                    }

                    int idx = m_ScreenSettingsList.FindIndex(x =>
                        x.Width == TempScreenSettings.Width &&
                        x.Height == TempScreenSettings.Height);

                    if (idx != -1)
                        cboxResolution.SelectedIndex = idx;
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Error");
                }
            }
            if (File.Exists(m_LocaleFileName))
            {
                var lines = File.ReadAllLines(m_LocaleFileName);
                foreach (var line in lines)
                {
                    var arrLocale = line.Split(' ');
                    if (arrLocale.Length < ((int)ELocaleType.TYPE_MAX - 1))
                        continue;

                    var localeName = arrLocale[(int)ELocaleType.LANGUAGE_NAME];
                    int idx = m_LocaleList.FindIndex(x => x.name == localeName);
                    if (idx != -1)
                    {
                        cboxLocale.SelectedIndex = idx;
                        break;
                    }
                }
            }
        }

        private void GetScreenSettings()
        {
            DEVMODE vDevMode = new DEVMODE();
            int i = 0;
#pragma warning disable CS8625 // Cannot convert null literal to non-nullable reference type.
            while (EnumDisplaySettings(null, i++, ref vDevMode))
            {
                if (vDevMode.dmPelsWidth < 800 && vDevMode.dmPelsHeight < 600)
                    continue;

                int index = m_ScreenSettingsList.FindIndex(x =>
                    x.Width == vDevMode.dmPelsWidth &&
                    x.Height == vDevMode.dmPelsHeight);

                SScreenSettings s = (index != -1) ? m_ScreenSettingsList[index]
                    : new SScreenSettings(vDevMode.dmPelsWidth, vDevMode.dmPelsHeight);

                s.AddFrequency(vDevMode.dmDisplayFrequency);

                if (index == -1)
                    m_ScreenSettingsList.Add(s);
            }
#pragma warning restore CS8625 // Cannot convert null literal to non-nullable reference type.

            foreach (SScreenSettings s in m_ScreenSettingsList)
                cboxResolution.Items.Add(s.ToString());

            if (cboxResolution.Items.Count > 0)
                cboxResolution.SelectedIndex = 0;
        }

        private void FormConfig_Load(object sender, EventArgs e)
        {
#if DARK_MODE
            pboxDarkMode.Show();
#endif
            SetCurrentLanguage();
            InitializeDictionary(currentLanguage);
            LoadLocaleList();
            SetStrings();
            GetScreenSettings();
            LoadSettings();
        }

        private void btnCANCEL_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void btnOK_Click(object sender, EventArgs e)
        {
            using (var configFile = new StreamWriter(m_ConfigFileName))
            {
                configFile.WriteLine($"WIDTH\t\t\t\t\t\t{m_ScreenSettingsList.ElementAt(cboxResolution.SelectedIndex).Width}");
                configFile.WriteLine($"HEIGHT\t\t\t\t\t\t{m_ScreenSettingsList.ElementAt(cboxResolution.SelectedIndex).Height}");
                configFile.WriteLine($"FREQUENCY\t\t\t\t\t{cboxFrequency.Text}");
                configFile.WriteLine($"OBJECT_CULLING\t\t\t\t1"); // default
                configFile.WriteLine($"MUSIC_VOLUME\t\t\t\t{((double)trbarBGM.Value / trbarBGM.Maximum).ToString("F3", System.Globalization.CultureInfo.InvariantCulture)}");
                configFile.WriteLine($"VOICE_VOLUME\t\t\t\t{tboxSFX.Text}");
                configFile.WriteLine($"GAMMA\t\t\t\t\t\t{cboxGamma.Text}");
                configFile.WriteLine($"IS_SAVE_ID\t\t\t\t\t0"); // default
                configFile.WriteLine($"SAVE_ID\t\t\t\t\t\t0"); // default
                configFile.WriteLine($"PRE_LOADING_DELAY_TIME\t\t20"); // default
                configFile.WriteLine($"DECOMPRESSED_TEXTURE\t\t0"); // default
                configFile.WriteLine($"NIGHT_MODE_ON\t\t\t\t{(rbtnNightOn.Checked ? 1 : 0)}");
                configFile.WriteLine($"FOG_MODE_ON\t\t\t\t\t{(rbtnFogOn.Checked ? 1 : 0)}");
                configFile.WriteLine($"SNOW_MODE_ON\t\t\t\t{(rbtnSnowOn.Checked ? 1 : 0)}");
                configFile.WriteLine($"SNOW_TEXTURE_MODE\t\t\t{(rbtnSnowTextureOn.Checked ? 1 : 0)}");
                configFile.WriteLine($"WINDOWED\t\t\t\t\t{(rbtnWindowMode.Checked ? 1 : 0)}");
                configFile.WriteLine($"SHOW_MOBLEVEL\t\t\t\t1"); // default
                configFile.WriteLine($"SHOW_MOBAIFLAG\t\t\t\t1"); // default
                configFile.WriteLine($"CHAT_FILTER_DICE\t\t\t1"); // default
                configFile.WriteLine($"SHADOW_QUALITY_LEVEL\t\t{cboxShadowQuality.SelectedIndex}");
                configFile.WriteLine($"SHADOW_TARGET_LEVEL\t\t\t{cboxTargetShadow.SelectedIndex}");
                configFile.WriteLine($"EFFECT_LEVEL\t\t\t\t{cboxEffect.SelectedIndex}");
                configFile.WriteLine($"PRIVATE_SHOP_LEVEL\t\t\t{cboxPrivateShop.SelectedIndex}");
                configFile.WriteLine($"DROP_ITEM_LEVEL\t\t\t\t{cboxDropItem.SelectedIndex}");
                configFile.WriteLine($"PET_STATUS\t\t\t\t\t{(rbtnPetOn.Checked ? 0 : 1)}"); // Swap 0,1 if your client handles them in the other way!
                configFile.WriteLine($"NPC_NAME_STATUS\t\t\t\t{(rbtnNPCNameOn.Checked ? 0 : 1)}"); // Swap 0,1 if your client handles them in the other way!
                configFile.WriteLine($"EVENT_BANNER_FLAG\t\t\t0"); // default
#if DARK_MODE
                configFile.WriteLine($"CONFIG_DARK_MODE\t\t\t{(m_DarkModeEnabled ? 1 : 0)}"); // default
#endif
            }

            if (cboxLocale.SelectedIndex != -1 && cboxLocale.SelectedIndex < m_LocaleList.Count)
            {
                var locale = m_LocaleList.ElementAt(cboxLocale.SelectedIndex);
                using (var localeFile = new StreamWriter(m_LocaleFileName))
                    localeFile.Write(locale.ToString());
            }

            btnCANCEL.PerformClick();
        }

        private void ChangeColors(Control parent)
        {
            foreach (Control c in parent.Controls)
            {
                if (c is GroupBox || c is CheckBox)
                    c.ForeColor = m_DarkModeEnabled ? SystemColors.ControlText : Color.White;

                ChangeColors(c);
            }
        }

        private void pboxDarkMode_Click(object sender, EventArgs e)
        {
            if (m_DarkModeEnabled)
            {
                BackColor = SystemColors.Control;
                pboxDarkMode.Image = global::Metin2Config.Properties.Resources.night_off;
            }
            else
            {
                BackColor = DarkBackColor;
                pboxDarkMode.Image = global::Metin2Config.Properties.Resources.night_on;
            }

            ChangeColors(this);
            m_DarkModeEnabled = !m_DarkModeEnabled;
        }

        private void ShowSlowWarning(object sender, EventArgs e)
        {
            MessageBox.Show(GetString("CONFIG_STRING_SPEEDLOW"), GetString("CONFIG_STRING_NOTIC"));
        }

        private void trbarBGM_Scroll(object sender, EventArgs e)
        {
            tboxBGM.Text = ((double)trbarBGM.Value / trbarBGM.Maximum).ToString("F1", System.Globalization.CultureInfo.InvariantCulture);
        }

        private void trbarSFX_Scroll(object sender, EventArgs e)
        {
            tboxSFX.Text = (trbarSFX.Value / 10).ToString();
        }

        private void cboxResolution_SelectedIndexChanged(object sender, EventArgs e)
        {
            cboxFrequency.Items.Clear();

            if (cboxResolution.SelectedIndex == -1 || cboxResolution.SelectedIndex >= m_ScreenSettingsList.Count)
                return;

            var Screen = m_ScreenSettingsList.ElementAt(cboxResolution.SelectedIndex);
            foreach (var frequency in Screen.FrequencyList)
                cboxFrequency.Items.Add(frequency);

            if (cboxFrequency.Items.Count > 0)
                cboxFrequency.SelectedIndex = 0;
        }

        private void cboxLocale_SelectedIndexChanged(object sender, EventArgs e)
        {
            cboxLocale.SelectedIndexChanged -= cboxLocale_SelectedIndexChanged;

            var locale = m_LocaleList.ElementAt(cboxLocale.SelectedIndex);
            currentLanguage = locale.name;
            InitializeDictionary(currentLanguage);
            SetStrings();

            for (int i = 0; i < m_LocaleList.Count; i++)
            {
                var tmpLocale = m_LocaleList.ElementAt(i);
                cboxLocale.Items[i] = GetString($"CONFIG_STRING_{tmpLocale.name.ToUpper()}");
            }

            cboxLocale.SelectedIndexChanged += cboxLocale_SelectedIndexChanged;
        }

    }
}