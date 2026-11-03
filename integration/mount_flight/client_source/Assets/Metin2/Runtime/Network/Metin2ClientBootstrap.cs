using System;
using System.Collections.Generic;
using System.Globalization;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using Metin2.Runtime.Mobile;
using UnityEngine;
using UnityEngine.UI;
using Metin2.Runtime.Patching;

namespace Metin2.Runtime.Network
{
    /// <summary>Standalone login, character selection and game-session bootstrap.</summary>
    public sealed class Metin2ClientBootstrap : MonoBehaviour
    {
        [SerializeField] private Metin2ServerConnectionProfile connectionProfile;
        [SerializeField] private string serverHost = "127.0.0.1";
        [SerializeField] private int authPort = 30052;
        [SerializeField] private int gamePort = 30054;
        [SerializeField] private string clientVersionFilename = "metin2client.bin";
        // The Auriga game core compares this exact string in CInputLogin::Entergame (input_login.cpp:739)
        // with config.cpp:87 g_stClientVersion. The official EUROPE client sends "20251204"
        // (PythonNetworkStreamPhaseGame.cpp SendClientVersionPacket); a mismatch is an immediate
        // DelayedDisconnect(0) right after CG_ENTERGAME.
        [SerializeField] private string clientVersionTimestamp = Metin2ProtocolCodec.ExpectedClientVersion;
        [SerializeField] private Metin2GameRuntime gameRuntime;
        [SerializeField] private Metin2PlayerState playerState;
        [SerializeField] private Metin2NetworkEntityWorld entityWorld;
        // No raceCatalog field. A serialized Metin2RaceCatalog reference here hard-referenced
        // all 375 actor prefabs, which made every monster, NPC and mount model resident
        // before the login screen was drawn - 2458.98 MiB of source assets exclusive to this
        // one field, 55% of the scene closure. Metin2RacePrefabs resolves one race on demand
        // through Metin2RacePrefabIndex, the way CRaceManager::__LoadRaceData does
        // (Migration/STARTUP_MEMORY_AND_UI_LOAD_ROOT_CAUSE_AUDIT_2026-08-25.md section 13).
        [SerializeField] private Metin2ItemCatalog itemCatalog;
        // No serialized equipment catalog and no serialized player prefab array. Together these
        // two roots were the whole remaining startup load: the catalog hard-referenced 3060
        // armour, hair and weapon definitions, and the eight player prefabs share 1343 MiB of
        // their 1346 MiB closure with them, because the original's armour REPLACES the body model
        // rather than layering over it (CInstanceBase::SetArmor -> SetShape, InstanceBase.cpp:757,
        // :2610-2627). Splitting either alone frees almost nothing; both are deferred here
        // (Migration/STARTUP_MEMORY_AND_UI_LOAD_ROOT_CAUSE_AUDIT_2026-08-25.md section 17).
        private Metin2EquipmentCatalog equipmentCatalogCache;
        private bool equipmentCatalogResolved;

        /// <summary>
        /// The lightweight runtime catalog: all the value-type metadata, no definition references.
        /// A property rather than a field because it is read on the login and select paths as well
        /// as in game, and a field would have to be populated before each of them.
        /// </summary>
        private Metin2EquipmentCatalog equipmentCatalog
        {
            get
            {
                if (equipmentCatalogResolved) return equipmentCatalogCache;
                equipmentCatalogResolved = true;
                equipmentCatalogCache = Resources.Load<Metin2EquipmentCatalog>(
                    Metin2EquipmentCatalog.RuntimeResourceName);
                if (equipmentCatalogCache == null)
                    Debug.LogWarning("METIN2_EQUIPMENT_RUNTIME_MISSING - no armour, hair or weapon " +
                                     "will resolve. Run Metin2/Build/Equipment And Player Split.");
                return equipmentCatalogCache;
            }
        }
        [SerializeField] private Metin2QuestState questState;
        // No mapCatalog field. A serialized Metin2MapCatalog reference here put all 80 map
        // prefabs into the build scene's dependency graph, so Unity loaded every map's
        // terrain, water, buildings and trees before the login screen was drawn - 3.34 GiB
        // of assets on a 142 KB scene. Metin2WorldRuntime now resolves one map on demand
        // through Metin2MapIndex, the way CMapManager::LoadMap does
        // (Migration/STARTUP_MEMORY_AND_UI_LOAD_ROOT_CAUSE_AUDIT_2026-08-25.md).
        [SerializeField] private Metin2.Runtime.Metin2UiCatalog uiCatalog;
        private Metin2GameUi gameUi;
        [SerializeField] private Metin2SkillCatalog skillCatalog;
        [SerializeField] private Metin2EmotionCatalog emotionCatalog;
        /// <summary>locale/hu/locale_game.txt and skilldesc.txt - every tooltip string comes from here.</summary>
        [SerializeField] private Metin2LocaleText localeText;
        [SerializeField] private Metin2SkillDescriptionCatalog skillDescriptionCatalog;
        /// <summary>Job names as playersettingmodule.py keys its skill tables, in race order.</summary>
        private static readonly string[] SkillJobs = { "WARRIOR", "ASSASSIN", "SURA", "SHAMAN" };
        /// <summary>Name from GC_MAIN_CHARACTER; the character window has no other source for it.</summary>
        private string mainCharacterName = string.Empty;
        private string mainCharacterJob = string.Empty;
        private int mainCharacterSkillGroup = 1;
        private Metin2SessionState sessionState;
        /// <summary>
        /// The raw race from GC_MAIN_CHARACTER. The item tooltip needs it rather than the job name:
        /// CanEquip (uitooltip.py:481) checks both the job (InstanceBase.h:1119 race % 4) and the sex
        /// (:1125 RaceToSex), and -1 means the packet has not arrived yet.
        /// </summary>
        private int mainCharacterRace = -1;
        /// <summary>Placeholders that survive the switch to the original UI because nothing replaces
        /// them yet: the quest dialog carries NPC conversations, the chat panel carries chat.</summary>
        private Metin2QuestPanel questPanel;
        private Metin2ChatCombatPanel chatPanel;
        private Metin2DamageTextTail damageTextTail;
        private Metin2StaminaState staminaState;
        private Metin2StatusEffectsState statusEffectsState;
        private Metin2AffectShowerPanel affectShower;
        private Metin2PvpState pvpState;
        private Metin2ItemShopState itemShopState;
        private Metin2ItemShopPanel itemShopPanel;
        private Metin2PetState petState;
        private Metin2LoverMiningState loverMiningState;
        private Metin2DungeonState dungeonInfoState;
        // uirune.py receives these independently on the command channel.  Keep them outside the
        // window because both commands normally arrive during login, before the player opens it.
        private int runeStatus;
        private int? runeEffectFlag;
        private Metin2EffectCatalog pointEffectCatalog;
        /// <summary>
        /// Authoritative quick-slot state, independent of the task-bar UI's own lifetime. The server
        /// replays saved registrations via GC_QUICKSLOT_ADD right after ENTER_GAME - often before
        /// BuildOriginalGameUi() has created the bar and subscribed to it - and it only replays once,
        /// so a packet that arrives with no listener is gone for good. This buffer is written from
        /// EnsureRuntime() onward (subscribed before StartReceiving, see EnsureRuntime), and
        /// BuildOriginalGameUi() replays it into every freshly built bar.
        /// </summary>
        private readonly Metin2QuickSlotBar.Registration[] quickSlotState =
            new Metin2QuickSlotBar.Registration[Metin2QuickSlotBar.TotalSlots];
        [SerializeField, Tooltip("Writes the raw bytes of item, point and guild packets to Migration/Logs/framing_trace.log. Off by default; turn on to investigate a stream desync.")]
        private bool traceGuildFraming;
        [SerializeField, Tooltip("Writes a METIN2_ENTER_TIMING report for every select/warp/reconnect: how long each blocking stage of the enter-world sequence took, which nested work dominated it, and which assets entered memory. Diagnostic only - it measures the sequence, it does not change it.")]
        private bool traceEnterTiming = true;

        private InputField loginInput, passwordInput;
        private Button loginButton;
        private Text statusText;
        private RectTransform characterRoot;
        private RectTransform createCharacterPanel, deleteCharacterPanel, changeNamePanel;
        private InputField characterNameInput, privateCodeInput, changeNameInput;
        private Text createRaceText, createShapeText;
        [SerializeField] private Metin2OriginalUiBinder originalUi;
        private Metin2MobileUiRoot mobileUiRoot;
        private Metin2CharacterCreationFrontend creationFrontend;
        private byte accountEmpire;
        private bool characterOperationInProgress;
        private Canvas platformUiCanvas;
        private Transform platformUiHost;
        private bool originalUiActive;
        private bool inGame;
        private string accountLogin = string.Empty;
        private uint accountLoginKey;
        private readonly System.Collections.Generic.List<GameObject> gameUiPanels = new System.Collections.Generic.List<GameObject>();
        private Metin2AuthSession gameSession;
        private Metin2LoadingScreen loadingScreen;
        private int selectedCharacterIndex = -1;
        private string activeGameHost = string.Empty;
        private int activeGamePort;
        private bool gameTransitionInProgress;
        private CancellationTokenSource lifetime;
        private CancellationTokenSource guildMarkTransferLifetime;
        private uint[] clientKey;
        private Metin2ProtocolCodec.CharacterList currentCharacters;
        private Metin2PlayerController localController;
        private Metin2ClientCameraBinding clientCameraBinding;
        private Metin2NetworkEntity localMountEntity;
        private Metin2CombatController combatController;
        private int activeComboType;
        private Metin2GroundItemWorld groundItemWorld;
        /// <summary>Lives on the player instance, so the task bar's mouse-mode fly-outs need it kept here.</summary>
        private Metin2InputController inputController;
        /// <summary>mouseModule.mouseController - one carrier shared by every draggable surface.</summary>
        private Metin2CursorAttachment cursorAttachment;
        private readonly Metin2PremiumState premiumState = new Metin2PremiumState();
        /// <summary>The visible mouse pointer from mousemodule.py; separate from the carried item icon.</summary>
        private Metin2OriginalCursor originalCursor;
        private Metin2.Runtime.Metin2WorldRuntime worldRuntime;
        private int pendingCreateIndex = -1, pendingDeleteIndex = -1, pendingChangeNameIndex = -1;
        private byte pendingCreateRace, pendingCreateShape;
        private int characterSelectButtonCount, characterCreateButtonCount, characterDeleteButtonCount;
        private static readonly string[] RaceLabels = { "JOB_WARRIOR", "JOB_ASSASSIN", "JOB_SURA", "JOB_SHAMAN", "JOB_WARRIOR", "JOB_ASSASSIN", "JOB_SURA", "JOB_SHAMAN" };
        private Coroutine xmasBoomRoutine;
        private static readonly float[] XMasBoomTimes = { 2f, 5f, 7f, 10f, 20f };
        private static readonly int[] XMasBoomCounts = { 5, 2, 3, 3, 5 };

        public int CharacterButtonCount => characterSelectButtonCount;
        public int CharacterCreateButtonCount => characterCreateButtonCount;
        public int CharacterDeleteButtonCount => characterDeleteButtonCount;
        public bool HasCharacterCreatePanel => createCharacterPanel != null;
        public bool HasCharacterDeletePanel => deleteCharacterPanel != null;
        public bool HasCharacterChangeNamePanel => changeNamePanel != null;

#if UNITY_STANDALONE_WIN && (DEVELOPMENT_BUILD || UNITY_EDITOR || METIN2_BENCHMARK_BUILD)
        public bool BenchmarkReady => inGame && !gameTransitionInProgress && localController != null;
        public string BenchmarkCharacterName => mainCharacterName;
        public string BenchmarkMapName => worldRuntime == null ? string.Empty : worldRuntime.CurrentMapName;
        public Metin2PlayerController BenchmarkPlayer => localController;
        public Metin2NetworkEntityWorld BenchmarkEntities => entityWorld;
        public bool BenchmarkConnected => gameRuntime != null && gameRuntime.IsReceiving && gameRuntime.ReceiveFailure == null;

        public async Task BenchmarkEnterCharacterAsync(string name, CancellationToken token)
        {
            if (!Metin2.Runtime.Metin2LiveBenchmark.Requested) throw new InvalidOperationException("Benchmark opt-in required");
            int slot = -1;
            if (currentCharacters.Characters != null)
                for (int index = 0; index < currentCharacters.Characters.Length; index++)
                    if (currentCharacters.Characters[index].Id != 0 &&
                        string.Equals(currentCharacters.Characters[index].Name, name, StringComparison.OrdinalIgnoreCase))
                    { slot = index; break; }
            if (slot < 0) throw new InvalidOperationException("Requested benchmark character not found");
            var summary = currentCharacters.Characters[slot];
            if (summary.ChangeName) throw new InvalidOperationException("Character needs a rename; benchmark will not modify it");
            string host = FormatAddress(summary.Address);
            int port = summary.Port;
            if (string.IsNullOrEmpty(host) || port == 0) { host = serverHost; port = gamePort; }
            await EnterCharacterCoreAsync(slot, host, port, "benchmark", token);
        }

        public Task BenchmarkToggleMountAsync(CancellationToken token)
        {
            if (!Metin2.Runtime.Metin2LiveBenchmark.Requested || !BenchmarkReady)
                throw new InvalidOperationException("Benchmark is not in game");
            return gameSession.SendChatAsync(ChatTypeTalking, "/ride", token);
        }
        public Task BenchmarkSpawnTestMobsAsync(int count,CancellationToken token)
        {
            if(!Metin2.Runtime.Metin2LiveBenchmark.Requested||!BenchmarkReady||
                !Metin2.Runtime.Metin2LiveBenchmark.HasArgument("-benchmark-spawn-authorized-mobs")||
                !string.Equals(mainCharacterName,"[ADMIN]Razor",StringComparison.OrdinalIgnoreCase)||
                (count!=100&&count!=30))throw new InvalidOperationException("Authorized benchmark spawn guard failed");
            return gameSession.SendChatAsync(ChatTypeTalking,"/m 657 "+count,token);
        }
        public Task BenchmarkControlledPopulationAsync(int race,CancellationToken token,int count=100)
        {
            if(!Metin2.Runtime.Metin2LiveBenchmark.Requested||!BenchmarkReady||!BenchmarkConnected||
                !Metin2.Runtime.Metin2LiveBenchmark.HasArgument("-benchmark-controlled-populations")||
                !string.Equals(mainCharacterName,"[ADMIN]Razor",StringComparison.OrdinalIgnoreCase)||
                (race!=0&&race!=8001&&race!=657))
                throw new InvalidOperationException("Authorized population guard failed");
            if(count!=100&&(!Metin2.Runtime.Metin2LiveBenchmark.HasArgument("-benchmark-metin-scaling")||
                race!=8001||(count!=1&&count!=10&&count!=25&&count!=50)))
                throw new InvalidOperationException("Authorized population count guard failed");
            // Human-authorized, fixed commands only. Never accept arbitrary chat input.
            return gameSession.SendChatAsync(ChatTypeTalking,race==0?"/p all":"/m "+race+" "+count.ToString(System.Globalization.CultureInfo.InvariantCulture),token);
        }
#endif
        public bool IsCharacterChangeNamePanelOpen => changeNamePanel != null && changeNamePanel.gameObject.activeSelf;
        public int PendingCharacterChangeNameIndex => pendingChangeNameIndex;
        public string Status => statusText == null ? string.Empty : statusText.text;
        /// <summary>Raised immediately before the platform quit request; used by deterministic gates.</summary>
        public event Action ApplicationQuitRequested;

        private void Awake()
        {
            if (connectionProfile == null)
                connectionProfile = Resources.Load<Metin2ServerConnectionProfile>(Metin2ServerConnectionProfile.ResourceName);
            if (connectionProfile == null)
                throw new InvalidOperationException("Missing Metin2 server connection profile in Resources.");
            connectionProfile.Validate();
            clientCameraBinding = new Metin2ClientCameraBinding(gameObject);
            serverHost = connectionProfile.Host;
            authPort = connectionProfile.AuthPort;
            gamePort = connectionProfile.GamePort;
            Metin2ServerInfo.Configure(connectionProfile);
            clientVersionTimestamp = connectionProfile.ClientVersion;
            Metin2TlsTransport.DefaultOptions = connectionProfile.Tls;
            Metin2Language.Changed += RefreshLocalizedStatus;
            Metin2ClientSettings.Current.Changed += RefreshGameOptionTargetBoard;
#if UNITY_ANDROID || UNITY_IOS
            Screen.sleepTimeout = SleepTimeout.NeverSleep;
#endif
            lifetime = new CancellationTokenSource();
            Metin2ClientSettings.Current.ApplyRuntime();
            ConfigurePlatformUi();
            BuildUi();
            EnsureOriginalCursor();
            EnsureRuntime();
            // UI and the shared hit/swing/spell/footstep clips, at Core scope, before anything can
            // ask for them. Every other audio family is loaded with the map, race or track that
            // needs it. Fire and forget: the login screen must not wait on audio.
            StartCoroutine(PreloadCoreAudioAfterPatch());
        }

        /// <summary>
        /// The AUTO_PICKUP option. CPythonPlayer installs a repeating 4-per-second event whose
        /// only job is PickCloseItem() while CPythonSystem::GetPickUpMode() is on; the closest
        /// ground item inside the native pickup radius is sent on every tick, so the player keeps
        /// hoovering drops without pressing Z. RequestClosest mirrors PickCloseItem's 3 m range.
        /// </summary>
        private float autoPickupTimer;
        private float nextPreviewStateSave;
        private InputField[] storagePasswordFields;
        private void Update()
        {
            if(localController!=null && ResolveUiHost()!=null)EnsureMissionPassClient();
            missionPassClient?.SetInGame(localController!=null);
            if(localController!=null&&entityWorld!=null&&Time.unscaledTime>=nextPreviewStateSave)
            {
                nextPreviewStateSave=Time.unscaledTime+2f;
                Metin2CharacterPreviewState.Save(mainCharacterName,mainCharacterRace,entityWorld.LocalParts,KnownMountPreviewRace(),KnownPetPreviewRace());
            }
            if(Input.GetKeyDown(KeyCode.Tab)&&gameUi!=null&&gameUi.IsOpen("changepassworddialog")&&storagePasswordFields!=null&&UnityEngine.EventSystems.EventSystem.current!=null)
            {
                var selectedInput=UnityEngine.EventSystems.EventSystem.current.currentSelectedGameObject;
                for(int i=0;i<storagePasswordFields.Length;i++)
                    if(storagePasswordFields[i]!=null&&storagePasswordFields[i].gameObject==selectedInput)
                    {
                        int step=Input.GetKey(KeyCode.LeftShift)||Input.GetKey(KeyCode.RightShift)?2:1;
                        var target=storagePasswordFields[(i+step)%3];target.Select();target.ActivateInputField();break;
                    }
            }
            if (groundItemWorld == null) return;
            if (!Metin2ClientSettings.Current.AutoPickup) { autoPickupTimer = 0f; return; }
            autoPickupTimer -= Time.unscaledDeltaTime;
            if (autoPickupTimer > 0f) return;
            autoPickupTimer = .25f;
            groundItemWorld.RequestClosest();
        }

        private System.Collections.IEnumerator PreloadCoreAudioAfterPatch()
        {
            yield return new WaitUntil(() => Metin2PatchBootstrap.IsReady || lifetime.IsCancellationRequested);
            if (!lifetime.IsCancellationRequested)
                _ = Metin2SoundManager.Instance.PreloadCoreAudioAsync(lifetime.Token);
        }

        /// <summary>
        /// The single platform UI selection point. Desktop does not enter this block and keeps the
        /// serialized scene Canvas, catalog and login/select prefabs byte-for-byte unchanged.
        /// </summary>
        private void ConfigurePlatformUi()
        {
#if UNITY_ANDROID || UNITY_IOS
            GameObject mobileCanvasPrefab = Resources.Load<GameObject>(Metin2MobileUiRoot.ResourcePath);
            Metin2UiCatalog mobileCatalog = Resources.Load<Metin2UiCatalog>(Metin2MobileUiRoot.CatalogResourcePath);
            if (mobileCanvasPrefab == null || mobileCatalog == null)
            {
                Debug.LogError("METIN2_MOBILE_UI_ASSETS_MISSING - run Metin2/Mobile UI/Create Initial Independent Mobile UI");
                return;
            }

            GameObject namedWindowsCanvas = GameObject.Find("Metin2ClientCanvas");
            Canvas windowsCanvas = namedWindowsCanvas != null
                ? namedWindowsCanvas.GetComponent<Canvas>()
                : FindFirstObjectByType<Canvas>();
            if (windowsCanvas != null) windowsCanvas.gameObject.SetActive(false);
            GameObject mobileCanvas = Instantiate(mobileCanvasPrefab);
            mobileCanvas.name = "Metin2MobileClientCanvas";
            mobileUiRoot = mobileCanvas.GetComponent<Metin2MobileUiRoot>();
            if (mobileUiRoot == null)
            {
                Debug.LogError("METIN2_MOBILE_UI_ROOT_MISSING");
                if (windowsCanvas != null) windowsCanvas.gameObject.SetActive(true);
                return;
            }

            mobileUiRoot.EmoticonRequested -= TriggerLocalEmoticon;
            mobileUiRoot.EmoticonRequested += TriggerLocalEmoticon;

            platformUiCanvas = mobileUiRoot.UiCanvas;
            platformUiHost = mobileUiRoot.ContentRoot;
            uiCatalog = mobileCatalog;
            if (originalUi == null) originalUi = GetComponent<Metin2OriginalUiBinder>();
            if (originalUi != null && mobileCatalog.TryGet("login", out GameObject loginPrefab))
            {
                mobileCatalog.TryGet("selectcharacter", out GameObject selectPrefab);
                originalUi.ConfigurePrefabs(loginPrefab, selectPrefab, true);
            }
            mobileUiRoot.SetGameplayVisible(false);
            Debug.Log("METIN2_PLATFORM_UI selected=mobile catalog=independent safeArea=1 controls=1");
#endif
        }

        private Canvas ResolveUiCanvas()
        {
            if (platformUiCanvas != null) return platformUiCanvas;

            // Never attach the real game UI to the diagnostics canvas.  That canvas is
            // intentionally non-interactive and F9 disables it, so using the first Canvas
            // found made every window ignore pointer events and disappear with the debug HUD.
            // Prefer the scene's explicit desktop UI canvas, then an interactive non-debug
            // canvas in the same scene.  This only selects the runtime host; it does not alter
            // any generated Windows UI prefab or its layout.
            platformUiCanvas = Metin2UiCanvasResolver.Find(this);
            if (platformUiCanvas != null && platformUiCanvas.GetComponent<GraphicRaycaster>() == null)
                platformUiCanvas.gameObject.AddComponent<GraphicRaycaster>();
            return platformUiCanvas;
        }

        private Transform ResolveUiHost()
        {
            if (platformUiHost != null) return platformUiHost;
            Canvas canvas = ResolveUiCanvas();
            return canvas != null ? canvas.transform : null;
        }

        private Transform ResolveFrontEndUiHost()
            => mobileUiRoot != null ? mobileUiRoot.FrontEndRoot : ResolveUiHost();

        private void EnsureOriginalCursor()
        {
            if (originalCursor == null)
                originalCursor = GetComponent<Metin2OriginalCursor>() ?? gameObject.AddComponent<Metin2OriginalCursor>();
            Canvas host = ResolveUiCanvas();
            Transform parent = ResolveUiHost();
            if (host != null && parent != null) originalCursor.Initialize(parent);
        }

        public void BuildUi()
        {
            if (statusText != null || originalUiActive) return;

            // Prefer the generated 1:1 replica of the original login / character-select screens.
            // The programmatic placeholder below stays as the fallback for scenes that have not
            // been rebuilt with Metin2UiLayoutImporter yet.
            if (originalUi == null) originalUi = GetComponent<Metin2OriginalUiBinder>();
            if (originalUi != null)
            {
                Canvas host = ResolveUiCanvas();
                if (host == null)
                {
                    var canvasObject = new GameObject("Metin2ClientCanvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
                    host = canvasObject.GetComponent<Canvas>(); host.renderMode = RenderMode.ScreenSpaceOverlay;
                }
                Transform uiParent = ResolveFrontEndUiHost() ?? host.transform;
                // The classic client lays out in raw screen pixels and derives every board position
                // from the live SCREEN_WIDTH / SCREEN_HEIGHT. Metin2UiResponsiveLayout reproduces that
                // math, so the canvas must map one UI unit to one screen pixel; any ScaleWithScreenSize
                // mode would apply a second, conflicting transform.
                if (mobileUiRoot == null)
                {
                    CanvasScaler hostScaler = host.GetComponent<CanvasScaler>();
                    if (hostScaler == null) hostScaler = host.gameObject.AddComponent<CanvasScaler>();
                    hostScaler.uiScaleMode = CanvasScaler.ScaleMode.ConstantPixelSize;
                    hostScaler.scaleFactor = 1f;
                }
                if(uiCatalog!=null&&uiCatalog.TryGet("selectcharacterwindow",out var aurigaSelectPrefab))originalUi.ConfigureSelectPrefab(aurigaSelectPrefab);
                if (!originalUiActive) originalUi.ConfigureEndpoint(connectionProfile);
                if (originalUi.Build(uiParent))
                {
                    originalUiActive = true;
                    creationFrontend=GetComponent<Metin2CharacterCreationFrontend>()??gameObject.AddComponent<Metin2CharacterCreationFrontend>();
                    Metin2UiCatalog frontCatalog=mobileUiRoot!=null?Resources.Load<Metin2UiCatalog>(Metin2MobileUiRoot.CatalogResourcePath):uiCatalog;
                    if(frontCatalog==null||!frontCatalog.TryGet("createcharacterwindow",out _))frontCatalog=uiCatalog;
                    creationFrontend.Build(uiParent,frontCatalog,equipmentCatalog,lifetime.Token);
                    creationFrontend.EmpireRequested+=SelectAccountEmpire;
                    creationFrontend.AppearanceChanged+=(race,shape)=>{pendingCreateRace=race;pendingCreateShape=shape;};
                    creationFrontend.CreateRequested+=SubmitCharacterCreate;
                    creationFrontend.CancelRequested+=CancelCreationFrontend;
                    gamePort = originalUi.SelectedGamePort;
                    originalUi.ChannelSelected -= SelectLoginChannel;
                    originalUi.ChannelSelected += SelectLoginChannel;
                    // A placeholder panel may have been baked into the scene by an earlier build.
                    Transform stale = uiParent.Find("LoginPanel");
                    if (stale != null)
                    {
                        if (Application.isPlaying) Destroy(stale.gameObject); else DestroyImmediate(stale.gameObject);
                    }
                    originalUi.LoginRequested -= LoginFromOriginalUi; originalUi.LoginRequested += LoginFromOriginalUi;
                    originalUi.StartRequested -= SelectCharacter; originalUi.StartRequested += SelectCharacter;
                    originalUi.CreateRequested -= OpenCreateCharacter; originalUi.CreateRequested += OpenCreateCharacter;
                    originalUi.DeleteRequested -= OpenDeleteCharacter; originalUi.DeleteRequested += OpenDeleteCharacter;
                    originalUi.ExitSelectRequested -= ReturnToLoginFromCharacterSelect;
                    originalUi.ExitSelectRequested += ReturnToLoginFromCharacterSelect;
                    originalUi.BindPreview(null, equipmentCatalog);
                    originalUi.ShowLogin();
                    try { Metin2SoundManager.Instance.FadeInMusic("BGM/login_window.mp3"); } catch {}
                    // The create / delete dialogs have no counterpart in the staged layouts yet,
                    // so keep the functional overlays on top of the replica.
                    BuildCharacterDialogs(uiParent, Metin2.Runtime.Metin2UiFont.Get());
                    return;
                }
            }

            Canvas canvas = ResolveUiCanvas();
            if (canvas == null)
            {
                var canvasObject = new GameObject("Metin2ClientCanvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
                canvas = canvasObject.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay;
                canvasObject.GetComponent<CanvasScaler>().uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            }
            Font font = Metin2.Runtime.Metin2UiFont.Get();
            var panel = CreateRect("LoginPanel", ResolveUiHost() ?? canvas.transform, new Vector2(440, 420), Vector2.zero);
            panel.gameObject.AddComponent<Image>().color = new Color(0.04f, 0.05f, 0.07f, 0.94f);
            loginInput = CreateLocaleInput(panel, font, "LOGIN_INPUT_ID", new Vector2(0, 165), false);
            passwordInput = CreateLocaleInput(panel, font, "LOGIN_INPUT_PASSWORD", new Vector2(0, 115), true);
            loginButton = CreateLocaleButton(panel, font, "LOGIN_CONNECT", new Vector2(0, 65), () => LoginFromUi());
            statusText = CreateText(panel, font, Metin2Language.Get("UNITY_READY"), 15, new Vector2(0, 22), new Vector2(400, 40));
            statusLocaleKey = "UNITY_READY";
            characterRoot = CreateRect("Characters", panel, new Vector2(390, 150), new Vector2(0, -90));

            BuildCharacterDialogs(panel, font);
        }

        private void BuildCharacterDialogs(Transform panel, Font font)
        {
            if (createCharacterPanel != null) return;
            createCharacterPanel = CreateRect("CreateCharacterPanel", panel, new Vector2(400, 280), Vector2.zero);
            createCharacterPanel.gameObject.AddComponent<Image>().color = new Color(0.06f, 0.08f, 0.12f, 0.99f);
            CreateLocaleText(createCharacterPanel, font, "UNITY_CREATE_CHARACTER", 19, new Vector2(0, 105), new Vector2(360, 35));
            characterNameInput = CreateLocaleInput(createCharacterPanel, font, "CREATE_NAME", new Vector2(0, 55), false);
            createRaceText = CreateText(createCharacterPanel, font, CreateRaceLabel(0), 15, new Vector2(0, 10), new Vector2(220, 30));
            CreateButton(createCharacterPanel, font, "<", new Vector2(-145, 10), () => CycleCreateRace(-1), new Vector2(50, 30));
            CreateButton(createCharacterPanel, font, ">", new Vector2(145, 10), () => CycleCreateRace(1), new Vector2(50, 30));
            createShapeText = CreateText(createCharacterPanel, font, Metin2Language.Format("UNITY_NUMBERED_LABEL", Metin2Language.Get("CREATE_SHAPE"), 1), 15, new Vector2(0, -28), new Vector2(220, 30));
            CreateLocaleButton(createCharacterPanel, font, "CREATE_SHAPE", new Vector2(145, -28), () => CycleCreateShape(), new Vector2(80, 30));
            CreateLocaleButton(createCharacterPanel, font, "CREATE_CREATE", new Vector2(-90, -85), () => SubmitCharacterCreate(), new Vector2(150, 34));
            CreateLocaleButton(createCharacterPanel, font, "CANCEL", new Vector2(90, -85), () => CloseCharacterDialogs(), new Vector2(150, 34));
            createCharacterPanel.gameObject.SetActive(false);

            var deleteDialog=Metin2ModernDeleteDialog.Create(panel,SubmitCharacterDelete,CloseCharacterDialogs);
            deleteCharacterPanel=(RectTransform)deleteDialog.transform;
            privateCodeInput=deleteDialog.Code;

            changeNamePanel = CreateRect("ChangeNamePanel", panel, new Vector2(400, 220), Vector2.zero);
            changeNamePanel.gameObject.AddComponent<Image>().color = new Color(0.07f, 0.08f, 0.12f, 0.99f);
            CreateLocaleText(changeNamePanel, font, "SELECT_CHANGE_NAME_TITLE", 19, new Vector2(0, 70), new Vector2(360, 35));
            changeNameInput = CreateLocaleInput(changeNamePanel, font, "SELECT_INPUT_CHANGING_NAME", new Vector2(0, 20), false);
            changeNameInput.characterLimit = 24;
            CreateLocaleButton(changeNamePanel, font, "UNITY_RENAME", new Vector2(0, -50), () => SubmitCharacterRename(), new Vector2(150, 34));
            changeNamePanel.gameObject.SetActive(false);
        }

        public void ShowCharacters(Metin2ProtocolCodec.CharacterList list)
        {
            ShowCharactersAfterPreload(list);
        }

        private int characterPresentationGeneration;
        private async void ShowCharactersAfterPreload(Metin2ProtocolCodec.CharacterList list)
        {
            int generation = ++characterPresentationGeneration;
            currentCharacters = list;
            if(originalUiActive&&(AllCharacterSlotsEmpty(list)||accountEmpire==0))
            {
                CloseCharacterDialogs();pendingCreateIndex=AllCharacterSlotsEmpty(list)?0:-1;
                originalUi.Hide();creationFrontend.ShowEmpire(accountEmpire);
                return;
            }
            try
            {
                var jobs = new HashSet<int>();
                if (list.Characters != null)
                    foreach (Metin2ProtocolCodec.CharacterSummary character in list.Characters)
                        if (character.Id != 0 && !string.IsNullOrEmpty(character.Name) && character.Job < Metin2PlayerPrefabs.JobCount)
                            jobs.Add(character.Job);
                var loads = new List<Task>();
                // Preloads are owned by the login session lifetime. Without Start() there is no
                // session (deterministic editor harnesses and pre-Start callers), and the
                // presentation must still build synchronously instead of null-dereferencing.
                if (lifetime != null)
                    foreach (int job in jobs) loads.Add(Metin2PlayerPrefabs.PreloadAsync(job, Metin2ContentScope.Session, lifetime.Token));
                // Armour and hair are Addressable content since the equipment migration, and the
                // select preview resolves them synchronously while it builds each portrait. Preload
                // exactly the pieces the listed characters wear, so Metin2SelectCharacterPreview
                // never has to instantiate from an unloaded definition and no character is drawn
                // naked or bald for a frame.
                Metin2EquipmentCatalog selectEquipment = equipmentCatalog;
                if (lifetime != null && selectEquipment != null && list.Characters != null)
                    foreach (Metin2ProtocolCodec.CharacterSummary character in list.Characters)
                    {
                        if (character.Id == 0 || string.IsNullOrEmpty(character.Name) || character.Job >= Metin2PlayerPrefabs.JobCount) continue;
                        // MainPart is an item VNUM when the table knows it and a raw ShapeData
                        // appearance otherwise - the same rule CInstanceBase::SetArmor applies -
                        // and HairPart is always an appearance, never an item.
                        loads.Add(selectEquipment.PreloadItemAsync((byte)character.Job, character.MainPart, Metin2ContentScope.Session, lifetime.Token));
                        loads.Add(selectEquipment.PreloadArmorAsync((byte)character.Job, character.MainPart, Metin2ContentScope.Session, lifetime.Token));
                        loads.Add(selectEquipment.PreloadHairAsync((byte)character.Job, character.HairPart, Metin2ContentScope.Session, lifetime.Token));
                        var previousAppearance=Metin2CharacterPreviewState.Load(character.Name,character.Job);
                        if(previousAppearance!=null)
                        {
                            if(previousAppearance.weapon!=0)loads.Add(selectEquipment.PreloadWeaponAsync(previousAppearance.weapon,Metin2ContentScope.Session,lifetime.Token));
                            if(previousAppearance.mount!=0)
                            {
                                loads.Add(Metin2RacePrefabs.PreloadAsync(previousAppearance.mount,Metin2ContentScope.Session,lifetime.Token));
                                loads.Add(Metin2RaceDataBinder.PreloadAsync(previousAppearance.mount,Metin2ContentScope.Session,lifetime.Token));
                            }
                            if(previousAppearance.pet!=0)
                            {
                                loads.Add(Metin2RacePrefabs.PreloadAsync(previousAppearance.pet,Metin2ContentScope.Session,lifetime.Token));
                                loads.Add(Metin2RaceDataBinder.PreloadAsync(previousAppearance.pet,Metin2ContentScope.Session,lifetime.Token));
                            }
                        }
                    }
                // Refine armour/weapon shines are synchronous consumers of the effect catalog.
                // Preload only pc/common for character select so the preview never loses +7/+8/+9
                // shine while keeping the rest of the effect archive out of the login phase.
                Metin2EffectCatalog selectEffects = Resources.Load<Metin2EffectCatalog>("Metin2EffectCatalog");
                Task selectEffectLoad = lifetime != null && selectEffects != null
                    ? selectEffects.PreloadCharacterSelectEffectsAsync(Metin2ContentScope.Session, lifetime.Token)
                    : Task.CompletedTask;
                await Task.WhenAll(loads);
                try { await selectEffectLoad; }
                catch (OperationCanceledException) { throw; }
                catch (Exception effectException)
                {
                    Debug.LogWarning("METIN2_SELECT_EFFECT_PRELOAD_FAILED " + effectException.Message);
                }
                if (generation != characterPresentationGeneration || (lifetime != null && lifetime.IsCancellationRequested)) return;
            }
            catch (OperationCanceledException) { return; }
            catch (Exception exception) { ShowOperationFailure(exception); return; }
            if (originalUiActive)
            {
                EnsureRuntime();
                originalUi.ShowCharacters(list);
                try { Metin2SoundManager.Instance.FadeInMusic("BGM/characterselect.mp3"); Metin2SoundManager.Instance.PlaySound("sound/ui/loginok.wav"); } catch {}
                return;
            }
            if (characterRoot == null) BuildUi();
            EnsureRuntime();
            for (int i = characterRoot.childCount - 1; i >= 0; i--) DestroyImmediate(characterRoot.GetChild(i).gameObject);
            characterSelectButtonCount = characterCreateButtonCount = characterDeleteButtonCount = 0;
            int slotCount = Math.Max(4, list.Characters == null ? 0 : list.Characters.Length);
            for (int i = 0; i < slotCount; i++)
            {
                int index = i;
                Metin2ProtocolCodec.CharacterSummary character = list.Characters != null && i < list.Characters.Length ? list.Characters[i] : default;
                float y = 52 - index * 34;
                if (character.Id == 0 || string.IsNullOrEmpty(character.Name))
                {
                    Button create = CreateButton(characterRoot, statusText.font, Metin2Language.Format("UNITY_CREATE_IN_SLOT", index + 1), new Vector2(0, y), () => OpenCreateCharacter(index));
                    create.GetComponentInChildren<Text>().gameObject.AddComponent<Metin2LocalizedLabel>()
                        .ConfigureFormat("UNITY_CREATE_IN_SLOT", () => new object[] { index + 1 });
                    characterCreateButtonCount++;
                }
                else
                {
                    string label = character.ChangeName
                        ? Metin2Language.Format("UNITY_RENAME_LABEL", Metin2Language.Get("UNITY_RENAME"), character.Name)
                        : Metin2Language.Format("UNITY_CHARACTER_LABEL", character.Name, Metin2Language.Get("SELECT_LEVEL"), character.Level);
                    Button select = CreateButton(characterRoot, statusText.font, label, new Vector2(-38, y), () => SelectCharacter(index), new Vector2(260, 30));
                    select.GetComponentInChildren<Text>().gameObject.AddComponent<Metin2LocalizedLabel>()
                        .ConfigureFormat(character.ChangeName ? "UNITY_RENAME_LABEL" : "UNITY_CHARACTER_LABEL", () => character.ChangeName
                            ? new object[] { Metin2Language.Get("UNITY_RENAME"), character.Name }
                            : new object[] { character.Name, Metin2Language.Get("SELECT_LEVEL"), character.Level });
                    CreateLocaleButton(characterRoot, statusText.font, "SELECT_DELETE", new Vector2(132, y), () => OpenDeleteCharacter(index), new Vector2(72, 30));
                    characterSelectButtonCount++; characterDeleteButtonCount++;
                }
            }
            SetStatusLocale(CharacterButtonCount == 0 ? "UNITY_CREATE_CHARACTER" : "UNITY_SELECT_CHARACTER");
        }

        private void OpenCreateCharacter(int index)
        {
            if(originalUiActive&&creationFrontend!=null)
            {
                pendingCreateIndex=index;
                originalUi.Hide();
                if(AllCharacterSlotsEmpty(currentCharacters))creationFrontend.ShowEmpire(accountEmpire);
                else creationFrontend.ShowCreate();
                return;
            }
            pendingCreateIndex = index; pendingCreateRace = 0; pendingCreateShape = 0;
            if (characterNameInput != null) characterNameInput.text = string.Empty;
            RefreshCreateLabels();
            deleteCharacterPanel?.gameObject.SetActive(false); createCharacterPanel?.gameObject.SetActive(true);
            SetStatusLocale("UNITY_CHOOSE_CHARACTER_DETAILS");
        }

        private void OpenDeleteCharacter(int index)
        {
            pendingDeleteIndex = index;
            if(currentCharacters.Characters!=null&&index>=0&&index<currentCharacters.Characters.Length)
                deleteCharacterPanel?.GetComponent<Metin2ModernDeleteDialog>()?.SetCharacter(currentCharacters.Characters[index].Name);
            if (privateCodeInput != null) privateCodeInput.text = string.Empty;
            createCharacterPanel?.gameObject.SetActive(false); deleteCharacterPanel?.gameObject.SetActive(true);
            SetStatusLocale("UNITY_PRIVATE_CODE_PROMPT");
        }

        private void OpenChangeCharacterName(int index)
        {
            pendingChangeNameIndex = index;
            if (changeNameInput != null) changeNameInput.text = string.Empty;
            createCharacterPanel?.gameObject.SetActive(false); deleteCharacterPanel?.gameObject.SetActive(false); changeNamePanel?.gameObject.SetActive(true);
            SetStatusLocale("UNITY_RENAME_REQUIRED");
        }

        private void CycleCreateRace(int delta)
        {
            pendingCreateRace = (byte)((pendingCreateRace + delta + RaceLabels.Length) % RaceLabels.Length);
            RefreshCreateLabels();
        }

        private void CycleCreateShape() { pendingCreateShape = (byte)(1 - pendingCreateShape); RefreshCreateLabels(); }
        private void RefreshCreateLabels()
        {
            if (createRaceText != null) createRaceText.text = CreateRaceLabel(pendingCreateRace);
            if (createShapeText != null) createShapeText.text = Metin2Language.Format("UNITY_NUMBERED_LABEL", Metin2Language.Get("CREATE_SHAPE"), pendingCreateShape + 1);
        }

        private static string CreateRaceLabel(int race)
        {
            bool male = race == 0 || race == 2 || race == 5 || race == 7;
            return Metin2Language.Format("UNITY_CLASS_GENDER", Metin2Language.Get(RaceLabels[race]),
                Metin2Language.Get(male ? "CREATE_MAN" : "CREATE_WOMAN"));
        }

        private async void SubmitCharacterCreate()
        {
            if(characterOperationInProgress)return;
            characterOperationInProgress=true;creationFrontend?.SetBusy(true);
            try
            {
                if (gameSession == null) throw new InvalidOperationException("Connect before creating a character");
                if(originalUiActive&&creationFrontend!=null&&!creationFrontend.IsCreateVisible)throw new InvalidOperationException("Select an empire before creating a character");
                if(originalUiActive&&accountEmpire<1)throw new InvalidOperationException("Select an empire before creating a character");
                string name = creationFrontend!=null&&creationFrontend.IsCreateVisible?creationFrontend.CharacterName:(characterNameInput == null ? string.Empty : characterNameInput.text.Trim());
                if (string.IsNullOrEmpty(name)) throw new InvalidOperationException("Character name is required");
                SetStatusLocale("UNITY_CREATING_CHARACTER");
                Metin2ProtocolCodec.CharacterCreateResult result = await gameSession.CreateCharacterAsync((byte)pendingCreateIndex,name,pendingCreateRace,pendingCreateShape,lifetime.Token,creationFrontend!=null?creationFrontend.SkillGroup:(byte)1,creationFrontend!=null?creationFrontend.HairAppearance:(ushort)0);
                if (!result.Success) { Debug.LogWarning("METIN2_CHARACTER_CREATE_FAILED type=" + result.FailureType); SetStatusLocale("CREATE_FAILURE"); creationFrontend?.ShowError(Metin2Language.Get("CREATE_FAILURE")); return; }
                UpdateCharacterSlot(result.Index, result.Character); CloseCharacterDialogs(); ShowCharacters(currentCharacters);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
            finally {characterOperationInProgress=false;creationFrontend?.SetBusy(false);}
        }

        private static bool AllCharacterSlotsEmpty(Metin2ProtocolCodec.CharacterList list)
        {
            if(list.Characters!=null)foreach(var character in list.Characters)if(character.Id!=0)return false;
            return true;
        }
        private async void SelectAccountEmpire(byte empire)
        {
            if(characterOperationInProgress)return;
            characterOperationInProgress=true;creationFrontend.SetBusy(true);
            try
            {
                if(gameSession==null)throw new InvalidOperationException("Connect before selecting an empire");
                var result=await gameSession.SelectEmpireAsync(empire,lifetime.Token);
                accountEmpire=result.Empire;currentCharacters=result.CharacterList;
                pendingCreateRace=pendingCreateShape=0;
                if(AllCharacterSlotsEmpty(currentCharacters))creationFrontend.ShowCreate();
                else{creationFrontend.Hide();ShowCharacters(currentCharacters);}
                Debug.Log("METIN2_EMPIRE_SELECTED empire="+accountEmpire);
            }
            catch(Exception exception){ShowOperationFailure(exception);}
            finally {characterOperationInProgress=false;creationFrontend.SetBusy(false);}
        }
        private void CancelCreationFrontend()
        {
            if(characterOperationInProgress)return;
            bool wasEmpire=creationFrontend.IsEmpireVisible;creationFrontend.Hide();
            if(wasEmpire)ReturnToLoginFromCharacterSelect();else ShowCharacters(currentCharacters);
        }

        private async void SubmitCharacterDelete()
        {
            if(characterOperationInProgress)return;
            var dialog=deleteCharacterPanel?.GetComponent<Metin2ModernDeleteDialog>();
            try
            {
                string code = privateCodeInput == null ? string.Empty : privateCodeInput.text.Trim();
                if(code.Length==0||code.Length>7){dialog?.ShowStatusLocale("UNITY_WRONG_PRIVATE_CODE");SetStatusLocale("UNITY_WRONG_PRIVATE_CODE");return;}
                if (gameSession == null) throw new InvalidOperationException("Connect before deleting a character");
                characterOperationInProgress=true;dialog?.SetBusy(true);
                SetStatusLocale("SELECT_DELEING");
                dialog?.ShowStatusLocale("SELECT_DELEING",false);
                Metin2AuthSession.CharacterDeleteResult result = await gameSession.DeleteCharacterAsync((byte)pendingDeleteIndex, code, lifetime.Token);
                if (!result.Success) { SetStatusLocale(dialog!=null?dialog.ShowFailure(result.WrongPrivateCode):result.WrongPrivateCode?"UNITY_WRONG_PRIVATE_CODE":"SELECT_CAN_NOT_DELETE");return; }
                UpdateCharacterSlot(result.Index, default); CloseCharacterDialogs(); ShowCharacters(currentCharacters);
            }
            catch (Exception exception) { ShowOperationFailure(exception);if(!(exception is OperationCanceledException))dialog?.ShowStatusLocale(statusLocaleKey??"UNITY_CLIENT_OPERATION_FAILED"); }
            finally{characterOperationInProgress=false;dialog?.SetBusy(false);}
        }

        private async void SubmitCharacterRename()
        {
            try
            {
                if (gameSession == null) throw new InvalidOperationException("Connect before renaming a character");
                if (pendingChangeNameIndex < 0) throw new InvalidOperationException("No character selected for rename");
                string name = changeNameInput == null ? string.Empty : changeNameInput.text.Trim();
                if (string.IsNullOrEmpty(name)) throw new InvalidOperationException("Character name is required");
                SetStatusLocale("UNITY_RENAMING_CHARACTER");
                Metin2AuthSession.CharacterNameChangeResult result = await gameSession.ChangeCharacterNameAsync((byte)pendingChangeNameIndex, name, lifetime.Token);
                if (!result.Success) { Debug.LogWarning("METIN2_CHARACTER_RENAME_FAILED type=" + result.FailureType); SetStatusLocale("UNITY_RENAME_FAILED"); return; }
                ApplyCharacterNameChange(pendingChangeNameIndex, result.Change);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// Applies the authoritative <c>GC_CHANGE_NAME</c> reply. The original select phase finds
        /// the reply PID among its four local slots before removing bChangeName; never let a reply
        /// for another character mutate the currently selected UI slot.
        /// </summary>
        private void ApplyCharacterNameChange(int index, Metin2ProtocolCodec.CharacterNameChange change)
        {
            if (currentCharacters.Characters == null || index < 0 || index >= currentCharacters.Characters.Length)
                throw new InvalidOperationException("Character rename response selected an invalid slot");
            Metin2ProtocolCodec.CharacterSummary previous = currentCharacters.Characters[index];
            if (previous.Id == 0 || previous.Id != change.Id)
                throw new InvalidOperationException("Character rename response did not match the selected slot");
            if (string.IsNullOrEmpty(change.Name)) throw new InvalidOperationException("Character rename response had an empty name");
            UpdateCharacterSlot(index, previous.WithName(change.Name, false));
            CloseCharacterDialogs();
            ShowCharacters(currentCharacters);
        }

        private void UpdateCharacterSlot(int index, Metin2ProtocolCodec.CharacterSummary character)
        {
            if (index < 0 || index >= Metin2ProtocolCodec.CharacterSlotCount)
                throw new ArgumentOutOfRangeException(nameof(index));
            int oldLength = currentCharacters.Characters == null ? 0 : currentCharacters.Characters.Length;
            int length = Metin2ProtocolCodec.CharacterSlotCount;
            var characters = new Metin2ProtocolCodec.CharacterSummary[length];
            if (oldLength > 0) Array.Copy(currentCharacters.Characters, characters, oldLength);
            characters[index] = character;
            currentCharacters = new Metin2ProtocolCodec.CharacterList(characters, currentCharacters.Handle, currentCharacters.RandomKey);
        }

        private void CloseCharacterDialogs()
        {
            creationFrontend?.Hide();
            createCharacterPanel?.gameObject.SetActive(false); deleteCharacterPanel?.gameObject.SetActive(false); changeNamePanel?.gameObject.SetActive(false);
            pendingCreateIndex = pendingDeleteIndex = pendingChangeNameIndex = -1;
        }

        /// <summary>
        /// introselect.py ExitSelect / OnPressExitKey. SetLoginPhase leaves the select descriptor
        /// and reconstructs the login phase; the Unity equivalent must therefore dispose the live
        /// game-core transport instead of merely swapping two visible panels.
        /// </summary>
        private void ReturnToLoginFromCharacterSelect()
        {
            if (inGame || gameTransitionInProgress) return;
            TearDownStep("selectExitStopReceiving", () => gameRuntime?.StopReceiving());
            TearDownStep("selectExitDisposeSession", () => { gameSession?.Dispose(); gameSession = null; });
            TearDownStep("selectExitReleaseSessionAssets", ReleaseSessionAssetCaches);
            CloseCharacterDialogs();
            currentCharacters = default;
            accountLoginKey = 0;
            selectedCharacterIndex = -1;
            activeGameHost = string.Empty;
            activeGamePort = 0;
            ApplyGameUiVisibility();
            if (originalUiActive) originalUi.ShowLogin();
            SetStatus(string.Empty);
            Debug.Log("METIN2_SELECT_EXIT route=login sessionDisposed=1");
        }

        private async void LoginFromUi()
        {
            try { await LoginAsync(loginInput.text, passwordInput.text, lifetime.Token); }
            catch (OperationCanceledException) { SetStatusLocale("UNITY_CANCELLED"); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private bool loginInProgress;

        private void SelectLoginChannel(int port)
        {
            if (!loginInProgress) gamePort = port;
        }

        public async Task LoginAsync(string login, string password, CancellationToken token)
        {
            if (!Metin2PatchBootstrap.IsReady)
            {
                SetStatusLocale("UNITY_PATCH_WAIT");
                return;
            }
            // A second login while the first is still running disposes the session the first one is
            // awaiting, which surfaced as "Session is not in game login phase". Both the login button
            // and the saved-account slots raise LoginRequested, so re-entry has to be refused here.
            if (loginInProgress) { SetStatusLocale("LOGIN_CONNETING"); return; }
            loginInProgress = true;
            if (originalUiActive) originalUi.SetConnectionBusy(true);
            if (loginButton != null) loginButton.interactable = false;
            SetStatusLocale("UNITY_LOGIN_AUTH"); clientKey = CreateClientKey();
            // Auriga LOGIN3 carries a hardware id; SystemInfo is main-thread only and LoginAsync
            // continues on the thread pool, so the value is primed here.
            Metin2AuthSession.Hwid = Metin2ProtocolCodec.ComputeHwid(SystemInfo.deviceUniqueIdentifier);
            try
            {
                uint loginKey;
                using (var auth = new Metin2AuthSession())
                {
                    await auth.ConnectAsync(serverHost, authPort, token);
                    Metin2AuthSession.LoginResult result = await auth.LoginAsync(login, password, clientKey, ActiveLanguage, token);
                    if (!result.Success) throw new InvalidOperationException("Login failed: " + result.Status);
                    loginKey = result.LoginKey;
                }
                accountLogin = login; accountLoginKey = loginKey;
                SetStatusLocale("UNITY_LOGIN_GAME");
                Metin2AuthSession.GameLoginResult gameLogin = await ConnectGameCoreAsync(serverHost, gamePort, token);
                if (!gameLogin.Success) throw new InvalidOperationException("Game login failed: " + gameLogin.Status);
                accountEmpire=gameLogin.Empire;
                ShowCharacters(gameLogin.CharacterList);
            }
            catch (Exception exception) when (!(exception is OperationCanceledException))
            {
                // intrologin.py plays this for both connection failures and rejected login
                // answers.  It is a login-phase acknowledgement, not a generic exception sound.
                Metin2SoundManager.Instance.PlaySound("sound/ui/loginfail.wav");
                throw;
            }
            finally { loginInProgress = false; if (loginButton != null) loginButton.interactable = true; if (originalUiActive) originalUi.SetConnectionBusy(false); }
        }

        /// <summary>
        /// Opens a game-core connection and completes the LOGIN2 handshake, returning the character list.
        /// </summary>
        private async Task<Metin2AuthSession.GameLoginResult> ConnectGameCoreAsync(string host, int port, CancellationToken token)
        {
            if (!Metin2PatchBootstrap.IsReady)
                throw new InvalidOperationException("Game content update must finish before connecting.");
            // A receiver owns the old session transport. Stop it before disposing/replacing that
            // transport; otherwise its async read can fault later and race the new game core.
            if (gameSession != null)
                Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"BOOTSTRAP_REPLACE_SESSION next={host}:{port}");
            gameRuntime?.StopReceiving();
            gameSession?.Dispose();
            gameSession = new Metin2AuthSession();
            gameSession.ItemMoveSending+=(fromWindow,fromCell,toWindow,toCell)=>playerState?.NotifyInventoryMove(fromWindow,fromCell,toWindow,toCell);
            Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"BOOTSTRAP_NEW_GAME_SESSION endpoint={host}:{port}");
            // Armed before the first byte is read. Started later - as it was, after the character
            // list and the loading burst had already been framed - the trace could not see the
            // packets that actually desync a stream, which is the only thing it exists for.
            if (traceGuildFraming) StartFramingTrace();
            // net.SetServerInfo in the original intro scripts: the minimap's ServerInfo row shows
            // "<server>, <channel>" for the core the player is actually on (root/uiminimap.py).
            Metin2ServerInfo.SetEndpoint(host, port);
            await gameSession.ConnectGameEndpointAsync(host, port, token);
            await gameSession.SendLoginByKeyAsync(accountLogin, accountLoginKey, clientKey, ActiveLanguage, token);
            return await gameSession.ReceiveGameLoginAsync(token);
        }

        /// <summary>
        /// Authoritative enter-game order, mirroring CPythonNetworkStream.
        ///
        /// The character list carries a per-character game-core address and port
        /// (TSimplePlayerInformation.lAddr / wPort). CPythonNetworkStream::ConnectGameServer drops the
        /// current socket and reconnects there before anything else - that is what net.DirectEnter does
        /// in introselect.py. Sending CG_PLAYER_SELECT to the wrong core makes the server fail with
        /// "GetValidLocation: cannot find sectree_map by map index N" and close the descriptor, because
        /// each core only serves the map indices in its own MAP_ALLOW list.
        ///
        /// After the redirect the order is:
        /// LOGIN2 -> character list -> CG_PLAYER_SELECT -> GC_PHASE(PHASE_LOADING) -> GC_MAIN_CHARACTER
        /// -> CG_CLIENT_VERSION -> CG_ENTERGAME -> GC_PHASE(PHASE_GAME).
        /// CInputLogin::Entergame is what calls CHARACTER::Show, so the world burst follows enter game.
        /// </summary>
        private async void SelectCharacter(int index)
        {
            try
            {
                if (currentCharacters.Characters == null || index < 0 || index >= currentCharacters.Characters.Length)
                    throw new InvalidOperationException("Character slot " + index + " is empty");
                Metin2ProtocolCodec.CharacterSummary summary = currentCharacters.Characters[index];
                if (summary.Id == 0 || string.IsNullOrEmpty(summary.Name)) throw new InvalidOperationException("Character slot " + index + " is empty");
                if (summary.ChangeName) { OpenChangeCharacterName(index); return; }

                string coreHost = FormatAddress(summary.Address);
                int corePort = summary.Port;
                if (string.IsNullOrEmpty(coreHost) || corePort == 0) { coreHost = serverHost; corePort = gamePort; }

                await EnterCharacterCoreAsync(index, coreHost, corePort, "select", lifetime.Token);
            }
            catch (Exception exception)
            {
                Debug.LogException(exception);
                if (gameTransitionInProgress || (loadingScreen != null && loadingScreen.IsVisible)) FailGameTransition(exception);
                else ShowOperationFailure(exception);
            }
        }

        /// <summary>The single original DirectEnter lifecycle used by character select and warp.</summary>
        private async Task EnterCharacterCoreAsync(int index, string host, int port, string reason, CancellationToken token)
        {
            if (gameTransitionInProgress) throw new InvalidOperationException("A game transition is already running");
            gameTransitionInProgress = true;
            // Diagnostic only. Every Measure/Report call below is a no-op while the flag is off,
            // and none of them changes the order of the original DirectEnter sequence.
            Metin2.Runtime.Metin2EnterWorldTiming.Enabled = traceEnterTiming;
            Metin2.Runtime.Metin2EnterWorldTiming.Begin(reason);
            using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("beginLoadingPhase"))
                BeginLoadingPhase(reason);
            try
            {
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("clearGamePhaseState"))
                    ClearGamePhaseState();
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("flushOldMapAssets"))
                    if (worldRuntime != null) await worldRuntime.FlushUnusedAssetsAsync(token);
                SetLoadingProgress(0f);
                Metin2AuthSession.GameLoginResult redirected;
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("connectGameCore"))
                    redirected = await ConnectGameCoreAsync(host, port, token);
                if (!redirected.Success) throw new InvalidOperationException("Game core login failed: " + redirected.Status);
                currentCharacters = redirected.CharacterList;
                SetLoadingProgress(10f);
                Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"DIRECT_ENTER_SELECT slot={index} reason={reason}");
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("select->PHASE_LOADING"))
                {
                    await gameSession.SendCharacterSelectAsync((byte)index, token);
                    await gameSession.WaitForGamePhaseAsync(Metin2ProtocolCodec.PhaseLoading, token);
                }
                SetLoadingProgress(30f);
                byte[] mainCharacterPacket;
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("waitMainCharacter"))
                    mainCharacterPacket = await gameSession.WaitForMainCharacterAsync(token);
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("sendClientVersion"))
                {
                    string platformVersionFilename = Application.platform == RuntimePlatform.Android ? "metin2client.android" :
                        Application.platform == RuntimePlatform.IPhonePlayer ? "metin2client.ios" : clientVersionFilename;
                    Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"DIRECT_ENTER_CLIENT_VERSION filename={platformVersionFilename} timestamp={clientVersionTimestamp}");
                    // CG_CLIENT_VERSION2 (0xf1) is what the EUROPE build sends; the server dispatches
                    // both headers to the same handler.
                    await gameSession.SendClientVersion2Async(platformVersionFilename, clientVersionTimestamp, token);
                }
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("ensureRuntime"))
                    EnsureRuntime();
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("preloadDestinationMap"))
                    if (worldRuntime != null)
                    {
                        Metin2GamePacketCodec.MainCharacter main = Metin2GamePacketCodec.ReadMainCharacter(mainCharacterPacket);
                        await worldRuntime.PreloadAtAsync(main.X, main.Y, token);
                    }
                Metin2ProtocolCodec.CharacterSummary selected = currentCharacters.Characters[index];
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("preloadSelectedPlayer"))
                {
                    var playerLoads = new List<Task>
                    {
                        Metin2PlayerPrefabs.PreloadAsync(selected.Job, Metin2ContentScope.Session, token),
                        Metin2RaceDataBinder.PreloadAsync(selected.Job, Metin2ContentScope.Map, token)
                    };
                    // The local player's own armour and hair, behind the loading screen rather than
                    // after SpawnLocalPlayer has already built the character. The inventory replay
                    // that follows still drives Metin2EquipmentRuntimeBinder, which loads anything
                    // this could not know about yet; this only removes the visible first-frame gap.
                    Metin2EquipmentCatalog enterEquipment = equipmentCatalog;
                    if (enterEquipment != null)
                    {
                        playerLoads.Add(enterEquipment.PreloadItemAsync((byte)selected.Job, selected.MainPart, Metin2ContentScope.Session, token));
                        playerLoads.Add(enterEquipment.PreloadArmorAsync((byte)selected.Job, selected.MainPart, Metin2ContentScope.Session, token));
                        playerLoads.Add(enterEquipment.PreloadHairAsync((byte)selected.Job, selected.HairPart, Metin2ContentScope.Session, token));
                    }
                    // GC_CREATE_FLY carries only an integer, so every indexed projectile must be
                    // resolvable the moment the packet can arrive. There are 16 rows and the whole
                    // flying closure is 8.7 MiB, so this is loaded once with the session rather than
                    // raced against a projectile spawn.
                    Metin2.Runtime.Metin2FlyingCatalog flyings =
                        Resources.Load<Metin2.Runtime.Metin2FlyingCatalog>("Metin2FlyingCatalog");
                    if (flyings != null) playerLoads.Add(flyings.PreloadIndexedAsync(Metin2ContentScope.Session, token));

                    // The old Resources catalog made every effect definition resident before the
                    // first frame. After Phase 8, synchronous consumers (damage digits, hit/critical
                    // flashes, click/target/quest markers and refine shines) need their small common
                    // families explicitly resident before gameplay starts. Session scope keeps these
                    // universal effects across warps without loading the whole effect archive.
                    Metin2.Runtime.Metin2EffectCatalog commonEffects =
                        Resources.Load<Metin2.Runtime.Metin2EffectCatalog>("Metin2EffectCatalog");
                    Task commonEffectLoad = commonEffects != null
                        ? commonEffects.PreloadCommonGameplayEffectsAsync(Metin2ContentScope.Session, token)
                        : Task.CompletedTask;

                    await Task.WhenAll(playerLoads);
                    try { await commonEffectLoad; }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception effectException)
                    {
                        Debug.LogWarning("METIN2_COMMON_EFFECT_PRELOAD_FAILED " + effectException.Message);
                    }

                    // Player jobs do not exist in the NPC/monster race-data catalog. Collect the
                    // selected prefab's actual MSA events and persistent affect loops directly so
                    // the first skill never becomes an Addressables preload trigger.
                    try
                    {
                        await Metin2RaceDataBinder.PreloadPlayerVisualEffectsAsync(
                            selected.Job, Metin2ContentScope.Map, token);
                    }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception effectException)
                    {
                        Debug.LogWarning($"METIN2_PLAYER_EFFECT_PRELOAD_FAILED job={selected.Job} {effectException.Message}");
                    }
                    try
                    {
                        // NPC/monster race data does not contain the eight player jobs.  Acquire
                        // the selected PC/PC2 family directly before SpawnLocalPlayer, then retain
                        // the race-data path for any player motion definitions that do exist.
                        await Metin2SoundManager.Instance.PreloadPlayerMotionAudioAsync(
                            selected.Job, Metin2ContentScope.Map, token);
                        await Metin2RaceDataBinder.PreloadMotionSoundsAsync(selected.Job, Metin2ContentScope.Map, token);
                    }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception audioException)
                    {
                        Debug.LogWarning($"METIN2_PLAYER_AUDIO_PRELOAD_FAILED race={selected.Job} {audioException.Message}");
                    }
                }
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("spawnLocalPlayer+LoadAt"))
                    SpawnLocalPlayer(index);
                SetLoadingProgress(80f);
                // The guild/party/messenger singletons have to be listening before the first
                // replay - see ConnectPersistentGameEvents. The replay lists are per character, so
                // a previous character's guild state must not survive into this one.
                guildReplay.Clear(); messengerReplay.Clear();
                characterWindowGuild = null; characterWindowGuildMaster = string.Empty;
                ConnectPersistentGameEvents();
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("replayDeferred#1"))
                    DispatchDeferredLoadingPackets();
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("waitInitialStream"))
                    if (worldRuntime != null)
                        await worldRuntime.WaitForInitialStreamAsync(token, ratio => SetLoadingProgress(80f + ratio * 10f));
                SetLoadingProgress(90f);
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("enterGame->PHASE_GAME"))
                {
                    Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, "DIRECT_ENTER_ENTERGAME");
                    await gameSession.SendEnterGameAsync(token);
                    await gameSession.SendMountFlightAsync(0,0,0,token);
                    await gameSession.WaitForGamePhaseAsync(Metin2ProtocolCodec.PhaseGame, token);
                }
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("replayDeferred#2"))
                    DispatchDeferredLoadingPackets();
                SetLoadingProgress(100f);

                selectedCharacterIndex = index; activeGameHost = host; activeGamePort = port;
                using (Metin2.Runtime.Metin2EnterWorldTiming.Measure("buildOriginalGameUi"))
                    BuildOriginalGameUi();
                // CPythonNetworkStream installs its phase window before it starts dispatching that
                // phase. Do the same: GC_SHOP must never arrive in the old receiver-before-panel
                // gap, otherwise the server considers the shop open while the client dropped START.
                Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, "DIRECT_ENTER_START_GAME_RUNTIME_RECEIVER");
                gameRuntime.StartReceiving(gameSession);
                inGame = true; ApplyGameUiVisibility();
                if (originalUiActive) originalUi.Hide();
                loadingScreen?.Hide();
                SetStatusLocale("UNITY_IN_GAME");
                // CPythonNetworkStream::__RecvGuildMark... the client opens a second connection to
                // the mark server once it is in the world and pulls every guild's emblem
                // (CGuildMarkDownloader). Without it no guild mark can ever be drawn.
                DownloadGuildMarks();
                Debug.Log($"METIN2_DIRECT_ENTER_SUCCESS reason={reason} host={host} port={port} slot={index}");
            }
            finally { gameTransitionInProgress = false; Metin2.Runtime.Metin2EnterWorldTiming.Report(); }
        }

        private void DispatchDeferredLoadingPackets()
        {
            if (gameSession == null || gameRuntime == null) return;
            byte[][] packets = gameSession.DrainDeferredGamePackets();
            if (!Metin2.Runtime.Metin2EnterWorldTiming.IsActive)
            {
                foreach (byte[] packet in packets) DispatchLoadingPacket(packet);
                return;
            }
            // The replay is one frame of work whose composition is invisible from the outside:
            // GC_CHARACTER_ADD spawns actors, item packets touch the catalogs. Attribute the cost
            // per packet header so a single dominant header is identifiable.
            var watch = new System.Diagnostics.Stopwatch();
            foreach (byte[] packet in packets)
            {
                watch.Restart();
                DispatchLoadingPacket(packet);
                watch.Stop();
                Metin2.Runtime.Metin2EnterWorldTiming.Add(
                    "packet:" + (packet != null && packet.Length > 0 ? packet[0].ToString() : "empty"),
                    watch.Elapsed.TotalMilliseconds);
            }
        }

        /// <summary>
        /// Dispatches one replayed loading packet, naming it if it throws.
        ///
        /// A codec exception here aborts the whole enter-world sequence, and the exception alone
        /// says which reader failed but not what it was given. Without the bytes, a desync costs a
        /// round trip to reproduce with tracing on. The packet is rethrown unchanged: a header the
        /// codec cannot read means the stream may be misaligned, and continuing past that would
        /// build the world from packets read at the wrong offsets.
        /// </summary>
        private void DispatchLoadingPacket(byte[] packet)
        {
            try { gameRuntime.DispatchPacket(packet); }
            catch (Exception exception)
            {
                Debug.LogError($"METIN2_LOADING_PACKET_FAILED header={(packet != null && packet.Length > 0 ? packet[0].ToString() : "empty")} " +
                               $"length={(packet == null ? 0 : packet.Length)} bytes={Hex(packet, 64)} {exception}");
                throw;
            }
        }

        /// <summary>Hex dump of at most <paramref name="limit"/> bytes, for framing diagnostics.</summary>
        private static string Hex(byte[] data, int limit)
        {
            if (data == null || data.Length == 0) return "-";
            int count = Math.Min(limit, data.Length);
            var text = new System.Text.StringBuilder(count * 3);
            for (int i = 0; i < count; i++) { if (i > 0) text.Append(' '); text.Append(data[i].ToString("X2")); }
            if (count < data.Length) text.Append(" ...");
            return text.ToString();
        }

        private void BeginLoadingPhase(string reason)
        {
            Metin2BroadcastBanner.Clear(ResolveUiHost());
            inGame = false; ApplyGameUiVisibility();
            if (inputController != null) inputController.enabled = false;
            if (gameUi != null) gameUi.HideOpenWindows(null);
            if (originalUiActive) originalUi.Hide();
            Canvas canvas = ResolveUiCanvas();
            if (loadingScreen == null)
            {
                loadingScreen = GetComponent<Metin2LoadingScreen>();
                if (loadingScreen == null) loadingScreen = gameObject.AddComponent<Metin2LoadingScreen>();
            }
            loadingScreen.Initialize(uiCatalog);
            if (!loadingScreen.Show(canvas == null ? null : ResolveFrontEndUiHost())) Debug.LogWarning("METIN2_LOADING_WINDOW_MISSING");
            // Keep the shipped locale key, including its historical spelling, so a
            // language change while entering the world refreshes the status as well.
            SetStatusLocale("LOGIN_CONNETING");
            Debug.Log("METIN2_LOADING_PHASE_BEGIN reason=" + reason);
        }

        private void SetLoadingProgress(float progress) => loadingScreen?.SetProgress(progress);

        /// <summary>
        /// Runs one teardown step and never lets it abort the remaining ones.
        ///
        /// `__LeaveGamePhase` (PythonNetworkStreamPhaseGame.cpp:920-943) destroys the actor,
        /// character and item managers unconditionally - a half torn-down phase is not a state the
        /// original can be in. The port had no such guarantee: a single throwing subsystem skipped
        /// every later step, and because <see cref="FailGameTransition"/> reruns the same routine
        /// it threw a second time and never reached `loadingScreen.Hide()` or the login phase. One
        /// stale event subscription therefore hung the client on the loading screen instead of
        /// showing LOGIN_CONNECT_FAILURE. The failure is logged loudly per step so a broken
        /// subsystem stays visible rather than silently swallowed.
        /// </summary>
        private static void TearDownStep(string name, Action step)
        {
            try { step(); }
            catch (Exception exception) { Debug.LogError($"METIN2_TEARDOWN_STEP_FAILED step={name} {exception}"); }
        }

        /// <summary>
        /// The old map's live GameObjects are torn down first, then these on-demand caches release
        /// their strong UnityEngine.Object references. FlushUnusedAssetsAsync yields until Destroy()
        /// has completed and only then runs Unity's unused-asset sweep.
        ///
        /// This is map-transition cleanup. It is intentionally NOT done per monster death: a dead
        /// monster is commonly respawned from the same race moments later, and unloading its prefab/
        /// clips on every death would turn normal combat into asset-load/GC stalls.
        /// </summary>
        private void ReleaseMapScopedAssetCaches()
        {
            // Panels are long-lived HUD objects. Drop their RawImage references before releasing
            // the Map-scope minimap/atlas Addressable root.
            gameUi?.GetPanel<Metin2MiniMapPanel>("minimap")?.ClearMapAsset();
            gameUi?.GetPanel<Metin2AtlasWindowPanel>("atlaswindow")?.ClearMapAsset();
            // The server's NPC/WARP list belongs to the map being left; the next map falls back to
            // its authored atlas points until its own GC_NPC_POSITION arrives.
            GetComponent<Metin2NpcPositionState>()?.Clear();
            // CPythonMiniMap::__LoadAtlasMarkInfo clears the guild areas on every atlas load, so a
            // previous map's lands never linger until the new map's GC_LAND_LIST arrives.
            GetComponent<Metin2GuildLandState>()?.Clear();
            Metin2RacePrefabs.ReleaseMapScopedAssets();
            Metin2RaceDataBinder.ReleaseMapScopedAssets();
            equipmentCatalog?.ReleaseMapScopedAssets();
            FindFirstObjectByType<Metin2SoundManager>()?.ReleaseMapScopedAssets();
            Metin2.Runtime.Metin2EffectRuntime.ReleaseMapScopedMaterials();
            // The effect and flying catalogs are lightweight metadata now, so loading them here is
            // free; what this drops is their resolved definition references, so the unused-asset
            // sweep that follows can free the particle meshes and textures of the map being left.
            // Never called per effect instance: a live particle system keeps its own reference.
            Resources.Load<Metin2.Runtime.Metin2EffectCatalog>("Metin2EffectCatalog")?.ReleaseMapScopedAssets();
            Resources.Load<Metin2.Runtime.Metin2FlyingCatalog>("Metin2FlyingCatalog")?.ReleaseMapScopedAssets();
            // ClearWorld/Unload above have scheduled destruction of every map-scoped runtime
            // object. The loading transition yields before the unused-assets sweep, so releasing
            // Addressables here cannot invalidate a live prefab, material, clip or AudioSource.
            Metin2.Runtime.Metin2ContentProvider.ReleaseMapScope();
        }

        private static void ReleaseSessionAssetCaches()
        {
            Metin2PlayerPrefabs.Reset();
            Metin2.Runtime.Metin2ContentProvider.ReleaseSessionScope();
        }

        private void ClearGamePhaseState()
        {
            if (localController != null && entityWorld != null)
                Metin2CharacterPreviewState.Save(mainCharacterName, mainCharacterRace, entityWorld.LocalParts, KnownMountPreviewRace(), KnownPetPreviewRace());
            locallyHitMetins.Clear();
            gameRuntime?.ClearLeaderboardData();
            activeComboType = 0;
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.SetComboActive(false);
            gameUi?.TaskBarPanel?.QuickSlots?.SetComboActive(false);
            // Every fresh character/game-core burst replays quick-slot registrations. Clear the
            // previous character's buffered icons even when the old phase ended by disconnect.
            TearDownStep("quickSlots", ClearQuickSlotsForCharacterChange);
            TearDownStep("guildRenewal", () => { guildRenewalPanel?.ClosePayDialog(); guildRenewalState?.Reset(); });
            TearDownStep("costumeVisibility", () =>
            {
                Array.Clear(hiddenCostumeParts,0,hiddenCostumeParts.Length);
                antiExpBlocked = false;
                mobileUiRoot?.SetAntiExpBlocked(false);
                for(int part=1;part<=4;part++)gameUi?.SetCostumeHidden(part,false);
            });
            TearDownStep("guildMarkTransfer", StopGuildMarkTransfer);
            TearDownStep("stopReceiving", () => gameRuntime?.StopReceiving());
            TearDownStep("entityWorld", () => entityWorld?.ClearWorld());
            TearDownStep("groundItemWorld", () => groundItemWorld?.ClearWorld());
            TearDownStep("playerState", () => playerState?.ClearSessionState());
            TearDownStep("statusEffects", () => statusEffectsState?.ClearSessionState());
            TearDownStep("flyState", () => GetComponent<Metin2FlyState>()?.ClearSessionState());
            TearDownStep("whisperState", ClearWhisperState);
            TearDownStep("targetBoard", () => targetBoard?.Close());
            TearDownStep("advancedDuel", () => advancedDuel?.HandleCommand(new[]{"BINARY_Duel_Delete"}));
            TearDownStep("guildBuild", () => GetComponent<Metin2GuildBuildDirector>()?.Teardown());
            TearDownStep("questButtons", () => questButtonPanel?.Clear());
            TearDownStep("chatWindow", () => chatWindow?.SetGameplayVisible(false));
            TearDownStep("itemShop", () => itemShopPanel?.Close());
            TearDownStep("offlineShopManager", () => { offlineShopPanel?.CloseLocal(false); aurigaOfflineShopState?.Reset(); });
            TearDownStep("offlineShopCustomer", () => offlineShopCustomerPanel?.CloseLocal(false));
            TearDownStep("offlineShopDialogs", CloseOfflineShopDialogs);
            TearDownStep("attachMetinDialog", () => { if (attachMetinDialog != null) Destroy(attachMetinDialog.gameObject); attachMetinDialog = null; });
            TearDownStep("costumeAttrRemoveDialog", () => { if (costumeAttrRemoveDialog != null) Destroy(costumeAttrRemoveDialog.gameObject); costumeAttrRemoveDialog = null; });
            TearDownStep("worldRuntime", () => worldRuntime?.Unload());
            TearDownStep("localController", () =>
            {
                if (localController != null) Destroy(localController.gameObject);
            });
            localController = null; localMountEntity = null; combatController = null; inputController = null;
            TearDownStep("flyingInstances", () =>
            {
                foreach (Metin2.Runtime.Metin2FlyingInstance fly in FindObjectsOfType<Metin2.Runtime.Metin2FlyingInstance>(true))
                    if (fly != null) Destroy(fly.gameObject);
            });
            TearDownStep("mapScopedAssetCaches", ReleaseMapScopedAssetCaches);
        }

        /// <summary>
        /// Point 17 contract item 4: a failed transition disposes the session, clears actor/item/
        /// player/map state, shows the login phase and displays LOGIN_CONNECT_FAILURE. Every step
        /// is guarded, because the one thing this routine may never do is leave the player looking
        /// at a loading screen with no session behind it and no way back to login.
        /// </summary>
        private void FailGameTransition(Exception exception)
        {
            gameUi?.CancelLootTeleport();
            intentionalExitCommand = null;
            Debug.LogError("METIN2_GAME_TRANSITION_FAILED " + exception);
            // A dropped/rejected game-core connection returns to the same login screen through
            // intrologin.OnConnectFailure, whose audible outcome is loginfail.wav.
            Metin2SoundManager.Instance.PlaySound("sound/ui/loginfail.wav");
            TearDownStep("failStopReceiving", () => gameRuntime?.StopReceiving());
            TearDownStep("failDisposeSession", () => { gameSession?.Dispose(); gameSession = null; });
            TearDownStep("failReleaseSessionAssets", ReleaseSessionAssetCaches);
            // A receive/framing failure is also a game-phase exit.  Hide the imported original
            // windows before any quest/dialog close callback can restore its previous snapshot.
            TearDownStep("failHideWindows", () => gameUi?.HideOpenWindows(null));
            TearDownStep("failClearPhase", ClearGamePhaseState);
            TearDownStep("failGuildMarkStore", Metin2GuildMarkStore.Clear);
            TearDownStep("failHideLoading", () => loadingScreen?.Hide());
            inGame = false;
            gameTransitionInProgress = false;
            selectedCharacterIndex = -1;
            activeGameHost = string.Empty;
            activeGamePort = 0;
            currentCharacters = default;
            accountLoginKey = 0;
            TearDownStep("failGameUiVisibility", ApplyGameUiVisibility);
            TearDownStep("failShowLogin", () => { if (originalUiActive) originalUi.ShowLogin(); });
            TearDownStep("failMessage", () => { SetStatusLocale("LOGIN_CONNECT_FAILURE"); PopupMessageLocale("LOGIN_CONNECT_FAILURE"); });
        }

        private void EnsureRuntime()
        {
            if (playerState == null) playerState = GetComponent<Metin2PlayerState>() ?? gameObject.AddComponent<Metin2PlayerState>();
            if (entityWorld == null) entityWorld = GetComponent<Metin2NetworkEntityWorld>() ?? gameObject.AddComponent<Metin2NetworkEntityWorld>();
            entityWorld.Initialize(null, null, equipmentCatalog, null, itemCatalog);
            // GC_MOVE.dwTime uses the handshake-synchronised server-frame clock. The original
            // remote actor queue (PushTCPState/StateProcess) is only deterministic when packet and
            // playback timestamps share that clock.
            entityWorld.TimeProvider = () => gameSession != null
                ? gameSession.ServerTimeMilliseconds
                : unchecked((uint)Environment.TickCount);
            entityWorld.BindTextTailLocale(localeText);
            if (worldRuntime == null) worldRuntime = GetComponent<Metin2.Runtime.Metin2WorldRuntime>() ?? gameObject.AddComponent<Metin2.Runtime.Metin2WorldRuntime>(); entityWorld.BindWorld(worldRuntime);
            if (gameRuntime == null) gameRuntime = GetComponent<Metin2GameRuntime>() ?? gameObject.AddComponent<Metin2GameRuntime>();
            if (groundItemWorld == null) groundItemWorld = GetComponent<Metin2GroundItemWorld>() ?? gameObject.AddComponent<Metin2GroundItemWorld>();
            groundItemWorld.Initialize(itemCatalog, worldRuntime);
            groundItemWorld.PickupRequested -= SendItemPickup; groundItemWorld.PickupRequested += SendItemPickup;
            gameRuntime.Initialize(playerState, entityWorld, groundItemWorld);
            // Existing shop signs are replayed with ENTER_GAME before the offline-shop windows
            // are built. Subscribe now so nearby shops (including premium boards) are not lost.
            if (offlineShopWorldSigns == null)
                offlineShopWorldSigns = GetComponent<Metin2OfflineShopWorldSigns>() ?? gameObject.AddComponent<Metin2OfflineShopWorldSigns>();
            offlineShopWorldSigns.Bind(gameRuntime, entityWorld);
            gameRuntime.ShopChanged -= HandleNpcShopPacket;
            gameRuntime.ShopChanged += HandleNpcShopPacket;
            if (damageTextTail == null) damageTextTail = GetComponent<Metin2DamageTextTail>() ?? gameObject.AddComponent<Metin2DamageTextTail>();
            damageTextTail.Bind(playerState, gameRuntime, entityWorld, localController == null ? null : localController.transform);
            damageTextTail.BindLocale(localeText);
            gameRuntime.MainCharacterReceived -= HandleMainCharacter; gameRuntime.MainCharacterReceived += HandleMainCharacter;
            gameRuntime.PhaseChanged -= HandleGamePhase; gameRuntime.PhaseChanged += HandleGamePhase;
            // RecvTargetPacket -> SetHPTargetBoard is the authoritative live HP route. The codec
            // already decoded GC_TARGET, but the board previously had no subscriber at all.
            gameRuntime.TargetChanged -= HandleServerTarget; gameRuntime.TargetChanged += HandleServerTarget;
            gameRuntime.TargetHealthChanged -= HandleServerTargetHealth;gameRuntime.TargetHealthChanged += HandleServerTargetHealth;
            gameRuntime.WarpRequested -= HandleWarp; gameRuntime.WarpRequested += HandleWarp;
            gameRuntime.ReceiveFailed -= HandleReceiveFailure; gameRuntime.ReceiveFailed += HandleReceiveFailure;
            gameRuntime.ExchangeReceived -= HandleExchange; gameRuntime.ExchangeReceived += HandleExchange;
            gameRuntime.StorageReceived -= HandleStorage; gameRuntime.StorageReceived += HandleStorage;
            if (pvpState == null) pvpState = GetComponent<Metin2PvpState>() ?? gameObject.AddComponent<Metin2PvpState>();
            gameRuntime.DuelStarted -= pvpState.Apply; gameRuntime.DuelStarted += pvpState.Apply;
            gameRuntime.DuelStarted -= HandleDuelStartedTargetBoard; gameRuntime.DuelStarted += HandleDuelStartedTargetBoard;
            gameRuntime.PvpChanged -= pvpState.Apply; gameRuntime.PvpChanged += pvpState.Apply;
            entityWorld.BindTextTailRelationships(pvpState, GetComponent<Metin2PartyPanel>(),
                GuildPanelOrNull());
            Metin2ItemUseState itemUseState = GetComponent<Metin2ItemUseState>() ?? gameObject.AddComponent<Metin2ItemUseState>();
            gameRuntime.ItemUsePresented -= itemUseState.Apply; gameRuntime.ItemUsePresented += itemUseState.Apply;
            if (staminaState == null) staminaState = GetComponent<Metin2StaminaState>() ?? gameObject.AddComponent<Metin2StaminaState>();
            staminaState.Bind(playerState);
            Metin2StatusEffectsState statusEffects = statusEffectsState = GetComponent<Metin2StatusEffectsState>() ?? gameObject.AddComponent<Metin2StatusEffectsState>();
            gameRuntime.FishingReceived -= statusEffects.ApplyFishing; gameRuntime.FishingReceived += statusEffects.ApplyFishing;
            gameRuntime.FishingReceived -= HandleFishingMotion; gameRuntime.FishingReceived += HandleFishingMotion;
            gameRuntime.WalkModeReceived -= statusEffects.ApplyWalkMode; gameRuntime.WalkModeReceived += statusEffects.ApplyWalkMode;
            statusEffects.WalkModeChanged -= HandleWalkMode; statusEffects.WalkModeChanged += HandleWalkMode;
            gameRuntime.AffectAdded -= statusEffects.AddAffect; gameRuntime.AffectAdded += statusEffects.AddAffect;
            gameRuntime.AffectRemoved -= statusEffects.RemoveAffect; gameRuntime.AffectRemoved += statusEffects.RemoveAffect;
            // Original uiminimap.py falls back to chr.IsOverfallPremium().  Character ADD/UPDATE
            // carries that packed flag (CInstanceBase::AFFECT_OVERFALL_PREMIUM == bit 41), so bind
            // the same route instead of trusting only the textual OverfallPremiumInfo command.
            entityWorld.LocalAffectFlagsChanged -= HandleLocalAffectFlagsChanged;
            entityWorld.LocalAffectFlagsChanged += HandleLocalAffectFlagsChanged;
            HandleLocalAffectFlagsChanged(entityWorld.LocalAffectFlags);
            Metin2QuestTargetState questTargets = GetComponent<Metin2QuestTargetState>() ?? gameObject.AddComponent<Metin2QuestTargetState>();
            gameRuntime.QuestTargetCreated -= questTargets.Create; gameRuntime.QuestTargetCreated += questTargets.Create;
            gameRuntime.QuestTargetUpdated -= questTargets.UpdateTarget; gameRuntime.QuestTargetUpdated += questTargets.UpdateTarget;
            gameRuntime.QuestTargetDeleted -= questTargets.Delete; gameRuntime.QuestTargetDeleted += questTargets.Delete;
            Metin2QuestTargetMarkers questMarkers = GetComponent<Metin2QuestTargetMarkers>() ?? gameObject.AddComponent<Metin2QuestTargetMarkers>();
            questMarkers.Bind(questTargets, entityWorld, worldRuntime);
            dungeonInfoState = GetComponent<Metin2DungeonState>() ?? gameObject.AddComponent<Metin2DungeonState>();
            gameRuntime.DungeonReceived -= dungeonInfoState.Apply; gameRuntime.DungeonReceived += dungeonInfoState.Apply;
            dungeonInfoState.QuestInputRequested -= SendDungeonQuestInput; dungeonInfoState.QuestInputRequested += SendDungeonQuestInput;
            dungeonInfoState.RankingOpenRequested -= OpenDungeonRanking; dungeonInfoState.RankingOpenRequested += OpenDungeonRanking;
            loverMiningState = GetComponent<Metin2LoverMiningState>() ?? gameObject.AddComponent<Metin2LoverMiningState>();
            gameRuntime.LoverReceived -= loverMiningState.ApplyLover; gameRuntime.LoverReceived += loverMiningState.ApplyLover;
            gameRuntime.LoverReceived -= HandleLoverInfo; gameRuntime.LoverReceived += HandleLoverInfo;
            gameRuntime.LovePointChanged -= loverMiningState.ApplyLovePoint; gameRuntime.LovePointChanged += loverMiningState.ApplyLovePoint;
            gameRuntime.LovePointChanged -= HandleLovePoint; gameRuntime.LovePointChanged += HandleLovePoint;
            gameRuntime.DigMotionReceived -= loverMiningState.ApplyDig; gameRuntime.DigMotionReceived += loverMiningState.ApplyDig;
            gameRuntime.DigMotionReceived -= HandleDigMotion; gameRuntime.DigMotionReceived += HandleDigMotion;
            sessionState = GetComponent<Metin2SessionState>();if(sessionState==null)sessionState=gameObject.AddComponent<Metin2SessionState>();
            gameRuntime.ServerTimeReceived -= sessionState.ApplyTime; gameRuntime.ServerTimeReceived += sessionState.ApplyTime;
            gameRuntime.SkillGroupChanged -= sessionState.ApplySkillGroup; gameRuntime.SkillGroupChanged += sessionState.ApplySkillGroup;
            gameRuntime.SkillGroupChanged -= HandleSkillGroupChanged; gameRuntime.SkillGroupChanged += HandleSkillGroupChanged;
            gameRuntime.SkillCooldownEnded -= HandleSkillCooldownEnded; gameRuntime.SkillCooldownEnded += HandleSkillCooldownEnded;
            gameRuntime.ChannelReceived -= sessionState.ApplyChannel; gameRuntime.ChannelReceived += sessionState.ApplyChannel;
            gameRuntime.PointChanged -= HandlePointChange; gameRuntime.PointChanged += HandlePointChange;
            gameRuntime.SyncPositionsReceived -= entityWorld.ApplySyncPositions; gameRuntime.SyncPositionsReceived += entityWorld.ApplySyncPositions;
            gameRuntime.OwnershipReceived -= entityWorld.ApplyOwnership; gameRuntime.OwnershipReceived += entityWorld.ApplyOwnership;
            entityWorld.PushPositionReady -= SendPushSyncPosition; entityWorld.PushPositionReady += SendPushSyncPosition;
            entityWorld.EntityDied -= RecordHitMetinDeath; entityWorld.EntityDied += RecordHitMetinDeath;
            entityWorld.EntityDied -= HandleTargetEntityInvalidated; entityWorld.EntityDied += HandleTargetEntityInvalidated;
            entityWorld.EntityRemoved -= HandleTargetEntityInvalidated; entityWorld.EntityRemoved += HandleTargetEntityInvalidated;
            Metin2FlyState flyState = GetComponent<Metin2FlyState>() ?? gameObject.AddComponent<Metin2FlyState>();
            flyState.Bind(entityWorld);
            gameRuntime.FlyTargetReceived -= flyState.ApplyTarget; gameRuntime.FlyTargetReceived += flyState.ApplyTarget;
            gameRuntime.FlyCreated -= flyState.ApplyCreate; gameRuntime.FlyCreated += flyState.ApplyCreate;
            Metin2NpcPositionState npcPositions = GetComponent<Metin2NpcPositionState>() ?? gameObject.AddComponent<Metin2NpcPositionState>();
            gameRuntime.NpcPositionsReceived -= npcPositions.Replace; gameRuntime.NpcPositionsReceived += npcPositions.Replace;
            Metin2GuildLandState guildLands = GetComponent<Metin2GuildLandState>() ?? gameObject.AddComponent<Metin2GuildLandState>();
            gameRuntime.GuildLandsReceived -= guildLands.Replace; gameRuntime.GuildLandsReceived += guildLands.Replace;
            // GC_GUILD INFO sub 8 with ADVANCED_GUILD_INFO: trophies + war counters. The guild
            // window may not exist yet, so the last packet is kept for the window creation path.
            gameRuntime.GuildInfoReceived -= OnGuildInfoPacket; gameRuntime.GuildInfoReceived += OnGuildInfoPacket;
            // GC_SYMBOL_DATA (133): the guild symbol bitmap the CRC probe asked for.
            gameRuntime.GuildSymbolDataReceived -= OnGuildSymbolData; gameRuntime.GuildSymbolDataReceived += OnGuildSymbolData;
            guildRenewalState = GetComponent<Metin2GuildRenewalState>() ?? gameObject.AddComponent<Metin2GuildRenewalState>();
            // GC_SWITCHBOT (171): the table update drives the M9 switchbot board.
            gameRuntime.SwitchbotReceived -= OnSwitchbotPacket; gameRuntime.SwitchbotReceived += OnSwitchbotPacket;
            // GC_BOSS_POSITION (222) -> minimap boss marks; GC_ITEM_ON_TITLE (246) -> actor title.
            gameRuntime.BossPositionsReceived -= OnBossPositions; gameRuntime.BossPositionsReceived += OnBossPositions;
            gameRuntime.ItemOnTitleReceived -= OnItemOnTitle; gameRuntime.ItemOnTitleReceived += OnItemOnTitle;
            gameRuntime.FakeShopSignReceived -= OnFakeShopSign; gameRuntime.FakeShopSignReceived += OnFakeShopSign;
            // GC_FISHING_NEW (223) and GC_ACCE (215) feed the M9 boards.
            gameRuntime.FishingNewReceived -= OnFishingNewPacket; gameRuntime.FishingNewReceived += OnFishingNewPacket;
            gameRuntime.AcceReceived -= OnAccePacket; gameRuntime.AcceReceived += OnAccePacket;
            // GC_EVENT_MANAGER (147): the event calendar LOAD envelope.
            gameRuntime.EventManagerReceived -= OnEventManagerPacket; gameRuntime.EventManagerReceived += OnEventManagerPacket;
            Metin2SpecificEffectRuntime specificEffects = GetComponent<Metin2SpecificEffectRuntime>() ?? gameObject.AddComponent<Metin2SpecificEffectRuntime>();
            specificEffects.Bind(entityWorld);
            gameRuntime.SpecificEffectReceived -= specificEffects.Apply; gameRuntime.SpecificEffectReceived += specificEffects.Apply;
            Metin2DragonSoulRefineState dragonSoulRefine = GetComponent<Metin2DragonSoulRefineState>() ?? gameObject.AddComponent<Metin2DragonSoulRefineState>();
            gameRuntime.DragonSoulRefineReceived -= dragonSoulRefine.Apply; gameRuntime.DragonSoulRefineReceived += dragonSoulRefine.Apply;
            gameRuntime.WikiReceived -= OnWikiResponse; gameRuntime.WikiReceived += OnWikiResponse;
            itemShopState = GetComponent<Metin2ItemShopState>() ?? gameObject.AddComponent<Metin2ItemShopState>();
            gameRuntime.ItemShopReceived -= itemShopState.Apply; gameRuntime.ItemShopReceived += itemShopState.Apply;
            // Auriga /ishop answers on GC_ITEMSHOP (93) with the TIShopData payload.
            gameRuntime.ItemShopPayloadReceived -= itemShopState.ApplyAuriga; gameRuntime.ItemShopPayloadReceived += itemShopState.ApplyAuriga;
            // NEW_PET_SYSTEM state: the values arrive on the command channel (RunServerCommand).
            petState = GetComponent<Metin2PetState>() ?? gameObject.AddComponent<Metin2PetState>();
            petState.Unsummoned -= OnPetUnsummoned; petState.Unsummoned += OnPetUnsummoned;
            petState.IncubatorRequested -= OnPetIncubatorRequested; petState.IncubatorRequested += OnPetIncubatorRequested;
            // GC_MOUNT_INVENTORY (224): the mount bag is a full snapshot, stored on the shared
            // player state under window 5 (Auriga EWindows::MOUNT_INVENTORY).
            if (playerState != null)
            {
                gameRuntime.MountInventoryReceived -= playerState.ApplyMountInventory;
                gameRuntime.MountInventoryReceived += playerState.ApplyMountInventory;
            }
            gameRuntime.MountInventoryReceived -= OnMountInventoryReceived;
            gameRuntime.MountInventoryReceived += OnMountInventoryReceived;
            Metin2ShopSearchState shopSearch = GetComponent<Metin2ShopSearchState>() ?? gameObject.AddComponent<Metin2ShopSearchState>();
            gameRuntime.ShopSearchResultReceived -= shopSearch.Add; gameRuntime.ShopSearchResultReceived += shopSearch.Add;
            gameRuntime.GuildCreationRequested -= ShowGuildCreationDialog; gameRuntime.GuildCreationRequested += ShowGuildCreationDialog;
            gameRuntime.ShopSignReceived -= ApplyPrivateShopSign; gameRuntime.ShopSignReceived += ApplyPrivateShopSign;
            if (questState == null) questState = GetComponent<Metin2QuestState>() ?? gameObject.AddComponent<Metin2QuestState>();
            gameRuntime.QuestInfoReceived -= questState.Apply; gameRuntime.QuestInfoReceived += questState.Apply;
            // Quest letters are an ENTER_GAME one-shot replay. Subscribe before StartReceiving;
            // BuildOriginalGameUi happens afterwards and is too late to be their first listener.
            gameRuntime.QuestScriptReceived -= ShowQuestDialog;
            gameRuntime.QuestScriptReceived += ShowQuestDialog;
            // Subscribed here, before StartReceiving() is ever called, so the ENTER_GAME quick-slot
            // replay cannot arrive with nobody listening (see quickSlotState above).
            gameRuntime.QuickSlotAdded -= ApplyQuickSlot; gameRuntime.QuickSlotAdded += ApplyQuickSlot;
            gameRuntime.QuickSlotDeleted -= ClearQuickSlot; gameRuntime.QuickSlotDeleted += ClearQuickSlot;
            gameRuntime.QuickSlotSwapped -= SwapQuickSlot; gameRuntime.QuickSlotSwapped += SwapQuickSlot;
            EnsurePlayerUi();
        }

        private void EnsurePlayerUi()
        {
            Canvas canvas = ResolveUiCanvas(); if (canvas == null) return;
            Transform uiHost = ResolveUiHost() ?? canvas.transform;
            Metin2HudPanel hud = canvas.GetComponentInChildren<Metin2HudPanel>(true);
            if (hud == null) { RectTransform rect = CreateRect("PlayerHud", uiHost, new Vector2(240, 95), Vector2.zero); rect.anchorMin = rect.anchorMax = new Vector2(0, 1); rect.pivot = new Vector2(0, 1); rect.anchoredPosition = new Vector2(20, -20); hud = rect.gameObject.AddComponent<Metin2HudPanel>(); }
            hud.Bind(playerState);
            Metin2InventoryPanel inventory = canvas.GetComponentInChildren<Metin2InventoryPanel>(true);
            if (inventory == null) { RectTransform rect = CreateRect("Inventory", uiHost, new Vector2(230, 800), Vector2.zero); rect.anchorMin = rect.anchorMax = new Vector2(1, 1); rect.pivot = new Vector2(1, 1); rect.anchoredPosition = new Vector2(-20, -20); inventory = rect.gameObject.AddComponent<Metin2InventoryPanel>(); }
            inventory.WorldReleaseRequested -= HandleInventoryWorldRelease; inventory.WorldReleaseRequested += HandleInventoryWorldRelease;
            inventory.ItemLinkRequested -= InsertInventoryChatLink; inventory.ItemLinkRequested += InsertInventoryChatLink;
            inventory.Bind(playerState, itemCatalog);
            inventory.UseRequested -= UseInventoryItem; inventory.UseRequested += UseInventoryItem;
            inventory.MoveRequested -= MoveInventoryItem; inventory.MoveRequested += MoveInventoryItem;
            inventory.UseToItemRequested -= UseInventoryItemOnItem; inventory.UseToItemRequested += UseInventoryItemOnItem;
            inventory.AttachMetinRequested -= OpenAttachMetinDialog; inventory.AttachMetinRequested += OpenAttachMetinDialog;
            inventory.RefineRequested -= OpenRefineForScroll; inventory.RefineRequested += OpenRefineForScroll;
            Metin2ShopPanel shop = canvas.GetComponentInChildren<Metin2ShopPanel>(true);
            if (shop == null) { RectTransform rect = CreateRect("Shop", uiHost, new Vector2(400, 650), Vector2.zero); shop = rect.gameObject.AddComponent<Metin2ShopPanel>(); }
            shop.Bind(gameRuntime, itemCatalog); shop.BuyRequested -= BuyShopItem; shop.BuyRequested += BuyShopItem;
            Metin2QuestPanel quest = canvas.GetComponentInChildren<Metin2QuestPanel>(true);
            if (quest == null) { RectTransform rect = CreateRect("QuestDialog", uiHost, new Vector2(540, 540), Vector2.zero); rect.gameObject.AddComponent<Image>().color = new Color(.04f, .05f, .07f, .94f); quest = rect.gameObject.AddComponent<Metin2QuestPanel>(); }
            quest.Bind(gameRuntime, questState); quest.AnswerRequested -= AnswerQuest; quest.AnswerRequested += AnswerQuest; quest.ConfirmationRequested -= ConfirmQuest; quest.ConfirmationRequested += ConfirmQuest; quest.gameObject.SetActive(false);
            questPanel = quest;
            Metin2ChatCombatPanel chat = canvas.GetComponentInChildren<Metin2ChatCombatPanel>(true);
            if (chat == null) { RectTransform rect = CreateRect("ChatCombat", uiHost, new Vector2(500, 230), Vector2.zero); rect.anchorMin = rect.anchorMax = new Vector2(0, 0); rect.pivot = new Vector2(0, 0); rect.anchoredPosition = new Vector2(20, 20); chat = rect.gameObject.AddComponent<Metin2ChatCombatPanel>(); }
            // The chat half of this legacy panel was retired: the real chat window (uichat.py
            // parity) owns input, output and markup. Only the combat readout stays bound.
            chat.Bind(gameRuntime, playerState);
            chatPanel = chat;
            Metin2ViewEquipmentPanel viewEquipment = canvas.GetComponentInChildren<Metin2ViewEquipmentPanel>(true);
            if (viewEquipment == null) { RectTransform rect = CreateRect("ViewEquipment", uiHost, new Vector2(380, 500), Vector2.zero); rect.gameObject.AddComponent<Image>().color = new Color(.04f, .05f, .07f, .97f); viewEquipment = rect.gameObject.AddComponent<Metin2ViewEquipmentPanel>(); }
            viewEquipment.Bind(gameRuntime, itemCatalog);
            viewEquipment.BindToolTips(EnsureToolTips());
            // Offline shop is bound later from its four imported original prefabs. A generated
            // fallback here used to subscribe a second consumer and made manager/customer snapshots
            // overwrite each other.

            gameUiPanels.Clear();
            gameUiPanels.Add(hud.gameObject); gameUiPanels.Add(inventory.gameObject); gameUiPanels.Add(shop.gameObject);
            gameUiPanels.Add(quest.gameObject); gameUiPanels.Add(chat.gameObject);
            gameUiPanels.Add(viewEquipment.gameObject);
            // Marked so the original UI can remove them wholesale; see Metin2PlaceholderUi.
            // The chat panel is deliberately NOT marked: no original window is bound to chat yet, and
            // destroying it would silently drop the chat log. It is hidden instead.
            // The quest dialog placeholder IS marked now that Metin2QuestDialogPanel replaces it -
            // the rule stays "remove only what has a real replacement".
            foreach (GameObject panel in gameUiPanels)
            {
                if (panel == null) continue;
                if (chatPanel != null && panel == chatPanel.gameObject) continue;
                if (panel.GetComponent<Metin2PlaceholderUi>() == null) panel.AddComponent<Metin2PlaceholderUi>();
            }
            ApplyGameUiVisibility();
        }

        /// <summary>
        /// The gameplay panels are created up front so the bootstrap validators can inspect them,
        /// but they must not cover the login and character-selection screens. Visibility is driven
        /// through a CanvasGroup rather than SetActive so every existing lookup keeps working.
        /// </summary>
        private void ApplyGameUiVisibility()
        {
            mobileUiRoot?.SetGameplayVisible(inGame);
            chatWindow?.SetGameplayVisible(inGame);
            affectShower?.SetGameplayVisible(inGame);
            foreach (GameObject panel in gameUiPanels)
            {
                if (panel == null) continue;
                // Unity's null-coalescing operator bypasses the overloaded == comparison, so a
                // missing component would be returned as a fake-null instead of being created.
                CanvasGroup group = panel.GetComponent<CanvasGroup>();
                if (group == null) group = panel.AddComponent<CanvasGroup>();
                group.alpha = inGame ? 1f : 0f;
                group.blocksRaycasts = inGame;
                group.interactable = inGame;
            }
        }

        private void BuyShopItem(byte position) => BuyFromShop(position);
        private void HandleLocalAffectFlagsChanged(ulong flags)
        {
            premiumState.SetAffectActive((flags & (1UL << 41)) != 0UL);
            toolTipController?.SetGameMaster((flags & 1UL) != 0);
        }

        /// <summary>
        /// CPythonNetworkStream::__CanActMainInstance.  This server's GC_STUN main-actor path uses
        /// the same dead/non-actionable state bit as Die(), so LocalStateFlags is the authoritative
        /// Unity-side gate for every packet the original protects with CanAct().
        /// </summary>
        private bool CanActMainInstance()
        {
            if (entityWorld == null || entityWorld.LocalPlayerRoot == null) return false;
            // CActorInstance::CanAct() blocks dead/stun/sleep/faint too.  The combat controller
            // already folds the authoritative HP, AFFECT_STUN and knock-down/die motion state into
            // the same predicate used by skill input, so reuse it as the central packet gate.
            return combatController != null
                ? combatController.CanActMainInstance
                : (entityWorld.LocalStateFlags & 1) == 0;
        }

        private void PlayItemSound(byte window, ushort cell, bool use)
        {
            if (playerState == null || itemCatalog == null ||
                !playerState.TryGetItem(window, cell, out Metin2GamePacketCodec.ItemData item) ||
                !itemCatalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry definition)) return;
            string sound = use ? Metin2ItemSoundRules.GetUseSound(definition) : Metin2ItemSoundRules.GetDropSound(definition);
            if (!string.IsNullOrEmpty(sound)) Metin2SoundManager.Instance.PlaySound(sound);
        }

        private void PlayStorageItemDropSound(byte storageWindow, ushort position)
        {
            foreach (Metin2StoragePanel storage in FindObjectsOfType<Metin2StoragePanel>(true))
            {
                if (storage.Window != storageWindow || !storage.TryGetItem(position, out Metin2GamePacketCodec.ItemData item) ||
                    itemCatalog == null || !itemCatalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry definition)) continue;
                Metin2SoundManager.Instance.PlaySound(Metin2ItemSoundRules.GetDropSound(definition));
                return;
            }
        }

        private async void UseInventoryItem(byte window, ushort cell)
        {
            if (!CanActMainInstance()) return;
            try
            {
                PlayItemSound(window, cell, true);
                // uiinventory.py:3218-3220: using the inventory-expansion key (72320) opens the
                // confirmation whose accept sends ENVANTER_BLACK (net.Envanter_expansion).
                if (window == 1 && playerState != null &&
                    playerState.TryGetItem(window, cell, out Metin2GamePacketCodec.ItemData expansion) &&
                    expansion.Vnum == 72320)
                    AskQuestion2Locale("ENVANTER_EXPANS_1", string.Empty, SendInventoryExpansion);
                if (gameSession != null) await gameSession.SendItemUseAsync(window, cell, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendInventoryExpansion()
        {
            if (!CanActMainInstance()) return;
            try { if (gameSession != null) await gameSession.SendInventoryExpansionAsync(lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void MoveInventoryItem(byte fromWindow, ushort fromCell, byte toWindow, ushort toCell, int count) { if (!CanActMainInstance()) return; try { PlayItemSound(fromWindow, fromCell, false); if (gameSession != null) await gameSession.SendItemMoveAsync(fromWindow, fromCell, toWindow, toCell, count, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void UseInventoryItemOnItem(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell)
        {
            if (!CanActMainInstance()) return;
            // uiinventory.py:2372-2376: USE_REMOVE_ATTR_COSTUME opens the attribute dialog first;
            // the use-to-item packet is only sent once the player accepts a slot.
            if (IsCostumeAttrRemoveItem(sourceWindow, sourceCell))
            {
                OpenCostumeAttrRemoveDialog(sourceWindow, sourceCell, targetWindow, targetCell);
                return;
            }
            try { if (gameSession != null) await gameSession.SendItemUseToItemAsync(sourceWindow, sourceCell, targetWindow, targetCell, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2CostumeAttrRemoveDialog costumeAttrRemoveDialog;
        private bool IsCostumeAttrRemoveItem(byte window, ushort cell)
        {
            if (itemCatalog == null || playerState == null) return false;
            if (!playerState.TryGetItem(window, cell, out Metin2GamePacketCodec.ItemData item)) return false;
            return itemCatalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) &&
                   entry.subType == Metin2ItemConstants.UseRemoveAttrCostume;
        }

        private void OpenCostumeAttrRemoveDialog(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell)
        {
            if (costumeAttrRemoveDialog == null)
            {
                costumeAttrRemoveDialog = Metin2CostumeAttrRemoveDialog.Create(ResolveUiHost(), itemCatalog, playerState, EnsureToolTips());
                costumeAttrRemoveDialog.ChatRequested -= SendChat; costumeAttrRemoveDialog.ChatRequested += SendChat;
                costumeAttrRemoveDialog.UseToItemRequested -= SendItemUseToItem;
                costumeAttrRemoveDialog.UseToItemRequested += SendItemUseToItem;
            }
            costumeAttrRemoveDialog.SetToolTips(EnsureToolTips());
            costumeAttrRemoveDialog.Open(sourceWindow, sourceCell, targetWindow, targetCell);
        }

        private async void SendItemUseToItem(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell)
        {
            try { if (gameSession != null) await gameSession.SendItemUseToItemAsync(sourceWindow, sourceCell, targetWindow, targetCell, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2AttachMetinDialog attachMetinDialog;
        private void OpenAttachMetinDialog(ushort sourceCell, ushort targetCell)
            => OpenAttachMetinDialog(1, sourceCell, 1, targetCell);

        private void OpenAttachMetinDialog(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell)
        {
            if (!CanActMainInstance() || playerState == null || itemCatalog == null ||
                !playerState.TryGetItem(sourceWindow, sourceCell, out Metin2GamePacketCodec.ItemData source) ||
                !playerState.TryGetItem(targetWindow, targetCell, out Metin2GamePacketCodec.ItemData target) ||
                !itemCatalog.TryGet(source.Vnum, out Metin2ItemCatalog.Entry stone) ||
                !itemCatalog.TryGet(target.Vnum, out Metin2ItemCatalog.Entry equipment)) return;

            Metin2AttachMetinResult result = Metin2AttachMetinPolicy.Evaluate(stone, equipment, targetCell, target.Sockets);
            if (result != Metin2AttachMetinResult.Ok)
            {
                string message = null;
                switch (result)
                {
                    case Metin2AttachMetinResult.NotMatchableItem:
                        message = Metin2Language.Format("REFINE_FAILURE_CAN_NOT_ATTACH", stone.name); break;
                    case Metin2AttachMetinResult.NoMatchableSocket:
                        message = Metin2Language.Format("REFINE_FAILURE_NO_SOCKET0", stone.name); break;
                    case Metin2AttachMetinResult.NoGoldSocket:
                        message = Metin2Language.Format("REFINE_FAILURE_NO_GOLD_SOCKET", stone.name); break;
                    case Metin2AttachMetinResult.TargetIsEquipped:
                        message = Metin2Language.Get("REFINE_FAILURE_EQUIP_ITEM"); break;
                }
                if (!string.IsNullOrEmpty(message)) chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, message);
                return;
            }

            Transform host = ResolveUiHost();
            if (host == null) return;
            if (attachMetinDialog == null)
                attachMetinDialog = Metin2AttachMetinDialog.Create(uiCatalog, host, localeText, playerState,
                    itemCatalog, skillDescriptionCatalog);
            attachMetinDialog?.Open(sourceWindow, sourceCell, targetWindow, targetCell,
                () => UseInventoryItemOnItem(sourceWindow, sourceCell, targetWindow, targetCell));
        }
        private async void AnswerQuest(byte answer) { try { if (gameSession != null) await gameSession.SendScriptAnswerAsync(answer, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void ConfirmQuest(byte answer, uint pid) { try { if (gameSession != null) await gameSession.SendQuestConfirmAsync(answer, pid, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void SendChat(string message)
        {
            if (TryRunLocalAdminEnvironmentCommand(message)) return;
            MarkIntentionalExit(message);
            try { if (gameSession != null) await gameSession.SendChatAsync(0, message, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void SendRefine(ushort position, byte type) { try { if (gameSession != null) await gameSession.SendRefineAsync(position, type, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        /// <summary>
        /// CPythonPlayer::RequestAddLocalQuickSlot (:778): the local slot is converted to a global
        /// one before sending, because the server only knows global indices - the page is ours.
        /// </summary>
        private async void AddQuickSlotRegistration(int local, Metin2QuickSlotBar.SlotType type, int target)
        {
            if (!CanActMainInstance()) return;
            int page = gameUi?.TaskBarPanel?.QuickPage ?? 0;
            int global = page * Metin2QuickSlotBar.SlotsPerPage + local;
            if (global < 0 || global >= Metin2QuickSlotBar.TotalSlots) return;
            if ((uint)target > ushort.MaxValue) return;
            try { if (gameSession != null) await gameSession.SendQuickSlotAddAsync((byte)global, (byte)type, (ushort)target, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// player.SetSkillColor -> SendSkillColorPacket (PythonNetworkStreamPhaseGame.cpp:3251):
        /// CG_SKILL_COLOR (56) with the five layer colours. The server stores the block, consumes
        /// one 164406 stack for non-zero colours and echoes it back on the next character update.
        /// </summary>
        private async void SendSkillColor(int slot, uint color1, uint color2, uint color3, uint color4, uint color5)
        {
            if (!CanActMainInstance()) return;
            try
            {
                if (gameSession != null)
                    await gameSession.SendSkillColorAsync((byte)slot, color1, color2, color3, color4, color5, lifetime.Token);
                // player.SetSkillColor also applies locally - the window shows its own colour
                // before the server echo arrives.
                // The original sends only; colour changes are authoritative GC actor updates.
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>The local actor's GC 7/14 skill-colour block also feeds the player state.</summary>
        private void ApplyLocalSkillColorsToState() =>
            playerState?.ApplySkillColors(entityWorld != null ? entityWorld.LocalSkillColors : null);

        /// <summary>
        /// RefreshCharacter's guild/name half: every local identity change (join/leave guild,
        /// character update) rewrites the two identity labels and the guild tooltip data the way
        /// the original's StartGame + guild store do.
        /// </summary>
        private void RefreshCharacterWindowIdentity()
        {
            if (entityWorld == null) return;
            Metin2CharacterWindowPanel character = gameUi != null
                ? gameUi.GetPanel<Metin2CharacterWindowPanel>("characterwindow") : null;
            if (character == null) return;
            bool cachedGuild = characterWindowGuild.HasValue && characterWindowGuild.Value.GuildId == entityWorld.LocalGuildId;
            string guildName = entityWorld.LocalGuildId != 0
                ? (cachedGuild ? characterWindowGuild.Value.Name : entityWorld.ResolveGuildNamePlain(entityWorld.LocalGuildId))
                : string.Empty;
            character.SetGuildData(cachedGuild ? characterWindowGuild.Value.Level : (byte)0, cachedGuild ? characterWindowGuildMaster : string.Empty);
            character.BindRace(mainCharacterRace);
            character.SetIdentity(mainCharacterName, guildName);
            character.Refresh();
        }

        /// <summary>chat.AppendChat(CHAT_TYPE_INFO, ...) - a local-only info line, never a packet.</summary>
        private void AppendInfoLine(string text)
        {
            if (string.IsNullOrEmpty(text)) return;
            chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, text);
        }

        /// <summary>
        /// CPythonPlayer::RequestMoveGlobalQuickSlotToLocalQuickSlot (:769): the source arrives
        /// global already, only the destination is converted.
        /// </summary>
        private async void MoveQuickSlotRegistration(int globalFrom, int localTo)
        {
            if (!CanActMainInstance()) return;
            int page = gameUi?.TaskBarPanel?.QuickPage ?? 0;
            int globalTo = page * Metin2QuickSlotBar.SlotsPerPage + localTo;
            if (globalFrom < 0 || globalFrom >= Metin2QuickSlotBar.TotalSlots) return;
            if (globalTo < 0 || globalTo >= Metin2QuickSlotBar.TotalSlots) return;
            try { if (gameSession != null) await gameSession.SendQuickSlotSwapAsync((byte)globalFrom, (byte)globalTo, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void AddGlobalQuickSlotRegistration(int global, Metin2QuickSlotBar.SlotType type, int target)
        {
            if (!CanActMainInstance() || global < 0 || global >= Metin2QuickSlotBar.TotalSlots) return;
            if ((uint)target > ushort.MaxValue) return;
            try { if (gameSession != null) await gameSession.SendQuickSlotAddAsync((byte)global, (byte)type, (ushort)target, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void MoveGlobalQuickSlotRegistration(int globalFrom, int globalTo)
        {
            if (!CanActMainInstance()) return;
            if (globalFrom < 0 || globalFrom >= Metin2QuickSlotBar.TotalSlots ||
                globalTo < 0 || globalTo >= Metin2QuickSlotBar.TotalSlots) return;
            try { if (gameSession != null) await gameSession.SendQuickSlotSwapAsync((byte)globalFrom, (byte)globalTo, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// mouseModule.mouseController is a module-level singleton, so every draggable surface must
        /// share one instance - that is the whole reason a skill can leave the character window and
        /// land on the task bar. It hangs off the canvas rather than any one window, and is created
        /// on first use because windows and the task bar are built in no fixed order.
        /// </summary>
        private Metin2CursorAttachment EnsureCursorAttachment()
        {
            if (cursorAttachment == null)
            {
                cursorAttachment = GetComponent<Metin2CursorAttachment>() ?? gameObject.AddComponent<Metin2CursorAttachment>();
                Canvas canvas = ResolveUiCanvas();
                Transform host = ResolveUiHost();
                if (canvas != null && host != null) cursorAttachment.Initialize(host);
            }
            return cursorAttachment;
        }

        /// <summary>mouseModule.mouseController - one carrier shared by every draggable surface.</summary>
        private Metin2ToolTipController toolTipController;

        /// <summary>
        /// interfacemodule.py creates one ItemToolTip and one SkillToolTip for the whole interface and
        /// hands the same pair to the inventory, the task bar and the character page
        /// (SetItemToolTip / SetSkillToolTip). Same reasoning as EnsureCursorAttachment: it hangs off
        /// the canvas and is created on first use, because the windows are built in no fixed order.
        /// </summary>
        private Metin2ToolTipController EnsureToolTips()
        {
            if (toolTipController == null)
            {
                toolTipController = GetComponent<Metin2ToolTipController>() ?? gameObject.AddComponent<Metin2ToolTipController>();
                Canvas canvas = ResolveUiCanvas();
                Transform host = ResolveUiHost();
                if (canvas != null)
                    toolTipController.Initialize(uiCatalog, host ?? canvas.transform, localeText, itemCatalog,
                        skillDescriptionCatalog, emotionCatalog, playerState);
                toolTipController.BindCursor(EnsureCursorAttachment());
                // The race may already be known: GC_MAIN_CHARACTER arrives before the first window is
                // opened, exactly like the quick-slot replay of session 53.
                if (mainCharacterRace >= 0) toolTipController.SetCharacterRace(mainCharacterRace);
                toolTipController.SetGameMaster(entityWorld != null && (entityWorld.LocalAffectFlags & 1UL) != 0);
            }
            return toolTipController;
        }

        /// <summary>interfacemodule.py:125 __MakeChatWindow - one chat window for the whole session.</summary>
        private Metin2ChatWindow chatWindow;
        private Metin2LootLog lootLog;
        private Metin2LootLog EnsureLootLog()
        {
            if(lootLog==null)
            {
                Transform host=ResolveUiHost();
                var mobile=mobileUiRoot!=null?mobileUiRoot.transform.Find("MobileSafeArea/MobileControls"):null;
                if(mobile!=null)host=mobile;
                if(host!=null)lootLog=Metin2LootLog.Create(host,itemCatalog);
            }
            return lootLog;
        }
        private readonly Dictionary<string, Metin2WhisperDialog> whisperDialogs = new Dictionary<string, Metin2WhisperDialog>(StringComparer.OrdinalIgnoreCase);
        // CPythonChat::m_WhisperMap owns the lines independently from uiWhisper.WhisperDialog.
        // Closing clears one target; minimizing destroys only the window and keeps this store.
        private readonly Dictionary<string, List<Metin2WhisperRecord>> whisperHistory = new Dictionary<string, List<Metin2WhisperRecord>>(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> whisperGameMasters = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private int whisperWindowOpenPosition;
        private readonly List<Metin2GamePacketCodec.MessengerMessage> messengerReplay = new List<Metin2GamePacketCodec.MessengerMessage>();
        private Metin2PartyPanel partyPanel;
        private readonly List<Metin2GamePacketCodec.GuildMessage> guildReplay=new List<Metin2GamePacketCodec.GuildMessage>();
        private Metin2WhisperDialog untargetedWhisperDialog;

        private void EnsureChatWindow(Transform canvas)
        {
            if (chatWindow == null) chatWindow = Metin2ChatWindow.Create(uiCatalog, canvas, localeText);
            if (chatWindow == null) return;
            EnsureLootLog();

            chatWindow.ChatSubmitted -= SendChatTyped; chatWindow.ChatSubmitted += SendChatTyped;
            chatWindow.WhisperRequested -= OpenWhisperDialog; chatWindow.WhisperRequested += OpenWhisperDialog;
            chatWindow.ChatLogRequested -= ToggleChatLog; chatWindow.ChatLogRequested += ToggleChatLog;
            chatWindow.LootHistoryRequested -= ShowLootHistory; chatWindow.LootHistoryRequested += ShowLootHistory;
            // |Hmsg:Name|h hyperlinks open the whisper dialog for that name (interfacemodule.py:765).
            chatWindow.WhisperTargetRequested -= OpenWhisperForTarget; chatWindow.WhisperTargetRequested += OpenWhisperForTarget;
            chatWindow.BindToolTips(EnsureToolTips());
            chatWindow.BindLinkData(playerState, itemCatalog, EnsureCursorAttachment());
            if (gameRuntime != null)
            {
                gameRuntime.ChatReceived -= ShowChat;
                gameRuntime.ChatReceived += ShowChat;
                gameRuntime.WhisperReceived -= ShowWhisper;
                gameRuntime.WhisperReceived += ShowWhisper;
                gameRuntime.MessengerReceived -= ShowMessenger;
                gameRuntime.MessengerReceived += ShowMessenger;
                gameRuntime.ChatNewReceived -= ShowChatNew;
                gameRuntime.ChatNewReceived += ShowChatNew;
                gameRuntime.ChangeLanguageRequested -= OnServerRequestChangeLanguage;
                gameRuntime.ChangeLanguageRequested += OnServerRequestChangeLanguage;
                gameRuntime.TargetInfoReceived -= OnTargetInfoReceived;
                gameRuntime.TargetInfoReceived += OnTargetInfoReceived;
                gameRuntime.RecvLanguageReceived -= OnRecvLanguage;
                gameRuntime.RecvLanguageReceived += OnRecvLanguage;
            }
            ConnectPersistentGameEvents();
        }

        /// <summary>
        /// CPythonGuild, CPythonParty and CPythonMessenger are data singletons: they are fed by
        /// CPythonNetworkStream from the first packet of the game phase, long before any window
        /// exists, and a window only reads what the singleton already holds when it opens.
        ///
        /// The port subscribed the guild, party and messenger events inside EnsureChatWindow, which
        /// BuildOriginalGameUi only calls AFTER both deferred-packet replays. Every GC_GUILD the
        /// server pushes while the player is entering the world - GUILD_SUBHEADER_GC_INFO (8),
        /// _LIST (2), _GRADE (3), _COMMENTS (9) and _SKILL_INFO (12), all sent by CGuild::LoginMember
        /// - was therefore dispatched with no subscriber attached and dropped on the floor. Only
        /// packets that arrive after StartReceiving ever landed, which is exactly why the window saw
        /// GUILD_SUBHEADER_GC_GUILD_WAR_LIST (17) and nothing else. The same hole swallowed the
        /// login party and messenger state.
        ///
        /// Subscribing here, before the first replay, restores the singleton's lifetime. Both
        /// handlers are null-safe against a missing window and append to their replay list, so the
        /// window still gets the full history when it is finally built.
        /// </summary>
        private void ConnectPersistentGameEvents()
        {
            if (gameRuntime == null) return;
            gameRuntime.MessengerReceived -= ShowMessenger; gameRuntime.MessengerReceived += ShowMessenger;
            gameRuntime.PartyReceived -= ShowParty; gameRuntime.PartyReceived += ShowParty;
            gameRuntime.GuildReceived -= ShowGuild; gameRuntime.GuildReceived += ShowGuild;
        }

        /// <summary>
        /// GC_CHAT into the window. The client formats nothing here: RecvChatPacket
        /// (PythonNetworkStreamPhaseGame.cpp:1456, :1479) passes the raw <c>buf</c> - the server has
        /// already written "name: text" for player speech - and only the type decides the colour.
        /// </summary>
        /// <summary>
        /// A chat line is offered to the emoticon parser first, exactly as
        /// PythonNetworkStreamPhaseGame.cpp:1396-1417 does: on a match the client attaches the
        /// effect over the speaker's head and RETURNS, so the marker never reaches the chat window.
        /// Only the five speech types are eligible - INFO, NOTICE and the rest carry no speaker.
        ///
        /// This is the mechanism behind the mark over a quest NPC: the server sends "(!)" from the
        /// NPC's own vid, and index 9 of the table is !_mix_back.mse.
        /// </summary>
        private void ShowChat(Metin2GamePacketCodec.ChatMessage message)
        {
            // RecvChatPacket handles CHAT_TYPE_COMMAND before looking up dwVID. Commands are never
            // chat tails and never chat-window lines, even if a malformed/custom server supplies a VID.
            if (message.Type == Metin2ChatWindow.ChatTypeCommand) { RunServerCommand(message.Text); return; }
            if(message.Type==Metin2ChatWindow.ChatTypeInfoItem||message.Type==Metin2ChatWindow.ChatTypeInfoValue)
            { EnsureLootLog()?.AddMessage(message.Text);return; }
            if(message.Type==Metin2ChatWindow.ChatTypeNotice||message.Type==Metin2ChatWindow.ChatTypeBigNotice)
                Metin2BroadcastBanner.Show(ResolveUiHost(),message.Text,message.Type==Metin2ChatWindow.ChatTypeBigNotice);

            // The marker is a control message, never visible chat. Even when the actor already
            // has an emoticon (or its one-second cooldown rejects the effect), consume it here.
            // Previously Play() returning false let the marker fall through into the bubble/log.
            if (IsSpeech(message.Type) && Metin2EmoticonEffects.TryParse(
                    Metin2EmoticonEffects.StripSpeaker(message.Text), out int emoticon))
            {
                if (message.Vid != 0 && entityWorld != null &&
                    entityWorld.TryResolveActor(message.Vid, out Transform emoticonSpeaker))
                    Emoticons.Play(emoticonSpeaker, emoticon);
                return;
            }

            if (message.Vid != 0)
            {
                // Native parity: a VID-backed chat packet for an actor that no longer exists is
                // consumed. It must not become a free-floating chat-window line after despawn.
                if (entityWorld == null || !entityWorld.TryResolveActor(message.Vid, out Transform speaker)) return;

                string speech = IsSpeech(message.Type) ? Metin2EmoticonEffects.StripSpeaker(message.Text) : message.Text;
                // CPythonTextTail::RegisterChatTail receives the stripped speech text (or the raw
                // non-speech payload) for every VID-backed packet except SHOUT.
                if (message.Type != Metin2ChatWindow.ChatTypeShout && !string.IsNullOrEmpty(speech))
                    Metin2ChatBubble.Show(speaker, speech);

                // RecvChatPacket appends VID-backed lines to the chat log only for PCs. NPC/monster
                // speech exists solely as the overhead tail. The local actor is structurally outside
                // the remote entity dictionary in this port, but is always CHAR_TYPE_PC.
                bool isPc = entityWorld.IsLocalPlayer(message.Vid);
                if (!isPc && entityWorld.TryGetEntity(message.Vid, out Metin2NetworkEntity entity)) isPc = entity.Type == 6;
                if (isPc) chatWindow?.AppendChat(message.Type, message.Text);
                return;
            }

            // VID 0 follows the native global/system branch; the existing chat window owns notice
            // presentation/filtering, while the packet text itself remains unchanged.
            chatWindow?.AppendChat(message.Type, message.Text);
        }

        /// <summary>
        /// RecvChatPacketNew (PythonNetworkStreamPhaseGame.cpp:5956): the TEXTS_IMPROVEMENT chat.
        /// idx resolves through CTextTrans (locale_string.txt); the '#'-separated args fill the
        /// template's %s placeholders sequentially. DIALOG goes to the popup dialog and is never
        /// appended to the chat set; everything else lands in the chat window.
        /// </summary>
        private void ShowChatNew(Metin2GamePacketCodec.ChatNewMessage message)
        {
            // Receipt metadata is sufficient even without an indexed locale template.
            if(message.Index==102&&message.Type!=Metin2ChatWindow.ChatTypeDialog)
            {
                string[] receipt=(message.Text??string.Empty).Split(new[]{'#'},2);
                EnsureLootLog()?.AddMessage(message.Text,receipt.Length>1?receipt[1]:null,receipt.Length>1?receipt[0]:null);
                return;
            }
            string template = Metin2Language.GetIndexedString(message.Index);
            if (string.IsNullOrEmpty(template))
            {
                if(message.Type==Metin2ChatWindow.ChatTypeInfoItem||message.Type==Metin2ChatWindow.ChatTypeInfoValue)
                    EnsureLootLog()?.AddMessage(message.Text);
                return;
            }
            string text = Metin2ServerChatFormatter.Format(template,message.Text,out int missingArguments);
            if(missingArguments>0)
                Debug.LogWarning($"METIN2_CHAT_FORMAT_MISSING index={message.Index} missing={missingArguments} payloadChars={message.Text?.Length??0}");
            if (message.Type == Metin2ChatWindow.ChatTypeDialog) { PopupMessage(text); return; }
            // Original pickup packet 102 carries count#localized item name, even on legacy INFO.
            if(message.Index==102||message.Type==Metin2ChatWindow.ChatTypeInfoItem||message.Type==Metin2ChatWindow.ChatTypeInfoValue)
            {
                string[] args=(message.Text??string.Empty).Split('#');
                EnsureLootLog()?.AddMessage(text,message.Index==102&&args.Length>1?args[1]:null,message.Index==102&&args.Length>1?args[0]:null);
                return;
            }
            if(message.Type==Metin2ChatWindow.ChatTypeNotice||message.Type==Metin2ChatWindow.ChatTypeBigNotice)
                Metin2BroadcastBanner.Show(ResolveUiHost(),text,message.Type==Metin2ChatWindow.ChatTypeBigNotice);
            chatWindow?.AppendChat(message.Type, text);
        }

        /// <summary>constinfo.py TRANSFORM_LANG: the server's 10-language numeric mapping.</summary>
        private static string TransformLang(byte code)
        {
            switch (code)
            {
                case 1: return "en";
                case 2: return "ro";
                case 3: return "it";
                case 4: return "tr";
                case 5: return "de";
                case 6: return "pl";
                case 7: return "pt";
                case 8: return "es";
                case 9: return "cz";
                case 10: return "hu";
                default: return null;
            }
        }

        private static byte TransformLangCode(string language)
        {
            switch (language)
            {
                case "en": return 1;
                case "ro": return 2;
                case "it": return 3;
                case "tr": return 4;
                case "de": return 5;
                case "pl": return 6;
                case "pt": return 7;
                case "es": return 8;
                case "cz": return 9;
                case "hu": return 10;
                default: return 0;
            }
        }

        /// <summary>
        /// game.py BINARY_RequestChangeLanguage: on enter-world the server asserts its stored
        /// descriptor language; a client with a different locale answers with its own language code.
        /// Locales the server mapping cannot express (fr/dk/nl/ru/gr) adopt the server's value.
        /// </summary>
        private async void OnServerRequestChangeLanguage(byte language)
        {
            string serverLanguage = TransformLang(language);
            if (string.IsNullOrEmpty(serverLanguage)) return;
            string current = Metin2Language.Code;
            if (current == serverLanguage) return;
            byte own = TransformLangCode(current);
            if (own != 0)
            {
                try { if (gameSession != null) await gameSession.SendChangeLanguageAsync(own, lifetime.Token); }
                catch (Exception exception) { ShowOperationFailure(exception); }
                return;
            }
            Metin2Language.Select(serverLanguage);
        }

        /// <summary>BINARY_AddTargetMonsterDropInfo: one GC_TARGET_INFO row appended to the drop board.</summary>
        private void OnTargetInfoReceived(Metin2GamePacketCodec.TargetInfo info)
        {
            if (targetBoard != null) targetBoard.AddMonsterDropInfo(info.Vid, info.Race, info.Vnum, info.Count);
        }

        private async void SendTargetInfoLoad(uint vid)
        {
            try { if (gameSession != null) await gameSession.SendTargetInfoLoadAsync(vid, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void ToggleChatLog()
        {
            if (chatWindow != null) chatWindow.ToggleChatLog();
        }
        private void ShowLootHistory()=>EnsureLootLog()?.ShowHistory();

        private Sprite ResolveItemIcon(uint vnum)
        {
            if (itemCatalog != null && itemCatalog.TryGet(vnum, out Metin2ItemCatalog.Entry entry)) return entry.icon;
            return null;
        }

        /// <summary>
        /// game.py's serverCommandList (:1937-1990), reached through
        /// CPythonNetworkStream::ServerCommand -> BINARY_ServerCommand_Run ->
        /// stringCommander.Analyzer.Run. The split is Python's line.split(): the first token is the
        /// command, the rest are arguments.
        ///
        /// This corrects something I got wrong earlier. RefineSuceededMessage and RefineFailedMessage
        /// are NOT dead code - nothing in the C++ calls them because the server calls them BY NAME
        /// over this channel. The server sends CHAT_TYPE_COMMAND "RefineSuceeded" (spelled with one
        /// c, as the table has it) and that string IS the trigger.
        ///
        /// An unknown command is named rather than dropped. The table has around forty entries -
        /// safebox, mall, private shop, party requests, horse state, the wedding set, the Christmas
        /// events - and each needs the panel it drives before it can do anything. Logging the name
        /// is how we find out which ones this server actually uses.
        /// </summary>
        private readonly System.Collections.Generic.HashSet<string> loggedServerCommands =
            new System.Collections.Generic.HashSet<string>(StringComparer.Ordinal);
        private readonly System.Collections.Generic.HashSet<string> loggedUnhandledCommands =
            new System.Collections.Generic.HashSet<string>(StringComparer.Ordinal);

        private void ApplyComboSkillState()
        {
            combatController?.SetComboType(activeComboType);
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?
                .SetComboActive(activeComboType > 0);
            gameUi?.TaskBarPanel?.QuickSlots?.SetComboActive(activeComboType > 0);
        }

        private Metin2AdvancedDuelPanel advancedDuel;
        private Metin2AdvancedDuelPanel EnsureAdvancedDuel()
        {
            if(advancedDuel==null)
            {
                Transform host=ResolveUiHost();if(host==null)return null;
                advancedDuel=gameObject.AddComponent<Metin2AdvancedDuelPanel>();
                advancedDuel.Configure(uiCatalog,host,itemCatalog,EnsureToolTips());
                advancedDuel.AvailableYang=()=>playerState!=null?playerState.Gold:0;
                advancedDuel.ChatRequested+=SendChat;
                var equipment=host.GetComponentInChildren<Metin2ViewEquipmentPanel>(true);
                if(equipment==null){var rect=CreateRect("ViewEquipment",host,new Vector2(380,500),Vector2.zero);equipment=rect.gameObject.AddComponent<Metin2ViewEquipmentPanel>();equipment.Bind(gameRuntime,itemCatalog);equipment.BindToolTips(EnsureToolTips());}
                equipment.Intercept=advancedDuel.TryShowEquipment;
            }
            return advancedDuel;
        }

        private Metin2MissionPassClient missionPassClient;
        private Metin2SpyQuestClient spyQuestClient;
        private Metin2CaravanClient caravanClient;
        private Metin2EclipseRaidClient eclipseRaidClient;
        private Metin2MissionPassClient EnsureMissionPassClient()
        {
            if(missionPassClient==null)missionPassClient=gameObject.AddComponent<Metin2MissionPassClient>();
            missionPassClient.Configure(ResolveUiHost(),itemCatalog,SendChat,text=>chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo,text));
            missionPassClient.BindToolTips(EnsureToolTips());
            missionPassClient.SetInGame(localController!=null);
            return missionPassClient;
        }
        private void RunServerCommand(string line)
        {
            if (string.IsNullOrWhiteSpace(line)) return;
            string[] tokens = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
            if (tokens.Length == 0) return;
            string command = tokens[0];
            if(command.StartsWith("er_",StringComparison.Ordinal)){
                if(eclipseRaidClient==null)eclipseRaidClient=gameObject.AddComponent<Metin2EclipseRaidClient>();
                eclipseRaidClient.Configure(entityWorld,ResolveUiHost(),SendChat,text=>chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo,text));
                eclipseRaidClient.HandleServerCommand(tokens);return;
            }
            if(command.StartsWith("cv_",StringComparison.Ordinal)){
                if(caravanClient==null)caravanClient=gameObject.AddComponent<Metin2CaravanClient>();
                caravanClient.Configure(entityWorld,ResolveUiHost(),SendChat,text=>chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo,text),()=>mainCharacterRace);
                caravanClient.Handle(tokens);return;
            }
            if(command.StartsWith("spy_",StringComparison.Ordinal)){
                if(spyQuestClient==null)spyQuestClient=gameObject.AddComponent<Metin2SpyQuestClient>();
                spyQuestClient.Configure(entityWorld,ResolveUiHost(),SendChat,text=>chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo,text),()=>mainCharacterRace);
                spyQuestClient.Handle(tokens);return;
            }
            if(command.StartsWith("amp_",StringComparison.Ordinal)){EnsureMissionPassClient().Handle(tokens);return;}
            if (command == "herb_notice")
            {
                if(tokens.Length==2 && tokens[1].StartsWith("HERB_",StringComparison.Ordinal))
                    chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo,Metin2Language.Get(tokens[1]));
                return;
            }
            if(command.StartsWith("herb_",StringComparison.Ordinal) && entityWorld!=null && entityWorld.HandleHerbalismCommand(tokens))return;
            if (command == "woodcut_notice")
            {
                if (tokens.Length == 2 && tokens[1].StartsWith("WOODCUT_", StringComparison.Ordinal))
                    chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, Metin2Language.Get(tokens[1]));
                return;
            }
            if (command.StartsWith("woodcut_", StringComparison.Ordinal) && entityWorld != null &&
                entityWorld.HandleWoodcutCommand(tokens)) return;
            if(command=="channel"&&tokens.Length==2&&byte.TryParse(tokens[1],out byte confirmedChannel))
            {if(sessionState!=null)sessionState.ApplyChannel(confirmedChannel);else Metin2ServerInfo.SetChannel(confirmedChannel);return;}
            if(command.StartsWith("BINARY_Duel_",StringComparison.Ordinal)){EnsureAdvancedDuel()?.HandleCommand(tokens);return;}
            if(command=="recv_doctrine"){OpenDoctrineChoose();return;}
            // Logged once per distinct name, never per occurrence. The server sends some of these
            // constantly - StartStaminaConsume and StopStaminaConsume arrive every time running
            // starts and stops - and in the editor each Debug.Log captures a stack trace, so a
            // per-occurrence log turns a routine packet into a visible stall.
            if (loggedServerCommands.Add(command))
                Debug.Log($"METIN2_SERVER_COMMAND first cmd={command} args={tokens.Length - 1}");
            // NEW_PET_SYSTEM (game.py:2451-2465) drives the pet through this same command channel;
            // the state consumes its named commands before the switch falls through.
            if (petState != null)
            {
                string[] petArguments = tokens.Length > 1 ? new string[tokens.Length - 1] : Array.Empty<string>();
                for (int index = 1; index < tokens.Length; index++) petArguments[index - 1] = tokens[index];
                if (petState.HandleCommand(command, petArguments)) return;
            }
            // Guild renewal (ENABLE_GUILD_RENEWAL_BY_RAZOR93, guild_renewal.cpp SendFullStateTo):
            // the whole board state arrives on the command channel as gr_* lines.
            if (guildRenewalState != null)
            {
                string[] renewalArguments = tokens.Length > 1 ? new string[tokens.Length - 1] : Array.Empty<string>();
                for (int index = 1; index < tokens.Length; index++) renewalArguments[index - 1] = tokens[index];
                if (guildRenewalState.HandleCommand(command, renewalArguments))
                {
                    if (command == "gr_open")
                    {
                        EnsureGuildRenewalPanel();
                        gameUi?.Open("guildrenewalwindow");
                    }
                    return;
                }
            }
            if (dungeonInfoState != null && (command == "DungeonInfo" || command == "CleanDungeonInfo" ||
                command == "CleanDungeonRanking" || command == "GetDungeonInfo" || command == "UpdateDungeonInfo" ||
                command == "UpdateDungeonRanking" || command == "UpdateMyDungeonRanking" || command == "OpenDungeonRanking"))
            {
                string[] dungeonArguments=tokens.Length>1?new string[tokens.Length-1]:Array.Empty<string>();
                for(int index=1;index<tokens.Length;index++)dungeonArguments[index-1]=tokens[index];
                if(dungeonInfoState.HandleCommand(command,dungeonArguments))return;
                Debug.LogWarning($"METIN2_DUNGEON_COMMAND_INVALID line=\"{line}\"");return;
            }
            // ENABLE_BIOLOGIST_UI (game.py:2558-2562): server-commander driven board.
            if (customSystems != null && (command == "biologist" || command == "biologist_delivered" ||
                command == "biologist_time" || command == "biologist_reward" || command == "biologist_close" ||
                command == "biologist_next" || command == "biologistch_clear" || command == "biologistch_append" ||
                command == "biologistch_open" || command == "biologistch_close"))
            {
                string[] biologArguments = tokens.Length > 1 ? new string[tokens.Length - 1] : Array.Empty<string>();
                for (int index = 1; index < tokens.Length; index++) biologArguments[index - 1] = tokens[index];
                if (customSystems.HandleBiologCommand(command, biologArguments)) return;
            }
            // game.py:3329 ManagerGiftSystem (dailygift.py): the daily reward state arrives as
            // ManagerGiftSystem <sub>|args lines on the command channel.
            if (command == "ManagerGiftSystem")
            {
                int separator = line.IndexOf(' ');
                string payload = separator >= 0 ? line.Substring(separator + 1) : string.Empty;
                if (gameUi != null)
                {
                    // The original creates wnddailygift at startup, so a state line can arrive
                    // before the player ever opened the window; EnsureWindow matches that.
                    gameUi.EnsureWindow("dailygiftnew");
                    if (payload.StartsWith("Show", StringComparison.Ordinal)) { gameUi.Open("dailygiftnew"); return; }
                    gameUi.GetPanel<Metin2DailyGiftPanel>("dailygiftnew")?.ApplyCommand(payload);
                }
                return;
            }
            switch (command)
            {
#if UNITY_STANDALONE_WIN
                case "recv_whispersys":
                    if(gameUi!=null)
                    {
                        var window=gameUi.EnsureWindow("whispermanager");
                        if(window!=null)
                        {
                            var panel=window.GetComponent<Metin2GmWhisperPanel>()??window.AddComponent<Metin2GmWhisperPanel>();
                            panel.Bind(SendGmWhisper,accept=>AskQuestion2Locale("WHISPER_ADMIN_QUESTION_SEND","",accept),()=>gameUi.Close("whispermanager"),uiCatalog);
                            gameUi.Toggle("whispermanager");
                        }
                    }
                    return;
#endif
                case "OnRecvWhisperAdminSystem":
                    if(tokens.Length>=4&&int.TryParse(tokens[3],out int whisperColor))
                    {
                        string name=tokens[1],message=tokens[2].Replace("#"," ");
                        string[] colors={"ffffffff","ffff796a","ffb1ff80","ff46deff"};
                        message="|c"+colors[Mathf.Clamp(whisperColor,0,3)]+"|H|h"+message+"|h|r";
                        string formatted=Metin2Language.Format("WHISPER_ADMIN_MESSAGE",DateTime.Now.ToString("yyyy-MM-dd HH:mm"),message);
                        var record=new Metin2WhisperRecord(0,formatted);GetWhisperHistory(name).Add(record);whisperGameMasters.Add(name);
                        if(whisperDialogs.TryGetValue(name,out var dialog)&&dialog!=null&&dialog.gameObject.activeSelf){dialog.Append(record);dialog.SetGameMasterLook();}
                        else EnsureWhisperIcons()?.Show(name,true);
                    }
                    return;
                case "manage_exp_status":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], out int expStatus) && (expStatus == 0 || expStatus == 1))
                    {
                        antiExpBlocked = expStatus == 1;
                        mobileUiRoot?.SetAntiExpBlocked(antiExpBlocked);
#if UNITY_STANDALONE_WIN
                        gameUi?.TaskBarPanel?.SetWindowsAntiExp(antiExpBlocked);
#endif
                    }
                    return;
                // ConsoleEnable is refused before the table is even consulted (ServerCommand:134-136).
                case "ConsoleEnable": return;
                // ENABLE_SAVEPOINT_SYSTEM, game.py:2568-2571 and :3639-3649. The request alone is
                // not an open operation: the server appends six authoritative slots and only then
                // sends open_savepoint. Dropping these commands made F11/sidebar appear dead.
                case "append_savepoint":
                case "update_savepoint":
                    if (TryApplySavepointCommand(tokens)) return;
                    Debug.LogWarning($"METIN2_SAVEPOINT_COMMAND_INVALID line=\"{line}\""); return;
                case "open_savepoint": OpenSavepointFromServer(); return;
                // ENABLE_NEW_CRAFT_SYSTEM_RAZOR93. trigger.cpp OnClickStoneCraft (NPC 9005,
                // mob_proto bOnClickType = ON_CLICK_STONE_CRAFT) sends "stone_craft_open" after the
                // CG_ON_CLICK; do_stonecraft answers /stonecraft make|makeall. The window itself is
                // the Python-built uistonecraft.StoneCraftWindow.
                case "stone_craft_open": EnsureCustomSystems().OpenStoneCraft(); return;
                case "ShowMeSafeboxPassword": OpenSafeboxPasswordDialog(false); return;
                case "ShowMeMallPassword": OpenSafeboxPasswordDialog(true); return;
                case "CloseSafebox": CloseStorageFromServer("safeboxwindow"); return;
                case "CloseMall": CloseStorageFromServer("mallwindow"); return;
                // ENABLE_LOCKED_EXTRA_INVENTORY, game.py:2649 ExtraInventoryRefresh. The unlock
                // state lives in the authoritative player item stream; repainting the already
                // bound panel is the native client's complete response to this command.
                case "RefreshExpandInventory":
                    extraInventoryPanel?.Refresh();
                    return;
                case "SetBodyCostumeHidden": if (TryApplyCostumeHidden(1, tokens)) return; break;
                case "SetHairCostumeHidden": if (TryApplyCostumeHidden(2, tokens)) return; break;
                case "SetAcceCostumeHidden": if (TryApplyCostumeHidden(3, tokens)) return; break;
                case "SetWeaponCostumeHidden": if (TryApplyCostumeHidden(4, tokens)) return; break;
                // ENABLE_CHRISTMAS_WHEEL_OF_DESTINY, game.py:2539-2543 and 3651-3668.
                // /wheel first asks the client to send option 0; the server then streams sixteen
                // icons followed by OPEN. Spin completion is acknowledged by the panel with option 3.
                case "BINARY_WHEEL_ASKOPEN": SendWheelOption(0); return;
                case "BINARY_WHEEL_ICON":
                    if (tokens.Length == 4 && uint.TryParse(tokens[1], out uint wheelIconVnum) &&
                        byte.TryParse(tokens[2], out byte wheelIconCount) && int.TryParse(tokens[3], out int wheelIconIndex))
                    { EnsureCustomSystems().ApplyWheelIcon(wheelIconVnum, wheelIconCount, wheelIconIndex); return; }
                    Debug.LogWarning($"METIN2_WHEEL_ICON_INVALID line=\"{line}\""); return;
                case "BINARY_WHEEL_OPEN":
                    if (tokens.Length == 3 && int.TryParse(tokens[1], out int wheelPrice) && int.TryParse(tokens[2], out int wheelFree))
                    { EnsureCustomSystems().OpenWheel(wheelPrice, wheelFree); return; }
                    Debug.LogWarning($"METIN2_WHEEL_OPEN_INVALID line=\"{line}\""); return;
                case "BINARY_WHEEL_TURN":
                    if (tokens.Length == 5 && uint.TryParse(tokens[1], out uint wheelRewardVnum) &&
                        byte.TryParse(tokens[2], out byte wheelRewardCount) && int.TryParse(tokens[3], out int wheelLoops) &&
                        int.TryParse(tokens[4], out int wheelDestination))
                    { EnsureCustomSystems().ApplyWheelTurn(wheelRewardVnum, wheelRewardCount, wheelLoops, wheelDestination); return; }
                    Debug.LogWarning($"METIN2_WHEEL_TURN_INVALID line=\"{line}\""); return;
                case "BINARY_WHEEL_CLOSE": EnsureCustomSystems().CloseWheelFromServer(); return;
                // game.py serverCommandList -> interface.RuneStatus / RuneAffect.  rune_affect is
                // the persisted quest flag rune.hide_effect (1 hidden, 0 visible), not a local
                // toggle preference.
                case "rune":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], NumberStyles.Integer,
                        CultureInfo.InvariantCulture, out int parsedRuneStatus))
                    {
                        runeStatus = parsedRuneStatus;
                        gameUi?.GetPanel<Metin2RunePanel>("runewindow")?.SetStatus(runeStatus);
                        return;
                    }
                    Debug.LogWarning($"METIN2_RUNE_STATUS_INVALID line=\"{line}\""); return;
                case "rune_affect":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], NumberStyles.Integer,
                        CultureInfo.InvariantCulture, out int parsedRuneEffect))
                    {
                        runeEffectFlag = parsedRuneEffect;
                        gameUi?.GetPanel<Metin2RunePanel>("runewindow")?.SetEffectFlag(parsedRuneEffect);
                        return;
                    }
                    Debug.LogWarning($"METIN2_RUNE_AFFECT_INVALID line=\"{line}\""); return;
                // CPythonNetworkStreamCommand.cpp:171-176 handles this below the Python command
                // table and posts WM_QUIT. It is the server's authoritative completion signal for
                // /quit after the 3/10 second countdown, not a line that belongs in the chat log.
                case "quit": RequestApplicationQuit(); return;
                case "combo":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], NumberStyles.Integer,
                        CultureInfo.InvariantCulture, out int serverComboFlag) && serverComboFlag >= 0)
                    {
                        Metin2GamePacketCodec.SkillLevel comboSkill = playerState != null && playerState.Skills.Count > 122
                            ? playerState.Skills[122] : default;
                        int learnedComboLevel = comboSkill.Level;
                        // The server sends the absolute level (40 at P) and CheckComboHack
                        // checks ComboSequenceBySkillLevel[min(2, m_bComboIndex)]. Its level-2
                        // sequence differs from level 1; playing the displayed P1 level here
                        // sends a wrong fifth motion and eventually triggers a disconnect.
                        activeComboType = serverComboFlag > 0 && learnedComboLevel > 0
                            ? Math.Min(serverComboFlag, 2) : 0;
                        ApplyComboSkillState();
                        Debug.Log($"METIN2_COMBO_STATE server={serverComboFlag} learned={learnedComboLevel} type={activeComboType}");
                        return;
                    }
                    Debug.LogWarning($"METIN2_COMBO_COMMAND_INVALID line=\"{line}\""); return;
                case "StartStaminaConsume":
                    if (tokens.Length == 3 && int.TryParse(tokens[1], out int perSecond) &&
                        int.TryParse(tokens[2], out int current) && staminaState != null &&
                        staminaState.StartConsume(perSecond, current)) return;
                    Debug.LogWarning($"METIN2_STAMINA_COMMAND_INVALID line=\"{line}\""); return;
                case "StopStaminaConsume":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], out int stoppedAt) &&
                        staminaState != null && staminaState.StopConsume(stoppedAt)) return;
                    Debug.LogWarning($"METIN2_STAMINA_COMMAND_INVALID line=\"{line}\""); return;
                // PythonNetworkStreamCommand.cpp:586-708. These CHAT_TYPE_COMMAND messages are the
                // authoritative presentation route for every client observing an emotion; the
                // compact GC_MOVE/FUNC_EMOTION packet cannot carry two motion ids plus target VID.
                case "french_kiss": RunDualEmotion(tokens, "french_kiss_with_", "french_kiss_with_"); return;
                case "kiss": RunDualEmotion(tokens, "kiss_with_", "kiss_with_"); return;
                case "slap": RunDualEmotion(tokens, "slap_hurt", "slap_hit", false); return;
                case "clap": RunSoloEmotion(tokens, "clap"); return;
                case "cheer1": RunSoloEmotion(tokens, "cheers_1"); return;
                case "cheer2": RunSoloEmotion(tokens, "cheers_2"); return;
                case "dance1": RunSoloEmotion(tokens, "dance_1"); return;
                case "dance2": RunSoloEmotion(tokens, "dance_2"); return;
                case "dance3": RunSoloEmotion(tokens, "dance_3"); return;
                case "dance4": RunSoloEmotion(tokens, "dance_4"); return;
                case "dance5": RunSoloEmotion(tokens, "dance_5"); return;
                case "dance6": RunSoloEmotion(tokens, "dance_6"); return;
                case "dig_motion": RunSoloEmotion(tokens, "dig"); return;
                case "congratulation": RunSoloEmotion(tokens, "congratulation"); return;
                case "forgive": RunSoloEmotion(tokens, "forgive"); return;
                case "angry": RunSoloEmotion(tokens, "angry"); return;
                case "attractive": RunSoloEmotion(tokens, "attractive"); return;
                case "sad": RunSoloEmotion(tokens, "sad"); return;
                case "shy": RunSoloEmotion(tokens, "shy"); return;
                case "cheerup": RunSoloEmotion(tokens, "cheerup"); return;
                case "banter": RunSoloEmotion(tokens, "banter"); return;
                case "joy": RunSoloEmotion(tokens, "joy"); return;
                // s_emotionDict additions (PythonNetworkStreamCommand.cpp): the Auriga client
                // registers the newer solo actions with the same fallback table.
                case "selfie": RunSoloEmotion(tokens, "selfie"); return;
                case "dance7": RunSoloEmotion(tokens, "dance_7"); return;
                case "doze": RunSoloEmotion(tokens, "doze"); return;
                case "exercise": RunSoloEmotion(tokens, "exercise"); return;
                case "pushup": RunSoloEmotion(tokens, "pushup"); return;
                case "ClientPlatform":
                    if (tokens.Length == 3 && uint.TryParse(tokens[1], NumberStyles.None, CultureInfo.InvariantCulture, out uint platformVid) &&
                        byte.TryParse(tokens[2], NumberStyles.None, CultureInfo.InvariantCulture, out byte platformCode))
                    { entityWorld?.SetClientPlatform(platformVid, platformCode); return; }
                    Debug.LogWarning($"METIN2_CLIENT_PLATFORM_INVALID line=\"{line}\""); return;
                case "setblockmode":
                    if (tokens.Length == 2 && int.TryParse(tokens[1], out int blockMode))
                    {
                        Metin2ClientSettings.Current.AcknowledgeBlockMode(blockMode);
                        gameUi?.GetPanel<Metin2GameOptionPanel>("gameoptiondialog")?.ApplyServerBlockMode(blockMode);
                        return;
                    }
                    Debug.LogWarning($"METIN2_BLOCK_MODE_COMMAND_INVALID line=\"{line}\""); return;
                case "OverfallPremiumInfo":
                    if (premiumState.Apply(tokens)) return;
                    Debug.LogWarning($"METIN2_PREMIUM_COMMAND_INVALID line=\"{line}\""); return;
                case "RefineSuceeded": RefineSucceeded(); return;
                case "RefineFailed": RefineFailed(); return;
                // game.py:1943,2017-2018 -> interface.PartyHealReady -> uiparty.PartyMenu.
                // It is a command-channel cooldown notification, not a party GC packet.
                case "PartyHealReady": EnsurePartyPanel()?.PartyHealReady(); return;
                // game.py:1960-1961 serverCommandList -> __PartyRequestQuestion / __PartyRequestDenied.
                // char.cpp:4494 answers /party_request with CHAT_TYPE_COMMAND "PartyRequest <vid>"
                // to the LEADER; there is no GC_PARTY packet for it, so without these two arms the
                // leader was never asked and the joiner waited for a dialog that could not appear.
                case "PartyRequest":
                    if (tokens.Length == 2 && uint.TryParse(tokens[1], out uint partyRequestVid)) { ShowPartyRequest(partyRequestVid); return; }
                    Debug.LogWarning($"METIN2_PARTY_REQUEST_COMMAND_INVALID line=\"{line}\""); return;
                case "PartyRequestDenied": PopupMessageLocale("PARTY_REQUEST_DENIED"); return;
                case "xmas_snow":
                case "xmas_song": SetXMasSong(tokens.Length > 1 && tokens[1] == "1"); return;
                case "xmas_boom":
                case "newyear_boom": SetXMasBoom(tokens.Length > 1 && tokens[1] == "1"); return;
                case "messenger_auth":
                    if (tokens.Length == 2 && tokens[1].Length <= 24) { ShowMessengerFriendInvite(tokens[1]); return; }
                    Debug.LogWarning($"METIN2_MESSENGER_AUTH_COMMAND_INVALID line=\"{line}\""); return;
                case "lover_login":
                    loverMiningState?.SetOnline(true);
                    gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow")?.SetLoverOnline(true); return;
                case "lover_logout":
                    loverMiningState?.SetOnline(false);
                    gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow")?.SetLoverOnline(false); return;
                case "lover_divorce":
                    loverMiningState?.ClearLover();
                    gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow")?.ClearLover(); return;
                // These affect the lover icon on the affect shower, not messenger membership.
                case "lover_near": case "lover_far": return;
                case "ClearOfflineShopSales": offlineShopPanel?.ClearHistory(); return;
                case "OfflineShopSales":
                    if(tokens.Length>=6&&uint.TryParse(tokens[2],out uint saleVnum)&&uint.TryParse(tokens[3],out uint saleCount)&&long.TryParse(tokens[4],out long salePrice)&&
                       uint.TryParse(tokens[tokens.Length>=7?6:5],out uint saleDate)){offlineShopPanel?.AddHistory(tokens[1],saleVnum,saleCount,salePrice,saleDate);return;}
                    Debug.LogWarning($"METIN2_OFFLINE_SHOP_SALE_COMMAND_INVALID line=\"{line}\"");return;
                case "OpenOfflineShopSalesWindow": return;
                case "BINARY_CloseOfflineShop": offlineShopPanel?.CloseLocal(false);ShowOfflineShopError("OFFLINE_SHOP_FORCE_CLOSE");return;
                case "OpenOfflineShop": ShowShopInfo("OFFLINE_SHOP_OPEN_WITH_BUTTON");return;
                // game.py:1941 serverCommandList "CloseRestartWindow" -> __RestartDialog_Close
                // (game.py:2131-2132) -> interface.CloseRestartDialog. The server sends it both when
                // a restart command is accepted and when it refuses one while still alive.
                case "CloseRestartWindow": CloseRestartDialog(); return;
                default:
                    if (loggedUnhandledCommands.Add(command))
                        Debug.LogWarning($"METIN2_SERVER_COMMAND unhandled={command} line=\"{line}\"");
                    return;
            }
        }

        private bool IsXMasMap()
        {
            string map = worldRuntime == null ? string.Empty : worldRuntime.CurrentMapName;
            return map != "metin2_map_n_flame_01" && map != "metin2_map_n_desert_01" &&
                   map != "metin2_map_spiderdungeon" && map != "metin2_map_deviltower1";
        }

        // game.py::__XMasSong_Enable and __XMasBoom_Enable. These are server commands, not
        // calendar-driven client behaviour.
        private void SetXMasSong(bool enabled)
        {
            if (enabled)
            {
                if (IsXMasMap()) Metin2SoundManager.Instance.FadeInMusic("BGM/xmas.mp3");
            }
            else Metin2SoundManager.Instance.FadeInMusic("BGM/m2bg.mp3");
        }

        private void SetXMasBoom(bool enabled)
        {
            if (xmasBoomRoutine != null) { StopCoroutine(xmasBoomRoutine); xmasBoomRoutine = null; }
            if (enabled && IsXMasMap()) xmasBoomRoutine = StartCoroutine(PlayXMasBoom());
        }

        private System.Collections.IEnumerator PlayXMasBoom()
        {
            float started = Time.time;
            for (int batch = 0; batch < XMasBoomTimes.Length; batch++)
            {
                while (Time.time - started <= XMasBoomTimes[batch]) yield return null;
                Vector3 centre = localController != null ? localController.transform.position : Vector3.zero;
                for (int count = 0; count < XMasBoomCounts[batch]; count++)
                {
                    Vector3 position = centre + new Vector3(UnityEngine.Random.Range(-1.5f, 1.5f), 0f,
                        UnityEngine.Random.Range(-1.5f, 1.5f));
                    Metin2SoundManager.Instance.PlaySound3D(position, "sound/common/etc/salute.mp3");
                }
            }
            xmasBoomRoutine = null;
        }

        private readonly struct EmotionActor
        {
            public readonly Transform Root;
            public readonly Metin2CharacterMotionSet Motions;
            public readonly Metin2PlayerController Local;
            public readonly Metin2NetworkEntity Remote;
            public readonly int Job;
            public EmotionActor(Transform root, Metin2CharacterMotionSet motions,
                                Metin2PlayerController local, Metin2NetworkEntity remote, int job)
            { Root = root; Motions = motions; Local = local; Remote = remote; Job = job; }
        }

        private static readonly string[] EmotionJobNames = { "warrior", "assassin", "sura", "shaman" };

        private bool TryResolveEmotionActor(uint vid, out EmotionActor actor)
        {
            if (entityWorld != null && entityWorld.IsLocalPlayer(vid) && localController != null)
            {
                actor = new EmotionActor(localController.transform,
                    localController.GetComponentInChildren<Metin2CharacterMotionSet>(true),
                    localController, null, Mathf.Max(0, mainCharacterRace) % 4);
                return actor.Motions != null;
            }
            if (entityWorld != null && entityWorld.TryGetEntity(vid, out Metin2NetworkEntity remote) && remote != null)
            {
                actor = new EmotionActor(remote.transform,
                    remote.GetComponentInChildren<Metin2CharacterMotionSet>(true),
                    null, remote, remote.Race % 4);
                return actor.Motions != null;
            }
            actor = default;
            return false;
        }

        private void RunSoloEmotion(string[] tokens, string state)
        {
            // cmd_emotion.cpp broadcasts "command actorVid 0" even for a solo motion.
            // Accept the zero target field; rejecting all three-token messages made other
            // clients silently drop every solo emotion while the sender played it locally.
            if ((tokens.Length != 2 && (tokens.Length != 3 || tokens[2] != "0")) ||
                !uint.TryParse(tokens[1], NumberStyles.None,
                    CultureInfo.InvariantCulture, out uint vid) ||
                !TryResolveEmotionActor(vid, out EmotionActor actor) ||
                !actor.Motions.PlayNamedOnce(state))
                Debug.LogWarning($"METIN2_EMOTION_COMMAND_INVALID state={state} line=\"{string.Join(" ", tokens)}\"");
        }

        private void RunDualEmotion(string[] tokens, string firstBase, string secondBase,
                                    bool appendTargetJob = true)
        {
            if (tokens.Length != 3 ||
                !uint.TryParse(tokens[1], NumberStyles.None, CultureInfo.InvariantCulture, out uint firstVid) ||
                !uint.TryParse(tokens[2], NumberStyles.None, CultureInfo.InvariantCulture, out uint secondVid) ||
                !TryResolveEmotionActor(firstVid, out EmotionActor first) ||
                !TryResolveEmotionActor(secondVid, out EmotionActor second))
            {
                Debug.LogWarning($"METIN2_DUAL_EMOTION_COMMAND_INVALID line=\"{string.Join(" ", tokens)}\"");
                return;
            }
            first.Motions.ReserveEvent(.5f);
            second.Motions.ReserveEvent(.5f);
            StartCoroutine(PlayDualEmotion(firstVid, secondVid, firstBase, secondBase, appendTargetJob));
        }

        private System.Collections.IEnumerator PlayDualEmotion(uint firstVid, uint secondVid,
            string firstBase, string secondBase, bool appendTargetJob)
        {
            // ActDualEmotion schedules the state at +500 ms and approaches until the actors are
            // about 100 native units apart. Allow the authoritative MOVE packets to do the travel;
            // never teleport either actor locally.
            float earliest = Time.time + .5f;
            float deadline = Time.time + 2f;
            while (Time.time < earliest || Time.time < deadline &&
                   TryResolveEmotionActor(firstVid, out EmotionActor waitingFirst) &&
                   TryResolveEmotionActor(secondVid, out EmotionActor waitingSecond) &&
                   Vector3.Distance(waitingFirst.Root.position, waitingSecond.Root.position) > 1.25f)
                yield return null;

            if (!TryResolveEmotionActor(firstVid, out EmotionActor first) ||
                !TryResolveEmotionActor(secondVid, out EmotionActor second)) yield break;
            first.Local?.FaceImmediately(second.Root.position);
            first.Remote?.FaceEmotionTarget(second.Root.position);
            second.Local?.FaceImmediately(first.Root.position);
            second.Remote?.FaceEmotionTarget(first.Root.position);

            string firstState = appendTargetJob ? firstBase + EmotionJobNames[second.Job] : firstBase;
            string secondState = appendTargetJob ? secondBase + EmotionJobNames[first.Job] : secondBase;
            if (!first.Motions.PlayNamedOnce(firstState) || !second.Motions.PlayNamedOnce(secondState))
                Debug.LogWarning($"METIN2_DUAL_EMOTION_STATE_MISSING first={firstState} second={secondState}");
        }

        private void HandleWalkMode(Metin2GamePacketCodec.WalkMode message)
        {
            bool walking = message.Mode != 0; // packet.h: WALKMODE_RUN=0, WALKMODE_WALK=1
            if (entityWorld != null && message.Vid == entityWorld.LocalVid)
            {
                localController?.SetWalkMode(walking);
                return;
            }
            if (entityWorld != null && entityWorld.TryGetEntity(message.Vid, out Metin2NetworkEntity entity))
                entity.ApplyWalkMode(walking);
        }


        /// <summary>The five types the native offers to ParseEmoticon.</summary>
        private static bool IsSpeech(byte type) =>
            type == Metin2ChatWindow.ChatTypeTalking || type == Metin2ChatWindow.ChatTypeParty ||
            type == Metin2ChatWindow.ChatTypeGuild || type == Metin2ChatWindow.ChatTypeShout ||
            type == Metin2ChatWindow.ChatTypeWhisper;

        private Metin2EmoticonEffects emoticons;
        private Metin2EmoticonEffects Emoticons =>
            emoticons != null ? emoticons
                : emoticons = GetComponent<Metin2EmoticonEffects>() ?? gameObject.AddComponent<Metin2EmoticonEffects>();
        private void ShowWhisper(Metin2GamePacketCodec.WhisperMessage message)
        {
            string name = (message.From ?? string.Empty).Trim();
            if (string.IsNullOrEmpty(name)) return;

            Metin2WhisperRecord record = BuildWhisperRecord(message);
            if (message.Type > 5 && message.Type != 0xFF)
                Debug.LogWarning($"METIN2_WHISPER_UNKNOWN_TYPE mode={message.Type}");

            if (message.Type == 5) whisperGameMasters.Add(name);
            GetWhisperHistory(name).Add(record);

            // game.py never appends GC_WHISPER into the ordinary chat set. It appends to the
            // target-keyed CWhisper store and then RecvWhisper either updates the open dialog or
            // flashes/creates the mail button.
            if (whisperDialogs.TryGetValue(name, out Metin2WhisperDialog dialog) &&
                dialog != null && dialog.gameObject.activeSelf)
            {
                dialog.Append(record);
                if (message.Type == 5) dialog.SetGameMasterLook();
                return;
            }
            Metin2WhisperIconPanel icons = EnsureWhisperIcons();
            bool created = icons != null && icons.Show(name, whisperGameMasters.Contains(name));
            if (created && chatWindow != null)
                chatWindow.AppendChat(Metin2ChatWindow.ChatTypeNotice, Metin2Language.Format("RECEIVE_MESSAGE", name));
        }

        private async void SendDungeonQuestInput(string value)
        {
            try { if(gameSession!=null)await gameSession.SendQuestInputAsync(value,lifetime.Token); }
            catch(Exception exception){ShowOperationFailure(exception);}
        }

        /// <summary>
        /// uidungeoninfo.py OpenRankingBoard/AnswerTeleport first set quest_cmd and then call
        /// event.QuestButtonClick(dungeonData["quest_index"]).  The quest responds with
        /// GetDungeonInfo CMD, at which point Metin2DungeonState emits the pending CG quest input.
        /// </summary>
        private async void RequestDungeonQuestCommand()
        {
            try
            {
                if (gameSession != null && dungeonInfoState != null && dungeonInfoState.QuestIndex > 0)
                    await gameSession.SendScriptButtonAsync((uint)dungeonInfoState.QuestIndex, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void ConfirmDungeonWarp(int entryIndex)
        {
            if (gameUi == null || dungeonInfoState == null || entryIndex < 0 || entryIndex >= dungeonInfoState.Entries.Count) return;
            string mapKey = "MAP_NAME_" + dungeonInfoState.Entries[entryIndex].MapIndex.ToString(CultureInfo.InvariantCulture);
            string mapName = Metin2Language.Get(mapKey);
            GameObject window = gameUi.Open("questiondialog");
            if (window == null) return;
            Metin2QuestionDialogPanel dialog = window.GetComponent<Metin2QuestionDialogPanel>() ??
                                                window.AddComponent<Metin2QuestionDialogPanel>();
            dialog.Open(Metin2Language.Format("DUNGEON_INFO_DO_YOU_TELEPORT", mapName), () =>
            {
                if (dungeonInfoState.RequestWarp(entryIndex)) RequestDungeonQuestCommand();
            });
        }

        private void OpenDungeonRanking()
        {
            GameObject window = gameUi?.Open("dungeonrankingwindow");
            window?.GetComponent<Metin2DungeonRankingPanel>()?.Bind(dungeonInfoState);
        }

        public static Metin2WhisperRecord BuildWhisperRecord(Metin2GamePacketCodec.WhisperMessage message)
        {
            string name = (message.From ?? string.Empty).Trim();
            byte lineType = message.Type;
            string text;
            switch (message.Type)
            {
                case 0: // WHISPER_TYPE_CHAT
                case 5: // WHISPER_TYPE_GM
                    text = name + " : " + message.Text;
                    break;
                case 1: text = Metin2Language.Format("CANNOT_WHISPER_NOT_LOGON", name); lineType = 0xFF; break;
                case 2: text = Metin2Language.Format("CANNOT_WHISPER_DEST_REFUSE", name); lineType = 0xFF; break;
                case 3: text = Metin2Language.Format("CANNOT_WHISPER_SELF_REFUSE", name); lineType = 0xFF; break;
                case 4: // WHISPER_TYPE_ERROR is presented as a system line by game.py.
                case 0xFF:
                    text = message.Text;
                    lineType = 0xFF;
                    break;
                default:
                    text = Metin2Language.Get("UNITY_CLIENT_OPERATION_FAILED");
                    lineType = 0xFF;
                    break;
            }

            return new Metin2WhisperRecord(lineType, text);
        }

        private Metin2WhisperIconPanel whisperIcons;
        private Metin2QuestTargetState questTargetState;

        /// <summary>Wires the minimap/atlas window panels as the game UI instantiates them.</summary>
        private void WireMapWindow(string screen, GameObject window)
        {
            if (screen == "atlaswindow")
            {
                Metin2AtlasWindowPanel panel = window.GetComponent<Metin2AtlasWindowPanel>();
                if (panel != null)
                {
                    panel.BindWorld(worldRuntime);
                    panel.BindEntities(entityWorld);
                    panel.BindMapAsset(worldRuntime != null ? worldRuntime.CurrentMiniMapAsset : null);
                    panel.BindNpcPositions(GetComponent<Metin2NpcPositionState>() ?? gameObject.AddComponent<Metin2NpcPositionState>());
                    panel.BindQuestTargets(questTargetState);
                    panel.BindGuildLands(GetComponent<Metin2GuildLandState>() ?? gameObject.AddComponent<Metin2GuildLandState>());
                    panel.BindGuildNames(entityWorld);
                    panel.SetBossPositions(atlasBossPositions);
                    panel.BossTeleportRequested-=ConfirmAtlasBossTeleport;
                    panel.BossTeleportRequested+=ConfirmAtlasBossTeleport;
                    panel.Build();
                }
            }            else if (screen == "minimap")
            {
                Metin2MiniMapPanel panel = window.GetComponent<Metin2MiniMapPanel>();
                if (panel != null)
                {
                    panel.BindWorld(worldRuntime);
                    panel.BindEntities(entityWorld);
                    panel.BindMapAsset(worldRuntime != null ? worldRuntime.CurrentMiniMapAsset : null);
                    panel.BindPlayerState(playerState);
                    panel.BindPremium(premiumState, localeText);
                    panel.BindQuestTargets(questTargetState);
                    panel.ConfigureChannelPresentation(mobileUiRoot != null);
                    panel.Build();
                    // uiminimap.py: AtlasShowButton.SetEvent(self.ShowAtlas) - the button only ever
                    // SHOWS the large map (ToggleAtlasWindow is the separate key binding). Without a
                    // listener here the event was raised into nothing and the button did nothing.
                    panel.WindowToggleRequested -= OpenGameWindowByName;
                    panel.WindowToggleRequested += OpenGameWindowByName;
                    panel.ChannelChangeRequested -= RequestChannelChange;
                    panel.ChannelChangeRequested += RequestChannelChange;
                    panel.ShowMiniMap();
                }
            }
        }

        /// <summary>uiminimap.py::ShowAtlas - open, never toggle.</summary>
        private void OpenGameWindowByName(string screen)
        {
            if (gameUi == null || string.IsNullOrEmpty(screen)) return;
            gameUi.Open(screen);
        }

        private void RebindMapWindowAssets()
        {
            if (worldRuntime == null) return;
            Metin2MiniMapAsset asset = worldRuntime.CurrentMiniMapAsset;
            gameUi?.GetPanel<Metin2MiniMapPanel>("minimap")?.BindMapAsset(asset);
            gameUi?.GetPanel<Metin2AtlasWindowPanel>("atlaswindow")?.BindMapAsset(asset);
        }

        private Metin2WhisperIconPanel EnsureWhisperIcons()
        {
            if (whisperIcons != null) return whisperIcons;
            Canvas canvas = ResolveUiCanvas(); Transform host = ResolveUiHost(); if (canvas == null || host == null) return null;
            whisperIcons = host.GetComponent<Metin2WhisperIconPanel>() ?? host.gameObject.AddComponent<Metin2WhisperIconPanel>();
            whisperIcons.Bind(Metin2.Runtime.Metin2UiFont.Get());
            whisperIcons.WhisperIconClicked -= OpenWhisperDialog; whisperIcons.WhisperIconClicked += OpenWhisperDialog;
            return whisperIcons;
        }
        private void ShowMessenger(Metin2GamePacketCodec.MessengerMessage message)
        {
            if (message.SubHeader == 6 && message.Entries != null && message.Entries.Length > 0)
            {
                ShowMessengerFriendInvite(message.Entries[0].Name);
                return;
            }
            // Resolve BEFORE the replay list grows: creating the window here replays everything
            // recorded so far, and this message must not then be applied a second time.
            Metin2MessengerPanel messengerPanel = EnsureMessengerPanel();
            messengerReplay.Add(message); if (messengerReplay.Count > 512) messengerReplay.RemoveAt(0);
            messengerPanel?.Apply(message);
            // The packet that adds a friend may be the one that created the panel, so an already
            // open target board has no Changed subscription to fire.
            targetBoard?.RefreshFromMessengerState();
        }
        private void ShowParty(Metin2GamePacketCodec.PartyMessage message)
        {
            if(message.Header==77){ShowPartyInvite(message.Pid);return;}
            Metin2PartyPanel panel=EnsurePartyPanel();panel?.Apply(message);if(panel!=null){QuestButtons.SetPartyOpen(panel.HasParty);entityWorld?.BindTextTailRelationships(pvpState,panel,GuildPanelOrNull());}
            // The packet that created the panel is also the one that fills it, so an already open
            // target board has no Changed subscription yet.
            targetBoard?.RefreshFromPartyState();
        }
        private void ShowGuild(Metin2GamePacketCodec.GuildMessage message)
        {
            if(message.SubHeader==14){ShowGuildInvite(message.Id,message.Name);return;}
            entityWorld?.ApplyGuildMessage(message);
            // GC_GUILD sub 8 (INFO) carries the local guild's name and level; RefreshCharacter
            // paints them into the character window and the guild tooltip reads the same values.
            if (message.SubHeader == 8 && message.Info.HasValue)
            {
                Metin2GamePacketCodec.GuildInfo info = message.Info.Value;
                string master = string.Empty;
                if (message.Members != null)
                    foreach (Metin2GamePacketCodec.GuildMember member in message.Members)
                        if (member.Pid == info.MasterPid) { master = member.Name; break; }
                if (string.IsNullOrEmpty(master) && info.MasterPid == 0) master = string.Empty;
                characterWindowGuild = info;
                characterWindowGuildMaster = master;
                Metin2CharacterWindowPanel character = gameUi != null
                    ? gameUi.GetPanel<Metin2CharacterWindowPanel>("characterwindow") : null;
                if (character != null)
                {
                    character.SetGuildData(info.Level, master);
                    character.SetIdentity(mainCharacterName, info.Name);
                }
            }
            // Resolve BEFORE the replay list grows: creating the window here replays everything
            // recorded so far, and this message must not then be applied a second time.
            Metin2GuildPanel guildPanel=EnsureGuildPanel();
            bool contributionOnly=guildPanel!=null&&guildPanel.IsContributionOnlyUpdate(message);
            guildReplay.Add(message);if(guildReplay.Count>1024)guildReplay.RemoveAt(0);
            guildPanel?.Apply(message);
            guildRenewalPanel?.RefreshDisplay();
            if(message.SubHeader==10||contributionOnly)
            {
                // Donation acknowledgements change EXP, not membership/PvP relationships.
                // Avoid replay scans and rebinding every nearby actor for each offer.
                if(message.SubHeader==10&&characterWindowGuild.HasValue)
                {
                    var previous=characterWindowGuild.Value;
                    characterWindowGuild=new Metin2GamePacketCodec.GuildInfo(previous.MemberCount,previous.MaxMemberCount,previous.GuildId,previous.MasterPid,message.Id,message.Value,previous.Name,previous.Gold,previous.HasLand);
                    if(previous.Level!=message.Value)RefreshCharacterWindowIdentity();
                }
                return;
            }
            if (characterWindowGuild.HasValue)
            {
                foreach (var prior in guildReplay)
                    if (prior.Members != null)
                        foreach (var member in prior.Members)
                            if (member.Pid == characterWindowGuild.Value.MasterPid && !string.IsNullOrEmpty(member.Name))
                                characterWindowGuildMaster = member.Name;
            }
            RefreshCharacterWindowIdentity();
            MessengerPanelOrNull()?.ApplyGuild(message);
            entityWorld?.BindTextTailRelationships(pvpState,GetComponent<Metin2PartyPanel>(),guildPanel);
            // BindSkillEligibility ran during enter-world, before any guild state existed.
            combatController?.BindGuildState(guildPanel);
            // The grade packet that grants AUTH_ADD_MEMBER may be the one that just created the
            // panel, so an already-open target board has no Changed subscription to fire.
            targetBoard?.RefreshFromGuildState();
        }
        private void HandleLoverInfo(Metin2GamePacketCodec.LoverInfo message)
            => gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow")?.SetLover(message);
        private void HandlePointChange(Metin2GamePacketCodec.PointChange change)
        {
            const byte PointLevel = 1;
            const byte PointGold = 11;
            const byte PointLevelStep = 25;
            if(change.Type==PointLevel&&change.Value==5&&entityWorld!=null&&entityWorld.IsLocalPlayer(change.Vid)&&sessionState!=null&&sessionState.SkillGroup==0)
                OpenDoctrineChoose();

            if ((change.Type == PointLevel || change.Type == PointLevelStep) &&
                entityWorld != null && entityWorld.TryResolveActor(change.Vid, out Transform actor) && actor != null)
            {
                if (pointEffectCatalog == null)
                    pointEffectCatalog = Resources.Load<Metin2EffectCatalog>("Metin2EffectCatalog");
                string path = change.Type == PointLevel
                    ? "d:/ymir work/effect/etc/levelup_1/level_up.mse"
                    : "d:/ymir work/effect/etc/skillup/skillup_1.mse";
                if (pointEffectCatalog != null && pointEffectCatalog.TryGet(path, out Metin2EffectDefinition definition))
                    Metin2EffectRuntime.Create(definition, actor);
                else
                    Debug.LogWarning($"METIN2_POINT_EFFECT_MISSING type={change.Type} vid={change.Vid} path={path}");
            }

            // RecvPointChange updates another actor's text-tail level even though CPythonPlayer's
            // own status table is left untouched for non-main VIDs.
            if (change.Type == PointLevel && entityWorld != null && !entityWorld.IsLocalPlayer(change.Vid) &&
                entityWorld.TryGetEntity(change.Vid, out Metin2NetworkEntity remote) && remote != null)
                remote.SetCharacterLevel((uint)Math.Min(uint.MaxValue, Math.Max(0L, change.Value)));

            // game.py::OnPickMoney is only called for the main actor and positive deltas.
            if (change.Type == PointGold && change.Amount > 0 && entityWorld != null && entityWorld.IsLocalPlayer(change.Vid))
            {
                EnsureLootLog()?.AddYang(change.Amount);
            }
        }
        private void OpenDoctrineChoose()
        {
            if(gameUi==null||sessionState==null||sessionState.SkillGroup!=0)return;
            var window=gameUi.Open("doctrinechoosewindow");if(window==null)return;
            var panel=window.GetComponent<Metin2DoctrineChoosePanel>();if(panel==null)panel=window.AddComponent<Metin2DoctrineChoosePanel>();
            panel.Configure(Array.IndexOf(SkillJobs,mainCharacterJob),skillCatalog);
            panel.SelectionRequested-=ConfirmDoctrineChoice;panel.SelectionRequested+=ConfirmDoctrineChoice;
        }
        private void ConfirmDoctrineChoice(int group,string name)
        {
            AskQuestion2(Metin2Language.Format("SKILL_SELECT_DIALOG2",name),"",()=>
            {SendChat("/doctrine_choose "+group);gameUi?.GetPanel<Metin2DoctrineChoosePanel>("doctrinechoosewindow")?.gameObject.SetActive(false);});
        }
        private void HandleLovePoint(byte value)
            => gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow")?.UpdateLovePoint(value);
        private void ShowGuildInvite(uint guildId,string guildName)
        {
            Canvas canvas=ResolveUiCanvas();Transform host=ResolveUiHost();if(canvas==null||host==null||uiCatalog==null||!uiCatalog.TryGet("questiondialog",out GameObject prefab)||prefab==null)return;GameObject dialog=Instantiate(prefab,host,false);PresentDirectQuestionDialog(dialog);Text message=FindNamed<Text>(dialog.transform,"message");BindLocaleText(message,"UNITY_PREFIX_TEXT",()=>new object[]{guildName,Metin2Language.Get("GUILD_DO_YOU_JOIN")});Button accept=FindNamed<Button>(dialog.transform,"accept"),cancel=FindNamed<Button>(dialog.transform,"cancel");if(accept!=null)accept.onClick.AddListener(()=>{AnswerGuildInvite(guildId,true);Destroy(dialog);});if(cancel!=null)cancel.onClick.AddListener(()=>{AnswerGuildInvite(guildId,false);Destroy(dialog);});
        }
        private void ShowMessengerFriendInvite(string name)
        {
            if (gameUi == null) return;
            // interfaceModule owns the MessengerWindow from login onward. Our windows are lazy, so
            // instantiate it once and leave it hidden while the independent question is shown -
            // EnsureWindow builds and binds it without ever touching the Escape stack.
            Metin2MessengerPanel panel = EnsureMessengerPanel();
            panel?.ShowFriendInvite(name, accepted => SendChat("/messenger_auth " + (accepted ? "y " : "n ") + name));
        }
        private void ShowGuildCreationDialog()
        {
            Canvas canvas=ResolveUiCanvas();Transform host=ResolveUiHost();if(canvas==null||host==null||uiCatalog==null||!uiCatalog.TryGet("inputdialog",out GameObject prefab)||prefab==null)return;
            GameObject dialog=Instantiate(prefab,host,false);Text title=FindNamed<Text>(dialog.transform,"TitleName");BindLocaleText(title,"UNITY_CREATE_GUILD");
            InputField input=FindNamed<InputField>(dialog.transform,"InputValue");if(input!=null){input.characterLimit=12;input.ActivateInputField();}
            Button accept=FindNamed<Button>(dialog.transform,"AcceptButton"),cancel=FindNamed<Button>(dialog.transform,"CancelButton");
            if(accept!=null)accept.onClick.AddListener(()=>{if(input!=null&&!string.IsNullOrWhiteSpace(input.text))AnswerGuildCreation(input.text.Trim());Destroy(dialog);});if(cancel!=null)cancel.onClick.AddListener(()=>Destroy(dialog));
        }
        private async void AnswerGuildCreation(string name){try{if(gameSession!=null)await gameSession.SendGuildCreateAsync(name,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private void ApplyPrivateShopSign(Metin2GamePacketCodec.OfflineShopSign sign)
        {
            // CPythonNetworkStream::RecvShopSignPacket marks the main actor as being in private-shop
            // mode when the sign belongs to our own VID. __UseSkill checks that state before every
            // other eligibility rule. The local player has no Metin2NetworkEntity, so preserve it
            // explicitly on the combat controller.
            if (entityWorld != null && sign.Vid == entityWorld.LocalVid)
            {
                if (combatController != null) combatController.PrivateShopOpen = !string.IsNullOrEmpty(sign.Sign);
                return;
            }
            if(entityWorld!=null&&entityWorld.TryGetEntity(sign.Vid,out Metin2NetworkEntity entity))entity.ApplyShopSign(sign.Sign);
        }
        private async void AnswerGuildInvite(uint id,bool accept){try{if(gameSession!=null)await gameSession.SendGuildInviteAnswerAsync(id,accept,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        /// <summary>
        /// The live guild state, WITHOUT showing the window. CPythonGuild exists from login in the
        /// original; here the roster, the grade table and their authority flags live on the guild
        /// window's panel, so the window object has to be instantiated (inactive) as soon as the
        /// first GC_GUILD packet arrives or nothing outside the window can read guild state.
        /// </summary>
        private Metin2GuildPanel EnsureGuildPanel()
            => gameUi == null ? null : gameUi.EnsureWindow("guildwindow")?.GetComponent<Metin2GuildPanel>();

        /// <summary>The guild panel if it already exists - never creates one.</summary>
        private Metin2GuildPanel GuildPanelOrNull()
            => gameUi?.GetPanel<Metin2GuildPanel>("guildwindow");

        private Metin2GuildRenewalState guildRenewalState;
        private Metin2GuildRenewalPanel guildRenewalPanel;
        private Metin2GamePacketCodec.GuildInfoPacket? lastGuildInfo;
        /// <summary>The local guild's INFO block, cached for the character window's guild tooltip
        /// when the window opens after the packet already arrived.</summary>
        private Metin2GamePacketCodec.GuildInfo? characterWindowGuild;
        private string characterWindowGuildMaster = string.Empty;

        private Metin2AurigaCustomSystems customSystems;

        private Metin2PetPanel petPanel;

        /// <summary>
        /// NEW_PET_SYSTEM information board (uipetsystem.py) over the imported
        /// petinformationwindow prefab. The state itself lives in Metin2PetState; the panel only
        /// renders it and forwards the skill/feed actions as chat commands.
        /// </summary>
        private Metin2PetPanel EnsurePetPanel()
        {
            if (petPanel != null) return petPanel;
            if (gameUi == null) return null;
            GameObject window = gameUi.Open("petinformationwindow");
            if (window == null) { Debug.LogWarning("METIN2_PET_PANEL window=missing"); return null; }
            petPanel = window.GetComponent<Metin2PetPanel>() ?? window.AddComponent<Metin2PetPanel>();
            petPanel.BindOriginalWidgets(window.transform);
            petPanel.Configure(petState, itemCatalog, EnsureToolTips());
            petPanel.ChatRequested -= SendChat;
            petPanel.ChatRequested += SendChat;
            gameUi.Close("petinformationwindow");
            return petPanel;
        }

        /// <summary>Opens the pet information board; the taskbar pet button uses this.</summary>
        public void OpenPet()
        {
            Metin2PetPanel panel = EnsurePetPanel();
            if (panel == null) return;
            gameUi?.Open("petinformationwindow");
            panel.Open();
        }

        /// <summary>
        /// M9 custom-system boards. Created on demand; the windows reuse the imported original
        /// prefabs where one exists (battlepasswindow, cuberenewalwindow, biologwindow,
        /// rankingboardwindow) and compact runtime surfaces where the original builds the window in
        /// code (switchbot, wheel of destiny).
        /// </summary>
        private Metin2AurigaCustomSystems EnsureCustomSystems()
        {
            if (customSystems != null) return customSystems;
            customSystems = GetComponent<Metin2AurigaCustomSystems>() ?? gameObject.AddComponent<Metin2AurigaCustomSystems>();
            // The boards live on the UI canvas, not on the bootstrap object. Binding the canvas
            // host and the shared window manager gives them the same Open/Close/Toggle/escape
            // lifecycle as every imported window (map changes keep them alive).
            customSystems.Bind(gameRuntime, itemCatalog, uiCatalog, EnsureToolTips(), EnsureCursorAttachment(), playerState,
                ResolveUiHost(), gameUi);
            Metin2Razor93Leaderboard worldLeaderboard = GetComponent<Metin2Razor93Leaderboard>() ??
                                                        gameObject.AddComponent<Metin2Razor93Leaderboard>();
            worldLeaderboard.Bind(gameRuntime, worldRuntime);
            customSystems.BattlePassActionRequested -= SendBattlePassAction;
            customSystems.BattlePassActionRequested += SendBattlePassAction;
            customSystems.CubeRenewalRequested -= SendCubeRenewal;
            customSystems.CubeRenewalRequested += SendCubeRenewal;
            customSystems.WheelOptionRequested -= SendWheelOption;
            customSystems.WheelOptionRequested += SendWheelOption;
            customSystems.SwitchbotStartRequested -= SendSwitchbotStart;
            customSystems.SwitchbotStartRequested += SendSwitchbotStart;
            customSystems.SwitchbotStopRequested -= SendSwitchbotStop;
            customSystems.SwitchbotStopRequested += SendSwitchbotStop;
            customSystems.SwitchbotItemUseRequested -= SendSwitchbotItemUse;
            customSystems.SwitchbotItemUseRequested += SendSwitchbotItemUse;
            customSystems.StoneCraftRequested -= SendStoneCraft;
            customSystems.StoneCraftRequested += SendStoneCraft;
            customSystems.ItemMoveRequested -= SendCustomItemMove;
            customSystems.ItemMoveRequested += SendCustomItemMove;
            customSystems.ChatRequested -= SendChat;
            customSystems.ChatRequested += SendChat;
            customSystems.AcceRequested -= SendAcceRequest;
            customSystems.AcceRequested += SendAcceRequest;
            customSystems.FishingNewActionRequested -= SendFishingNewAction;
            customSystems.FishingNewActionRequested += SendFishingNewAction;
            customSystems.ItemShopRequested -= RequestOpenItemShop;
            customSystems.ItemShopRequested += RequestOpenItemShop;
            customSystems.NpcShopRequested -= SendNpcShopOpen;
            customSystems.NpcShopRequested += SendNpcShopOpen;
            return customSystems;
        }

        private async void SendNpcShopOpen(int shopId)
        {
            try { if (gameSession != null) await gameSession.SendOpenShopAsync(shopId, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendBattlePassAction(byte action)
        {
            try { if (gameSession != null) await gameSession.SendBattlePassActionAsync(action, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendCubeRenewal(byte subheader, uint indexItem, uint countItem, uint indexItemImprove)
        {
            try { if (gameSession != null) await gameSession.SendCubeRenewalAsync(subheader, indexItem, countItem, indexItemImprove, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendWheelOption(byte option)
        {
            try { if (gameSession != null) await gameSession.SendWheelDestinyAsync(option, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendSwitchbotStart(byte slot, byte[] alternatives)
        {
            try { if (gameSession != null) await gameSession.SendSwitchbotStartAsync(slot, alternatives, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendSwitchbotStop(byte slot)
        {
            try { if (gameSession != null) await gameSession.SendSwitchbotStopAsync(slot, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendSwitchbotItemUse(byte slot)
        {
            try { if (gameSession != null) await gameSession.SendItemUseAsync(9, slot, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>uistonecraft.py:218-220 OnCraft/OnCraftAll - plain chat commands the server's
        /// do_stonecraft handles (cmd_general.cpp:3072-3190).</summary>
        private void SendStoneCraft(uint vnum, bool all)
        {
            SendChat(all ? "/stonecraft makeall" : "/stonecraft make " + vnum.ToString(CultureInfo.InvariantCulture));
        }

        /// <summary>Custom-board item moves (switchbot tab drop) reuse the shared ITEM_MOVE writer.</summary>
        private void SendCustomItemMove(byte fromWindow, ushort fromCell, byte toWindow, ushort toCell, uint count)
            => MoveInventoryItem(fromWindow, fromCell, toWindow, toCell, (int)count);

        /// <summary>Opens one M9 board: 0 battle pass, 1 cube renewal, 2 biolog, 3 ranking, 4 wheel, 5 switchbot.</summary>
        public void OpenCustomSystem(int board)
        {
            Metin2AurigaCustomSystems systems = EnsureCustomSystems();
            switch (board)
            {
                case 0: systems.OpenBattlePass(); break;
                case 1: systems.OpenCubeRenewal(); break;
                case 2: systems.OpenBiolog(); break;
                case 3: systems.OpenRanking(); break;
                case 4: systems.OpenWheel(); break;
                case 5: systems.OpenSwitchbot(); break;
            }
        }

        private Metin2SavepointPanel savepointPanel;

        private Metin2SavepointPanel EnsureSavepointPanel()
        {
            if (savepointPanel != null) return savepointPanel;
            if (gameUi == null) return null;
            GameObject window = gameUi.Open("savepointwindow");
            if (window == null)
            {
                Debug.LogWarning("METIN2_SAVEPOINT window=missing name=savepointwindow");
                return null;
            }
            savepointPanel = window.GetComponent<Metin2SavepointPanel>() ?? window.AddComponent<Metin2SavepointPanel>();
            savepointPanel.GetCurrentMapPosition = CurrentSavepointPosition;
            savepointPanel.Bind();
            savepointPanel.SaveRequested -= PromptSavepointName;
            savepointPanel.SaveRequested += PromptSavepointName;
            savepointPanel.DeleteRequested -= PromptDeleteSavepoint;
            savepointPanel.DeleteRequested += PromptDeleteSavepoint;
            savepointPanel.GoRequested -= PromptGoSavepoint;
            savepointPanel.GoRequested += PromptGoSavepoint;
            savepointPanel.CloseRequested -= CloseSavepointDialogs;
            savepointPanel.CloseRequested += CloseSavepointDialogs;
            gameUi.Close("savepointwindow");
            return savepointPanel;
        }

        private void RequestOpenSavepoint()
        {
            Metin2SavepointPanel panel = EnsureSavepointPanel();
            if (panel == null) return;
            if (panel.IsOpen) { gameUi?.Close("savepointwindow"); return; }
            if (panel.HasSnapshot) { gameUi?.Open("savepointwindow"); panel.Open(); return; }
            SendChat("/open_savepoint");
        }

        private void OpenSavepointFromServer()
        {
            Metin2SavepointPanel panel = EnsureSavepointPanel();
            if (panel == null) return;
            gameUi?.Open("savepointwindow");
            panel.Open();
        }

        private bool TryApplySavepointCommand(string[] tokens)
        {
            if (tokens == null || tokens.Length != 6 ||
                !int.TryParse(tokens[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int id) ||
                !int.TryParse(tokens[3], NumberStyles.Integer, CultureInfo.InvariantCulture, out int mapIndex) ||
                !int.TryParse(tokens[4], NumberStyles.Integer, CultureInfo.InvariantCulture, out int x) ||
                !int.TryParse(tokens[5], NumberStyles.Integer, CultureInfo.InvariantCulture, out int y)) return false;
            Metin2SavepointPanel panel = EnsureSavepointPanel();
            return panel != null && panel.Apply(id, tokens[2], mapIndex, x, y);
        }

        private void PromptSavepointName(int slot)
        {
            OpenOfflineShopQuestion(Locale("SAVEPOINT_SAVEDIALOG"), () =>
                OpenOfflineShopInput(Locale("LOCALIZATION_CHANGE_NAME"), string.Empty, 8,
                    text => SendChat("/save_savepoint " + slot.ToString(CultureInfo.InvariantCulture) + " " + (text ?? string.Empty).Trim())));
        }

        private void PromptDeleteSavepoint(int slot)
            => OpenOfflineShopQuestion(Locale("SAVEPOINT_EMPTYITDIALOG"), () =>
                SendChat("/empty_savepoint " + slot.ToString(CultureInfo.InvariantCulture)));

        private void PromptGoSavepoint(int slot)
            => OpenOfflineShopQuestion(Locale("SAVEPOINT_TELEPORTDIALOG"), () =>
                SendChat("/go_savepoint " + slot.ToString(CultureInfo.InvariantCulture)));

        private Vector2Int CurrentSavepointPosition()
        {
            if (worldRuntime == null || !worldRuntime.HasMap || entityWorld == null || entityWorld.LocalPlayerRoot == null)
                return Vector2Int.zero;
            worldRuntime.Current.ToGlobal(entityWorld.LocalPlayerRoot.position, out int globalX, out int globalY);
            return new Vector2Int((globalX - worldRuntime.Current.baseX) / 100,
                                  (globalY - worldRuntime.Current.baseY) / 100);
        }

        private void CloseSavepointDialogs() => CloseOfflineShopDialogs();

        /// <summary>GC_GUILD INFO sub 8 with ADVANCED_GUILD_INFO: trophies and war counters.</summary>
        private void OnGuildInfoPacket(Metin2GamePacketCodec.GuildInfoPacket packet)
        {
            lastGuildInfo = packet;
            GuildPanelOrNull()?.ApplyTrophies(packet.Trophies, packet.Win, packet.Loss, packet.Draw);
        }

        /// <summary>GC_SYMBOL_DATA (133): persist the guild symbol bitmap for the mark texture.</summary>
        private void OnGuildSymbolData(Metin2GamePacketCodec.GuildSymbolData data)
        {
            if (Metin2GuildSymbolStore.Apply(data.GuildId, data.Data))
                Debug.Log($"METIN2_GUILD_SYMBOL_RECEIVED guild={data.GuildId} bytes={data.Data.Length}");
        }

        private void OnSwitchbotPacket(Metin2GamePacketCodec.SwitchbotPacket packet)
            => EnsureCustomSystems().ApplySwitchbot(packet);

        /// <summary>GC_BOSS_POSITION (222): CPythonMiniMap::ClearAtlasMarkInfoBoss + re-add.</summary>
        private Metin2GamePacketCodec.BossPosition[] atlasBossPositions=Array.Empty<Metin2GamePacketCodec.BossPosition>();
        private void OnBossPositions(Metin2GamePacketCodec.BossPosition[] positions)
        {
            atlasBossPositions=positions??Array.Empty<Metin2GamePacketCodec.BossPosition>();
            gameUi?.GetPanel<Metin2MiniMapPanel>("minimap")?.SetBossPositions(atlasBossPositions);
            gameUi?.GetPanel<Metin2AtlasWindowPanel>("atlaswindow")?.SetBossPositions(atlasBossPositions);
        }
        private void ConfirmAtlasBossTeleport(Metin2GamePacketCodec.BossPosition boss)
        {
            string name=Metin2MobNames.TryGetName(boss.NameId,out var resolved)?resolved:boss.NameId.ToString();
            string question=Metin2Language.Format("ATLASINFO_QUESTIONDIALOG1",name);
            AskQuestion2(question,Metin2Language.Get("ATLASINFO_QUESTIONDIALOG2"),()=>SendChat($"/gotoxy {boss.X/100} {boss.Y/100}"));
        }

        /// <summary>GC_ITEM_ON_TITLE (246): the player's title name changes in the text tail.</summary>
        private void OnItemOnTitle(Metin2GamePacketCodec.ItemOnTitleUpdate update)
            => entityWorld?.ApplyItemOnTitle(update.Vid, update.Name);

        /// <summary>GC 249: authoritative occupied mount/belt slot counts for one visible PC.</summary>
        private void OnFakeShopSign(Metin2GamePacketCodec.FakeShopSign sign)
            => entityWorld?.ApplyFakeShopSign(sign);

        private void OnFishingNewPacket(Metin2GamePacketCodec.FishingNewPacket packet)
            => EnsureCustomSystems().ApplyFishingNew(packet,
                entityWorld != null && packet.Vid == entityWorld.LocalVid);

        private void OnAccePacket(Metin2GamePacketCodec.AccePacket packet)
            => EnsureCustomSystems().ApplyAcce(packet);

        private async void SendFishingNewAction(byte subheader)
        {
            try
            {
                if (gameSession != null)
                    // CPythonNetworkStream::SendFishingPacketNew(r, i) sends vid=0, dir=r/5,
                    // need=0 and count=0. Stop/Catch/Failed all call it with r=0, i unused.
                    await gameSession.SendFishingNewAsync(subheader, 0u, 0, 0, 0, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void OnEventManagerPacket(Metin2GamePacketCodec.EventManagerPacket packet)
            => EnsureCustomSystems().ApplyEventManager(packet);

        private async void SendAcceRequest(byte subheader, bool window, uint price, byte position, byte itemWindow,
            ushort itemCell, uint itemVnum, uint minAbs, uint maxAbs)
        {
            try
            {
                if (gameSession != null)
                    await gameSession.SendAcceAsync(subheader, window, price, position, itemWindow, itemCell,
                        itemVnum, minAbs, maxAbs, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// uiguildrenewal.py over the imported guildrenewalwindow prefab. The window is created on
        /// demand; /gr_open asks the server for the full state.
        /// </summary>
        private Metin2GuildRenewalPanel EnsureGuildRenewalPanel()
        {
            if (guildRenewalPanel != null) return guildRenewalPanel;
            if (gameUi == null) return null;
            GameObject window = gameUi.Open("guildrenewalwindow");
            if (window == null) { Debug.LogWarning("METIN2_GUILD_RENEWAL window=missing"); return null; }
            guildRenewalPanel = window.GetComponent<Metin2GuildRenewalPanel>() ?? window.AddComponent<Metin2GuildRenewalPanel>();
            guildRenewalPanel.BindCatalog(uiCatalog, playerState);
            guildRenewalPanel.BindOriginalWidgets(window.transform);
            guildRenewalPanel.Configure(guildRenewalState, itemCatalog, EnsureCursorAttachment(), EnsureToolTips());
            guildRenewalPanel.BindProviders(
                () => GuildPanelOrNull() != null ? GuildPanelOrNull().GuildLevel : 0,
                pid => GuildPanelOrNull()?.MemberName(pid) ?? null,
                () => GuildPanelOrNull()?.Members);
            guildRenewalPanel.DepositItemRequested -= DepositGuildRenewalItem;
            guildRenewalPanel.DepositItemRequested += DepositGuildRenewalItem;
            guildRenewalPanel.DepositYangRequested -= DepositGuildRenewalYang;
            guildRenewalPanel.DepositYangRequested += DepositGuildRenewalYang;
            guildRenewalPanel.LevelUpRequested -= LevelUpGuildRenewal;
            guildRenewalPanel.LevelUpRequested += LevelUpGuildRenewal;
            guildRenewalPanel.PayRequested -= PayGuildRenewal;
            guildRenewalPanel.PayRequested += PayGuildRenewal;
            guildRenewalPanel.CloseRequested -= CloseGuildRenewal;
            guildRenewalPanel.CloseRequested += CloseGuildRenewal;
            guildRenewalPanel.InfoRequested -= ShowGuildRenewalInfo;
            guildRenewalPanel.InfoRequested += ShowGuildRenewalInfo;
            gameUi.Close("guildrenewalwindow");
            return guildRenewalPanel;
        }

        private void OpenGuildRenewal()
        {
            Metin2GuildRenewalPanel panel = EnsureGuildRenewalPanel();
            if (panel == null) return;
            gameUi?.Open("guildrenewalwindow");
            SendChat("/gr_open");
            guildRenewalState?.HandleCommand("gr_open", Array.Empty<string>());
        }

        private void CloseGuildRenewal()
        {
            guildRenewalPanel?.ClosePayDialog();
            guildRenewalState?.MarkClosed();
            gameUi?.Close("guildrenewalwindow");
        }

        private void DepositGuildRenewalItem(ushort cell, ushort count)
            => SendChat($"/gr_deposit_item {cell} {count}");
        private void ShowGuildRenewalInfo(string message) => chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, message);

        private void DepositGuildRenewalYang(long amount)
        {
            // The panel now owns the native PickMoneyDialog; this event carries its accepted
            // amount, not a guessed remaining-requirement maximum.
            if (amount > 0) SendChat($"/gr_deposit_yang {amount}");
        }

        private void LevelUpGuildRenewal() => SendChat("/gr_levelup");

        private void PayGuildRenewal(long yang, uint[] vnums, uint[] counts)
        {
            var builder = new System.Text.StringBuilder("/gr_pay_tax ");
            builder.Append(Math.Max(0L, yang).ToString(CultureInfo.InvariantCulture));
            int pairs = Math.Min(Metin2GuildRenewalState.RequirementCount,
                Math.Min(vnums?.Length ?? 0, counts?.Length ?? 0));
            for (int i = 0; i < pairs; i++)
                builder.Append(' ').Append(vnums[i].ToString(CultureInfo.InvariantCulture))
                    .Append(' ').Append(counts[i].ToString(CultureInfo.InvariantCulture));
            SendChat(builder.ToString());
        }

        /// <summary>
        /// The live messenger state, WITHOUT showing the window. CPythonMessenger owns the friend
        /// list from login in the original; here it lives on the messenger window's panel, which is
        /// created on demand - so it has to be instantiated (inactive) on the first messenger
        /// packet or nothing outside the window can ask who is a friend.
        /// </summary>
        private Metin2MessengerPanel EnsureMessengerPanel()
            => gameUi == null ? null : gameUi.EnsureWindow("messengerwindow")?.GetComponent<Metin2MessengerPanel>();

        /// <summary>The messenger panel if it already exists - never creates one.</summary>
        private Metin2MessengerPanel MessengerPanelOrNull()
            => gameUi?.GetPanel<Metin2MessengerPanel>("messengerwindow");

        private Metin2PartyPanel EnsurePartyPanel()
        {
            if(partyPanel!=null)return partyPanel;Canvas c=ResolveUiCanvas();Transform host=ResolveUiHost();if(c==null||host==null)return null;
            partyPanel=GetComponent<Metin2PartyPanel>()??gameObject.AddComponent<Metin2PartyPanel>();
            partyPanel.Initialize(uiCatalog,host,()=>entityWorld!=null?entityWorld.LocalVid:0,
                ()=>PartySkillLevel(),localeText);
            partyPanel.RemoveRequested-=RemovePartyMember;partyPanel.RemoveRequested+=RemovePartyMember;
            partyPanel.StateRequested-=SetPartyMemberState;partyPanel.StateRequested+=SetPartyMemberState;
            partyPanel.DistributionModeRequested-=SetPartyDistribution;partyPanel.DistributionModeRequested+=SetPartyDistribution;
            partyPanel.SkillRequested-=UsePartySkill;partyPanel.SkillRequested+=UsePartySkill;
            return partyPanel;
        }
        /// <summary>uiCommon.QuestionDialog SetAcceptText / SetCancelText.</summary>
        private void SetDialogButtonText(Button button, string localeKey)
        {
            if (button == null || localeText == null) return;
            Text label = button.GetComponentInChildren<Text>(true);
            if (label != null) label.text = localeText.Get(localeKey);
        }

        private static void PresentDirectQuestionDialog(GameObject dialog)
        {
#if UNITY_ANDROID || UNITY_IOS
            Metin2MobileWindowPresentation.Apply("questiondialog", dialog);
#endif
        }

        /// <summary>
        /// game.py __PartyRequestQuestion (:2298-2308): someone asked to join MY party. The answer
        /// is a chat command carrying the requester's VID, never a party packet.
        /// </summary>
        private void ShowPartyRequest(uint vid)
        {
            Canvas canvas=ResolveUiCanvas();Transform host=ResolveUiHost();if(canvas==null||host==null||uiCatalog==null||!uiCatalog.TryGet("questiondialog",out GameObject prefab)||prefab==null)return;
            GameObject dialog=Instantiate(prefab,host,false);PresentDirectQuestionDialog(dialog);string who=vid.ToString();
            if(entityWorld!=null&&entityWorld.TryGetEntity(vid,out Metin2NetworkEntity requester)&&!string.IsNullOrEmpty(requester.DisplayName))who=requester.DisplayName;
            Text message=FindNamed<Text>(dialog.transform,"message");BindLocaleText(message,"UNITY_PREFIX_TEXT",()=>new object[]{who,Metin2Language.Get("PARTY_DO_YOU_ACCEPT")});
            Button accept=FindNamed<Button>(dialog.transform,"accept"),cancel=FindNamed<Button>(dialog.transform,"cancel");
            SetDialogButtonText(accept,"UI_ACCEPT");SetDialogButtonText(cancel,"UI_DENY");
            if(accept!=null)accept.onClick.AddListener(()=>{SendChat("/party_request_accept "+vid);Destroy(dialog);});
            if(cancel!=null)cancel.onClick.AddListener(()=>{SendChat("/party_request_deny "+vid);Destroy(dialog);});
        }
        private void ShowPartyInvite(uint leaderPid)
        {
            Canvas canvas=ResolveUiCanvas();Transform host=ResolveUiHost();if(canvas==null||host==null||uiCatalog==null||!uiCatalog.TryGet("questiondialog",out GameObject prefab)||prefab==null)return;
            GameObject dialog=Instantiate(prefab,host,false);PresentDirectQuestionDialog(dialog);string leader=leaderPid.ToString();if(entityWorld!=null&&entityWorld.TryGetEntity(leaderPid,out Metin2NetworkEntity entity)&&!string.IsNullOrEmpty(entity.DisplayName))leader=entity.DisplayName;
            Text message=FindNamed<Text>(dialog.transform,"message");BindLocaleText(message,"UNITY_PREFIX_TEXT",()=>new object[]{leader,Metin2Language.Get("PARTY_DO_YOU_JOIN")});
            Button accept=FindNamed<Button>(dialog.transform,"accept"),cancel=FindNamed<Button>(dialog.transform,"cancel");
            SetDialogButtonText(accept,"UI_ACCEPT");SetDialogButtonText(cancel,"UI_DENY");
            if(accept!=null)accept.onClick.AddListener(()=>{AnswerPartyInvite(leaderPid,true);Destroy(dialog);});if(cancel!=null)cancel.onClick.AddListener(()=>{AnswerPartyInvite(leaderPid,false);Destroy(dialog);});
        }
        private async void AnswerPartyInvite(uint pid,bool accept){try{if(gameSession!=null)await gameSession.SendPartyInviteAnswerAsync(pid,accept,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private int PartySkillLevel()
        {
            const int partySkillVnum = 121;
            if (playerState == null || partySkillVnum >= playerState.Skills.Count) return 0;
            Metin2GamePacketCodec.SkillLevel skill = playerState.Skills[partySkillVnum];
            // GC_SKILL_LEVEL already contains the absolute level, including mastery.
            return skill.Level;
        }
        private async void RemovePartyMember(uint pid){try{if(gameSession!=null)await gameSession.SendPartyRemoveAsync(pid,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void SetPartyMemberState(uint pid,byte state,bool enabled){try{if(gameSession!=null)await gameSession.SendPartySetStateAsync(pid,state,enabled,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void SetPartyDistribution(byte mode){try{if(gameSession!=null)await gameSession.SendPartyParameterAsync(mode,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void UsePartySkill(byte skill,uint targetVid){try{if(gameSession!=null)await gameSession.SendPartyUseSkillAsync(skill,targetVid,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private static T FindNamed<T>(Transform root,string name)where T:Component{foreach(T c in root.GetComponentsInChildren<T>(true))if(c.gameObject.name==name)return c;return null;}

        private void OpenWhisperDialog()
        {
            Canvas canvas = ResolveUiCanvas(); Transform host = ResolveUiHost(); if (canvas == null || host == null) return;
            if (untargetedWhisperDialog == null)
            {
                untargetedWhisperDialog = CreateWhisperDialog(host);
                PositionNewWhisperDialog(untargetedWhisperDialog);
            }
            untargetedWhisperDialog?.OpenWithoutTarget();
        }
        private Metin2WhisperDialog GetWhisperDialog(string target)
        {
            target = (target ?? string.Empty).Trim();
            if (string.IsNullOrEmpty(target)) return null;
            if (whisperDialogs.TryGetValue(target, out Metin2WhisperDialog dialog) && dialog != null)
            {
                dialog.SetHistory(GetWhisperHistory(target));
                dialog.OpenWithTarget(target);
                ApplyKnownWhisperLanguage(target, dialog);
                if (whisperGameMasters.Contains(target)) dialog.SetGameMasterLook();
                whisperIcons?.Remove(target);
                return dialog;
            }
            Canvas canvas = ResolveUiCanvas(); Transform host = ResolveUiHost(); if (canvas == null || host == null) return null;
            dialog = CreateWhisperDialog(host); if (dialog == null) return null;
            PositionNewWhisperDialog(dialog);
            dialog.SetHistory(GetWhisperHistory(target));
            dialog.OpenWithTarget(target);
            ApplyKnownWhisperLanguage(target, dialog);
            if (whisperGameMasters.Contains(target)) dialog.SetGameMasterLook();
            whisperDialogs[target] = dialog;
            whisperIcons?.Remove(target);
            return dialog;
        }

        private readonly Dictionary<string, string> whisperLanguages = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        /// <summary>
        /// interfacemodule ShowWhisperDialog sends CG_REQUEST_LANGUAGE (121) for the partner and
        /// displays the answer; a cached value is applied immediately, otherwise the request is
        /// sent and GC_RECV_LANGUAGE (141) fills the title through OnRecvLanguage.
        /// </summary>
        private void ApplyKnownWhisperLanguage(string target, Metin2WhisperDialog dialog)
        {
            if (whisperLanguages.TryGetValue(target, out string language) && !string.IsNullOrEmpty(language))
                dialog.SetTargetLanguage(language);
            else
                SendWhisperLanguageRequest(target);
        }

        private async void SendWhisperLanguageRequest(string target)
        {
            try { if (gameSession != null) await gameSession.SendRequestLanguageAsync(target, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void OnRecvLanguage(Metin2GamePacketCodec.RecvLanguage info)
        {
            string name = (info.Name ?? string.Empty).Trim();
            if (string.IsNullOrEmpty(name)) return;
            whisperLanguages[name] = info.Language ?? string.Empty;
            if (whisperDialogs.TryGetValue(name, out Metin2WhisperDialog dialog) && dialog != null)
                dialog.SetTargetLanguage(info.Language);
        }
        /// <summary>interfacemodule ShowWhisperDialog: the mail button opens the dialog with the
        /// partner name and destroys its own icon.</summary>
        private void OpenWhisperDialog(string name) => GetWhisperDialog(name);
        private Metin2WhisperDialog CreateWhisperDialog(Transform canvas)
        {
            Metin2WhisperDialog dialog = Metin2WhisperDialog.Create(uiCatalog, canvas); if (dialog == null) return null;
            dialog.BindItemLinks(playerState,itemCatalog,EnsureCursorAttachment(),EnsureToolTips());
            dialog.SendRequested += SendWhisperTyped; dialog.CommandRequested += SendWhisperCommand;
            dialog.TargetAccepted += RegisterTemporaryWhisperDialog;
            dialog.Minimized += MinimizeWhisperDialog;
            dialog.Closed += CloseWhisperDialog;
            return dialog;
        }
        private async void SendWhisperTyped(Metin2WhisperDialog dialog, string target, string message)
        {
            var record = new Metin2WhisperRecord(0, mainCharacterName + " : " + message);
            GetWhisperHistory(target).Add(record);
            dialog.Append(record);
            try { if (gameSession != null) await gameSession.SendWhisperAsync(target, message, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void SendWhisperCommand(Metin2WhisperDialog _, string command)
        {
            try { if (gameSession != null) await gameSession.SendChatAsync(Metin2ChatWindow.ChatTypeTalking, command, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private List<Metin2WhisperRecord> GetWhisperHistory(string target)
        {
            if (!whisperHistory.TryGetValue(target, out List<Metin2WhisperRecord> records))
                whisperHistory[target] = records = new List<Metin2WhisperRecord>();
            return records;
        }

        private void RegisterTemporaryWhisperDialog(Metin2WhisperDialog dialog, string name)
        {
            if (dialog == null || dialog != untargetedWhisperDialog || string.IsNullOrWhiteSpace(name)) return;
            name = name.Trim();
            whisperIcons?.Remove(name);
            if (whisperDialogs.TryGetValue(name, out Metin2WhisperDialog oldDialog) && oldDialog != null && oldDialog != dialog)
                Destroy(oldDialog.gameObject);
            whisperDialogs[name] = dialog;
            untargetedWhisperDialog = null;
            dialog.SetHistory(GetWhisperHistory(name));
            if (whisperGameMasters.Contains(name)) dialog.SetGameMasterLook();
        }

        private void MinimizeWhisperDialog(Metin2WhisperDialog dialog)
        {
            if (dialog == null) return;
            string name = dialog.Target;
            if (dialog == untargetedWhisperDialog) untargetedWhisperDialog = null;
            if (!string.IsNullOrEmpty(name))
            {
                whisperDialogs.Remove(name);
                EnsureWhisperIcons()?.Show(name, whisperGameMasters.Contains(name), false);
            }
            Destroy(dialog.gameObject);
        }

        private void CloseWhisperDialog(Metin2WhisperDialog dialog)
        {
            if (dialog == null) return;
            string name = dialog.Target;
            if (dialog == untargetedWhisperDialog) untargetedWhisperDialog = null;
            if (!string.IsNullOrEmpty(name))
            {
                whisperDialogs.Remove(name);
                whisperHistory.Remove(name); // chat.ClearWhisper(name)
                whisperGameMasters.Remove(name);
                whisperIcons?.Remove(name);
            }
        }

        private void PositionNewWhisperDialog(Metin2WhisperDialog dialog)
        {
            if (dialog == null) return;
            dialog.SetWindowOffset(whisperWindowOpenPosition);
            whisperWindowOpenPosition = (whisperWindowOpenPosition + 1) % 5;
        }

        private void ClearWhisperState()
        {
            foreach (Metin2WhisperDialog dialog in whisperDialogs.Values)
                if (dialog != null) Destroy(dialog.gameObject);
            whisperDialogs.Clear();
            whisperHistory.Clear();
            whisperGameMasters.Clear();
            if (untargetedWhisperDialog != null) Destroy(untargetedWhisperDialog.gameObject);
            untargetedWhisperDialog = null;
            whisperIcons?.Clear();
            whisperWindowOpenPosition = 0;
        }

        /// <summary>The chat line's own send path, which already knows the type from its prefix.</summary>
        private async void SendChatTyped(byte type, string message)
        {
            if (TryRunLocalAdminEnvironmentCommand(message)) return;
            if (type == ChatTypeTalking) MarkIntentionalExit(message);
            try { if (gameSession != null) await gameSession.SendChatAsync(type, message, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// Local, visual-only GM controls. These commands are deliberately consumed before the
        /// network chat path, so testing weather never depends on a matching server command and
        /// cannot alter authoritative game state.
        /// </summary>
        private bool TryRunLocalAdminEnvironmentCommand(string message)
        {
            if (string.IsNullOrWhiteSpace(message)) return false;
            string[] words = message.Trim().Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            if (words.Length == 0) return false;
            string command = words[0].ToLowerInvariant();
            if (command != "/weather" && command != "/time") return false;

            if (!IsLocalEnvironmentAdmin())
            {
                ShowLocalEnvironmentMessage(Metin2Language.Get("UNITY_ADMIN_ENV_ONLY"));
                return true;
            }

            Metin2.Runtime.Metin2GraphicsProfileManager manager =
                Metin2.Runtime.Metin2GraphicsProfileManager.Instance ??
                Metin2.Runtime.Metin2GraphicsProfileManager.Ensure();
            Metin2.Runtime.Metin2DynamicSkyWeather sky =
                manager != null ? manager.GetComponent<Metin2.Runtime.Metin2DynamicSkyWeather>() : null;
            if (sky == null || manager.Mode == Metin2.Runtime.Metin2GraphicsMode.Classic)
            {
                ShowLocalEnvironmentMessage(Metin2Language.Get("UNITY_ENV_GRAPHICS_REQUIRED"));
                return true;
            }

            if (command == "/weather")
            {
                if (words.Length < 2 || words[1].Equals("help", StringComparison.OrdinalIgnoreCase))
                {
                    ShowLocalEnvironmentMessage(Metin2Language.Get("UNITY_WEATHER_COMMANDS"));
                    return true;
                }

                string value = words[1].ToLowerInvariant();
                if (value == "status")
                {
                    ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_WEATHER_STATUS",WeatherLabel(sky.Weather),Metin2Language.Get(sky.AutomaticWeather?"UNITY_SETTING_ON":"UNITY_SETTING_OFF"),sky.WeatherSecondsRemaining.ToString("0",Metin2Language.Culture),sky.timeOfDay.ToString("0.0",Metin2Language.Culture)));
                    return true;
                }
                if (value == "auto")
                {
                    bool turnOn = words.Length < 3 || words[2].Equals("on", StringComparison.OrdinalIgnoreCase);
                    if (words.Length >= 3 && words[2].Equals("off", StringComparison.OrdinalIgnoreCase)) turnOn = false;
                    else if (words.Length >= 3 && !words[2].Equals("on", StringComparison.OrdinalIgnoreCase))
                    {
                        ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_COMMAND_USAGE",Metin2Language.Get("UNITY_WEATHER_AUTO_COMMAND")));
                        return true;
                    }
                    sky.SetAutomaticWeather(turnOn);
                    ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_WEATHER_AUTO",Metin2Language.Get(turnOn?"UNITY_SETTING_ON":"UNITY_SETTING_OFF")));
                    return true;
                }

                Metin2.Runtime.Metin2WeatherState state;
                switch (value)
                {
                    case "sunny": case "sun": case "clear":
                        state = Metin2.Runtime.Metin2WeatherState.Clear; break;
                    case "rain":
                        state = Metin2.Runtime.Metin2WeatherState.Rain; break;
                    case "snow":
                        state = Metin2.Runtime.Metin2WeatherState.Snow; break;
                    case "thunder": case "storm": case "lightning":
                        state = Metin2.Runtime.Metin2WeatherState.Storm; break;
                    default:
                        ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_WEATHER_UNKNOWN",Metin2Language.Format("UNITY_COMMAND_USAGE",Metin2Language.Get("UNITY_WEATHER_SET_COMMAND"))));
                        return true;
                }
                sky.SetAutomaticWeather(false);
                sky.SetWeather(state);
                ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_WEATHER_SET",WeatherLabel(state),Metin2Language.Get("UNITY_SETTING_OFF")));
                return true;
            }

            if (words.Length < 2)
            {
                ShowLocalEnvironmentMessage(Metin2Language.Get("UNITY_TIME_COMMAND"));
                return true;
            }
            string time = words[1].ToLowerInvariant();
            float hour;
            if (time == "day" || time == "noon") hour = 13f;
            else if (time == "night" || time == "midnight") hour = 0f;
            else if (time == "dawn" || time == "morning") hour = 7f;
            else if (time == "dusk" || time == "evening") hour = 19f;
            else if (!float.TryParse(time, NumberStyles.Float, CultureInfo.InvariantCulture, out hour) || hour < 0f || hour > 24f)
            {
                ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_TIME_INVALID",Metin2Language.Format("UNITY_COMMAND_USAGE",Metin2Language.Get("UNITY_TIME_COMMAND"))));
                return true;
            }
            sky.SetTime(hour);
            ShowLocalEnvironmentMessage(Metin2Language.Format("UNITY_TIME_SET",sky.timeOfDay.ToString("0.0",Metin2Language.Culture)));
            return true;
        }

        private static string WeatherLabel(Metin2.Runtime.Metin2WeatherState weather)
        {
            switch(weather)
            {
                case Metin2.Runtime.Metin2WeatherState.Rain: return Metin2Language.Get("UNITY_WEATHER_RAIN");
                case Metin2.Runtime.Metin2WeatherState.Snow: return Metin2Language.Get("UNITY_WEATHER_SNOW");
                case Metin2.Runtime.Metin2WeatherState.Storm: return Metin2Language.Get("UNITY_WEATHER_STORM");
                default: return Metin2Language.Get("UNITY_WEATHER_CLEAR");
            }
        }
        private bool IsLocalEnvironmentAdmin()
        {
            string name = (mainCharacterName ?? string.Empty).Trim();
            string[] prefixes = { "[GM]", "[GA]", "[SA]", "[SGM]", "[TGM]", "[DEV]" };
            for (int i = 0; i < prefixes.Length; i++)
                if (name.StartsWith(prefixes[i], StringComparison.OrdinalIgnoreCase)) return true;
            return name.Equals("admin", StringComparison.OrdinalIgnoreCase);
        }

        private void ShowLocalEnvironmentMessage(string text)
        {
            if (chatWindow != null) chatWindow.AppendChat(Metin2ChatWindow.ChatTypeInfo, text);
            else chatPanel?.DisplayInfo(text);
            Debug.Log("METIN2_ADMIN_ENV " + text);
        }

        /// <summary>game.py:1312 - a registration dragged out onto the world is deleted.</summary>
        private async void DeleteQuickSlotRegistration(int global)
        {
            if (!CanActMainInstance()) return;
            if (global < 0 || global >= Metin2QuickSlotBar.TotalSlots) return;
            try { if (gameSession != null) await gameSession.SendQuickSlotDeleteAsync((byte)global, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private void HandleSkillCooldownEnded(byte skillVnum) => combatController?.EndSkillCooldown(skillVnum);
        // The legacy base-client CG_OFFLINESHOP buy (57) is dead code on this server: header 57 is
        // HEADER_CG_OPENSHOP here, and the live buy path is SendAurigaOfflineShopBuyAsync through
        // PromptOfflineShopBuy / the Auriga boards. Removed rather than left as a landmine.
        // Auriga CG_NEW_OFFLINESHOP (119) request paths. Every one of these replaces the legacy
        // 57/58 header calls: the Auriga server only knows the 119 subheader protocol.
        private async void RefreshOfflineShop() { try { if (gameSession != null) await gameSession.SendAurigaOfflineShopOpenOwnerAsync(lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void RefreshOfflineShopMoney()
        {
            // The safebox must be open before GetValutes is accepted; asking for zero valutes is
            // the original client's "refresh the window" call and answers with SHOP_SAFEBOX_REFRESH.
            try { if (gameSession != null) { await gameSession.SendAurigaOfflineShopSafeboxOpenAsync(lifetime.Token); await gameSession.SendAurigaOfflineShopSafeboxGetValutesAsync(0, lifetime.Token); } }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void CheckOfflineShop() { try { if (gameSession != null) await gameSession.SendAurigaOfflineShopOpenOwnerAsync(lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void CloseOfflineShop() { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendAurigaOfflineShopCloseBoardAsync(lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private void OnAurigaShopOpened(Metin2GamePacketCodec.AurigaShopInfo shop,Metin2GamePacketCodec.AurigaShopItem[] items)=>offlineShopCustomerPanel?.DisplayAurigaShop(shop,items);
        private void OnAurigaOwnerShopOpened(Metin2GamePacketCodec.AurigaShopInfo shop,Metin2GamePacketCodec.AurigaShopItem[] items,Metin2GamePacketCodec.AurigaShopItem[] sold,Metin2GamePacketCodec.AurigaOfferInfo[] offers)=>offlineShopPanel?.DisplayAurigaOwner(shop,items,sold);
        private void OnAurigaOwnerShopMissing()=>offlineShopPanel?.DisplayAurigaOwnerNoShop();
        private void OnAurigaShopClosed()=>offlineShopCustomerPanel?.CloseAuriga();
        private void OnAurigaSafeboxChanged(long yang,uint[] ids,Metin2GamePacketCodec.AurigaItemInfoEx[] items)=>offlineShopPanel?.SetAurigaMoney(yang);
        // GC_MOUNT_INVENTORY (224) answers the client's mount-bag open/refresh; the snapshot is
        // stored by the player state and the runtime grid repaints (uimountinventory.py:339
        // BINARY_RefreshMountInventory -> Refresh()). The packet is data, never visibility: the
        // original window is shown only by ToggleMountInventory, so a snapshot arriving during a
        // warp must not pop the window open (and the close path must not be defeated by the next
        // snapshot silently re-showing it).
        private void OnMountInventoryReceived(Metin2GamePacketCodec.MountInventoryPacket packet)
        {
            if (mountInventoryPanel == null) return;
            mountInventoryPanel.Refresh();
        }
        // NEW_PET_SYSTEM: PetUnsummon resets every pet window (game.py:3507 SetDefaultInfo + Close).
        private void OnPetUnsummoned() => Debug.Log("METIN2_PET_UNSUMMON");
        // OpenPetIncubator opens uipetincubatrice; the window prefab is part of the pet UI migration.
        private void OnPetIncubatorRequested() => Debug.Log("METIN2_PET_INCUBATOR_REQUESTED ASSET_PENDING_M10");
        private void SpawnLocalPlayer(int index)
        {
            if (index < 0 || currentCharacters.Characters == null || index >= currentCharacters.Characters.Length) return; var character = currentCharacters.Characters[index];
            GameObject prefab = Metin2PlayerPrefabs.Get(null, character.Job);
            GameObject instance = new GameObject("LocalPlayer_" + character.Name);
            GameObject visual = prefab != null ? Instantiate(prefab, instance.transform, false) : null;
            if (visual != null) visual.name = "Visual_" + prefab.name;
            // The converted character meshes are in Metin2 centimetres; the world is in metres.
            if (visual != null) Metin2.Runtime.Metin2Units.ApplyCharacterScale(visual);
            localController = instance.GetComponent<Metin2PlayerController>() ?? instance.AddComponent<Metin2PlayerController>();
            combatController = instance.GetComponent<Metin2CombatController>() ?? instance.AddComponent<Metin2CombatController>();
            combatController.BindSkillAnimations(playerState, skillDescriptionCatalog);
            combatController.SetComboType(activeComboType);
            combatController.BindEntityWorld(entityWorld);
            combatController.MetinFarmInfoRequested -= AppendInfoLine;
            combatController.MetinFarmInfoRequested += AppendInfoLine;
            combatController.BindSkillEligibility(worldRuntime, itemCatalog, equipmentCatalog, pvpState,
                GetComponent<Metin2PartyPanel>(), GuildPanelOrNull(), localEmpire);
            Metin2.Runtime.Metin2CharacterEquipment equipment = instance.GetComponentInChildren<Metin2.Runtime.Metin2CharacterEquipment>(true);
            Metin2EquipmentRuntimeBinder equipmentBinder = instance.GetComponent<Metin2EquipmentRuntimeBinder>() ?? instance.AddComponent<Metin2EquipmentRuntimeBinder>();
            equipmentBinder.Bind(playerState, equipmentCatalog, equipment, character.Job, itemCatalog);
            // The hairstyle is not an item: it rides in CHR_EQUIPPART_HAIR, which the select
            // list already carries as HairPart and the server repeats in every ADDITIONAL and
            // UPDATE for this character. Both feed the same setter.
            equipmentBinder.SetHairPart(character.HairPart);
            if (entityWorld != null)
            {
                entityWorld.LocalPartsChanged -= ApplyLocalParts; entityWorld.LocalPartsChanged += ApplyLocalParts;
                entityWorld.LocalMountChanged -= ApplyLocalMount; entityWorld.LocalMountChanged += ApplyLocalMount;
                entityWorld.LocalActorSpeedsChanged -= ApplyLocalActorSpeeds; entityWorld.LocalActorSpeedsChanged += ApplyLocalActorSpeeds;
                entityWorld.LocalTextTailChanged -= ApplyLocalTextTail; entityWorld.LocalTextTailChanged += ApplyLocalTextTail;
                localEquipmentBinder = equipmentBinder;
                for(int part=1;part<=4;part++)equipmentBinder.SetCostumeHidden(part,hiddenCostumeParts[part]);
                if (entityWorld.LocalParts != null && entityWorld.LocalParts.Length >= 4)
                    ApplyLocalParts(entityWorld.LocalParts);
            }
            if (visual != null)
            {
                localMountEntity = instance.AddComponent<Metin2NetworkEntity>();
                localMountEntity.Initialize(entityWorld != null ? entityWorld.LocalVid : 0u, character.Job, 6, 100, 100,
                    equipmentCatalog, null, visual.transform);
                localMountEntity.BindTextTail(localeText, instance.transform,
                    entityWorld == null ? string.Empty : entityWorld.ResolveGuildName(entityWorld.LocalGuildId));
                localMountEntity.SetTextTailMetadata(character.Name, character.Level,
                    entityWorld == null ? 0L : entityWorld.LocalAlignmentRaw,
                    entityWorld != null && entityWorld.LocalIdentityKnown ? entityWorld.LocalEmpire : localEmpire,
                    entityWorld == null ? 0u : entityWorld.LocalGuildId,
                    entityWorld == null ? string.Empty : entityWorld.ResolveGuildName(entityWorld.LocalGuildId),
                    entityWorld == null ? string.Empty : entityWorld.LocalLanguage);
                localMountEntity.SetClientPlatform(entityWorld != null ? entityWorld.LocalPlatform : (byte)0);
                localMountEntity.MountPresentationChanged += OnLocalMountPresentationChanged;
                localController.BindMountPresentation(localMountEntity);
            }
            localController.TimeProvider = () => gameSession != null ? gameSession.ServerTimeMilliseconds : unchecked((uint)Environment.TickCount);
            // Attach motion sound handler for local player parity (walk/attack/skill sounds)
            if (instance.GetComponent<Metin2MotionSoundHandler>() == null) instance.AddComponent<Metin2MotionSoundHandler>();
            Camera camera = clientCameraBinding?.Camera ?? Camera.main; Metin2FollowCamera follow = camera == null ? null : camera.GetComponent<Metin2FollowCamera>() ?? camera.gameObject.AddComponent<Metin2FollowCamera>();
            if (follow != null) Metin2ClientSettings.Current.ApplyCameraMode(true);
            Vector3 spawn = worldRuntime.LoadAt(character.X, character.Y); RebindMapWindowAssets(); spawn.y = worldRuntime.GroundHeight(spawn); localController.Initialize(spawn, camera == null ? null : camera.transform); localController.BindWorld(worldRuntime); localController.BindEntityWorld(entityWorld); localController.BindCombat(combatController); localController.LocalVid = entityWorld != null ? entityWorld.LocalVid : 0u; if (follow != null) { follow.BindWorld(worldRuntime); follow.Bind(instance.transform); }
            if (entityWorld != null) entityWorld.LocalPlayerRoot = instance.transform;
            ApplyLocalActorSpeeds(entityWorld != null ? entityWorld.LocalMoveSpeed : (ushort)100,
                                  entityWorld != null ? entityWorld.LocalAttackSpeed : (ushort)100);
            ApplyLocalMount(entityWorld != null ? entityWorld.LocalMountVnum : 0u);
            // PlayerController.Initialize and mount restoration are later lifecycle edges than the
            // first inventory replay. Reconcile the already-equipped weapon's motion mode once the
            // final local actor presentation exists.
            equipmentBinder.Refresh();
            if (statusEffectsState != null && localController.LocalVid != 0)
                localController.SetWalkMode(statusEffectsState.GetWalkMode(localController.LocalVid) != 0);
            entityWorld.FlightRequested-=SendFlight;entityWorld.FlightRequested+=SendFlight;
            entityWorld.AttachFlight(instance,localController.LocalVid,localController);
            localController.MoveRequested -= SendMove; localController.MoveRequested += SendMove;
            combatController.TargetRequested -= SendTarget; combatController.TargetRequested += SendTarget; combatController.AttackRequested -= SendAttack; combatController.AttackRequested += SendAttack; combatController.SkillRequested -= SendSkill; combatController.SkillRequested += SendSkill;
            combatController.FishingRequested -= SendFishing; combatController.FishingRequested += SendFishing;
            combatController.PresentationRequested -= SendCombatPresentation; combatController.PresentationRequested += SendCombatPresentation;
            combatController.ShootRequested -= SendShoot; combatController.ShootRequested += SendShoot;
            // The refusal never reaches the server - it is the client refusing to start the cast -
            // so it only has to reach the player, above their own head and in the log.
            combatController.SkillRefused -= ShowSkillRefusal; combatController.SkillRefused += ShowSkillRefusal;
            combatController.BindSkillTable(Resources.Load<Metin2SkillTable>("Metin2SkillTable"));
            // The bar lives in the game UI, which builds lazily; bind whichever instance exists.
            foreach (Metin2QuickSlotBar bar in FindObjectsOfType<Metin2QuickSlotBar>(true))
                BindQuickSlotCombat(bar);
            combatController.FlyTargetingRequested -= SendFlyTargeting; combatController.FlyTargetingRequested += SendFlyTargeting;
            // The victim's flinch and battle hit effect belong to the local collision, not to the
            // server's reply; only the damage number waits for GC_DAMAGE.
            combatController.LocalHitLanded -= PlayLocalHit; combatController.LocalHitLanded += PlayLocalHit;
            // __NEW_Pick skips the main instance when resolving the cursor.
            combatController.LocalVid = entityWorld == null ? 0u : entityWorld.LocalVid;
            combatController.TargetRequested -= ShowTargetIndicator; combatController.TargetRequested += ShowTargetIndicator;
            targetBoard = GetComponent<Metin2TargetBoardPanel>() ?? gameObject.AddComponent<Metin2TargetBoardPanel>();
            targetBoard.Bind(entityWorld, GetComponent<Metin2PartyPanel>(), MessengerPanelOrNull(),
                             GuildPanelOrNull(), pvpState, localeText, instance.transform, localEmpire);
            // Bind runs during the enter-world setup, long before the first GC_GUILD packet has had a
            // chance to create the guild panel, so the board looks it up again on every refresh.
            targetBoard.GuildProvider = GuildPanelOrNull;
            targetBoard.MessengerProvider = MessengerPanelOrNull;
            targetBoard.PartyProvider = () => partyPanel;
            targetBoard.ObserverModeProvider = () => combatController != null && combatController.ObserverMode;
            targetBoard.SetUiHost(ResolveUiHost());
            targetBoard.ButtonPressed -= OnTargetBoardButton; targetBoard.ButtonPressed += OnTargetBoardButton;
            targetBoard.CloseRequested -= ClearTargetFromBoard; targetBoard.CloseRequested += ClearTargetFromBoard;
            targetBoard.InfoRequested -= SendTargetInfoLoad; targetBoard.InfoRequested += SendTargetInfoLoad;
            targetBoard.ItemIconProvider = ResolveItemIcon;
            targetBoard.PlayerState=playerState;
            targetBoard.UiCatalog=uiCatalog;
            targetBoard.Items=itemCatalog;
            // Drop slot tooltips attach to the slot's left side through the shared controller.
            targetBoard.ToolTips = EnsureToolTips();
            // A normal target change only sends CG_TARGET. The PC board belongs specifically to
            // the camera-button click-without-drag OpenCharacterMenu path.
            combatController.CharacterMenuRequested -= OpenTargetBoard; combatController.CharacterMenuRequested += OpenTargetBoard;
            combatController.NpcClickRequested -= SendNpcClick; combatController.NpcClickRequested += SendNpcClick;
            combatController.GroundItemPickupRequested -= SendItemPickup; combatController.GroundItemPickupRequested += SendItemPickup;
            // There is no separate four-button skill bar in the original client.  Quick slots are
            // created only by uitaskbar.py; the former programmatic 1/2/3/4 fallback visibly flashed
            // between character select and BuildOriginalGameUi, so it must not be instantiated.
            // Original input model: left = smart, right and middle = camera, wheel = zoom only.
            Metin2InputController input = instance.GetComponent<Metin2InputController>() ?? instance.AddComponent<Metin2InputController>();
            input.Bind(localController, combatController, follow);
            input.BindCursor(originalCursor);
            input.BindChat(chatWindow);
            input.IsFishingMode = HasEquippedFishingRod;
            inputController = input;
            // Guild-land build flow (BULID_EnterGuildArea and the BuildGuildBuilding window).
            // game.py polls the guild area from the player update; the director does the same.
            Metin2GuildBuildDirector guildBuild = GetComponent<Metin2GuildBuildDirector>() ?? gameObject.AddComponent<Metin2GuildBuildDirector>();
            guildBuild.Configure(worldRuntime, entityWorld, GetComponent<Metin2GuildLandState>(), playerState,
                input, follow, () => gameUi, () => { Metin2GuildPanel guild = EnsureGuildPanel(); return guild != null && guild.IsMaster; },
                SendChat, PopupMessageLocale);
            damageTextTail?.SetLocal(entityWorld == null ? 0u : entityWorld.LocalVid, instance.transform);
            input.QuickSlotRequested -= UseQuickSlot; input.QuickSlotRequested += UseQuickSlot;
            input.QuickPageRequested -= SelectQuickPage; input.QuickPageRequested += SelectQuickPage;
            input.EmoticonRequested -= TriggerLocalEmoticon; input.EmoticonRequested += TriggerLocalEmoticon;
            // game.py binds C, V, N, I, M and L to the same windows the task bar buttons open.
            input.WindowToggleRequested -= ToggleGameWindow; input.WindowToggleRequested += ToggleGameWindow;
            input.ItemShopRequested -= RequestOpenItemShop; input.ItemShopRequested += RequestOpenItemShop;
            input.MountToggleRequested -= ToggleMount; input.MountToggleRequested += ToggleMount;
            input.QuestButtonsToggleRequested -= ToggleQuestButtons; input.QuestButtonsToggleRequested += ToggleQuestButtons;
            input.SystemDialogRequested -= HandleEscape; input.SystemDialogRequested += HandleEscape;
            ConnectGameUiSessionEvents();
            input.PickUpRequested -= PickUpClosestItem; input.PickUpRequested += PickUpClosestItem;
            input.FishingRequested -= SendFishing; input.FishingRequested += SendFishing;
            mobileUiRoot?.Bind(input);
            groundItemWorld?.BindPlayer(instance.transform);
            LogWorldDiagnostics(instance);
        }
        /// <summary>
        /// Builds the original task bar and its windows once the player is in game. When the UI
        /// catalog has not been generated yet the programmatic panels stay visible instead.
        /// </summary>
        private void BuildOriginalGameUi()
        {
            if (uiCatalog == null) return;
            Canvas host = ResolveUiCanvas();
            if (host == null) return;
            Transform uiHost = ResolveUiHost() ?? host.transform;
            if (gameUi == null) gameUi = GetComponent<Metin2GameUi>() ?? gameObject.AddComponent<Metin2GameUi>();
            gameUi.Initialize(uiCatalog, playerState, uiHost, itemCatalog, skillCatalog, emotionCatalog);
            for(int part=1;part<=4;part++)gameUi.SetCostumeHidden(part,hiddenCostumeParts[part]);
            // EnsureRuntime/EnsurePlayerController normally runs before this lazy UI is created, so
            // its old `if (gameUi != null)` subscription silently did nothing. The task-bar could
            // consequently open systemdialog without calling BindSystemDialog, leaving all eight
            // options inert until Escape happened to bind the same instance later.
            ConnectGameUiSessionEvents();
            // gameUi only exists from this point on - the map window wiring subscribed during
            // EnsureRuntime never fired because gameUi was still null there (empty minimap bug).
            gameUi.WindowCreated -= WireMapWindow; gameUi.WindowCreated += WireMapWindow;
            gameUi.WindowCreated -= ConnectOriginalWindow; gameUi.WindowCreated += ConnectOriginalWindow;
            gameUi.OfflineShopRequested -= OpenOfflineShopFromInventory;
            gameUi.OfflineShopRequested += OpenOfflineShopFromInventory;
            gameUi.InventoryFeatureRequested -= HandleInventoryFeature;
            gameUi.InventoryFeatureRequested += HandleInventoryFeature;
            if (gameRuntime != null) { gameRuntime.ExchangeReceived -= HandleExchange; gameRuntime.ExchangeReceived += HandleExchange; gameRuntime.StorageReceived -= HandleStorage; gameRuntime.StorageReceived += HandleStorage; }
            // The original quest dialog is bound from here on, so the placeholder may go - the rule
            // from the placeholder cleanup still holds: remove only what has a real replacement.
            if (gameRuntime != null)
            {
                gameRuntime.QuestScriptReceived -= ShowQuestDialog;
                gameRuntime.QuestScriptReceived += ShowQuestDialog;
            }
            if (!gameUi.BuildTaskBar()) { Debug.LogWarning("METIN2_UI_TASKBAR_MISSING - run Metin2/UI/Build login and select screens"); return; }
            gameUi.TaskBarPanel?.Configure(localeText, Metin2UiSpriteCatalog.Load());
            gameUi.ConfigureLootMeasurement(mainCharacterName);
#if UNITY_STANDALONE_WIN
            gameUi.TaskBarPanel?.ConfigureWindowsAntiExp(()=>SendChat("/manage_exp"),antiExpBlocked);
#endif

            EnsureChatWindow(uiHost);

            // interfacemodule.py ShowDefaultWindows (:883-886): wndTaskBar.Show() + wndMiniMap.Show()
            // + wndMiniMap.ShowMiniMap() - the minimap is part of the default HUD. Open() fires
            // WindowCreated, whose WireMapWindow binds the panel dependencies.
            GameObject minimapWindow = gameUi.Open("minimap");
            Metin2MiniMapPanel minimapPanel = minimapWindow != null ? minimapWindow.GetComponent<Metin2MiniMapPanel>() : null;
            if (minimapPanel != null && inputController != null)
                inputController.MiniMapScaleRequested += direction => { if (direction > 0) minimapPanel.ScaleUp(); else minimapPanel.ScaleDown(); };

            Metin2QuickSlotBar quickSlots = gameUi.TaskBarPanel?.QuickSlots;
            if (quickSlots != null)
            {
                // Without this the bar's own playerState/itemCatalog/skillCatalog/emotionCatalog
                // fields stay null forever, so Refresh() always falls through to the default
                // (empty) case in the switch below - registrations arrive fine via
                // GC_QUICKSLOT_ADD, but every icon lookup is skipped and the slot renders empty.
                quickSlots.Bind(playerState, itemCatalog);
                quickSlots.BindCatalogs(skillCatalog, emotionCatalog);
                quickSlots.SetComboActive(activeComboType > 0);
                // EnsurePlayerController binds bars that already exist, but the original taskbar is
                // normally instantiated later by this lazy UI path.  Keep the binding here as well:
                // without it Metin2QuickSlotBar.Update exits on a null combatController and a real
                // cast can never reach the icon, even though the standalone renderer proof passes.
                BindQuickSlotCombat(quickSlots);

                quickSlots.ItemUseRequested -= UseInventoryItem; quickSlots.ItemUseRequested += UseInventoryItem;
                quickSlots.SkillUseRequested -= UseHotbarSkill; quickSlots.SkillUseRequested += UseHotbarSkill;
                quickSlots.EmotionRequested -= UseHotbarEmotion; quickSlots.EmotionRequested += UseHotbarEmotion;

                quickSlots.BindCursor(EnsureCursorAttachment());
                quickSlots.BindToolTips(EnsureToolTips());
                quickSlots.AddRequested -= AddQuickSlotRegistration; quickSlots.AddRequested += AddQuickSlotRegistration;
                quickSlots.MoveRequested -= MoveQuickSlotRegistration; quickSlots.MoveRequested += MoveQuickSlotRegistration;
                quickSlots.AddGlobalRequested -= AddGlobalQuickSlotRegistration; quickSlots.AddGlobalRequested += AddGlobalQuickSlotRegistration;
                quickSlots.MoveGlobalRequested -= MoveGlobalQuickSlotRegistration; quickSlots.MoveGlobalRequested += MoveGlobalQuickSlotRegistration;
                quickSlots.DeleteRequested -= DeleteQuickSlotRegistration; quickSlots.DeleteRequested += DeleteQuickSlotRegistration;

                // GC_QUICKSLOT_ADD/DEL/SWAP are handled from EnsureRuntime() onward, before this bar
                // exists, into quickSlotState. Replay that buffer now so a bar built after the server's
                // one-shot login replay still shows what was saved - and a bar rebuilt later (e.g. after
                // BuildOriginalGameUi() runs again) does not start from empty either.
                for (int global = 0; global < quickSlotState.Length; global++)
                {
                    Metin2QuickSlotBar.Registration registration = quickSlotState[global];
                    if (!registration.IsEmpty) quickSlots.RegisterGlobal(global, registration.Type, registration.Position);
                }
            }

            // Android/iOS renders the same live player, quick-slot, chat and minimap state through
            // its independent Canvas. mobileUiRoot is null on Windows, so the desktop hierarchy and
            // input path do not execute any of this presentation code.
            // The server has accepted CG_ENTERGAME and sent PHASE_GAME by this point. A mobile HUD
            // presentation fault may leave a widget unavailable, but it must never dispose that
            // live session and return the player to login.
            try
            {
                mobileUiRoot?.BindGameplay(playerState, mainCharacterName, gameUi, quickSlots, chatWindow, minimapPanel);
                mobileUiRoot?.SetAntiExpBlocked(antiExpBlocked);
            }
            catch (Exception exception)
            {
                Debug.LogError("METIN2_MOBILE_HUD_BIND_FAILED_CONTINUING " + exception);
            }

            // uitaskbar.py:1150 player.SetMouseFunc(type, func) - the fly-outs drive the input model.
            Metin2MouseButtonModePanel mouseModes = gameUi.MouseButtonModes;
            if (mouseModes != null && inputController != null)
            {
                mouseModes.FunctionChanged -= ApplyMouseFunction; mouseModes.FunctionChanged += ApplyMouseFunction;
                // The panel loads mouse.cfg during its own bind, before this subscription exists, so
                // the state it already settled on has to be pulled rather than waited for.
                ApplyMouseFunction(Metin2MouseButtonModePanel.ButtonLeft, mouseModes.CurrentFunction(Metin2MouseButtonModePanel.ButtonLeft));
                ApplyMouseFunction(Metin2MouseButtonModePanel.ButtonRight, mouseModes.CurrentFunction(Metin2MouseButtonModePanel.ButtonRight));
            }
            // The placeholder panels are not merely redundant once the original task bar drives the
            // windows - they draw on top of it. Making them transparent left them in the hierarchy and
            // still missed the skill hotbar, which is created on a different path, so they are removed
            // outright by marker instead of by a hand-maintained list.
            int removed = Metin2PlaceholderUi.RemoveAll(uiHost);
            gameUiPanels.Clear();

            // The original chat window is bound from BuildOriginalGameUi onward, so this placeholder
            // is only kept for the target and damage readouts it also carries - it is hidden, not
            // removed, and its own chat log simply never becomes visible. The quest dialog manages
            // its own visibility and is left alone.
            if (chatPanel != null)
            {
                CanvasGroup group = chatPanel.GetComponent<CanvasGroup>();
                if (group == null) group = chatPanel.gameObject.AddComponent<CanvasGroup>();
                group.alpha = 0f; group.blocksRaycasts = false; group.interactable = false;
            }
            Debug.Log("METIN2_UI_PLACEHOLDERS_REMOVED count=" + removed);

            // game.py creates AffectShower directly at (10,10); it is not a generated Windows
            // window. Bind after removing placeholders, replaying any affects received during the
            // loading burst. ResolveUiHost selects the independent mobile host on Android/iOS.
            affectShower = GetComponent<Metin2AffectShowerPanel>() ??
                           gameObject.AddComponent<Metin2AffectShowerPanel>();
            affectShower.Bind(statusEffectsState, entityWorld, localeText, uiHost);
            affectShower.SetGameplayVisible(inGame);

            // Last, not at player spawn. Both need gameUi AND gameRuntime; the spawn-time call had
            // neither yet, returned immediately every time and was never retried, which is why a
            // shopkeeper opened nothing at all. Placing them at the end of this method means every
            // dependency above has already been assigned.
            EnsureShopPanel();
            EnsureRefinePanel();
            EnsureOfflineShopPanel();
            EnsureItemShopPanel(uiHost);
            EnsureAuxiliaryInventoryPanels(uiHost);
        }

        private Metin2AuxiliaryInventoryPanel mountInventoryPanel, extraInventoryPanel, beltInventoryPanel;

        /// <summary>
        /// The mount bag (window 5) and the extra inventory (window 8) use the imported original
        /// windows (uimountinventory.py / uiextrainventory.py staged as Metin2Ui_mountinventory and
        /// Metin2Ui_extrainventory). The runtime grid stays as the fallback for harnesses that
        /// cannot open a prefab, and both paths read the shared player state.
        /// </summary>
        private void EnsureAuxiliaryInventoryPanels(Transform host)
        {
            if (host == null || playerState == null) return;
            Metin2CursorAttachment attachment = EnsureCursorAttachment();
            Metin2ToolTipController tips = EnsureToolTips();
            if (mountInventoryPanel == null)
            {
                mountInventoryPanel = BindAuxiliaryWindow("mountinventory", host, Metin2AuxiliaryInventoryPanel.Kind.Mount,
                    attachment, tips);
            }
            if (extraInventoryPanel == null)
            {
                extraInventoryPanel = BindAuxiliaryWindow("extrainventory", host, Metin2AuxiliaryInventoryPanel.Kind.Extra,
                    attachment, tips);
            }
            // uiinventory.py:1000 self.wndBelt = BeltInventoryWindow(self) - the belt window is
            // created with the inventory and toggled by ToggleBeltInventory (:615).
            if (beltInventoryPanel == null)
            {
                beltInventoryPanel = BindAuxiliaryWindow("beltinventorywindow", host, Metin2AuxiliaryInventoryPanel.Kind.Belt,
                    attachment, tips);
            }
        }

        private Metin2AuxiliaryInventoryPanel BindAuxiliaryWindow(string screen, Transform host,
            Metin2AuxiliaryInventoryPanel.Kind kind, Metin2CursorAttachment attachment, Metin2ToolTipController tips)
        {
            GameObject window = gameUi != null ? gameUi.Open(screen) : null;
            if (window == null)
            {
                window = new GameObject("AuxiliaryInventory_" + screen, typeof(RectTransform));
                window.transform.SetParent(host, false);
            }
            Metin2AuxiliaryInventoryPanel panel = window.GetComponent<Metin2AuxiliaryInventoryPanel>() ??
                window.AddComponent<Metin2AuxiliaryInventoryPanel>();
            panel.Configure(playerState, itemCatalog, kind);
            panel.BindToolTips(tips);
            panel.BindCursor(attachment);
            // Touch presentation: the same item action strip the normal inventory builds
            // (equip/use/split), never a full-screen layer.
            if (mobileUiRoot != null) panel.ConfigureMobilePresentation();
            panel.SlotDropRequested -= OnAuxiliaryInventoryDrop;
            panel.SlotDropRequested += OnAuxiliaryInventoryDrop;
            panel.UseRequested -= OnAuxiliaryInventoryUse;
            panel.UseRequested += OnAuxiliaryInventoryUse;
            panel.SplitRequested -= OnAuxiliaryInventorySplit;
            panel.SplitRequested += OnAuxiliaryInventorySplit;
            panel.WorldReleaseRequested -= HandleAuxiliaryWorldRelease;
            panel.WorldReleaseRequested += HandleAuxiliaryWorldRelease;
            panel.ChatRequested -= SendChat;
            panel.ChatRequested += SendChat;
            panel.ItemLinkRequested -= InsertInventoryChatLink;
            panel.ItemLinkRequested += InsertInventoryChatLink;
            panel.WikiItemRequested -= OpenWikiItem;
            panel.WikiItemRequested += OpenWikiItem;
            // Each original auxiliary window closes only itself. A shared close callback used to
            // close all three and, on fallback windows, could leave the clicked window visible.
            panel.CloseRequested -= CloseAuxiliaryInventory;
            if (kind == Metin2AuxiliaryInventoryPanel.Kind.Mount)
            { panel.CloseRequested -= CloseMountInventory; panel.CloseRequested += CloseMountInventory; }
            else if (kind == Metin2AuxiliaryInventoryPanel.Kind.Belt)
            { panel.CloseRequested -= CloseBeltInventory; panel.CloseRequested += CloseBeltInventory; }
            else
            { panel.CloseRequested -= CloseExtraInventory; panel.CloseRequested += CloseExtraInventory; }
            if (gameUi != null) gameUi.Close(screen);
            else window.SetActive(false);
            return panel;
        }

        private void OpenWikiItem(uint vnum)
        {
            GameObject window = gameUi?.Open("wikiwindow");
            Metin2WikiPanel wiki = window != null ? window.GetComponent<Metin2WikiPanel>() : null;
            if (wiki == null) return;
            wiki.Bind(itemCatalog);
            wiki.BindToolTips(EnsureToolTips());
            wiki.WikiInfoRequested -= OnWikiInfoRequested;
            wiki.WikiInfoRequested += OnWikiInfoRequested;
            wiki.ShowItem(vnum);
        }

        private async void OnWikiInfoRequested(ulong returnId, uint vnum, bool isMob)
        {
            try { if (gameSession != null) await gameSession.SendWikiRequestAsync(returnId, vnum, isMob, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void OnWikiResponse(Metin2GamePacketCodec.WikiResponse response)
        {
            gameUi?.GetPanel<Metin2WikiPanel>("wikiwindow")?.ApplyWiki(response);
        }

        private void CloseAuxiliaryInventory()
        {
            if (mountInventoryPanel != null && mountInventoryPanel.IsOpen) gameUi?.Close("mountinventory");
            if (extraInventoryPanel != null && extraInventoryPanel.IsOpen) gameUi?.Close("extrainventory");
            if (beltInventoryPanel != null && beltInventoryPanel.IsOpen) gameUi?.Close("beltinventorywindow");
        }

        private void CloseMountInventory()
        {
            if (gameUi != null) gameUi.Close("mountinventory");
            else if (mountInventoryPanel != null) mountInventoryPanel.gameObject.SetActive(false);
        }

        private void CloseExtraInventory()
        {
            if (gameUi != null) gameUi.Close("extrainventory");
            else if (extraInventoryPanel != null) extraInventoryPanel.gameObject.SetActive(false);
        }

        private void CloseBeltInventory()
        {
            if (gameUi != null) gameUi.Close("beltinventorywindow");
            else if (beltInventoryPanel != null) beltInventoryPanel.gameObject.SetActive(false);
        }

        public void OpenMountInventory()
        {
            if (mountInventoryPanel == null) EnsureAuxiliaryInventoryPanels(ResolveUiHost());
            if (mountInventoryPanel == null) return;
            if (gameUi != null) gameUi.Open("mountinventory");
            else mountInventoryPanel.gameObject.SetActive(true);
            mountInventoryPanel.Refresh();
        }

        /// <summary>uiinventory.py ToggleMountInventory: the second press closes the complete
        /// window, and a later press opens and refreshes the same instance.</summary>
        public void ToggleMountInventory()
        {
            if (mountInventoryPanel == null) EnsureAuxiliaryInventoryPanels(ResolveUiHost());
            if (mountInventoryPanel == null) return;
            if (mountInventoryPanel.IsOpen) CloseMountInventory();
            else OpenMountInventory();
        }

        public void OpenExtraInventory()
        {
            if (extraInventoryPanel == null) EnsureAuxiliaryInventoryPanels(ResolveUiHost());
            if (extraInventoryPanel == null) return;
            if (gameUi != null) gameUi.Open("extrainventory");
            else extraInventoryPanel.gameObject.SetActive(true);
            extraInventoryPanel.Refresh();
        }

        /// <summary>interfacemodule.py:2897 ToggleExtraInventoryWindow. The U key, the sidebar
        /// ExtendIntentory and the mobile hub share this single action.</summary>
        public void ToggleExtraInventory()
        {
            if (extraInventoryPanel == null) EnsureAuxiliaryInventoryPanels(ResolveUiHost());
            if (extraInventoryPanel == null) return;
            if (extraInventoryPanel.IsOpen) CloseExtraInventory();
            else OpenExtraInventory();
        }

        /// <summary>dailygift.py:19-22 Open: show the window and request the state.</summary>
        public void OpenDailyGift()
        {
            if (gameUi == null) return;
            bool open = gameUi.IsOpen("dailygiftnew");
            if (open) { gameUi.Close("dailygiftnew"); return; }
            gameUi.Open("dailygiftnew");
            SendChat("/daily_reward_reload");
        }

        /// <summary>dailygift.py:173-178 GetReward: claim then reload the state.</summary>
        private void ClaimDailyReward()
        {
            SendChat("/daily_reward_get_reward");
            SendChat("/daily_reward_reload");
        }

        /// <summary>game.py:1777: the auto shout goes out as CHAT_TYPE_SHOUT.</summary>
        private async void SendAutoShout(string text)
        {
            try { if (gameSession != null) await gameSession.SendChatAsync(Metin2ChatWindow.ChatTypeShout, text, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2AutoShoutRunner autoShoutRunner;

        private Metin2AutoShoutRunner EnsureAutoShoutRunner()
        {
            if (autoShoutRunner == null) autoShoutRunner = GetComponent<Metin2AutoShoutRunner>();
            if (autoShoutRunner == null) autoShoutRunner = gameObject.AddComponent<Metin2AutoShoutRunner>();
            autoShoutRunner.ShoutRequested -= SendAutoShout;
            autoShoutRunner.ShoutRequested += SendAutoShout;
            return autoShoutRunner;
        }

        /// <summary>uiinventory.py:615 ToggleBeltInventory - the sidebar Belt icon and the mobile
        /// BELT_INVENTORY entry share this route.</summary>
        public void ToggleBeltInventory()
        {
            if (beltInventoryPanel == null) EnsureAuxiliaryInventoryPanels(ResolveUiHost());
            if (beltInventoryPanel == null) return;
            bool open = beltInventoryPanel.IsOpen;
            if (gameUi != null)
            {
                if (open) gameUi.Close("beltinventorywindow"); else gameUi.Open("beltinventorywindow");
            }
            else beltInventoryPanel.gameObject.SetActive(!open);
            if (!open) beltInventoryPanel.Refresh();
        }

        /// <summary>
        /// uimountinventory.py:357 __MoveAttachedItemToMountSlot / uiextrainventory drop: the
        /// carried item's source window decides which writer is used.
        /// </summary>
        private async void OnAuxiliaryInventoryDrop(byte window, ushort cell)
        {
            Metin2CursorAttachment attachment = EnsureCursorAttachment();
            if (attachment == null || !attachment.IsAttached) return;
            if (!CanActMainInstance() || gameSession == null) return;
            int source = attachment.SlotNumber;
            Metin2QuickSlotBar.SlotType type = attachment.Type;
            int count = Metin2AuxiliaryInventoryPanel.MoveCountFor(attachment);
            // uiextrainventory.py:690-697 SelectEmptySlot: safebox and mall items check out into
            // the extra window instead of a plain inventory move.
            if (attachment.Owner is Metin2StoragePanel storage)
            {
                byte target = window == 7 ? (byte)1 : window;
                attachment.Deattach();
                try
                {
                    if (storage.Window == 3) await gameSession.SendSafeboxCheckoutAsync((uint)source, target, cell, lifetime.Token);
                    else await gameSession.SendMallCheckoutAsync((uint)source, target, cell, lifetime.Token);
                }
                catch (Exception exception) { ShowOperationFailure(exception); }
                return;
            }
            attachment.Deattach();
            try
            {
                if (window == 7)
                {
                    // BeltInventoryWindow delegates to InventoryWindow.SelectEmptySlot. Its cells
                    // are absolute INVENTORY cells 500..659; neither side of this move uses the
                    // BELT_INVENTORY enum on the wire (server CHARACTER::GetItem has no case 7).
                    // Mount storage is the exception: it has dedicated checkout packets even when
                    // its destination is an ordinary inventory cell in the belt range.
                    if (type == Metin2QuickSlotBar.SlotType.MountInventory)
                    {
                        await gameSession.SendMountInventoryCheckoutAsync((ushort)source, 1, cell, lifetime.Token);
                        return;
                    }
                    byte fromWindow = type == Metin2QuickSlotBar.SlotType.ExtraInventory ? (byte)8
                        : type == Metin2QuickSlotBar.SlotType.DragonSoulInventory ? (byte)6
                        : (byte)1;
                    await gameSession.SendItemMoveAsync(fromWindow, (ushort)source, 1, cell, count, lifetime.Token);
                }
                else if (window == 5)
                {
                    if (type == Metin2QuickSlotBar.SlotType.MountInventory)
                        await gameSession.SendMountInventoryItemMoveAsync((ushort)source, cell, lifetime.Token);
                    else if (type == Metin2QuickSlotBar.SlotType.ExtraInventory)
                        await gameSession.SendMountInventoryCheckinAsync(8, (ushort)source, cell, lifetime.Token);
                    else if (type == Metin2QuickSlotBar.SlotType.BeltInventory)
                        await gameSession.SendMountInventoryCheckinAsync(1, (ushort)source, cell, lifetime.Token);
                    else
                        await gameSession.SendMountInventoryCheckinAsync(1, (ushort)source, cell, lifetime.Token);
                }
                else
                {
                    // uiextrainventory.py:716-725: normal -> extra and extra -> extra both send the
                    // carried amount, which is what keeps a split or a full stack intact.
                    if (type == Metin2QuickSlotBar.SlotType.MountInventory)
                        await gameSession.SendMountInventoryCheckoutAsync((ushort)source, 8, cell, lifetime.Token);
                    else if (type == Metin2QuickSlotBar.SlotType.ExtraInventory)
                        await gameSession.SendItemMoveAsync(8, (ushort)source, 8, cell, count, lifetime.Token);
                    else if (type == Metin2QuickSlotBar.SlotType.BeltInventory)
                        await gameSession.SendItemMoveAsync(1, (ushort)source, 8, cell, count, lifetime.Token);
                    else
                        await gameSession.SendItemMoveAsync(1, (ushort)source, 8, cell, count, lifetime.Token);
                }
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void OnAuxiliaryInventoryUse(byte window, ushort cell, int count)
        {
            if (!CanActMainInstance() || gameSession == null) return;
            // Belt panel uses the synthetic window value 7 only to identify its UI route. On the
            // Auriga wire belt cells are ordinary inventory positions (1, 500..659).
            byte wireWindow = window == 7 ? (byte)1 : window;
            // uiinventory.py:885-887 __UseItem: ITEM_FLAG_CONFIRM_WHEN_USE asks once; Ctrl (the
            // count > 1 from the panel) still applies to every use after the answer.
            if (playerState != null && itemCatalog != null &&
                playerState.TryGetItem(wireWindow, cell, out Metin2GamePacketCodec.ItemData item) &&
                itemCatalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) &&
                (entry.itemFlag & Metin2ItemConstants.ItemFlagConfirmWhenUse) != 0)
            {
                AskQuestion2Locale("INVENTORY_REALLY_USE_ITEM", string.Empty,
                    () => SendItemUseSequence(wireWindow, cell, count));
                return;
            }
            SendItemUseSequence(wireWindow, cell, count);
        }

        /// <summary>uiextrainventory.py:974-984 / uiinventory.py:178-184: Ctrl on a stack performs
        /// one use request per item, otherwise exactly one.</summary>
        private async void SendItemUseSequence(byte window, ushort cell, int count)
        {
            if (!CanActMainInstance() || gameSession == null) return;
            try
            {
                int uses = Mathf.Clamp(count, 1, 50000);
                for (int index = 0; index < uses; index++)
                    await gameSession.SendItemUseAsync(window, cell, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>uiextrainventory.py:764-770 Shift+click: the pick-amount dialog splits the stack
        /// and the chosen amount is attached to the shared cursor.</summary>
        private void OnAuxiliaryInventorySplit(byte window, ushort cell, int maximum)
        {
            if (gameUi == null || maximum <= 1) return;
            Metin2AuxiliaryInventoryPanel panel = window == 8 ? extraInventoryPanel
                : window == 7 ? beltInventoryPanel : mountInventoryPanel;
            if (panel == null) return;
            GameObject dialogObject = gameUi.Open("pickmoneydialog");
            if (dialogObject == null) return;
            Metin2PickAmountDialog dialog = dialogObject.GetComponent<Metin2PickAmountDialog>() ??
                                             dialogObject.AddComponent<Metin2PickAmountDialog>();
            dialog.OpenLocale(maximum, "PICK_ITEM_TITLE", count => panel.AttachSplit(cell, count));
        }

        /// <summary>
        /// uidragonsoul.py / uirune.py socket drop: the carried item's source window decides the
        /// writer. The DS bag is window 6, rune and DS equipment cells are the equipment window.
        /// </summary>
        private async void OnSocketWindowDrop(byte window, ushort cell)
        {
            Metin2CursorAttachment attachment = EnsureCursorAttachment();
            if (attachment == null || !attachment.IsAttached) return;
            if (!CanActMainInstance() || gameSession == null) return;
            int source = attachment.SlotNumber;
            Metin2QuickSlotBar.SlotType type = attachment.Type;
            int count = Metin2AuxiliaryInventoryPanel.MoveCountFor(attachment);
            attachment.Deattach();
            try
            {
                byte fromWindow = type == Metin2QuickSlotBar.SlotType.Switchbot ? (byte)9
                    : type == Metin2QuickSlotBar.SlotType.DragonSoulInventory ? (byte)6
                    : type == Metin2QuickSlotBar.SlotType.BeltInventory ? (byte)1
                    : type == Metin2QuickSlotBar.SlotType.MountInventory ? (byte)5
                    : type == Metin2QuickSlotBar.SlotType.ExtraInventory ? (byte)8
                    : (byte)1;
                await gameSession.SendItemMoveAsync(fromWindow, (ushort)source, window, cell, count, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>Dropping a mount / extra inventory item on a normal bag cell (uiinventory.py).</summary>
        private async void OnInventoryExternalDrop(Metin2CursorAttachment attachment, ushort cell)
        {
            if (attachment == null || !CanActMainInstance() || gameSession == null) return;
            int source = attachment.SlotNumber;
            Metin2QuickSlotBar.SlotType type = attachment.Type;
            int count = Metin2AuxiliaryInventoryPanel.MoveCountFor(attachment);
            // uiinventory.py:2122-2136: safebox and mall items check out into the bag cell.
            if (attachment.Owner is Metin2StoragePanel storage)
            {
                attachment.Deattach();
                try
                {
                    if (storage.Window == 3) await gameSession.SendSafeboxCheckoutAsync((uint)source, 1, cell, lifetime.Token);
                    else await gameSession.SendMallCheckoutAsync((uint)source, 1, cell, lifetime.Token);
                }
                catch (Exception exception) { ShowOperationFailure(exception); }
                return;
            }
            // uiinventory.py:2145-2173 SelectItemSlot: an item from the extra inventory (window 8)
            // or the belt (window 1, cells 500..659) dropped on an occupied bag cell is USED on it
            // when it is a refine scroll, a metin stone or one of the USE_TYPE_TUPLE /
            // ITEM_FLAG_APPLICABLE items (bonus adders); otherwise it is a move.
            byte carriedWindow = type == Metin2QuickSlotBar.SlotType.ExtraInventory ? (byte)8
                : type == Metin2QuickSlotBar.SlotType.BeltInventory ? (byte)1
                : type == Metin2QuickSlotBar.SlotType.Switchbot ? (byte)9
                : (byte)0;
            Metin2GamePacketCodec.ItemData targetItem = default;
            bool targetOccupied = playerState != null && playerState.TryGetItem(1, cell, out targetItem) && targetItem.Vnum != 0;
            if (targetOccupied && carriedWindow != 0 && itemCatalog != null &&
                playerState.TryGetItem(carriedWindow, (ushort)source, out Metin2GamePacketCodec.ItemData carried) &&
                itemCatalog.TryGet(carried.Vnum, out Metin2ItemCatalog.Entry carriedDefinition))
            {
                if (itemCatalog.IsRefineScroll(carried.Vnum))
                {
                    attachment.Deattach();
                    try { await gameSession.SendItemUseToItemAsync(carriedWindow, (ushort)source, 1, cell, lifetime.Token); }
                    catch (Exception exception) { ShowOperationFailure(exception); }
                    return;
                }
                if (carriedDefinition.itemType == Metin2ItemCatalog.ItemTypeMetin)
                {
                    attachment.Deattach();
                    OpenAttachMetinDialog(carriedWindow, (ushort)source, 1, cell);
                    return;
                }
                if (carried.Vnum != targetItem.Vnum && Metin2ItemCatalog.IsUseToItemSource(carriedDefinition))
                {
                    attachment.Deattach();
                    // The removal item asks which bonus to drop first; the use packet follows.
                    if (carriedDefinition.subType == Metin2ItemConstants.UseRemoveAttrCostume)
                    {
                        OpenCostumeAttrRemoveDialog(carriedWindow, (ushort)source, 1, cell);
                        return;
                    }
                    try { await gameSession.SendItemUseToItemAsync(carriedWindow, (ushort)source, 1, cell, lifetime.Token); }
                    catch (Exception exception) { ShowOperationFailure(exception); }
                    return;
                }
            }
            attachment.Deattach();
            try
            {
                // uiinventory.py __DropSrcItemToDestItemInInventory sends 0 for an occupied
                // destination (server merge/swap semantics) and SelectEmptySlot sends the carried
                // amount for an empty one.
                int moveCount = targetOccupied ? 0 : count;
                if (type == Metin2QuickSlotBar.SlotType.MountInventory)
                    await gameSession.SendMountInventoryCheckoutAsync((ushort)source, 1, cell, lifetime.Token);
                else if (type == Metin2QuickSlotBar.SlotType.ExtraInventory)
                    await gameSession.SendItemMoveAsync(8, (ushort)source, 1, cell, moveCount, lifetime.Token);
                else if (type == Metin2QuickSlotBar.SlotType.BeltInventory)
                    await gameSession.SendItemMoveAsync(1, (ushort)source, 1, cell, moveCount, lifetime.Token);
                else if (type == Metin2QuickSlotBar.SlotType.DragonSoulInventory)
                    await gameSession.SendItemMoveAsync(6, (ushort)source, 1, cell, moveCount, lifetime.Token);
                else if (type == Metin2QuickSlotBar.SlotType.Switchbot)
                    await gameSession.SendItemMoveAsync(9, (ushort)source, 1, cell, moveCount, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// Connects a taskbar created either before or after the player/combat runtime.  Keeping the
        /// two lifecycle edges on one helper prevents a rebuilt original UI from silently losing its
        /// live cooldown source.
        /// </summary>
        private void BindQuickSlotCombat(Metin2QuickSlotBar quickSlots)
        {
            if (quickSlots != null) quickSlots.BindCombat(combatController);
        }

        private float lastOfflineShopRequestAt = -10f;

        /// <summary>uiinventory.py ClickOPSButton. The inventory button raises both the dedicated
        /// OfflineShopRequested event and the OFFLINE_SHOP registry token, so the two paths are
        /// collapsed into a single server request.</summary>
        private void OpenOfflineShopFromInventory()
        {
            if (Time.unscaledTime - lastOfflineShopRequestAt < .5f) return;
            lastOfflineShopRequestAt = Time.unscaledTime;
            offlineShopPanel?.RequestManagerOpen();
            SendChat("/open_offlineshop");
        }

        private void SelectQuickPage(int page) => gameUi?.TaskBarPanel?.SetQuickPage(page);

        private void TriggerLocalEmoticon(int index)
        {
            if (localController == null) return;
            string marker = Metin2EmoticonEffects.Marker(index);
            if (string.IsNullOrEmpty(marker)) return;
            Emoticons.Play(localController.transform, index);
            SendChat(marker);
        }

        private void EnsureItemShopPanel(Transform host)
        {
            if (itemShopPanel != null || host == null || itemShopState == null) return;
            itemShopPanel = Metin2ItemShopPanel.Create(host, itemShopState, itemCatalog, localeText);
            itemShopPanel.BindToolTips(EnsureToolTips());
            itemShopPanel.BuyRequested -= BuyItemShop; itemShopPanel.BuyRequested += BuyItemShop;
            itemShopPanel.LimitedCountRequested -= RequestItemShopCount; itemShopPanel.LimitedCountRequested += RequestItemShopCount;
            itemShopPanel.PurchaseCoinsRequested -= OpenItemShopCoins; itemShopPanel.PurchaseCoinsRequested += OpenItemShopCoins;
            itemShopPanel.PurchasesRequested -= RequestItemShopLog; itemShopPanel.PurchasesRequested += RequestItemShopLog;
            // StartReceiving runs before this panel is built, so a GC 142 sub3 OPEN that arrived in
            // between set state.IsOpen without any listener on state.Changed: replay it or the
            // window stays closed until the next open request.
            if (itemShopState.IsOpen) itemShopPanel.Apply(new Metin2GamePacketCodec.ItemShopMessage(3,null,null,false,0,0,0,0));
        }

        private static void OpenItemShopCoins() => Application.OpenURL("https://metin2pegasus.eu/shop");

        private async void RequestOpenItemShop()
        {
            try
            {
                // uiitemshop.py:748-757 Open(): the cached window is shown immediately and only
                // the first open requests data. Depending on the server reply alone meant a
                // throttled /ishop data left the shop unopenable after the first close.
                if (itemShopState != null && itemShopState.AurigaItems.Count > 0)
                    itemShopPanel?.OpenFromCache();
                if (gameSession == null) return;
                // uiitemshop.py:754 caches the server update time, so the first open uses -1 and a
                // later open can be answered with the 9999 "no data" marker.
                await gameSession.SendItemShopOpenAsync(itemShopState != null ? itemShopState.UpdateTime : -1, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void RequestItemShopLog()
        {
            try { if (gameSession != null) await gameSession.SendItemShopLogAsync(lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private GameObject restartDialogWindow;

        /// <summary>
        /// uirestart.RestartDialog (uirestart.py:9-65) on the generated original layout: the window
        /// sits at the top-left (uiscript restartdialog.py x:50 y:50) with RESTART_HERE and
        /// RESTART_TOWN buttons that send the matching chat commands. Escape cannot close it
        /// (OnPressEscapeKey returns True).
        /// </summary>
        private void OnLocalPlayerDied()
        {
            targetBoard?.Close();
            if (restartDialogWindow != null && restartDialogWindow.activeSelf) return;
            if (gameUi == null) return;
            GameObject window = gameUi.Open("restartdialog");
            if (window == null) { Debug.LogWarning("METIN2_RESTART_DIALOG window=missing"); return; }
            restartDialogWindow = window;
            Button here = FindNamed<Button>(window.transform, "restart_here_button");
            Button town = FindNamed<Button>(window.transform, "restart_town_button");
            if (here != null)
            {
                here.onClick.RemoveAllListeners();
                here.onClick.AddListener(() => SendChat("/restart_here"));
                Text hereText = here.GetComponentInChildren<Text>(true);
                if (hereText != null) hereText.text = Locale("RESTART_HERE");
            }
            if (town != null)
            {
                town.onClick.RemoveAllListeners();
                town.onClick.AddListener(() => SendChat("/restart_town"));
                Text townText = town.GetComponentInChildren<Text>(true);
                if (townText != null) townText.text = Locale("RESTART_TOWN");
            }
            window.transform.SetAsLastSibling();
            Debug.Log("METIN2_RESTART_DIALOG opened");
        }

        private void CloseRestartDialog()
        {
            if (restartDialogWindow == null) return;
            gameUi?.Close("restartdialog");
            restartDialogWindow = null;
        }
        private async void BuyItemShop(uint id, ushort count)
        {
            try { if (gameSession != null) await gameSession.SendItemShopBuyAsync(id, count, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void RequestItemShopCount(ushort id)
        {
            try { if (gameSession != null) await gameSession.SendItemShopRequestCountAsync(id, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2OfflineShopPanel offlineShopPanel,offlineShopCustomerPanel;
        private Metin2OfflineShopCreatePanel offlineShopCreatePanel;
        private Metin2OfflineShopBuilderPanel offlineShopBuilderPanel;
        private Metin2OfflineShopWorldSigns offlineShopWorldSigns;
        private Metin2OfflineShopDraft offlineShopDraft;
        private Metin2QuestionDialogPanel offlineShopQuestion;
        private GameObject offlineShopInputWindow;
        private byte offlineModalPosition=byte.MaxValue;
        private Metin2AurigaOfflineShopState aurigaOfflineShopState;
        private Metin2AurigaOfflineShopBoards aurigaOfflineShopBoards;
        private uint offlineShopBuilderKasmirNpc;
        // The imported create window has no day/hour input, so the shop is opened for a week.
        // The original dialog computes dwDuration = days*24*60 + hours*60.
        private const uint AurigaOfflineShopDurationMinutes=7*24*60;
        private void EnsureOfflineShopPanel()
        {
            if (offlineShopPanel != null || gameUi == null || gameRuntime == null) return;
            if (offlineShopWorldSigns == null)
            {
                offlineShopWorldSigns=GetComponent<Metin2OfflineShopWorldSigns>()??gameObject.AddComponent<Metin2OfflineShopWorldSigns>();
                offlineShopWorldSigns.Bind(gameRuntime,entityWorld);
            }
            // Auriga GC_NEW_OFFLINESHOP (214) state: the single packet stream feeds this state,
            // which in turn drives the imported windows and resolves position -> item id for the
            // CG requests. The legacy 47-53 subscription below stays inert on the Auriga server.
            if(aurigaOfflineShopState==null)aurigaOfflineShopState=new Metin2AurigaOfflineShopState();
            aurigaOfflineShopState.Bind(gameRuntime);
            aurigaOfflineShopState.ShopOpened-=OnAurigaShopOpened;aurigaOfflineShopState.ShopOpened+=OnAurigaShopOpened;
            aurigaOfflineShopState.OwnerShopOpened-=OnAurigaOwnerShopOpened;aurigaOfflineShopState.OwnerShopOpened+=OnAurigaOwnerShopOpened;
            aurigaOfflineShopState.OwnerShopMissing-=OnAurigaOwnerShopMissing;aurigaOfflineShopState.OwnerShopMissing+=OnAurigaOwnerShopMissing;
            aurigaOfflineShopState.ShopClosed-=OnAurigaShopClosed;aurigaOfflineShopState.ShopClosed+=OnAurigaShopClosed;
            aurigaOfflineShopState.SafeboxChanged-=OnAurigaSafeboxChanged;aurigaOfflineShopState.SafeboxChanged+=OnAurigaSafeboxChanged;
            EnsureAurigaOfflineShopBoards();
            Metin2CursorAttachment attachment=EnsureCursorAttachment();Metin2ToolTipController tips=EnsureToolTips();
            GameObject window = gameUi.Open("offlineshopmanagerdialog");
            if (window == null) { Debug.LogWarning("METIN2_OFFLINE_SHOP_MANAGER window=missing"); return; }
            offlineShopPanel = window.GetComponent<Metin2OfflineShopPanel>() ?? window.AddComponent<Metin2OfflineShopPanel>();
            offlineShopPanel.BindOriginalWidgets(window.transform, true);
            offlineShopPanel.Configure(attachment,tips,playerState,localeText,offlineShopWorldSigns,entityWorld,entityWorld!=null?entityWorld.LocalPlayerRoot:null);
            offlineShopPanel.Bind(gameRuntime, itemCatalog);
            offlineShopPanel.RefreshRequested+=RefreshOfflineShop;offlineShopPanel.RefreshUnsoldRequested+=RefreshOfflineShopUnsold;offlineShopPanel.RefreshMoneyRequested+=RefreshOfflineShopMoney;offlineShopPanel.CheckRequested+=CheckOfflineShop;
            offlineShopPanel.AddItemRequested+=PromptOfflineShopAdd;offlineShopPanel.RemoveOrTakeRequested+=PromptOfflineShopRemoveOrTake;offlineShopPanel.ChangePriceRequested+=PromptOfflineShopChangePrice;
            offlineShopPanel.WithdrawRequested+=PromptOfflineShopWithdraw;offlineShopPanel.DestroyRequested+=PromptOfflineShopDestroy;offlineShopPanel.ChangeNameRequested+=PromptOfflineShopName;
            offlineShopPanel.CreateRequested+=OpenOfflineShopCreate;offlineShopPanel.TeleportRequested+=TeleportToOfflineShop;offlineShopPanel.HistoryRequested+=RequestOfflineShopHistory;offlineShopPanel.ErrorRequested+=ShowOfflineShopError;offlineShopPanel.ItemInvalidated+=InvalidateOfflineShopModal;
            gameUi.Close("offlineshopmanagerdialog");

            GameObject customer=gameUi.Open("offlineshopdialog");
            if(customer!=null)
            {
                offlineShopCustomerPanel=customer.GetComponent<Metin2OfflineShopPanel>()??customer.AddComponent<Metin2OfflineShopPanel>();
                offlineShopCustomerPanel.BindOriginalWidgets(customer.transform,false);offlineShopCustomerPanel.Configure(attachment,tips,playerState,localeText,offlineShopWorldSigns,entityWorld,entityWorld!=null?entityWorld.LocalPlayerRoot:null);offlineShopCustomerPanel.Bind(gameRuntime,itemCatalog);
                offlineShopCustomerPanel.BuyPromptRequested+=PromptOfflineShopBuy;offlineShopCustomerPanel.CloseRequested+=CloseOfflineShop;offlineShopCustomerPanel.ErrorRequested+=ShowOfflineShopError;offlineShopCustomerPanel.ItemInvalidated+=InvalidateOfflineShopModal;offlineShopCustomerPanel.WindowClosed+=CloseOfflineShopDialogs;
                // The original client closes its own shop manager before displaying another
                // player's shop. Otherwise the manager is exposed when the customer view closes.
                offlineShopCustomerPanel.WindowOpened+=()=>offlineShopPanel?.CloseLocal(false);
                gameUi.Close("offlineshopdialog");
            }
            Metin2InventoryPanel inventory=gameUi.GetPanel<Metin2InventoryPanel>("inventorywindow");
            if(inventory!=null){inventory.OfflineShopBuyDropRequested-=BuyOfflineShopDrop;inventory.OfflineShopBuyDropRequested+=BuyOfflineShopDrop;}

            GameObject create=gameUi.Open("offlineshopcreatedialog");
            if(create!=null){offlineShopCreatePanel=create.GetComponent<Metin2OfflineShopCreatePanel>()??create.AddComponent<Metin2OfflineShopCreatePanel>();offlineShopCreatePanel.BindOriginalWidgets(create.transform,localeText);offlineShopCreatePanel.BuilderRequested+=OpenOfflineShopBuilder;offlineShopCreatePanel.ErrorRequested+=ShowOfflineShopError;gameUi.Close("offlineshopcreatedialog");}
            GameObject builder=gameUi.Open("offlineshopbuilder");
            if(builder!=null)
            {
                offlineShopDraft=new Metin2OfflineShopDraft();offlineShopBuilderPanel=builder.GetComponent<Metin2OfflineShopBuilderPanel>()??builder.AddComponent<Metin2OfflineShopBuilderPanel>();
                offlineShopBuilderPanel.BindOriginalWidgets(builder.transform,offlineShopDraft,playerState,itemCatalog,attachment,tips);offlineShopBuilderPanel.PriceRequested+=PromptOfflineShopBuilderPrice;offlineShopBuilderPanel.BuildRequested+=BuildOfflineShop;offlineShopBuilderPanel.ErrorRequested+=ShowOfflineShopError;gameUi.Close("offlineshopbuilder");
            }
        }

        private void BuyOfflineShopDrop(byte position)=>offlineShopCustomerPanel?.RequestBuy(position);

        /// <summary>
        /// The Auriga 214 board windows the imported dialogs do not carry: shop list, search,
        /// offers, safebox and auction. Every intent maps to one CG 119 writer on the session.
        /// </summary>
        private void EnsureAurigaOfflineShopBoards()
        {
            if (aurigaOfflineShopBoards == null)
            {
                aurigaOfflineShopBoards = GetComponent<Metin2AurigaOfflineShopBoards>() ??
                    gameObject.AddComponent<Metin2AurigaOfflineShopBoards>();
                aurigaOfflineShopBoards.Bind(aurigaOfflineShopState, itemCatalog, EnsureToolTips());
                aurigaOfflineShopBoards.ListRequested += SendAurigaShopListRequest;
                aurigaOfflineShopBoards.OpenShopRequested += SendAurigaShopOpen;
                aurigaOfflineShopBoards.BuyFromSearchRequested += SendAurigaShopBuyFromSearch;
                aurigaOfflineShopBoards.FilterRequested += SendAurigaShopFilter;
                aurigaOfflineShopBoards.OfferListRequested += SendAurigaOfferList;
                aurigaOfflineShopBoards.OfferCreateRequested += SendAurigaOfferCreate;
                aurigaOfflineShopBoards.OfferAcceptRequested += SendAurigaOfferAccept;
                aurigaOfflineShopBoards.OfferCancelRequested += SendAurigaOfferCancel;
                aurigaOfflineShopBoards.SafeboxOpenRequested += SendAurigaSafeboxOpen;
                aurigaOfflineShopBoards.SafeboxCloseRequested += SendAurigaSafeboxClose;
                aurigaOfflineShopBoards.SafeboxItemRequested += SendAurigaSafeboxItem;
                aurigaOfflineShopBoards.SafeboxMoneyRequested += SendAurigaSafeboxMoney;
                aurigaOfflineShopBoards.AuctionListRequested += SendAurigaAuctionList;
                aurigaOfflineShopBoards.AuctionOpenRequested += SendAurigaAuctionOpen;
                aurigaOfflineShopBoards.AuctionOpenMyRequested += SendAurigaAuctionOpenMy;
                aurigaOfflineShopBoards.AuctionBidRequested += SendAurigaAuctionBid;
                aurigaOfflineShopBoards.AuctionExitRequested += SendAurigaAuctionExit;
                aurigaOfflineShopBoards.AuctionCreateRequested += SendAurigaAuctionCreate;
                aurigaOfflineShopBoards.CloseMyAuctionRequested += SendAurigaCloseMyAuction;
                aurigaOfflineShopBoards.ClickEntityRequested += SendAurigaShopClickEntity;
            }
            else
            {
                aurigaOfflineShopBoards.Bind(aurigaOfflineShopState, itemCatalog, EnsureToolTips());
            }
        }

        private async void SendAurigaShopListRequest() => await RunAurigaOfflineShopSend("list",
            (session, token) => session.SendAurigaOfflineShopRequestListAsync(token));
        private async void SendAurigaShopOpen(uint ownerId) => await RunAurigaOfflineShopSend("open",
            (session, token) => session.SendAurigaOfflineShopOpenAsync(ownerId, token));
        private async void SendAurigaShopBuyFromSearch(uint ownerId, uint itemId, long price) => await RunAurigaOfflineShopSend("buy-search",
            (session, token) => session.SendAurigaOfflineShopBuyAsync(ownerId, itemId, true, price, token));
        private async void SendAurigaShopFilter(byte type, byte subType, string name, long priceStart, long priceEnd,
            int levelStart, int levelEnd, uint wearFlag, int rarity) => await RunAurigaOfflineShopSend("filter",
            (session, token) => session.SendAurigaOfflineShopFilterRequestAsync(type, subType, name, priceStart, priceEnd,
                levelStart, levelEnd, wearFlag, null, rarity, token));
        private async void SendAurigaOfferList() => await RunAurigaOfflineShopSend("offers",
            (session, token) => session.SendAurigaOfflineShopOfferListRequestAsync(token));
        private async void SendAurigaOfferCreate(uint ownerId, uint itemId, long price) => await RunAurigaOfflineShopSend("offer-create",
            (session, token) => session.SendAurigaOfflineShopOfferCreateAsync(ownerId, itemId, price, token));
        private async void SendAurigaOfferAccept(uint offerId) => await RunAurigaOfflineShopSend("offer-accept",
            (session, token) => session.SendAurigaOfflineShopOfferAcceptAsync(offerId, token));
        private async void SendAurigaOfferCancel(uint offerId, uint ownerId) => await RunAurigaOfflineShopSend("offer-cancel",
            (session, token) => session.SendAurigaOfflineShopOfferCancelAsync(offerId, ownerId, token));
        private async void SendAurigaSafeboxOpen() => await RunAurigaOfflineShopSend("safebox",
            (session, token) => session.SendAurigaOfflineShopSafeboxOpenAsync(token));
        private async void SendAurigaSafeboxClose() => await RunAurigaOfflineShopSend("safebox-close",
            (session, token) => session.SendAurigaOfflineShopSafeboxCloseAsync(token));
        private async void SendAurigaSafeboxItem(uint itemId) => await RunAurigaOfflineShopSend("safebox-item",
            (session, token) => session.SendAurigaOfflineShopSafeboxGetItemAsync(itemId, token));
        private async void SendAurigaSafeboxMoney(long yang) => await RunAurigaOfflineShopSend("safebox-money",
            (session, token) => session.SendAurigaOfflineShopSafeboxGetValutesAsync(yang, token));
        private async void SendAurigaAuctionList() => await RunAurigaOfflineShopSend("auction-list",
            (session, token) => session.SendAurigaOfflineShopAuctionListRequestAsync(token));
        private async void SendAurigaAuctionOpen(uint ownerId) => await RunAurigaOfflineShopSend("auction-open",
            (session, token) => session.SendAurigaOfflineShopAuctionOpenAsync(ownerId, token));
        private async void SendAurigaAuctionOpenMy() => await RunAurigaOfflineShopSend("auction-mine",
            (session, token) => session.SendAurigaOfflineShopAuctionOpenMyAsync(token));
        private async void SendAurigaAuctionBid(uint ownerId, long price) => await RunAurigaOfflineShopSend("auction-bid",
            (session, token) => session.SendAurigaOfflineShopAuctionAddOfferAsync(ownerId, price, token));
        private async void SendAurigaAuctionExit(uint ownerId) => await RunAurigaOfflineShopSend("auction-exit",
            (session, token) => session.SendAurigaOfflineShopAuctionExitFromAsync(ownerId, token));
        private async void SendAurigaAuctionCreate(uint duration, byte window, ushort cell, long price) => await RunAurigaOfflineShopSend("auction-create",
            (session, token) => session.SendAurigaOfflineShopAuctionCreateAsync(duration, window, cell, price, token));
        private async void SendAurigaCloseMyAuction() => await RunAurigaOfflineShopSend("auction-close-mine",
            (session, token) => session.SendAurigaOfflineShopCloseMyAuctionAsync(token));
        private async void SendAurigaShopClickEntity(uint shopVid) => await RunAurigaOfflineShopSend("entity",
            (session, token) => session.SendAurigaOfflineShopClickEntityAsync(shopVid, token));

        private async System.Threading.Tasks.Task RunAurigaOfflineShopSend(string action,
            Func<Metin2AuthSession, System.Threading.CancellationToken, System.Threading.Tasks.Task> send)
        {
            if (!CanActMainInstance() || gameSession == null) return;
            try { await send(gameSession, lifetime.Token); }
            catch (Exception exception) { Debug.LogWarning($"METIN2_OFFLINE_SHOP_BOARD_SEND action={action} failed: {exception.Message}"); ShowOperationFailure(exception); }
        }

        /// <summary>Opens one of the Auriga 214 boards from the mobile HUD / system dialog.</summary>
        public void OpenAurigaOfflineShopBoard(int board)
        {
            EnsureOfflineShopPanel();
            EnsureAurigaOfflineShopBoards();
            aurigaOfflineShopBoards?.Toggle((Metin2AurigaOfflineShopBoards.Board)board);
        }
        private void PromptOfflineShopBuy(byte position,Metin2GamePacketCodec.OfflineShopItem item)
        {
            string name=itemCatalog!=null&&itemCatalog.TryGet(item.Vnum,out Metin2ItemCatalog.Entry entry)?entry.name:Metin2Language.Format("UNITY_ITEM_IDENTIFIER",item.Vnum);
            string money=GroupOfflineMoney(item.Price)+" "+Metin2Language.Get("MONETARY_UNIT0");
            string key=item.Count>1?"DO_YOU_BUY_ITEM2":"DO_YOU_BUY_ITEM1";
            string prompt=localeText!=null&&localeText.TryGet(key,out _)?(item.Count>1?localeText.Format(key,name,item.Count,money):localeText.Format(key,name,money)):name+" / "+money;
            offlineModalPosition=position;OpenOfflineShopQuestion(prompt,()=>ConfirmOfflineShopBuy(position,item));
        }
        private async void ConfirmOfflineShopBuy(byte position,Metin2GamePacketCodec.OfflineShopItem expected)
        {
            offlineModalPosition=byte.MaxValue;
            if(!CanActMainInstance()) return;
            if(offlineShopCustomerPanel==null||!offlineShopCustomerPanel.TryGetItem(position,out Metin2GamePacketCodec.OfflineShopItem current)||
                current.Vnum!=expected.Vnum||current.Count!=expected.Count||current.Price!=expected.Price||current.Status!=0)return;
            if(!offlineShopCustomerPanel.TryGetAurigaItemId(position,out uint itemId))return;
            try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopBuyAsync(offlineShopCustomerPanel.OwnerVid,itemId,false,0,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}
        }
        private void PromptOfflineShopAdd(byte display,byte window,ushort cell)
        {
            if(playerState==null||!playerState.TryGetItem(window,cell,out Metin2GamePacketCodec.ItemData item)||item.Vnum==0)return;
            if((item.AntiFlags&(Metin2ItemConstants.AntiFlagGive|Metin2ItemConstants.AntiFlagMyShop))!=0){ShowOfflineShopError("PRIVATE_SHOP_CANNOT_SELL_ITEM");return;}
            OpenOfflineShopInput(Locale("PRIVATE_SHOP_INPUT_PRICE_DIALOG_TITLE"),string.Empty,15,text=>
            {if(!TryOfflineMoney(text,out long price)||price<=0)return;ConfirmOfflineShopAdd(display,window,cell,item,price);});
        }
        private async void ConfirmOfflineShopAdd(byte display,byte window,ushort cell,Metin2GamePacketCodec.ItemData expected,long price)
        {
            if(playerState==null||!playerState.TryGetItem(window,cell,out Metin2GamePacketCodec.ItemData current)||current.Vnum!=expected.Vnum||current.Count!=expected.Count)return;
            try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopAddItemAsync(window,cell,Math.Min(price,Metin2OfflineShopDraft.MaximumPrice),lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}
        }
        private void PromptOfflineShopRemoveOrTake(byte position,Metin2GamePacketCodec.OfflineShopItem item,bool editing)
        {
            string name=itemCatalog!=null&&itemCatalog.TryGet(item.Vnum,out Metin2ItemCatalog.Entry e)?e.name:Metin2Language.Format("UNITY_ITEM_IDENTIFIER",item.Vnum);
            string prompt=localeText!=null&&localeText.TryGet("OFFLINE_SHOP_REMOVE_ITEM_QUESTION",out _)?localeText.Format("OFFLINE_SHOP_REMOVE_ITEM_QUESTION",name):name;
            offlineModalPosition=position;OpenOfflineShopQuestion(prompt,()=>ConfirmOfflineShopRemoveOrTake(position,item,editing));
        }
        private async void ConfirmOfflineShopRemoveOrTake(byte position,Metin2GamePacketCodec.OfflineShopItem expected,bool editing)
        {
            offlineModalPosition=byte.MaxValue;if(offlineShopPanel==null||!offlineShopPanel.TryGetItem(position,out Metin2GamePacketCodec.OfflineShopItem current)||current.Vnum!=expected.Vnum||current.Count!=expected.Count||current.Status==2)return;
            // Auriga removes by database item id. The server answers with a fresh SHOP_OPEN_OWNER,
            // so the row disappears from the window without a separate refresh round trip.
            if(!offlineShopPanel.TryGetAurigaItemId(position,out uint itemId))return;
            try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopRemoveItemAsync(itemId,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}
        }
        private void PromptOfflineShopChangePrice(byte position,Metin2GamePacketCodec.OfflineShopItem item)
        {
            offlineModalPosition=position;OpenOfflineShopInput(Locale("PRIVATE_SHOP_INPUT_CHANGE_PRICE_DIALOG_TITLE"),item.Price.ToString(CultureInfo.InvariantCulture),15,text=>
            {if(TryOfflineMoney(text,out long price)&&price>0)ConfirmOfflineShopChangePrice(position,item,price);});
        }
        private async void ConfirmOfflineShopChangePrice(byte position,Metin2GamePacketCodec.OfflineShopItem expected,long price)
        {
            offlineModalPosition=byte.MaxValue;if(offlineShopPanel==null||!offlineShopPanel.TryGetItem(position,out Metin2GamePacketCodec.OfflineShopItem current)||current.Vnum!=expected.Vnum||current.Count!=expected.Count||current.Status!=0)return;
            if(!offlineShopPanel.TryGetAurigaItemId(position,out uint itemId))return;
            try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopEditItemAsync(itemId,Math.Min(price,Metin2OfflineShopDraft.MaximumPrice),lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}
        }
        private void PromptOfflineShopWithdraw()
        {
            if(offlineShopPanel==null||offlineShopPanel.CurrentMoney<=0){ShowOfflineShopError("OFFLINE_SHOP_DONT_HAVE_MONEY_TO_WITHDRAW");return;}
            OpenOfflineShopInput(Locale("PICK_MONEY_TITLE"),offlineShopPanel.CurrentMoney.ToString(CultureInfo.InvariantCulture),15,text=>{if(TryOfflineMoney(text,out long amount)&&amount>0&&amount<=offlineShopPanel.CurrentMoney)WithdrawOfflineShopMoney(amount);});
        }
        private async void WithdrawOfflineShopMoney(long amount){try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopSafeboxGetValutesAsync(amount,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private void PromptOfflineShopDestroy()=>OpenOfflineShopQuestion(Locale("OFFLINE_SHOP_CLOSE_QUESTION"),DestroyOfflineShop);
        private async void DestroyOfflineShop(){try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopForceCloseAsync(lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private void PromptOfflineShopName()=>OpenOfflineShopInput(Locale("PRIVATE_SHOP_INPUT_NEW_NAME_DIALOG_TITLE"),string.Empty,32,text=>
        {string name=(text??string.Empty).Trim();if(name.Length==0)ShowOfflineShopError("OFFLINE_SHOP_CREATE_DONT_HAVE_NAME");else if(name.Length<3)ShowOfflineShopError("OFFLINE_SHOP_CREATE_SMALL_SHOP_NAME");else ChangeOfflineShopName(name);});
        private async void ChangeOfflineShopName(string name){try{if(gameSession!=null)await gameSession.SendAurigaOfflineShopChangeNameAsync(name,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private void OpenOfflineShopCreate(){if(offlineShopCreatePanel!=null){offlineShopPanel?.CloseLocal(false);offlineShopCreatePanel.Open(offlineShopPanel!=null&&offlineShopPanel.IsPremium);}}
        private void OpenOfflineShopBuilder(string name,byte npcStyle,byte boardStyle)
        {
            // KASMIR_PAKET_SYSTEM: the chosen NPC is transmitted as dwKasmirNpc so the shop entity
            // keeps the same look. The imported create window exposes the vnum it was opened with.
            offlineShopBuilderKasmirNpc=offlineShopCreatePanel!=null?offlineShopCreatePanel.NpcVnum:0;
            offlineShopBuilderPanel?.Open(name,npcStyle,boardStyle);
        }
        private void PromptOfflineShopBuilderPrice(byte display,byte window,ushort cell,Metin2GamePacketCodec.ItemData item)
        {OpenOfflineShopInput(Locale("PRIVATE_SHOP_INPUT_PRICE_DIALOG_TITLE"),string.Empty,15,text=>{if(TryOfflineMoney(text,out long price)&&price>0)offlineShopBuilderPanel?.AcceptPrice(display,window,cell,item.Vnum,item.Count,Math.Min(price,Metin2OfflineShopDraft.MaximumPrice));});}
        private async void BuildOfflineShop(string name,byte npcStyle,byte boardStyle,System.Collections.Generic.IReadOnlyList<Metin2GamePacketCodec.OfflineShopBuildItem> items)
        {
            // The Auriga create request is TShopInfo + TShopItemInfo(TItemPos + price); the legacy
            // display position has no wire field. npcStyle/boardStyle stay presentation-only.
            var stock=new System.Collections.Generic.List<Metin2GamePacketCodec.AurigaShopPositionItem>();
            if(items!=null)foreach(Metin2GamePacketCodec.OfflineShopBuildItem item in items)stock.Add(new Metin2GamePacketCodec.AurigaShopPositionItem(item.Window,item.Cell,item.Price));
            try{if(gameSession!=null){offlineShopPanel?.RequestManagerOpen();await gameSession.SendAurigaOfflineShopCreateAsync(name,AurigaOfflineShopDurationMinutes,offlineShopBuilderKasmirNpc,stock,lifetime.Token);}}catch(Exception e){ShowOperationFailure(e);}
        }
        private void TeleportToOfflineShop()=>SendChat("/teleport_to_offshop");
        private void RequestOfflineShopHistory()=>SendChat("/offlineshop_sales");
        private async void RefreshOfflineShopUnsold()
        {
            // Auriga has no legacy CG_OFFLINESHOP header.  Header 57 is CG_OPENSHOP and its
            // registered size is five bytes, so sending the old two-byte "refresh unsold"
            // request makes packet_info consume bytes from the next packet and disconnect.
            // Unsold items live in the new-offlineshop safebox; opening it returns its snapshot.
            try { if (gameSession != null) await gameSession.SendAurigaOfflineShopSafeboxOpenAsync(lifetime.Token); }
            catch (Exception e) { ShowOperationFailure(e); }
        }

        private void OpenOfflineShopQuestion(string prompt,Action accepted)
        {
            if(gameUi==null)return;GameObject window=gameUi.Open("questiondialog");if(window==null)return;
            offlineShopQuestion=window.GetComponent<Metin2QuestionDialogPanel>()??window.AddComponent<Metin2QuestionDialogPanel>();offlineShopQuestion.Open(prompt,accepted);
        }
        private void OpenOfflineShopInput(string title,string initial,int limit,Action<string> accepted)
        {
            CloseOfflineShopInput();if(gameUi==null)return;GameObject window=gameUi.Open("inputdialogofflineshop");if(window==null)return;offlineShopInputWindow=window;
            // inputdialogofflineshop.py builds InputValue through ui.MakeEditLine: the imported
            // prefab already carries the InputField on that node with its text component on the
            // child "Text". Searching for a Text component returned null here and left both the
            // AcceptButton and CancelButton unwired, killing every price/withdraw/name flow
            // (Master TODO 2.5).
            InputField input=FindNamed<InputField>(window.transform,"InputValue");
            if(input==null)
            {
                Text fallback=FindNamed<Text>(window.transform,"InputValue");if(fallback==null)return;
                input=fallback.GetComponent<InputField>()??fallback.gameObject.AddComponent<InputField>();input.textComponent=fallback;
            }
            Image inputHitArea=input.GetComponent<Image>();
            if(inputHitArea!=null){inputHitArea.raycastTarget=true;input.targetGraphic=inputHitArea;}
            input.transform.SetAsLastSibling();
            input.characterLimit=limit;input.text=initial??string.Empty;
            Text titleText=FindNamed<Text>(window.transform,"TitleName");if(titleText!=null)titleText.text=title;
            Button ok=FindNamed<Button>(window.transform,"AcceptButton"),cancel=FindNamed<Button>(window.transform,"CancelButton");
            if(ok!=null){ok.onClick.RemoveAllListeners();ok.onClick.AddListener(()=>{string value=input.text;CloseOfflineShopInput();accepted?.Invoke(value);});}
            if(cancel!=null){cancel.onClick.RemoveAllListeners();cancel.onClick.AddListener(CloseOfflineShopInput);}
            // uiofflineshop.py OpenPickMoneyDialog/OnPickMoney accepts the dialog on RETURN as well.
            input.onSubmit.RemoveAllListeners();input.onSubmit.AddListener(value=>{CloseOfflineShopInput();accepted?.Invoke(value);});
            input.Select();input.ActivateInputField();window.transform.SetAsLastSibling();
        }
        private void CloseOfflineShopInput(){if(offlineShopInputWindow!=null)offlineShopInputWindow.SetActive(false);offlineShopInputWindow=null;}
        private void CloseOfflineShopDialogs(){if(offlineShopQuestion!=null)offlineShopQuestion.Close();offlineShopQuestion=null;CloseOfflineShopInput();offlineModalPosition=byte.MaxValue;}
        private void InvalidateOfflineShopModal(byte position){if(offlineModalPosition!=position)return;CloseOfflineShopDialogs();}
        private void ShowOfflineShopError(string result)
        {
            string key=result;switch(result){case "NOT_ENOUGH_MONEY":key="SHOP_NOT_ENOUGH_MONEY";break;case "SOLDOUT":key="SHOP_SOLDOUT";break;case "INVENTORY_FULL":key="SHOP_INVENTORY_FULL";break;case "INVALID_POS":key="SHOP_INVALID_POS";break;case "PRICE_CHANGED":key="OFFLINE_SHOP_PRICE_CHANGED_RELOAD";break;}
            PopupMessageLocale(key);
        }
        private string Locale(string key)=>Metin2Language.Get(key);
        private static string GroupOfflineMoney(long value)=>Math.Max(0L,value).ToString("#,0",CultureInfo.InvariantCulture).Replace(',','.');
        private static bool TryOfflineMoney(string text,out long value)=>long.TryParse((text??string.Empty).Replace(".",string.Empty).Replace(",",string.Empty).Replace(" ",string.Empty),NumberStyles.Integer,CultureInfo.InvariantCulture,out value);

        /// <summary>
        /// Streams raw framing traces to <c>Migration/Logs/framing_trace.log</c>.
        /// The framer runs on a background thread, so writes are serialised through a lock; the file
        /// is truncated at the start of every session so it always describes the current login.
        /// </summary>
        private void StartFramingTrace()
        {
            string path = System.IO.Path.Combine(Application.dataPath, "..", "Migration", "Logs", "framing_trace.log");
            path = System.IO.Path.GetFullPath(path);
            var gate = new object();
            try
            {
                System.IO.Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path));
                System.IO.File.WriteAllText(path, "# Metin2 framing trace " + DateTime.Now.ToString("s") + "\n");
            }
            catch (Exception exception) { Debug.LogWarning("METIN2_FRAME_TRACE_FAILED " + exception.Message); return; }

            gameSession.TraceFraming(message =>
            {
                try { lock (gate) System.IO.File.AppendAllText(path, message + "\n"); }
                catch { /* tracing must never break the receive loop */ }
                // 152/153 are the hybridcrypt key and map-SDB packets. The server sends the SDB
                // block only when a warp crosses into a different map index
                // (char.cpp:5355-5362), so they are exactly the packets a teleport-only desync
                // would involve, and exactly the ones the old header list could not show.
            }, 19, 136, 75, 16, 20, 21, 152, 153, 160, 161, 162);
            Debug.Log("METIN2_FRAME_TRACE file=" + path);
        }

        /// <summary>
        /// Connects a window generated from the original layout to the network session. The panels
        /// themselves only raise intent events, so every packet still leaves through this class.
        /// </summary>
        private void ConnectOriginalWindow(string screen, GameObject window)
        {
            // WindowCreated is the earliest route-independent point at which the imported system
            // dialog exists. Connect its intents here so a task-bar click, Escape, or any future
            // direct Open("systemdialog") receives the same live session handlers.
            Metin2SystemDialogPanel systemDialog = window.GetComponent<Metin2SystemDialogPanel>();
            if (systemDialog != null)
            {
                systemDialog.CommandRequested -= SendSystemCommand;
                systemDialog.CommandRequested += SendSystemCommand;
                systemDialog.ScreenRequested -= OpenSystemScreen;
                systemDialog.ScreenRequested += OpenSystemScreen;
                systemDialog.ItemShopRequested -= RequestOpenItemShop;
                systemDialog.ItemShopRequested += RequestOpenItemShop;
                systemDialog.CloseRequested -= CloseSystemDialog;
                systemDialog.CloseRequested += CloseSystemDialog;
            }

            Metin2SocketWindowPanel socketWindow = window.GetComponent<Metin2SocketWindowPanel>();
            if (socketWindow != null)
            {
                socketWindow.Bind(playerState, itemCatalog, EnsureToolTips());
                socketWindow.BindCursor(EnsureCursorAttachment());
                socketWindow.SlotDropRequested -= OnSocketWindowDrop;
                socketWindow.SlotDropRequested += OnSocketWindowDrop;
            }

            Metin2RunePanel rune = window.GetComponent<Metin2RunePanel>();
            if (rune != null)
            {
                rune.Bind(playerState, socketWindow);
                rune.SetStatus(runeStatus);
                if (runeEffectFlag.HasValue) rune.SetEffectFlag(runeEffectFlag.Value);
                rune.ChatRequested -= SendChat; rune.ChatRequested += SendChat;
                rune.ConfirmationRequested -= ShowRuneConfirmation;
                rune.ConfirmationRequested += ShowRuneConfirmation;
            }

            Metin2DailyGiftPanel dailyGift = window.GetComponent<Metin2DailyGiftPanel>();
            if (dailyGift != null)
            {
                dailyGift.Bind(itemCatalog, EnsureToolTips());
                dailyGift.RewardRequested -= ClaimDailyReward;
                dailyGift.RewardRequested += ClaimDailyReward;
            }

            Metin2ShoutPanel shout = window.GetComponent<Metin2ShoutPanel>();
            if (shout != null)
            {
                shout.Bind();
                Metin2AutoShoutRunner runner = EnsureAutoShoutRunner();
                shout.StateChanged -= runner.SetState;
                shout.StateChanged += runner.SetState;
            }

            Metin2WikiPanel wiki = window.GetComponent<Metin2WikiPanel>();
            if (wiki != null)
            {
                wiki.Bind(itemCatalog);
                wiki.BindToolTips(EnsureToolTips());
                wiki.WikiInfoRequested -= OnWikiInfoRequested;
                wiki.WikiInfoRequested += OnWikiInfoRequested;
            }

            Metin2DungeonInfoPanel dungeonInfo = window.GetComponent<Metin2DungeonInfoPanel>();
            if (dungeonInfo != null)
            {
                dungeonInfo.Bind(dungeonInfoState, itemCatalog, EnsureToolTips());
                dungeonInfo.WarpConfirmationRequested -= ConfirmDungeonWarp;
                dungeonInfo.WarpConfirmationRequested += ConfirmDungeonWarp;
                dungeonInfo.QuestButtonRequested -= RequestDungeonQuestCommand;
                dungeonInfo.QuestButtonRequested += RequestDungeonQuestCommand;
            }
            Metin2DungeonRankingPanel dungeonRanking = window.GetComponent<Metin2DungeonRankingPanel>();
            if (dungeonRanking != null) dungeonRanking.Bind(dungeonInfoState);

            Metin2InventoryPanel inventory = window.GetComponent<Metin2InventoryPanel>();
            if (inventory != null)
            {
                inventory.UseRequested -= UseInventoryItem; inventory.UseRequested += UseInventoryItem;
                inventory.MoveRequested -= MoveInventoryItem; inventory.MoveRequested += MoveInventoryItem;
                inventory.UseToItemRequested -= UseInventoryItemOnItem; inventory.UseToItemRequested += UseInventoryItemOnItem;
                inventory.AttachMetinRequested -= OpenAttachMetinDialog; inventory.AttachMetinRequested += OpenAttachMetinDialog;
                inventory.SplitRequested -= OpenInventorySplit; inventory.SplitRequested += OpenInventorySplit;
                inventory.RefineRequested -= OpenRefineForScroll; inventory.RefineRequested += OpenRefineForScroll;
                inventory.BindCursor(EnsureCursorAttachment());
                inventory.BindToolTips(EnsureToolTips());
                inventory.WorldReleaseRequested -= HandleInventoryWorldRelease; inventory.WorldReleaseRequested += HandleInventoryWorldRelease;
                inventory.ItemLinkRequested -= InsertInventoryChatLink; inventory.ItemLinkRequested += InsertInventoryChatLink;
                inventory.ShopSellRequested -= SellInventoryItemToShop; inventory.ShopSellRequested += SellInventoryItemToShop;
                inventory.ExternalDropRequested -= OnInventoryExternalDrop; inventory.ExternalDropRequested += OnInventoryExternalDrop;
                inventory.ShopBuyDropRequested -= BuyFromShop; inventory.ShopBuyDropRequested += BuyFromShop;
                inventory.ShopInfoRequested -= ShowShopInfo; inventory.ShopInfoRequested += ShowShopInfo;
                inventory.SortRequested -= SortInventory; inventory.SortRequested += SortInventory;
                inventory.ExtendRequested -= OpenExtraInventory; inventory.ExtendRequested += OpenExtraInventory;
                EnsureInventoryActionButtons(inventory);
                shopPanel?.BindInventory(inventory);
            }

            Metin2CharacterWindowPanel character = window.GetComponent<Metin2CharacterWindowPanel>();
            if (character != null)
            {
                // GC_MAIN_CHARACTER carries the name; the guild is not known until GC_GUILD arrives.
                character.SetIdentity(mainCharacterName, string.Empty);
                character.BindSkills(skillCatalog, mainCharacterJob, mainCharacterSkillGroup,
                    localeText, skillDescriptionCatalog, Resources.Load<Metin2SkillTable>("Metin2SkillTable"), localEmpire);
                character.SetComboActive(activeComboType > 0);
                character.BindEmotions(emotionCatalog);
                character.BindQuests(questState);
                character.QuestOpenRequested -= OpenQuestDialog; character.QuestOpenRequested += OpenQuestDialog;
                // uicharacter.py raises the stat points through the chat channel (/stat st), not a
                // dedicated packet, so this goes out on the same path as typed chat.
                character.ChatRequested -= SendChat; character.ChatRequested += SendChat;
                character.SkillUseRequested -= UseSkillVnum; character.SkillUseRequested += UseSkillVnum;
                character.SkillUpRequested -= LevelUpSkill; character.SkillUpRequested += LevelUpSkill;
                character.SkillQuickSlotRequested -= RegisterSkillQuickSlot; character.SkillQuickSlotRequested += RegisterSkillQuickSlot;
                character.EmotionQuickSlotRequested -= RegisterEmotionQuickSlot; character.EmotionQuickSlotRequested += RegisterEmotionQuickSlot;
                // The same carrier the quick-slot bar uses - that shared instance is what lets a
                // skill dragged off this page land on the task bar (mouseModule's module-level
                // mouseController). Created lazily because a window can be built before the task bar.
                character.BindCursor(EnsureCursorAttachment());
                // Both the skill page and the emotion page hover through this one controller
                // (uicharacter.py:421 SetSkillToolTip, :288 the emotion tooltip).
                character.BindToolTips(EnsureToolTips());
                // uicharacter.py:781-801 __ClickExpandButton: the arrow button toggles the
                // character details sub-window (uicharacterdetails.py).
                character.DetailsToggleRequested -= ToggleCharacterDetails;
                character.DetailsToggleRequested += ToggleCharacterDetails;
                character.DetailsCloseRequested -= CloseCharacterDetails;
                character.DetailsCloseRequested += CloseCharacterDetails;
                // RefreshCharacter (race portrait) and RefreshAlignment/RefreshGuild read the
                // local actor's relationship fields straight off the entity world.
                character.BindRace(mainCharacterRace);
                character.BindEntityWorld(entityWorld);
                if (entityWorld != null)
                {
                    entityWorld.LocalSkillColorsChanged -= ApplyLocalSkillColorsToState;
                    entityWorld.LocalSkillColorsChanged += ApplyLocalSkillColorsToState;
                    entityWorld.LocalTextTailChanged -= RefreshCharacterWindowIdentity;
                    entityWorld.LocalTextTailChanged += RefreshCharacterWindowIdentity;
                }
                // __ClickEmotionSlot dual-action validation: the target board's vid and the
                // actor name resolver stand in for player.GetTargetVID / chr.GetNameByVID.
                character.BindTargetLookup(
                    () => targetBoard != null ? targetBoard.TargetVid : 0u,
                    vid => entityWorld != null ? entityWorld.ResolveDisplayName(vid) : string.Empty);
                // chat.AppendChat(CHAT_TYPE_INFO, EMOTION_CHOOSE_ONE) - local-only feedback.
                character.InfoChatRequested -= AppendInfoLine; character.InfoChatRequested += AppendInfoLine;
                // ENABLE_SKILL_COLOR_SYSTEM: the colour window the character window opens for
                // perfect-master skills; confirm sends CG_SKILL_COLOR (56).
                Metin2SkillColorWindow skillColorWindow =
                    Metin2SkillColorWindow.Ensure(window, character);
                skillColorWindow.Configure(playerState, EnsureToolTips(), uiCatalog);
                skillColorWindow.ConfirmRequested -= SendSkillColor;
                skillColorWindow.ConfirmRequested += SendSkillColor;
                skillColorWindow.InfoRequested -= AppendInfoLine;
                skillColorWindow.InfoRequested += AppendInfoLine;
                ApplyLocalSkillColorsToState();
                RefreshCharacterWindowIdentity();
            }

            Metin2GameOptionPanel gameOptions = window.GetComponent<Metin2GameOptionPanel>();
            if (gameOptions != null)
            {
                gameOptions.Configure(playerState, entityWorld);
                gameOptions.ChatRequested -= SendChat;
                gameOptions.ChatRequested += SendChat;
                gameOptions.InfoRequested -= AppendGameOptionInfo;
                gameOptions.InfoRequested += AppendGameOptionInfo;
            }
            window.GetComponent<Metin2SystemOptionPanel>()?.Configure(localeText);

            Metin2QuestDialogPanel questDialog = window.GetComponent<Metin2QuestDialogPanel>();
            if (questDialog != null)
            {
                questDialog.AnswerSelected -= AnswerQuest; questDialog.AnswerSelected += AnswerQuest;
            }
            Metin2MessengerPanel messenger = window.GetComponent<Metin2MessengerPanel>();
            if (messenger != null)
            {
                messenger.Configure(localeText, mainCharacterName);
                messenger.AddRequested -= AddMessengerFriend; messenger.AddRequested += AddMessengerFriend;
                messenger.RemoveRequested -= RemoveMessengerFriend; messenger.RemoveRequested += RemoveMessengerFriend;
                messenger.GuildRemoveRequested -= RemoveMessengerGuildMember; messenger.GuildRemoveRequested += RemoveMessengerGuildMember;
                messenger.WhisperRequested -= OpenWhisperForTarget; messenger.WhisperRequested += OpenWhisperForTarget;
                messenger.GuildRequested-=OpenGuildWindow;messenger.GuildRequested+=OpenGuildWindow;
                foreach (Metin2GamePacketCodec.MessengerMessage message in messengerReplay) messenger.Apply(message);
                foreach (Metin2GamePacketCodec.GuildMessage message in guildReplay) messenger.ApplyGuild(message);
                if (loverMiningState != null && !string.IsNullOrEmpty(loverMiningState.LoverName))
                {
                    messenger.SetLover(new Metin2GamePacketCodec.LoverInfo(loverMiningState.LoverName, loverMiningState.LovePoint));
                    messenger.SetLoverOnline(loverMiningState.IsOnline);
                }
            }
            Metin2GuildPanel guild=window.GetComponent<Metin2GuildPanel>();if(guild!=null){guild.Configure(localeText,playerState,skillCatalog,EnsureToolTips(),mainCharacterName);guild.PostCommentRequested-=PostGuildComment;guild.PostCommentRequested+=PostGuildComment;guild.DeleteCommentRequested-=DeleteGuildComment;guild.DeleteCommentRequested+=DeleteGuildComment;guild.RefreshCommentsRequested-=RefreshGuildComments;guild.RefreshCommentsRequested+=RefreshGuildComments;guild.OfferExperienceRequested-=OfferGuildExperience;guild.OfferExperienceRequested+=OfferGuildExperience;guild.ChangeMemberGradeRequested-=ChangeGuildMemberGrade;guild.ChangeMemberGradeRequested+=ChangeGuildMemberGrade;guild.ChangeMemberGeneralRequested-=ChangeGuildMemberGeneral;guild.ChangeMemberGeneralRequested+=ChangeGuildMemberGeneral;guild.ChangeGradeNameRequested-=ChangeGuildGradeName;guild.ChangeGradeNameRequested+=ChangeGuildGradeName;guild.ChangeGradeAuthorityRequested-=ChangeGuildGradeAuthority;guild.ChangeGradeAuthorityRequested+=ChangeGuildGradeAuthority;guild.ChargePowerRequested-=ChargeGuildPower;guild.ChargePowerRequested+=ChargeGuildPower;guild.UseSkillRequested-=UseGuildSkill;guild.UseSkillRequested+=UseGuildSkill;guild.ChatRequested-=SendChat;guild.ChatRequested+=SendChat;guild.UploadMarkRequested-=UploadGuildImage;guild.UploadMarkRequested+=UploadGuildImage;foreach(var message in guildReplay)guild.Apply(message);if(lastGuildInfo.HasValue)guild.ApplyTrophies(lastGuildInfo.Value.Trophies,lastGuildInfo.Value.Win,lastGuildInfo.Value.Loss,lastGuildInfo.Value.Draw);
                // uiguild.py:1749 __OnClickGuildRenewalButton sends /gr_open and the server answers
                // with the full gr_* command state.
                guild.RenewalRequested-=OpenGuildRenewal;guild.RenewalRequested+=OpenGuildRenewal;}
            Metin2ExchangePanel exchange = window.GetComponent<Metin2ExchangePanel>();
            if (exchange != null)
            {
                exchange.Bind(null, itemCatalog);
                exchange.BindInventory(gameUi?.GetPanel<Metin2InventoryPanel>("inventorywindow"));
                exchange.BindLocale(localeText);
                exchange.BindCursor(EnsureCursorAttachment());
                exchange.BindPlayerState(playerState);
                exchange.BindToolTips(EnsureToolTips());
                exchange.GetLocalPosition = () => entityWorld != null && entityWorld.LocalPlayerRoot != null ? (Vector3?)entityWorld.LocalPlayerRoot.transform.position : null;
                exchange.StatusRequested -= SetStatus; exchange.StatusRequested += SetStatus;
                exchange.MoneyPickRequested -= OpenExchangeMoneyPicker; exchange.MoneyPickRequested += OpenExchangeMoneyPicker;
                exchange.ItemAddRequested -= OfferExchangeItem; exchange.ItemAddRequested += OfferExchangeItem;
                exchange.GoldRequested -= OfferExchangeGold; exchange.GoldRequested += OfferExchangeGold;
                exchange.AcceptRequested -= AcceptExchange; exchange.AcceptRequested += AcceptExchange;
                exchange.CancelRequested -= CancelExchange; exchange.CancelRequested += CancelExchange;
            }
            Metin2PrivateShopBuilderPanel privateShopBuilder = window.GetComponent<Metin2PrivateShopBuilderPanel>();
            if (privateShopBuilder != null)
            {
                Metin2PrivateShopDraftHost host = GetComponent<Metin2PrivateShopDraftHost>() ?? gameObject.AddComponent<Metin2PrivateShopDraftHost>();
                Metin2PrivateShopDraft draft = host.Draft;
                privateShopBuilder.Bind(draft, gameUi?.GetPanel<Metin2InventoryPanel>("inventorywindow"));
                privateShopBuilder.BuildRequested -= BuildPrivateShop; privateShopBuilder.BuildRequested += BuildPrivateShop;
            }
            Metin2StoragePanel storage = window.GetComponent<Metin2StoragePanel>();
            if (storage != null)
            {
                storage.Bind(storage.Window, itemCatalog, gameUi?.GetPanel<Metin2InventoryPanel>("inventorywindow"), EnsureCursorAttachment());
                storage.CheckinRequested -= CheckinSafebox; storage.CheckinRequested += CheckinSafebox;
                storage.CheckoutRequested -= CheckoutStorage; storage.CheckoutRequested += CheckoutStorage;
                storage.MoveRequested -= MoveSafebox; storage.MoveRequested += MoveSafebox;
                storage.BindToolTips(EnsureToolTips());
                storage.GetLocalPosition=()=>entityWorld!=null&&entityWorld.LocalPlayerRoot!=null?(Vector3?)entityWorld.LocalPlayerRoot.transform.position:null;
                storage.CloseRequested-=CloseStorageRequested;storage.CloseRequested+=CloseStorageRequested;
                storage.ChangePasswordRequested-=OpenStorageChangePassword;storage.ChangePasswordRequested+=OpenStorageChangePassword;
                storage.InfoRequested-=StorageInfo;storage.InfoRequested+=StorageInfo;
            }
        }

        /// <summary>
        /// uiinventory.py:771-779 and :560-564. Shift-click opens the original PickMoneyDialog with
        /// PICK_ITEM_TITLE; accepting attaches only the chosen count to the shared mouse controller.
        /// </summary>
        private void OpenInventorySplit(byte window, ushort cell, int maximum)
        {
            if (window != 1 || gameUi == null) return;
            Metin2InventoryPanel inventory = gameUi.GetPanel<Metin2InventoryPanel>("inventorywindow");
            if (inventory == null) return;
            GameObject dialogObject = gameUi.Open("pickmoneydialog");
            if (dialogObject == null) return;
            Metin2PickAmountDialog dialog = dialogObject.GetComponent<Metin2PickAmountDialog>() ??
                                             dialogObject.AddComponent<Metin2PickAmountDialog>();
            dialog.OpenLocale(maximum, "PICK_ITEM_TITLE", count => inventory.AttachSplit(cell, count));
        }
        private void HandleExchange(Metin2GamePacketCodec.ExchangeMessage message)
        {
            if (gameUi == null) return;
            Metin2ExchangePanel panel;
            if (message.SubHeader == 0)
            {
                panel = gameUi.Open("exchangedialog")?.GetComponent<Metin2ExchangePanel>();
                if (panel != null)
                {
                    EnsureExchangeBindings(panel);
                    panel.SetPartner(message.Argument32, ResolveExchangePartnerName(message.Argument32));
                    if(entityWorld!=null&&entityWorld.TryGetEntity(message.Argument32,out var partner))panel.SetPortraits(mainCharacterRace,partner.Race,partner.CharacterLevel);
                    panel.Apply(message);
                }
            }
            else
            {
                panel = gameUi.GetPanel<Metin2ExchangePanel>("exchangedialog");
                if (panel != null)
                {
                    EnsureExchangeBindings(panel);
                    if (string.IsNullOrEmpty(panel.PartnerName) && panel.PartnerVid != 0)
                    {
                        string late = ResolveExchangePartnerName(panel.PartnerVid);
                        if (!string.IsNullOrEmpty(late)) panel.SetPartner(panel.PartnerVid, late);
                    }
                    panel.Apply(message);
                }
            }
        }
        private void EnsureExchangeBindings(Metin2ExchangePanel panel)
        {
            if (panel == null) return;
            panel.Bind(null, itemCatalog);
            panel.BindInventory(gameUi?.GetPanel<Metin2InventoryPanel>("inventorywindow"));
            panel.BindLocale(localeText);
            panel.BindCursor(EnsureCursorAttachment());
            panel.BindPlayerState(playerState);
            panel.BindToolTips(EnsureToolTips());
            panel.GetLocalPosition = () => entityWorld != null && entityWorld.LocalPlayerRoot != null ? (Vector3?)entityWorld.LocalPlayerRoot.transform.position : null;
            panel.StatusRequested -= SetStatus; panel.StatusRequested += SetStatus;
            panel.MoneyPickRequested -= OpenExchangeMoneyPicker; panel.MoneyPickRequested += OpenExchangeMoneyPicker;
            panel.ItemAddRequested -= OfferExchangeItem; panel.ItemAddRequested += OfferExchangeItem;
            panel.GoldRequested -= OfferExchangeGold; panel.GoldRequested += OfferExchangeGold;
            panel.AcceptRequested -= AcceptExchange; panel.AcceptRequested += AcceptExchange;
            panel.CancelRequested -= CancelExchange; panel.CancelRequested += CancelExchange;
        }
        private string ResolveExchangePartnerName(uint vid)
        {
            if (entityWorld != null && entityWorld.TryGetEntity(vid, out Metin2NetworkEntity entity) && entity != null && !string.IsNullOrEmpty(entity.DisplayName))
                return entity.DisplayName;
            return string.Empty;
        }
        private void OpenExchangeMoneyPicker()
        {
            if (!CanActMainInstance() || gameUi == null) return;
            Metin2ExchangePanel panel = gameUi.GetPanel<Metin2ExchangePanel>("exchangedialog");
            if (panel == null || !panel.IsTrading) return;
            if (panel.MineGold > 0) { SetStatusLocale("EXCHANGE_CANT_EDIT_MONEY"); return; }
            GameObject dialogObject = gameUi.Open("pickmoneydialog");
            if (dialogObject == null) return;
            Metin2PickAmountDialog dialog = dialogObject.GetComponent<Metin2PickAmountDialog>() ?? dialogObject.AddComponent<Metin2PickAmountDialog>();
            long owned = playerState != null ? playerState.Gold : 0L;
            if (owned <= 0) return;
            long maximum = Math.Min(owned, Metin2ProtocolCodec.GoldLimitExclusive - 1);
            dialog.OpenLongLocale(maximum, "EXCHANGE_MONEY", amount => { if (amount > 0) panel.OfferGold(amount); });
        }
        private async void OfferExchangeItem(byte window, ushort cell, byte display) { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendExchangeItemAddAsync(window, cell, display, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void OfferExchangeGold(long gold) { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendExchangeGoldAsync(gold, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void AcceptExchange() { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendExchangeAcceptAsync(lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void CancelExchange() { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendExchangeCancelAsync(lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private void HandleInventoryWorldRelease(byte window, ushort cell, int count, Vector2 screenPosition)
            => HandleItemWorldRelease(window, cell, count, screenPosition);

        /// <summary>game.py:1347 __PutItem: the extra inventory (and the belt, which shares the
        /// INVENTORY wire window) may be given or dropped; the mount bag has no such branch.</summary>
        private void HandleAuxiliaryWorldRelease(byte window, ushort cell, int count, Vector2 screenPosition)
        {
            if (window == 5) return;
            HandleItemWorldRelease(window == 7 ? (byte)1 : window, cell, count, screenPosition);
        }

        private void HandleItemWorldRelease(byte window, ushort cell, int count, Vector2 screenPosition)
        {
            if (!CanActMainInstance()) return;
            // game.py:1522 player.IsEquipmentSlot: only the 360..399 worn cells. The belt range
            // (500..659) shares window 1 and must stay droppable.
            if (IsEquipmentWorldDropBlocked(window,cell))
            { SetStatusLocale("EXCHANGE_FAILURE_EQUIP_ITEM"); return; }
            Camera camera = Camera.main;
            Metin2NetworkEntity entity = null;
            // Physics.Raycast can never find an actor here: spawned entities carry no colliders, which
            // is exactly why TryPickEntity exists and tests the bounding volume analytically the way
            // __NEW_Pick does. Using the raycast meant a drop on a blacksmith missed him entirely and
            // fell through to the "throw this away?" dialog.
            if (camera != null && entityWorld != null &&
                entityWorld.TryPickEntity(camera.ScreenPointToRay(screenPosition), camera.transform.position,
                                          entityWorld.LocalVid, out uint pickedVid))
                entityWorld.TryGetEntity(pickedVid, out entity);
            if (entity != null && entity.Vid != 0 && (entityWorld == null || entity.Vid != entityWorld.LocalVid))
            {
                if (entity.Type == 1 || entity.Type == 3 || entity.Type == 9) GiveItem(entity.Vid, window, cell, count);
                else StartExchangeWithItem(entity.Vid, window, cell);
                return;
            }
            ShowItemDropDialog(window, cell, count);
        }

        /// <summary>
        /// game.py:1621-1652 + root/uicommon.py QuestionDialogItem over uiscript/questiondialogitem.py:
        /// Drop (accept) / Destroy (Töröl) / Cancel. The previous dialog only had accept + cancel, so
        /// the original item destroy was unreachable.
        /// </summary>
        private void ShowItemDropDialog(byte window, ushort cell, int count)
        {
            Canvas canvas = ResolveUiCanvas(); Transform host = ResolveUiHost();
#if UNITY_ANDROID || UNITY_IOS
            const string dropScreen="questiondialog";
            if (canvas == null || host == null || uiCatalog == null || !uiCatalog.TryGet(dropScreen, out GameObject prefab) || prefab == null) { SetStatusLocale("UNITY_DROP_DIALOG_UNAVAILABLE"); return; }
            GameObject dialog = Instantiate(prefab, host, false);
#else
            if(canvas==null||host==null||uiCatalog==null)return;
            GameObject dialog=Metin2WindowsItemDropDialog.Create(uiCatalog,host);
            if(dialog==null){SetStatusLocale("UNITY_DROP_DIALOG_UNAVAILABLE");return;}
#endif
#if UNITY_ANDROID || UNITY_IOS
            Metin2MobileWindowPresentation.Apply("questiondialog", dialog);
#endif
            Metin2ItemCatalog.Entry entry = default;
            string itemName = playerState != null && itemCatalog != null &&
                              playerState.TryGetItem(window, cell, out Metin2GamePacketCodec.ItemData item) &&
                              itemCatalog.TryGet(item.Vnum, out entry) && !string.IsNullOrEmpty(entry.name)
                ? entry.name : "?";
            Text message = FindNamed<Text>(dialog.transform, "message");
            if (message != null)
                message.text = count > 1
                    ? Metin2Language.Format("HOW_MANY_ITEM_DO_YOU_DROP2", itemName, count)
                    : Metin2Language.Format("HOW_MANY_ITEM_DO_YOU_DROP1", itemName);
            Button accept = FindNamed<Button>(dialog.transform, "accept");
            Button cancel = FindNamed<Button>(dialog.transform, "cancel");
            Button destroy = FindNamed<Button>(dialog.transform, "destroy") ?? CreateItemDialogDestroyButton(accept);
            if (accept != null)
            {
                SetButtonLabel(accept, Metin2Language.Get("YES"));
                accept.onClick.AddListener(() => { DropItem(window, cell, count); Destroy(dialog); });
            }
            if (destroy != null)
            {
                SetButtonLabel(destroy, Metin2Language.Get("DESTROY"));
                destroy.onClick.AddListener(() => { DestroyItem(window, cell); Destroy(dialog); });
            }
            if (cancel != null)
            {
                SetButtonLabel(cancel, Metin2Language.Get("NO"));
                cancel.onClick.AddListener(() => Destroy(dialog));
            }
        }
        public static bool IsEquipmentWorldDropBlocked(byte window,ushort cell)
        {
#if UNITY_ANDROID || UNITY_IOS
            return window==1&&cell>=Metin2InventoryPanel.EquipmentStartIndex&&cell<Metin2PlayerState.BeltInventoryStartCell;
#else
            return window==1&&cell>=Metin2PlayerState.EquipmentStartCell&&cell<Metin2PlayerState.BeltInventoryStartCell;
#endif
        }

        /// <summary>questiondialogitem.py adds the destroy button between accept and cancel, at
        /// x=0 (centred) / y=63 on the same board. The imported accept/cancel rects keep the raw
        /// script x and are centred by the responsive layout, which cannot know a runtime child, so
        /// the destroy button is placed on the centre line here.</summary>
        private Button CreateItemDialogDestroyButton(Button accept)
        {
            RectTransform acceptRect = accept != null ? accept.transform as RectTransform : null;
            RectTransform board = acceptRect != null ? acceptRect.parent as RectTransform : null;
            if (board == null) return null;
            float boardWidth = board.rect.width > 0f ? board.rect.width : 340f;
            const float buttonWidth = 61f, buttonHeight = 21f, top = 63f;
            return Metin2RuntimeWindowArt.CreateButton(board, "destroy",
                "middle_button_01", "middle_button_02", "middle_button_03", Metin2Language.Get("DESTROY"),
                new Vector2((boardWidth - buttonWidth) * .5f, top), new Vector2(buttonWidth, buttonHeight));
        }

        private static void SetButtonLabel(Button button, string label)
        {
            if (button == null) return;
            Text text = button.GetComponentInChildren<Text>(true);
#if !UNITY_ANDROID && !UNITY_IOS
            if(text!=null&&text.TryGetComponent(out Metin2LocalizedLabel localized))localized.Key=null;
#endif
            if (text != null) text.text = label;
        }
        private async void GiveItem(uint vid, byte window, ushort cell, int count) { try { if (gameSession != null) await gameSession.SendGiveItemAsync(vid, window, cell, count, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void StartExchangeWithItem(uint vid, byte window, ushort cell) { if (!CanActMainInstance()) return; try { if (gameSession == null) return; await gameSession.SendExchangeStartAsync(vid, lifetime.Token); if (!CanActMainInstance()) return; await gameSession.SendExchangeItemAddAsync(window, cell, 0, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void DropItem(byte window, ushort cell, int count) { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendItemDropAsync(window, cell, 0, count, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        /// <summary>game.py:1603 __SendDestroyItemPacket: CG_ITEM_DESTROY, the dialog's destroy button.</summary>
        private async void DestroyItem(byte window, ushort cell) { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendItemDestroyAsync(window, cell, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void BuildPrivateShop(string name, System.Collections.Generic.IReadOnlyList<Metin2ProtocolCodec.PrivateShopItem> items) { try { if (gameSession != null) await gameSession.SendPrivateShopAsync(name, items, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private void HandleStorage(Metin2GamePacketCodec.StorageMessage message)
        {
            if (gameUi == null) return;
            string screen = message.Window == 3 ? "safeboxwindow" : "mallwindow";
            Metin2StoragePanel panel = (message.Header == 88 || message.Header == 122)
                ? gameUi.Open(screen)?.GetComponent<Metin2StoragePanel>()
                : gameUi.GetPanel<Metin2StoragePanel>(screen);
            if (message.Header == 88 || message.Header == 122) gameUi.Close("passworddialog");
            panel?.Apply(message);
        }
        private void StorageInfo(string text)=>PopupMessage(text);
        private void CloseStorageFromServer(string screen)
        {
            gameUi?.GetPanel<Metin2StoragePanel>(screen)?.ServerClose();
            gameUi?.Close(screen);gameUi?.Close("changepassworddialog");
        }
        private void CloseStorageRequested(Metin2StoragePanel storage)
        {
            SendChat(storage.Window==3?"/safebox_close":"/mall_close");
            gameUi?.Close(storage.Window==3?"safeboxwindow":"mallwindow");
            gameUi?.Close("changepassworddialog");
        }
        private void OpenStorageChangePassword()
        {
            var window=gameUi?.Open("changepassworddialog");if(window==null)return;
            var old=FindNamed<InputField>(window.transform,"old_password_value");
            var next=FindNamed<InputField>(window.transform,"new_password_value");
            var confirm=FindNamed<InputField>(window.transform,"new_password_check_value");
            var accept=FindNamed<Button>(window.transform,"accept_button");var cancel=FindNamed<Button>(window.transform,"cancel_button");
            if(old==null||next==null||confirm==null||accept==null||cancel==null){Debug.LogError("METIN2_SAFEBOX_CHANGE_PASSWORD_BIND_FAILED");gameUi.Close("changepassworddialog");return;}
            Action close=()=>{old.text=next.text=confirm.text=string.Empty;gameUi.Close("changepassworddialog");};
            Action submit=()=>
            {
                if(next.text!=confirm.text){PopupMessageLocale("SAFEBOX_WRONG_PASSWORD");next.text=confirm.text=string.Empty;next.Select();next.ActivateInputField();return;}
                bool Valid(string value)=>!string.IsNullOrEmpty(value)&&value.Length<=6&&!System.Linq.Enumerable.Any(value,char.IsWhiteSpace);
                if(!Valid(old.text)||!Valid(next.text))return;
                SendChat("/safebox_change_password "+old.text+" "+next.text);close();
            };
            foreach(var input in new[]{old,next,confirm}){input.contentType=InputField.ContentType.Password;input.characterLimit=6;input.text=string.Empty;input.onSubmit.RemoveAllListeners();}
            storagePasswordFields=new[]{old,next,confirm};
            old.onSubmit.AddListener(_=>{next.Select();next.ActivateInputField();});
            next.onSubmit.AddListener(_=>{confirm.Select();confirm.ActivateInputField();});
            confirm.onSubmit.AddListener(_=>submit());
            accept.onClick.RemoveAllListeners();accept.onClick.AddListener(()=>submit());
            cancel.onClick.RemoveAllListeners();cancel.onClick.AddListener(()=>close());
            old.Select();old.ActivateInputField();
        }
        private void OpenSafeboxPasswordDialog(bool mall)
        {
            if (gameUi == null) return;
            GameObject window = gameUi.Open("passworddialog");
            if (window == null) return;
            InputField input = FindNamed<InputField>(window.transform, "password_value");
            Text title = FindNamed<Text>(window.transform, "TitleName");
            Button accept = FindNamed<Button>(window.transform, "accept_button");
            Button cancel = FindNamed<Button>(window.transform, "cancel_button");
            if (input == null || accept == null || cancel == null)
            {
                Debug.LogError("METIN2_SAFEBOX_PASSWORD_DIALOG_BIND_FAILED");
                gameUi.Close("passworddialog"); return;
            }
            title.text = Metin2Language.Get(mall ? "MALL_PASSWORD_TITLE" : "PASSWORD_TITLE");
            input.contentType = InputField.ContentType.Password;
            input.characterLimit = 6; input.text = string.Empty; input.ForceLabelUpdate();
            Action submit = () =>
            {
                string password = input.text ?? string.Empty;
                if (password.Length == 0 || password.Length > 6) return;
                gameUi.Close("passworddialog");
                SendChat((mall ? "/mall_password " : "/safebox_password ") + password);
            };
            accept.onClick.RemoveAllListeners(); accept.onClick.AddListener(() => submit());
            cancel.onClick.RemoveAllListeners(); cancel.onClick.AddListener(() => gameUi.Close("passworddialog"));
            input.onSubmit.RemoveAllListeners(); input.onSubmit.AddListener(_ => submit());
            input.Select(); input.ActivateInputField(); window.transform.SetAsLastSibling();
            Debug.Log("METIN2_SAFEBOX_PASSWORD_DIALOG_OPEN kind=" + (mall ? "mall" : "safebox"));
        }
        private async void CheckinSafebox(uint position, byte sourceWindow, ushort sourceCell) { try { PlayItemSound(sourceWindow, sourceCell, false); if (gameSession != null) await gameSession.SendSafeboxCheckinAsync(position, sourceWindow, sourceCell, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void CheckoutStorage(byte storageWindow, uint position, ushort targetCell) { try { PlayStorageItemDropSound(storageWindow, (ushort)position); if (gameSession == null) return; if (storageWindow == 3) await gameSession.SendSafeboxCheckoutAsync(position, 1, targetCell, lifetime.Token); else await gameSession.SendMallCheckoutAsync(position, 1, targetCell, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private async void MoveSafebox(ushort from, ushort to) { try { PlayStorageItemDropSound(3, from); if (gameSession != null) await gameSession.SendSafeboxMoveAsync(from, to, lifetime.Token); } catch (Exception e) { ShowOperationFailure(e); } }
        private void HandleDigMotion(Metin2GamePacketCodec.DigMotion message)
        {
            if (entityWorld != null && entityWorld.ApplyDigMotion(message.Vid, message.TargetVid, message.Count)) return;
            // The local player is deliberately absent from the remote entity registry.
            if (entityWorld != null && message.Vid == entityWorld.LocalVid && localController != null)
            {
                Transform target = entityWorld.TryGetActor(message.TargetVid, out Transform actor) ? actor : null;
                if (target != null)
                {
                    Vector3 direction = target.position - localController.transform.position; direction.y = 0f;
                    if (direction.sqrMagnitude > .0004f) localController.transform.rotation = Quaternion.LookRotation(direction, Vector3.up);
                }
                localController.GetComponentInChildren<Metin2CharacterMotionSet>(true)?.PlayNamedRepeated("dig", message.Count);
            }
        }
        private void HandleFishingMotion(Metin2GamePacketCodec.FishingMessage message)
        {
            // FISH carries an item id instead of an actor VID. The other subheaders animate the
            // actor identified by info, including the locally controlled actor absent from entities.
            if (message.SubHeader == 5 || entityWorld == null) return;
            if (message.SubHeader == 0)
                Debug.Log($"METIN2_FISHING_START vid={message.Info}");
            else if (message.SubHeader == 2)
                Debug.Log($"METIN2_FISHING_REACT_RECEIVED vid={message.Info}");
            if (message.Info == entityWorld.LocalVid)
            {
                Metin2CharacterMotionSet motion = localController?.GetComponentInChildren<Metin2CharacterMotionSet>(true);
                if (message.SubHeader == 2 && motion != null && motion.FishingCastActive)
                    Debug.Log($"METIN2_FISHING_REACT vid={message.Info} icon=" +
                              Emoticons.Play(localController.transform, Metin2EmoticonEffects.Fish));
                motion?.ApplyFishingEvent(message.SubHeader);
            }
            else if (entityWorld.TryGetEntity(message.Info, out Metin2NetworkEntity actor))
            {
                if (message.SubHeader == 2 && actor.FishingCastActive)
                    Debug.Log($"METIN2_FISHING_REACT vid={message.Info} icon=" +
                              Emoticons.Play(actor.transform, Metin2EmoticonEffects.Fish));
                actor.ApplyFishing(message.SubHeader, message.Direction);
            }
        }
        private void HandleSkillGroupChanged(byte group)
        {
            if(group==1||group==2)gameUi?.GetPanel<Metin2DoctrineChoosePanel>("doctrinechoosewindow")?.gameObject.SetActive(false);
            // CPythonNetworkStream::RecvChangeSkillGroupPacket clears skill data and refreshes the
            // character window. Our authoritative skill packets repopulate the state, while the UI
            // must immediately resolve its skill-table group against the new server value.
            mainCharacterSkillGroup = group < 1 ? 1 : group;
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.BindSkills(skillCatalog, mainCharacterJob, mainCharacterSkillGroup, null, null, null, localEmpire);
        }
        private void OpenGuildWindow()=>gameUi?.Open("guildwindow");
        private async void PostGuildComment(string text){try{if(gameSession==null)throw new InvalidOperationException("A játékszerver-kapcsolat nem aktív.");Debug.Log($"METIN2_GUILD_COMMENT_SEND_START chars={text?.Length??0}");await gameSession.SendGuildPostCommentAsync(text,lifetime.Token);Debug.Log("METIN2_GUILD_COMMENT_SEND_OK");}catch(OperationCanceledException){}catch(Exception e){Debug.LogWarning("METIN2_GUILD_COMMENT_SEND_FAILED "+e);ShowOperationFailure(e);}}
        private async void DeleteGuildComment(uint id){try{if(gameSession!=null)await gameSession.SendGuildDeleteCommentAsync(id,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void RefreshGuildComments(){try{if(gameSession!=null)await gameSession.SendGuildRefreshCommentsAsync(lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void OfferGuildExperience(uint value)
        {
            try
            {
                if(gameSession==null)return;
                // The wire field is uint32 but the server consumes each amount as int.
                // Two safe, sequential offers can still invest the full uint32 EXP pool.
                while(value>=100)
                {
                    uint chunk=Math.Min(value,2147483600u);
                    await gameSession.SendGuildOfferAsync(chunk,lifetime.Token);value-=chunk;
                }
            }
            catch(Exception e){ShowOperationFailure(e);}
        }
        private async void ChangeGuildMemberGrade(uint pid,byte grade){try{if(gameSession!=null)await gameSession.SendGuildChangeMemberGradeAsync(pid,grade,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void ChangeGuildMemberGeneral(uint pid,bool general){try{if(gameSession!=null)await gameSession.SendGuildChangeMemberGeneralAsync(pid,general,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void ChangeGuildGradeName(byte grade,string name){try{if(gameSession!=null)await gameSession.SendGuildChangeGradeNameAsync(grade,name,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void ChangeGuildGradeAuthority(byte grade,byte authority){try{if(gameSession!=null)await gameSession.SendGuildChangeGradeAuthorityAsync(grade,authority,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void ChargeGuildPower(uint amount){try{if(gameSession!=null)await gameSession.SendGuildChargePowerAsync(amount,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        private async void UseGuildSkill(uint vnum,uint target){try{if(gameSession!=null)await gameSession.SendGuildUseSkillAsync(vnum,target,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}
        /// <summary>
        /// CGuildMarkDownloader::Connect - the mark server is the same address as the game core on
        /// The connection profile's MarkPort. Fired once per enter-world; a failure is logged
        /// and never blocks the session.
        /// </summary>
        private void DownloadGuildMarks()
        {
            // The mark server needs the GC_LOGIN_SUCCESS handle/random key (PythonNetworkStream.cpp:
            // m_kMarkAuth, filled in PythonNetworkStreamPhaseLogin.cpp:177). Without them the
            // download used to be skipped in silence, so a missing emblem looked identical to a
            // failed transfer.
            if(currentCharacters.Handle==0||currentCharacters.RandomKey==0)
            {
                Debug.LogWarning($"METIN2_GUILD_MARK_DOWNLOAD_SKIPPED handle={currentCharacters.Handle} randomKey={currentCharacters.RandomKey} - GC_LOGIN_SUCCESS did not carry the mark auth");
                return;
            }
            // CGuildMarkManager is session-scoped in the original client. Warps keep the loaded
            // atlas; an explicit upload invokes the downloader directly to refresh it.
            if(Metin2GuildMarkStore.HasSnapshot)
            {
                Debug.Log("METIN2_GUILD_MARK_DOWNLOAD_SKIPPED snapshot-ready");
                return;
            }
            Debug.Log($"METIN2_GUILD_MARK_DOWNLOAD_START host={serverHost} port={MarkServerPort} handle={currentCharacters.Handle}");
            StartCoroutine(Metin2GuildMarkDownloader.Run(serverHost,MarkServerPort,currentCharacters.Handle,currentCharacters.RandomKey,BeginGuildMarkTransfer(),null));
        }
        /// <summary>The profile's shared guild mark/symbol port.</summary>
        private int MarkServerPort => connectionProfile.MarkPort;
        private void UploadGuildImage(bool symbol,string path)
        {
            uint guildId=gameUi?.GetPanel<Metin2GuildPanel>("guildwindow")?.GuildId??0;
            Debug.Log($"METIN2_GUILD_MARK_UPLOAD_START symbol={symbol} file={path} host={serverHost} port={MarkServerPort} handle={currentCharacters.Handle} randomKey={currentCharacters.RandomKey} guild={guildId}");
            StartCoroutine(Metin2GuildImagePicker.SelectAndUpload(symbol,path,serverHost,MarkServerPort,currentCharacters.Handle,currentCharacters.RandomKey,guildId,BeginGuildMarkTransfer(),SetStatus));
        }

        private CancellationToken BeginGuildMarkTransfer()
        {
            StopGuildMarkTransfer();
            guildMarkTransferLifetime = lifetime != null
                ? CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token)
                : new CancellationTokenSource();
            return guildMarkTransferLifetime.Token;
        }

        private void StopGuildMarkTransfer()
        {
            if(guildMarkTransferLifetime==null)return;
            guildMarkTransferLifetime.Cancel();
            guildMarkTransferLifetime.Dispose();
            guildMarkTransferLifetime=null;
        }

        private void OpenWhisperForTarget(string target) => GetWhisperDialog(target);
#if UNITY_STANDALONE_WIN
        private async void SendGmWhisper(string text,string language,int color)
        {
            try{if(gameSession!=null)await gameSession.SendWhisperAdminAsync(text,language,color,lifetime.Token);}
            catch(Exception error){ShowOperationFailure(error);}
        }
#endif
        private async void AddMessengerFriend(string name)
        { try { if (gameSession != null) await gameSession.SendMessengerAddByNameAsync(name, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void RemoveMessengerFriend(string name)
        {
            try { if (gameSession != null) await gameSession.SendMessengerRemoveAsync(name, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void RemoveMessengerGuildMember(uint pid)
        { try { if (gameSession != null) await gameSession.SendGuildRemoveMemberAsync(pid, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }

        /// <summary>
        /// game.py __PressNumKey / __PressQuickSlot: 1..9 map to slots 0..8, F1..F4 to 4..7.
        /// The original routes the key through RequestUseLocalQuickSlot, so what the slot holds
        /// decides whether an item, a skill or an emotion fires. The programmatic hotbar stays as a
        /// fallback for when the original task bar has not been generated.
        /// </summary>
        /// <summary>
        /// uicharacter.py __SelectQuest -> event.QuestButtonClick -> SendScriptButtonPacket.
        /// The caller already set the high bit, which marks the request as "open this quest".
        /// </summary>
        private async void OpenQuestDialog(uint questIndex)
        {
            try { if (gameSession != null) await gameSession.SendScriptButtonAsync(questIndex, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// Routes a quest script to the original dialog window instead of the placeholder panel.
        /// uiquest.py shows the thinboard, the stripped text and one BarButton per [NEXT] marker.
        /// </summary>
        private void ShowQuestDialog(Metin2GamePacketCodec.QuestScript script)
        {
            // Parsed and the scrolls applied BEFORE any window is touched. CPythonEventManager
            // handles EVENT_TYPE_QUEST_BUTTON / _CLOSE for every script it runs
            // (PythonEventManager.cpp:570-601), with no dialog involved - the scrolls are a
            // side-channel that happens to ride in the same packet. Doing it after gameUi.Open, as
            // this used to, put the call behind three early returns: no gameUi, a [SELECT_ITEM]
            // script, and a missing questdialog component. A script that carried only scrolls hit
            // one of those every time, which is why the main screen stayed empty.
            Metin2QuestScript parsed = Metin2QuestScript.Parse(script.Source);
            if (parsed.QuestButtonEntries.Count > 0 || parsed.QuestButtonCloses.Count > 0)
                Debug.Log($"METIN2_QUEST_SCRIPT buttons={parsed.QuestButtonEntries.Count} " +
                          $"closes={parsed.QuestButtonCloses.Count}");
            QuestButtons.Apply(parsed);

            if (gameUi == null) return;
            // GetDungeonInfo INPUT#1 is a server-driven, hidden quest-input handshake.
            // The command is handled by DungeonState, not by a visible text-input dialog.
            if (dungeonInfoState != null && dungeonInfoState.ShouldHideInputDialog(parsed))
            { gameUi.Close("questdialog"); return; }
            if (script.Source.IndexOf("[SELECT_ITEM]", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                GameObject selectWindow=gameUi.Open("selectitemwindow");Metin2ScriptSelectItemPanel select=selectWindow==null?null:selectWindow.GetComponent<Metin2ScriptSelectItemPanel>();
                if(select!=null){select.Bind(playerState,itemCatalog);select.BindToolTips(EnsureToolTips());select.SelectionRequested-=AnswerScriptSelectItem;select.SelectionRequested+=AnswerScriptSelectItem;select.ShowCandidates();}return;
            }
            // An empty script is a close request, and never a reason to open the window first.
            if (string.IsNullOrWhiteSpace(parsed.Text) && parsed.Options.Count == 0 &&
                !parsed.HasInput && !parsed.HasLongInput && !parsed.HasInputAddCancel)
            { gameUi.Close("questdialog"); return; }

            GameObject window = gameUi.Open("questdialog");
            Metin2QuestDialogPanel dialog = window == null ? null : window.GetComponent<Metin2QuestDialogPanel>();
            if (dialog == null) { Debug.LogWarning("METIN2_QUEST_DIALOG panel=missing name=questdialog"); return; }

            // interfacemodule.py OpenQuestWindow hides the other windows behind the locked dialog
            // and restores exactly those on close.
            // A continued page updates the already locked window. Preserve the first page's list:
            // replacing it with the now-empty result would make the final close restore nothing.
            if (!Metin2QuestDialogPanel.IsDialogLocked)
                hiddenByQuest = gameUi.HideOpenWindows("questdialog");
            dialog.Closed -= RestoreWindowsAfterQuest; dialog.Closed += RestoreWindowsAfterQuest;
            dialog.InputSubmitted -= SendDungeonQuestInput;
            dialog.InputSubmitted += SendDungeonQuestInput;
            dialog.Show(parsed);
        }

        /// <summary>Windows hidden while a quest dialog is locked; restored verbatim afterwards.</summary>
        private System.Collections.Generic.List<string> hiddenByQuest;

        private void RestoreWindowsAfterQuest()
        {
            gameUi?.RestoreWindows(hiddenByQuest);
            hiddenByQuest = null;
        }
        private async void AnswerScriptSelectItem(uint selection){try{if(gameSession!=null)await gameSession.SendScriptSelectItemAsync(selection,lifetime.Token);}catch(Exception e){ShowOperationFailure(e);}}

        /// <summary>
        /// GC_QUICKSLOT_ADD. The packet's own "pos" is the global slot (0..31); the payload carries
        /// the registration type and its target. For a skill the target is a *skill slot*, not a
        /// vnum, so it is resolved through the same table the character window uses - storing the
        /// raw slot would cast the wrong skill.
        /// </summary>
        private void ApplyQuickSlot(Metin2GamePacketCodec.QuickSlotAdd add)
        {
            var type = (Metin2QuickSlotBar.SlotType)add.Type;
            int target = add.Target;
            if (type == Metin2QuickSlotBar.SlotType.Skill && skillCatalog != null)
            {
                // Language slots 107/108 have no static table binding (see Metin2SkillCatalog);
                // without the empire branch they would resolve to 0 and keep the raw slot number,
                // which collides with real vnums (e.g. 107 is a shaman skill).
                if ((add.Target == Metin2SkillCatalog.LanguageSlotFirst || add.Target == Metin2SkillCatalog.LanguageSlotSecond) &&
                    Metin2SkillCatalog.TryGetLanguageSkill(localEmpire, add.Target, out uint language) && language != 0)
                {
                    target = (int)language;
                }
                else
                {
                    string group = add.Target >= Metin2CharacterWindowPanel.SupportSlotStart
                        ? "SUPPORT"
                        : mainCharacterSkillGroup.ToString(System.Globalization.CultureInfo.InvariantCulture);
                    uint vnum = skillCatalog.GetSkillAt(mainCharacterJob, group, add.Target);
                    if (vnum != 0) target = (int)vnum;
                }
            }
            // Written to the buffer first (see quickSlotState) so the registration survives even if
            // this arrives - as the login replay usually does - before the bar exists.
            if (add.Position < quickSlotState.Length)
                quickSlotState[add.Position] = new Metin2QuickSlotBar.Registration(type, target);
            gameUi?.TaskBarPanel?.QuickSlots?.RegisterGlobal(add.Position, type, target);
        }

        /// <summary>The MBT_LEFT / MBT_RIGHT indices the fly-out reports are the ones SetMouseFunction takes.</summary>
        private void ApplyMouseFunction(int button, Metin2InputController.MouseFunction function)
            => inputController?.SetMouseFunction(button, function);

        private void ClearQuickSlot(byte position)
        {
            if (position < quickSlotState.Length) quickSlotState[position] = default;
            gameUi?.TaskBarPanel?.QuickSlots?.ClearGlobal(position);
        }

        private void SwapQuickSlot(byte from, byte to)
        {
            if (from < quickSlotState.Length && to < quickSlotState.Length)
                (quickSlotState[from], quickSlotState[to]) = (quickSlotState[to], quickSlotState[from]);
            gameUi?.TaskBarPanel?.QuickSlots?.SwapGlobal(from, to);
        }

        private void UseSkillVnum(uint vnum) => combatController?.UseSkill(vnum);

        /// <summary>
        /// uicharacter.py OnPressedSlotButton: levelling a skill is a chat command, not a packet.
        /// </summary>
        private void LevelUpSkill(uint vnum) => SendChat("/skillup " + vnum.ToString(System.Globalization.CultureInfo.InvariantCulture));

        /// <summary>
        /// CPythonPlayer::RequestAddToEmptyLocalQuickSlot (:789): walk the current page for the first
        /// free slot and send CG_QUICKSLOT_ADD for it. The argument is the source *skill slot*, which
        /// is what the packet carries - the server echoes GC_QUICKSLOT_ADD and ApplyQuickSlot turns
        /// it back into a vnum for display.
        ///
        /// This used to register client-side only, so a Ctrl-click survived until the next login and
        /// then vanished. Registering locally as well would double up against that echo.
        /// </summary>
        private void RegisterSkillQuickSlot(int sourceSlot)
            => RegisterInFirstFreeQuickSlot(Metin2QuickSlotBar.SlotType.Skill, sourceSlot);

        /// <summary>uicharacter.py:316 - Ctrl on an emotion registers its index the same way.</summary>
        private void RegisterEmotionQuickSlot(int emotionIndex)
            => RegisterInFirstFreeQuickSlot(Metin2QuickSlotBar.SlotType.Emotion, emotionIndex);

        private void RegisterInFirstFreeQuickSlot(Metin2QuickSlotBar.SlotType type, int target)
        {
            Metin2QuickSlotBar bar = gameUi?.TaskBarPanel?.QuickSlots;
            if (bar == null) return;
            for (int slot = 0; slot < Metin2QuickSlotBar.SlotsPerPage; slot++)
            {
                if (!bar.Get(slot).IsEmpty) continue;
                AddQuickSlotRegistration(slot, type, target);
                return;
            }
            SetStatusLocale("UNITY_NO_FREE_QUICKSLOT");
        }

        /// <summary>
        /// A quick slot holding a skill registration stores the skill vnum as its position, which is
        /// what the cast path needs - the character window has no hotbar slot index to offer.
        /// </summary>
        private void UseHotbarSkill(int skillVnum) => UseSkillVnum((uint)skillVnum);
        private void UseHotbarEmotion(int emotionIndex)
        {
            GameObject window = gameUi?.EnsureWindow("characterwindow");
            window?.GetComponent<Metin2CharacterWindowPanel>()?.ActEmotion(emotionIndex);
        }

        private void UseQuickSlot(int slot)
        {
            Metin2QuickSlotBar bar = gameUi?.TaskBarPanel?.QuickSlots;
            if (bar != null && !bar.Get(slot).IsEmpty)
            {
                if (Metin2.Runtime.CombatProof.Enabled)
                {
                    Metin2QuickSlotBar.Registration registration = bar.Get(slot);
                    Debug.Log($"METIN2_COMBAT_PROOF_QUICKSLOT slot={slot + 1} type={registration.Type} position={registration.Position}");
                }
                bar.Use(slot); return;
            }
        }

        /// <summary>Keyboard window toggles, mapped to the generated uiscript windows.</summary>
        private bool pvpEquipmentBlocked;
        private bool antiExpBlocked;
        private void RequestChannelChange(int channel)
        {
            if(gameSession==null||gameTransitionInProgress||!CanActMainInstance()||
                !Metin2MiniMapPanel.TryGetChannelCommand(channel,out string command))return;
            // The server validates combat/cooldown/map restrictions and supplies GC_WARP.
            // Never reconnect speculatively or change the displayed channel on button press.
            SendChat(command);
        }
        private void ToggleGameWindow(string window)
        {
            if(window=="CHANNEL_CHANGE")
            {
                var minimap=gameUi?.GetPanel<Metin2MiniMapPanel>("minimap");
                minimap?.ToggleChannelChooser(mobileUiRoot!=null?mobileUiRoot.ControlsRoot:platformUiHost);return;
            }
            if(window!=null&&window.StartsWith("CHANNEL_",StringComparison.Ordinal)&&int.TryParse(window.Substring(8),out int channel))
            {RequestChannelChange(channel);return;}
            if (window == "ANTI_EXP") { SendChat("/manage_exp"); return; }
            if (gameUi == null) return;
            switch (window)
            {
                case "LOOT_MEASUREMENT": gameUi.Toggle("lootmeasurement"); break;
                // game.py binds C / V / N to the same character window but to different tabs, so the
                // key also selects the page instead of only toggling visibility.
                case "STATUS": ToggleCharacterWindow(0); break;
                case "SKILL": ToggleCharacterWindow(1); break;
                case "QUEST": ToggleCharacterWindow(3); break;
                case "INVENTORY": gameUi.Toggle("inventorywindow"); break;
                case "MAP": gameUi.Toggle("minimap"); break;
                // interfacemodule.py PressMKey: plain M toggles the atlas window, Shift+M the minimap.
                case "ATLASWINDOW": gameUi.Toggle("atlaswindow"); break;
                case "MINIMAP": gameUi.Toggle("minimap"); break;
                // uichat.py ChatLogWindow has no uiscript - the chat window owns its runtime-built log.
                case "CHATLOG": ToggleChatLog(); break;
                case "MESSENGER": gameUi.Toggle("messengerwindow"); break;
                case "EMOTICON": ToggleCharacterWindow(2); break;
                case "EVENT_CALENDAR": EnsureCustomSystems().ToggleEventManager(); break;
                // uiinventory.py:504-505 OpenBattlepass -> net.SendBattlePassAction(1). The window is
                // opened by the server's GC_BATTLE_PASS_OPEN answer, never by the button itself.
                case "BATTLE_PASS": EnsureCustomSystems().RequestBattlePass(); break;
                case "WHISPER_SYSTEM": SendChat("/open_whispersys"); break;
                case "PVP_EQUIPMENT":
                    pvpEquipmentBlocked = !pvpEquipmentBlocked;
                    SendChat("/pvp_block_equipment " + (pvpEquipmentBlocked ? "BLOCK" : "UNBLOCK"));
                    break;
                case "SAVEPOINT": RequestOpenSavepoint(); break;
                // uicommon/interfacemodule OpenShoutWindow: the staged shout window is the exact
                // layout; its /shout actions arrive with the chat-command pass.
                case "SHOUT":
                case "AUTO_SHOUT": gameUi.Toggle("shout"); break;
                // uiinventory.py:615 ToggleBeltInventory. The imported beltinventorywindow is the
                // window the sidebar's Belt icon targets; the desktop/mobile registry uses the same
                // token so all three entry points share one action.
                case "BELT_INVENTORY": ToggleBeltInventory(); break;
                // uiinventory.py:628 OpenDailyGift -> dailygift.py Open(): showing the window asks
                // the server for the current reward state with /daily_reward_reload.
                case "DAILY_GIFT": OpenDailyGift(); break;
                // uidragonsoul.py DSSButton and uirune.py RuneButton: the staged socket windows show
                // the live DS / rune cells from the shared player state.
                case "DRAGON_SOUL": gameUi.Toggle("dragonsoulwindow"); break;
                case "RUNE": ToggleRuneWindow(); break;
                // interfacemodule.py:2897 ToggleExtraInventoryWindow - the U key, the sidebar
                // ExtendIntentory and the mobile hub all toggle the same window.
                case "EXTRA_INVENTORY": ToggleExtraInventory(); break;
                case "OFFLINE_SHOP": OpenOfflineShopFromInventory(); break;
                case "SWITCHBOT": EnsureCustomSystems().ToggleSwitchbot(); break;
                // game.py:3621 ToggleWikiWindow: the staged wikiwindow opens with the catalog-driven
                // panel (the C++ content module and its art are not shipped).
                case "WIKI": gameUi.Toggle("wikiwindow"); break;
                case "MOUNT_TOGGLE": ToggleMount(); break;
                case "HORSE_BACK": SendChat("/user_horse_back"); break;
                case "HORSE_FEED": SendChat("/user_horse_feed"); break;
                case "GUILD": gameUi.Toggle("guildwindow"); break;
                case "PARTY": EnsurePartyPanel()?.Refresh(); break;
                case "PET": OpenPet(); break;
                case "MOUNT_INVENTORY": ToggleMountInventory(); break;
                case "BIOLOG": OpenCustomSystem(2); break;
                case "DUNGEON": gameUi.Toggle("dungeoninfowindow"); break;
                case "RANKING": EnsureCustomSystems().ToggleRanking(); break;
                case "FISHING": OpenFeaturePlaceholder("UNITY_FISHING", "FISHING"); break;
                case "ACCE": gameUi.Toggle("acce_combinewindow"); break;
                case "CUBE": OpenCustomSystem(1); break;
                case "EVENT_MANAGER": EnsureCustomSystems().ToggleEventManager(); break;
                case "SETTINGS": HandleEscape(); break;
            }
        }

        /// <summary>uirune.py Open: the server-owned status gates the window.</summary>
        private void ToggleRuneWindow()
        {
            if (gameUi == null) return;
            if (gameUi.IsOpen("runewindow")) { gameUi.Close("runewindow"); return; }
            if (runeStatus == 2)
            {
                chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, Metin2Language.Get("RUNE_IS_UNLOCKED1"));
                chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, Metin2Language.Get("RUNE_IS_UNLOCKED2"));
                gameUi.Close("runewindow");
                return;
            }
            if (runeStatus != 1)
            {
                chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, Metin2Language.Get("RUNE_LOCKED"));
                gameUi.Close("runewindow");
                return;
            }
            GameObject window = gameUi.Open("runewindow");
            Metin2RunePanel panel = window != null ? window.GetComponent<Metin2RunePanel>() : null;
            panel?.Refresh();
        }

        private string RuneItemName(byte window, ushort cell)
        {
            if (playerState != null && playerState.TryGetItem(window, cell, out Metin2GamePacketCodec.ItemData item) &&
                itemCatalog != null && itemCatalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) &&
                !string.IsNullOrEmpty(entry.name)) return entry.name;
            return string.Empty;
        }

        /// <summary>uirune.py OnQuestionDialog, including its item-name substitutions.</summary>
        private void ShowRuneConfirmation(string command, string localeKey)
        {
            if (gameUi == null || string.IsNullOrEmpty(command)) return;
            string text = Metin2Language.Get(localeKey);
            string[] tokens = command.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
            if (tokens.Length == 3 && (tokens[1] == "a" || tokens[1] == "d") &&
                int.TryParse(tokens[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out int wearIndex))
                text = Metin2Language.Format(localeKey,
                    RuneItemName(1, (ushort)(Metin2PlayerState.EquipmentStartCell + wearIndex)));
            else if (tokens.Length == 3 && tokens[0] == "/rune_charge" &&
                int.TryParse(tokens[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int chargeWear) &&
                ushort.TryParse(tokens[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out ushort bottleCell))
                text = Metin2Language.Format(localeKey,
                    RuneItemName(1, (ushort)(Metin2PlayerState.EquipmentStartCell + chargeWear)),
                    RuneItemName(1, bottleCell));

            GameObject dialogObject = gameUi.Open("questiondialog");
            if (dialogObject == null) return;
            Metin2QuestionDialogPanel dialog = dialogObject.GetComponent<Metin2QuestionDialogPanel>() ??
                                                 dialogObject.AddComponent<Metin2QuestionDialogPanel>();
            dialog.Open(text, () => SendChat(command));
        }

        private void OpenFeaturePlaceholder(string localeKey, string feature)
        {
            SetStatusLocale(localeKey);
            Debug.LogWarning("METIN2_FEATURE_UI_PENDING feature=" + feature);
        }

        /// <summary>
        /// Opens the character window on a given tab. Pressing the same key again closes it, but a key
        /// for a different tab switches pages instead of closing, which is how the original behaves
        /// because C / V / N each call Show() plus SetState() rather than a plain toggle.
        /// </summary>
        private void ToggleCharacterWindow(int tab)
        {
            if (gameUi == null) return;
            Metin2CharacterWindowPanel panel = gameUi.GetPanel<Metin2CharacterWindowPanel>("characterwindow");
            if (gameUi.IsOpen("characterwindow") && (panel == null || panel.State == tab))
            {
                gameUi.Close("characterwindow");
                return;
            }
            gameUi.Open("characterwindow");
            gameUi.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.SetState(tab);
        }

        private async void SendFlight(byte action,int altitude,uint time)
        {
            try{if(gameSession!=null)await gameSession.SendMountFlightAsync(action,altitude,time,lifetime.Token);}
            catch(Exception exception){ShowOperationFailure(exception);}
        }
        private async void SendMove(Metin2PlayerController.MoveCommand command)
        {
            if (!CanActMainInstance()) return;
            try { if (gameSession != null) await gameSession.SendMoveAsync(command.Function, command.Argument, command.Rotation, command.X, command.Y, command.Time, command.Duration, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void SendPushSyncPosition(uint victimVid, Vector3 destination)
        {
            try
            {
                if(gameSession==null || victimVid==0)return;
                int x,y;
                if(worldRuntime!=null && worldRuntime.HasMap)worldRuntime.Current.ToGlobal(destination,out x,out y);
                else{x=Mathf.RoundToInt(destination.x*100f);y=Mathf.RoundToInt(destination.z*100f);}
                await gameSession.SendSyncPositionAsync(victimVid,x,y,lifetime.Token);
                if(Metin2.Runtime.CombatProof.Enabled)
                    Debug.Log($"METIN2_COMBAT_PROOF_PUSH_SYNC_SENT victim={victimVid} x={x} y={y}");
            }
            catch(Exception exception){ShowOperationFailure(exception);}
        }
        private async void SendTarget(uint vid)
        {
            try
            {
                if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_TARGET_PACKET vid={vid}");
                if (gameSession != null)
                {
                    await gameSession.SendTargetAsync(vid, lifetime.Token);
                    if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_TARGET_SENT vid={vid}");
                }
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private void SendCombatPresentation(byte function, byte argument)
        {
            damageTextTail?.MarkLocalCombatAction();
            localController?.EmitAction(function, argument);
        }
        private uint KnownMountPreviewRace()
        {
            if(entityWorld==null)return 0;
            if(entityWorld.LocalMountVnum!=0)return entityWorld.LocalMountVnum;
            if(localController==null||string.IsNullOrEmpty(mainCharacterName))return 0;
            uint race=0;
            foreach(var actor in entityWorld.Entities)
            {
                if(actor==null||actor.IsDead||actor.DisplayName!=mainCharacterName+"'s Mount"||
                    (actor.transform.position-localController.transform.position).sqrMagnitude>900f)continue;
                if(race!=0&&race!=actor.Race)return 0;
                race=actor.Race;
            }
            return race;
        }
        private uint KnownPetPreviewRace()
        {
            if(entityWorld==null||localController==null||string.IsNullOrEmpty(mainCharacterName))return 0;
            // PetName is sent by CNewPetActor::Summon from the same m_name as actor SetName.
            // Do not guess from nearby NPCs or display another player's differently named pet.
            uint race=0;
            foreach(var actor in entityWorld.Entities)
            {
                bool ownedName=actor!=null&&(actor.DisplayName==mainCharacterName+"'s Pet"||
                    (petState!=null&&petState.HasPet&&!string.IsNullOrEmpty(petState.Name)&&actor.DisplayName==petState.Name));
                if(actor==null||actor.IsDead||!ownedName||
                    (actor.transform.position-localController.transform.position).sqrMagnitude>900f)continue;
                if(race!=0&&race!=actor.Race)return 0;
                race=actor.Race;
            }
            return race;
        }

        private void InsertInventoryChatLink(string markup)
        {
            if (chatWindow == null) return;
            if (!Metin2ChatWindow.IsTyping) chatWindow.OpenChat();
            chatWindow.InsertItemLinkAtCaret(markup);
        }

        private readonly HashSet<uint> locallyHitMetins = new HashSet<uint>();
        private void RecordHitMetinDeath(uint vid)
        {
            if (locallyHitMetins.Remove(vid)) gameUi?.RecordMetinKill();
        }
        private void PlayLocalHit(Metin2CombatController.LocalHitPresentation hit)
        {
            if (entityWorld != null && entityWorld.TryGetEntity(hit.Vid, out var entity) && entity != null && entity.Type == 2)
            {
                locallyHitMetins.Add(hit.Vid);
                Metin2MetinSelectionGuide.ShowIfNeeded(ResolveUiHost(), mainCharacterName);
            }
            damageTextTail?.PlayLocalHit(hit);
        }
        private void ShowTargetIndicator(uint vid) { entityWorld?.ApplyTargetIndicator(vid); damageTextTail?.SetTarget(vid); }
        private async void SendAttack(byte type, uint vid) { if (!CanActMainInstance()) return; try { damageTextTail?.MarkLocalCombatAction(); if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_ATTACK_PACKET type={type} vid={vid}"); if (gameSession != null) { await gameSession.SendAttackAsync(type, vid, 0, 0, lifetime.Token); if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_ATTACK_SENT type={type} vid={vid}"); } } catch (Exception exception) { ShowOperationFailure(exception); } }
        private async void SendSkill(uint vnum, uint vid) { try { damageTextTail?.MarkLocalCombatAction(); if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_SKILL_PACKET vnum={vnum} vid={vid}"); if (gameSession != null) { await gameSession.SendUseSkillAsync(vnum, vid, lifetime.Token); if (Metin2.Runtime.CombatProof.Enabled) Debug.Log($"METIN2_COMBAT_PROOF_SKILL_SENT vnum={vnum} vid={vid}"); } } catch (Exception exception) { ShowOperationFailure(exception); } }
        /// <summary>
        /// CG_SHOOT. A bow's damage arrives through this and nothing else - the swing itself sends no
        /// CG_ATTACK (PythonPlayerEventHandler.cpp:241-245).
        /// </summary>
        private async void SendShoot(byte type)
        {
            try
            {
                damageTextTail?.MarkLocalCombatAction();
                if (gameSession != null) await gameSession.SendShootAsync(type, lifetime.Token);
                Debug.Log($"METIN2_COMBAT_SHOOT_SENT type={type}");
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// CG_FLY_TARGETING. The position is the TARGET's fly point in global map coordinates, the
        /// same conversion the movement packet uses (SendFlyTargetingPacket runs the position through
        /// __LocalPositionToGlobalPosition, PythonNetworkStreamPhaseGame.cpp:2752-2770).
        /// </summary>
        private async void SendFlyTargeting(uint targetVid)
        {
            try
            {
                if (gameSession == null || entityWorld == null) return;
                if (!entityWorld.TryResolveActor(targetVid, out Transform target) || target == null) return;
                Vector3 point = Metin2.Runtime.Metin2Units.ActorCentre(target);
                int x, y;
                if (worldRuntime != null && worldRuntime.HasMap) worldRuntime.Current.ToGlobal(point, out x, out y);
                else { x = Mathf.RoundToInt(point.x * 100f); y = Mathf.RoundToInt(point.z * 100f); }
                await gameSession.SendFlyTargetingAsync(targetVid, x, y, lifetime.Token);
                Debug.Log($"METIN2_COMBAT_FLY_TARGETING_SENT vid={targetVid} x={x} y={y}");
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void ShowSkillRefusal(string reason) => damageTextTail?.ShowSkillRefusal(reason);

        /// <summary>CHAT_TYPE_TALKING - the channel the original sends its slash commands on.</summary>
        private const byte ChatTypeTalking = 0;
        /// <summary>
        /// uiinventory.py RefineItem (:883-895). Re-read against the script rather than assumed: the
        /// body sends the use-item-to-item packet and then returns, so the CG_REFINE request and the
        /// refineDialog.Open call further down are both dead code behind that return. The scroll path
        /// therefore carries the SCROLL cell and the TARGET cell, and no window is expected to open -
        /// the server answers with the item update. Sending CG_REFINE(targetCell) here, as this used
        /// to, asked the blacksmith question instead and the scroll was never consumed.
        /// </summary>
        private async void OpenRefineForScroll(ushort scrollCell, ushort targetCell)
        {
            if (!CanActMainInstance()) return;
            try
            {
                Debug.Log($"METIN2_REFINE_SCROLL scroll={scrollCell} target={targetCell}");
                if (gameSession != null)
                    await gameSession.SendItemUseToItemAsync(1, scrollCell, 1, targetCell, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>Ctrl+G: __PressGKey sends /ride, which the server answers with GC_MOUNT.</summary>
        private async void ToggleMount()
        {
            try { if (gameSession != null) await gameSession.SendChatAsync(ChatTypeTalking, "/ride", lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>interfaceModule.IsQBHide - one flag for every quest scroll on the main screen.</summary>
        private bool questButtonsHidden;

        private void ToggleQuestButtons()
        {
            questButtonsHidden = !questButtonsHidden;
            QuestButtons.SetHidden(questButtonsHidden);
            QuestButtonsHiddenChanged?.Invoke(questButtonsHidden);
            Debug.Log($"METIN2_QUEST_BUTTONS hidden={(questButtonsHidden ? 1 : 0)}");
        }

        private Metin2QuestButtonPanel questButtonPanel;

        /// <summary>The main screen's quest scrolls, created on first use.</summary>
        private Metin2QuestButtonPanel QuestButtons
        {
            get
            {
                if (questButtonPanel != null)
                {
                    questButtonPanel.SetUiHost(ResolveUiHost());
                    EnsureMobileQuestButton();
                    return questButtonPanel;
                }
                questButtonPanel = GetComponent<Metin2QuestButtonPanel>() ?? gameObject.AddComponent<Metin2QuestButtonPanel>();
                questButtonPanel.SetUiHost(ResolveUiHost());
                questButtonPanel.QuestClicked -= SendQuestButton;
                questButtonPanel.QuestClicked += SendQuestButton;
                EnsureMobileQuestButton();
                return questButtonPanel;
            }
        }

        private Metin2.Runtime.Mobile.Metin2MobileQuestButton mobileQuestButton;

        /// <summary>
        /// Mobile collapses the desktop scroll column into one HUD quest button with a count
        /// badge; the panel keeps the data and the trigger path, only its per-quest views hide.
        /// </summary>
        private void EnsureMobileQuestButton()
        {
            if (mobileUiRoot == null || questButtonPanel == null) return;
            if (mobileQuestButton == null)
            {
                var host = new GameObject("MobileQuestButtonHost");
                mobileQuestButton = host.AddComponent<Metin2.Runtime.Mobile.Metin2MobileQuestButton>();
                mobileQuestButton.Configure(mobileUiRoot.ControlsRoot, mobileUiRoot.ContentRoot, questButtonPanel);
                mobileQuestButton.QuestLogRequested += () =>
                {
                    try { inputController?.RequestMobileWindow("QUEST"); }
                    catch (System.Exception) { }
                };
            }
            questButtonPanel.SetMobileCollapsed(true);
        }

        /// <summary>__StartQuest -> event.QuestButtonClick -> CG_SCRIPT_BUTTON.</summary>
        private async void SendQuestButton(int index)
        {
            try { if (gameSession != null) await gameSession.SendScriptButtonAsync((uint)index, lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>Raised when Ctrl+Q flips the quest-scroll visibility.</summary>
        public event Action<bool> QuestButtonsHiddenChanged;

        /// <summary>
        /// game.py:1613-1623, with the window stack asked first: a window that is open consumes the
        /// key, and only an escape that nothing consumed reaches the system dialog.
        /// </summary>
        private void HandleEscape()
        {
            Metin2GuildBuildDirector guildBuild = GetComponent<Metin2GuildBuildDirector>();
            if (guildBuild != null && guildBuild.HandleEscape()) return;
            if(gameUi?.GetPanel<Metin2MiniMapPanel>("minimap")?.CloseChannelChooser()==true)return;
            if (gameUi != null && gameUi.CloseTopWindow()) return;
            OpenSystemDialog();
        }

        /// <summary>
        /// uisystem.py's SystemDialog. The prefab was already reachable from the task bar and from
        /// escape, but nothing bound its buttons, so the menu opened and every entry was inert.
        /// </summary>
        private void OpenSystemDialog()
        {
            GameObject window = gameUi?.Open("systemdialog");
            if (window == null) return;
            Metin2SystemDialogPanel panel = window.GetComponent<Metin2SystemDialogPanel>() ??
                                            window.AddComponent<Metin2SystemDialogPanel>();
            panel.CommandRequested -= SendSystemCommand; panel.CommandRequested += SendSystemCommand;
            panel.ScreenRequested -= OpenSystemScreen; panel.ScreenRequested += OpenSystemScreen;
            panel.ItemShopRequested -= RequestOpenItemShop; panel.ItemShopRequested += RequestOpenItemShop;
            panel.CloseRequested -= CloseSystemDialog; panel.CloseRequested += CloseSystemDialog;
            panel.Bind();
        }

        /// <summary>The task-bar route: the window is already open, so only the binding is needed.</summary>
        private void BindSystemDialog() => OpenSystemDialog();

        /// <summary>
        /// Connects intent-only <see cref="Metin2GameUi"/> events to the live game session.
        ///
        /// UI creation is lazy and happens after the player controller on the normal enter-world
        /// path. Keeping this idempotent and calling it from both sides of that lifecycle boundary
        /// makes task-bar, keyboard and rebuilt-UI routes equivalent.
        /// </summary>
        private void ConnectGameUiSessionEvents()
        {
            if (gameUi == null) return;
            gameUi.SystemDialogOpened -= BindSystemDialog;
            gameUi.SystemDialogOpened += BindSystemDialog;
            gameUi.ItemShopRequested -= RequestOpenItemShop;
            gameUi.ItemShopRequested += RequestOpenItemShop;
            gameUi.InventoryFeatureRequested -= HandleInventoryFeature;
            gameUi.InventoryFeatureRequested += HandleInventoryFeature;
        }

        private readonly bool[] hiddenCostumeParts = new bool[5];
        private bool TryApplyCostumeHidden(int part, string[] tokens)
        {
            if (part < 1 || part > 4 || tokens == null || tokens.Length != 2 ||
                !int.TryParse(tokens[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int value)) return false;
            hiddenCostumeParts[part] = value != 0;
            gameUi?.SetCostumeHidden(part,hiddenCostumeParts[part]);
            localEquipmentBinder?.SetCostumeHidden(part,hiddenCostumeParts[part]);
            Debug.Log($"METIN2_COSTUME_HIDDEN part={part} hidden={(hiddenCostumeParts[part] ? 1 : 0)}");
            return true;
        }
        private void HandleInventoryFeature(string feature)
        {
            if (feature != null && feature.StartsWith("HIDE_COSTUME:", StringComparison.Ordinal) &&
                int.TryParse(feature.Substring(13), out int part) && part >= 1 && part <= 4)
            {
                hiddenCostumeParts[part] = !hiddenCostumeParts[part];
                gameUi?.SetCostumeHidden(part,hiddenCostumeParts[part]);
                SendChat("/hide_costume " + part + " " + (hiddenCostumeParts[part] ? 1 : 0));
                return;
            }
            switch (feature)
            {
                // uiinventory.py __InitLeftPanel (:748-854). The sidebar and the hotkey/mobile
                // registry must reach the same action. Keep every sidebar feature on the one
                // ToggleGameWindow registry instead of maintaining a second, drifting switch.
                // The five authored feature buttons (AnimalButton / ExtendIntentory / RankingButton
                // / BattlepassButton / SwitchbotButton) raise the same InventoryFeatureRequested
                // event as the runtime-created sidebar icons, so they must be listed here too -
                // they used to be dropped, which is why those buttons were dead.
                case "BELT_INVENTORY": case "EVENT_CALENDAR": case "WIKI": case "DUNGEON":
                case "BIOLOG": case "DAILY_GIFT": case "AUTO_SHOUT": case "SAVEPOINT":
                case "RUNE": case "DRAGON_SOUL": case "OFFLINE_SHOP":
                case "MOUNT_INVENTORY": case "EXTRA_INVENTORY": case "RANKING":
                case "BATTLE_PASS": case "SWITCHBOT":
                    ToggleGameWindow(feature); break;
            }
        }

        private void CloseSystemDialog() => gameUi?.Close("systemdialog");
        private void OpenSystemScreen(string screen) => gameUi?.Open(screen);

        private async void SendSystemCommand(string command)
        {
            try
            {
                Debug.Log($"METIN2_SYSTEM_COMMAND command={command}");
                if (gameSession != null) await gameSession.SendChatAsync(ChatTypeTalking, command, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2ShopPanel shopPanel;

        /// <summary>
        /// The NPC shop. GC_SHOP (38) was decoded and its event raised, but nothing subscribed and no
        /// window was ever created, so clicking a shopkeeper sent the click, got the shop back, and
        /// dropped it on the floor. Metin2ShopPanel builds its own controls and subscribes itself in
        /// Bind, then shows on SHOP_SUBHEADER_GC_START and hides on _END (Packet.h:2046-2047).
        /// </summary>
        private Metin2RefinePanel refinePanel;

        private void HandleNpcShopPacket(Metin2GamePacketCodec.ShopPacket packet)
        {
            if (shopPanel != null) return;
            EnsureShopPanel();
            // Event invocation uses the subscriber snapshot taken before EnsureShopPanel added its
            // own listener, so the first START packet must be delivered explicitly.
            shopPanel?.Display(packet);
        }

        private Metin2CharacterDetailsPanel characterDetailsPanel;

        /// <summary>
        /// uicharacter.py __ClickExpandButton / __ClickMinimizeButton: the arrow button opens the
        /// details sub-window next to the character window and the same button closes it. The layout
        /// already places CharacterDetailsWindow at x=274 (the main window sits at x=24 + 250).
        /// </summary>
        private void ToggleCharacterDetails()
        {
            if (characterDetailsPanel != null && characterDetailsPanel.gameObject.activeSelf)
            {
                characterDetailsPanel.gameObject.SetActive(false);
                SetCharacterDetailsArrow(false);
                return;
            }
            if (gameUi == null) return;
            GameObject window = gameUi.Open("characterdetailswindow");
            if (window == null) return;
            characterDetailsPanel = window.GetComponent<Metin2CharacterDetailsPanel>() ??
                                    window.AddComponent<Metin2CharacterDetailsPanel>();
            characterDetailsPanel.Bind(playerState);
            characterDetailsPanel.AttachTo(gameUi.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.BoardRect);
            SetCharacterDetailsArrow(true);
        }

        /// <summary>Hide(): closing the character window closes the details window with it.</summary>
        private void CloseCharacterDetails()
        {
            if (characterDetailsPanel != null && characterDetailsPanel.gameObject.activeSelf)
                characterDetailsPanel.gameObject.SetActive(false);
            SetCharacterDetailsArrow(false);
        }

        private void SetCharacterDetailsArrow(bool open) =>
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.SetDetailsOpen(open);

        private void EnsureShopPanel()
        {
            if (shopPanel != null) return;
            if (gameUi == null || gameRuntime == null)
            { Debug.LogWarning($"METIN2_SHOP_PANEL skipped gameUi={(gameUi != null)} runtime={(gameRuntime != null)}"); return; }
            // The imported shopdialog window, not a hand-drawn stand-in: it already has the forty
            // Slot_N cells, the Buy/Sell buttons and the frame. Open() instantiates it from the
            // catalog and the panel then binds those widgets; it starts hidden and GC_SHOP shows it.
            GameObject window = gameUi.Open("shopdialog");
            if (window == null) { Debug.LogWarning("METIN2_SHOP_PANEL window=missing name=shopdialog"); return; }
            shopPanel = window.GetComponent<Metin2ShopPanel>() ?? window.AddComponent<Metin2ShopPanel>();
            shopPanel.BuyRequested -= BuyFromShop; shopPanel.BuyRequested += BuyFromShop;
            shopPanel.StackBuyRequested -= StackBuyFromShop; shopPanel.StackBuyRequested += StackBuyFromShop;
            shopPanel.SellRequested -= SellFromShop; shopPanel.SellRequested += SellFromShop;
            shopPanel.InfoRequested -= ShowShopInfo; shopPanel.InfoRequested += ShowShopInfo;
            shopPanel.ErrorRequested -= ShowShopError; shopPanel.ErrorRequested += ShowShopError;
            shopPanel.StateClosed -= CloseShopModals; shopPanel.StateClosed += CloseShopModals;
            shopPanel.CloseRequested -= CloseShop; shopPanel.CloseRequested += CloseShop;
            Metin2InventoryPanel inventory = gameUi.GetPanel<Metin2InventoryPanel>("inventorywindow");
            if (inventory != null)
            {
                inventory.ShopSellRequested -= SellInventoryItemToShop; inventory.ShopSellRequested += SellInventoryItemToShop;
                inventory.ShopBuyDropRequested -= BuyFromShop; inventory.ShopBuyDropRequested += BuyFromShop;
                inventory.ShopInfoRequested -= ShowShopInfo; inventory.ShopInfoRequested += ShowShopInfo;
            }
            shopPanel.Bind(gameRuntime, itemCatalog, inventory, EnsureCursorAttachment(), originalCursor);
            shopPanel.BindPlayerState(playerState);
            shopPanel.BindPlayerTransform(() => entityWorld != null && entityWorld.LocalPlayerRoot != null
                ? entityWorld.LocalPlayerRoot
                : localController != null ? localController.transform : null);
            shopPanel.BindToolTips(EnsureToolTips());
            gameUi.Close("shopdialog");
        }

        /// <summary>
        /// The imported refinedialog window - Board / TitleBar / SuccessPercentage / Cost /
        /// AcceptButton / CancelButton, the same children RefineDialogNew.__LoadScript binds. Built
        /// here for the same reason the shop is: it needs gameUi, which does not exist at player
        /// spawn. It starts hidden and GC_REFINE_INFORMATION shows it.
        /// </summary>
        private void EnsureRefinePanel()
        {
            if (refinePanel != null)
            {
                // BuildOriginalGameUi clears this list on every warp, but keeps the imported
                // window. Re-register it or its phase-hidden CanvasGroup stays alpha=0 forever.
                if (!gameUiPanels.Contains(refinePanel.gameObject)) gameUiPanels.Add(refinePanel.gameObject);
                return;
            }
            if (gameUi == null || gameRuntime == null)
            { Debug.LogWarning($"METIN2_REFINE_PANEL skipped gameUi={(gameUi != null)} runtime={(gameRuntime != null)}"); return; }
            GameObject window = gameUi.Open("refinedialog");
            if (window == null) { Debug.LogWarning("METIN2_REFINE_PANEL window=missing name=refinedialog"); return; }
            refinePanel = window.GetComponent<Metin2RefinePanel>() ?? window.AddComponent<Metin2RefinePanel>();
            refinePanel.Opening -= ShowRefineWindow; refinePanel.Opening += ShowRefineWindow;
            refinePanel.ConfirmRequested -= SendRefine; refinePanel.ConfirmRequested += SendRefine;
            refinePanel.CancelRequested -= SendRefine; refinePanel.CancelRequested += SendRefine;
            // The ui catalog and the player state are what let the dialog build the real item tool
            // tip it sizes itself from - RefineDialogNew derives every dimension from that tool tip.
            refinePanel.QuestionLocaleRequested -= AskQuestion2Locale; refinePanel.QuestionLocaleRequested += AskQuestion2Locale;
            refinePanel.Bind(gameRuntime, itemCatalog, localeText, uiCatalog, playerState);
            if (!gameUiPanels.Contains(window)) gameUiPanels.Add(window);
            gameUi.Close("refinedialog");
        }

        private void ShowRefineWindow() => gameUi?.Open("refinedialog");

        /// <summary>
        /// uicommon.QuestionDialog2, opened on demand from the imported questiondialog2 window. The
        /// refine dialog is the first caller; anything else that needs a two-line yes/no box goes
        /// through here rather than building its own.
        /// </summary>
        private void AskQuestion2(string line1, string line2, Action onAccept,bool useLocale=false)
        {
            if (gameUi == null) { Debug.LogWarning("METIN2_QUESTION2 gameUi=missing"); return; }
            GameObject window = gameUi.Open("questiondialog2");
            if (window == null) { Debug.LogWarning("METIN2_QUESTION2 window=missing name=questiondialog2"); return; }
            // Sorting Canvases on doctrine/guild pickers outrank ordinary sibling ordering.
            var popupCanvas=window.GetComponent<Canvas>();
            if(popupCanvas==null)popupCanvas=window.AddComponent<Canvas>();
            popupCanvas.overrideSorting=true;popupCanvas.sortingOrder=30100;
            if(window.GetComponent<GraphicRaycaster>()==null)window.AddComponent<GraphicRaycaster>();
            window.transform.SetAsLastSibling();
            Metin2QuestionDialog2Panel panel = window.GetComponent<Metin2QuestionDialog2Panel>();
            if(panel==null)panel=window.AddComponent<Metin2QuestionDialog2Panel>();
            if(useLocale)panel.OpenLocale(line1,line2,onAccept);
            else panel.Open(line1,line2,onAccept);
            // Bind/Open applies the mobile front-order helper, so impose modal priority last.
            popupCanvas.sortingOrder=30100;
        }

        /// <summary>
        /// game.py PopupMessage (:754-756) on the imported popupdialog window. Shared, so anything
        /// that needs a one-line acknowledge box uses this rather than building its own.
        /// </summary>
        private void AskQuestion2Locale(string key1,string key2,Action onAccept)
            => AskQuestion2(key1,key2,onAccept,true);
        private void PopupMessageLocale(string key)=>PopupMessage(key,true);

        private void PopupMessage(string text,bool useLocale=false)
        {
            if (gameUi == null) { Debug.LogWarning("METIN2_POPUP gameUi=missing"); return; }
            GameObject window = gameUi.Open("popupdialog");
            if (window == null) { Debug.LogWarning("METIN2_POPUP window=missing name=popupdialog"); return; }
            Metin2PopupDialogPanel panel =
                window.GetComponent<Metin2PopupDialogPanel>() ?? window.AddComponent<Metin2PopupDialogPanel>();
            if(useLocale)panel.OpenLocale(text);
            else panel.Open(text);
        }

        /// <summary>game.py RefineSuceededMessage / RefineFailedMessage (:2040-2046).</summary>
        public void RefineSucceeded() { Metin2SoundManager.Instance.PlaySound("sound/ui/make_soket.wav"); PopupMessageLocale("REFINE_SUCCESS"); }
        public void RefineFailed() { Metin2SoundManager.Instance.PlaySound("sound/ui/jaeryun_fail.wav"); PopupMessageLocale("REFINE_FAILURE"); }

        private async void CloseShop()
        {
            CloseShopModals();
            try { if (gameSession != null) await gameSession.SendShopEndAsync(lifetime.Token); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2QuestionDialogPanel shopQuestionDialog;

        private void CloseShopModals()
        {
            if (shopQuestionDialog != null) shopQuestionDialog.Close();
            shopQuestionDialog = null;
        }

        private void ShowShopInfo(string localeKey)
        {
            string text = localeText != null ? localeText.Get(localeKey) : localeKey;
            chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, text);
        }

        private void ShowShopError(string localeKey)
            => PopupMessageLocale(localeKey);

        /// <summary>uiinventory.py:1609 Sort_Inventory sends the /item_check chat command; the
        /// server answers with the reordered bag.</summary>
        private void SortInventory() => SendChat("/item_check");

        /// <summary>
        /// The base-layout inventory prefab carries no sort/extend controls; the Auriga window
        /// (inventorywindowex) does. When neither was bound, two compact controls are added to the
        /// window so the feature is reachable on mobile.
        /// </summary>
        private void EnsureInventoryActionButtons(Metin2InventoryPanel inventory)
        {
            if (inventory == null || inventory.BoundExtraActionCount > 0) return;
            Transform host = inventory.transform;
            if (host.Find("InventorySortButton") != null) return;
            Button sort = MakeInventoryActionButton(host, "InventorySortButton", "ITEMSHOP_SORT", -44f, 44f);
            Button extend = MakeInventoryActionButton(host, "InventoryExtendButton", "UNITY_HUD_MOUNT", -44f, 10f);
            inventory.BindExtraActions(sort, extend);
            Debug.Log("METIN2_INVENTORY_ACTION_BUTTONS sort=1 extend=1");
        }

        private static Button MakeInventoryActionButton(Transform host, string name, string localeKey,
            float x, float y)
        {
            var go = new GameObject(name, typeof(RectTransform), typeof(Image), typeof(Button));
            RectTransform rect = (RectTransform)go.transform;
            rect.SetParent(host, false);
            rect.anchorMin = rect.anchorMax = new Vector2(1f, 1f);
            rect.pivot = new Vector2(1f, 1f);
            rect.anchoredPosition = new Vector2(x, y);
            rect.sizeDelta = new Vector2(92f, 28f);
            Image image = go.GetComponent<Image>();
            image.color = new Color32(52, 66, 82, 255);
            Button button = go.GetComponent<Button>();
            button.targetGraphic = image;
            var labelObject = new GameObject("Text", typeof(RectTransform), typeof(Text));
            RectTransform labelRect = (RectTransform)labelObject.transform;
            labelRect.SetParent(rect, false);
            labelRect.anchorMin = Vector2.zero; labelRect.anchorMax = Vector2.one;
            labelRect.offsetMin = labelRect.offsetMax = Vector2.zero;
            Text label = labelObject.GetComponent<Text>();
            label.font = Metin2.Runtime.Metin2UiFont.Get();
            label.fontSize = 11;
            label.alignment = TextAnchor.MiddleCenter;
            label.color = Color.white;
            label.text = Metin2Language.Get(localeKey);
            label.gameObject.AddComponent<Metin2LocalizedLabel>().Key = localeKey;
            return button;
        }

        private void BuyFromShop(byte displayPosition)
        {
            if (!CanActMainInstance()) return;
            if (shopPanel == null || !shopPanel.TryGetItem(displayPosition, out Metin2GamePacketCodec.ShopItem item)) return;
            string question = Metin2ShopPurchasePrompt.Build(localeText, itemCatalog, item);
            if (gameUi == null) { Debug.LogWarning("METIN2_SHOP_BUY_QUESTION gameUi=missing"); return; }
            GameObject window = gameUi.Open("questiondialog");
            if (window == null) { Debug.LogWarning("METIN2_SHOP_BUY_QUESTION window=missing name=questiondialog"); return; }
            Metin2QuestionDialogPanel panel =
                window.GetComponent<Metin2QuestionDialogPanel>() ?? window.AddComponent<Metin2QuestionDialogPanel>();
            shopQuestionDialog = panel;
            panel.OpenShopPurchase(question.Split('\n')[0], item, itemCatalog, () => ConfirmShopBuy(displayPosition));
        }

        private async void ConfirmShopBuy(byte displayPosition)
        {
            shopQuestionDialog = null;
            if (!CanActMainInstance()) return;
            try
            {
                Debug.Log($"METIN2_SHOP_BUY_CONFIRMED position={displayPosition}");
                if (gameSession != null) await gameSession.SendShopBuyAsync(displayPosition, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>SHOP_SUBHEADER_CG_BUY2 (4): the stack dialog already asked for the amount, so
        /// the original sends it without a second confirmation.</summary>
        private async void StackBuyFromShop(byte displayPosition, byte count)
        {
            if (!CanActMainInstance()) return;
            try
            {
                Debug.Log($"METIN2_SHOP_STACK_BUY position={displayPosition} count={count}");
                if (gameSession != null) await gameSession.SendShopBuyMultipleAsync(displayPosition, count, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private void SellInventoryItemToShop(ushort cell)
            => SellFromShop(cell, 0, false);

        private void SellFromShop(ushort cell, int requestedCount, bool fromAttachment)
        {
            if (!CanActMainInstance()) return;
            if (!Metin2NpcShopSalePolicy.TryPrepare(playerState, itemCatalog, localeText, cell,
                    requestedCount, fromAttachment, out Metin2NpcShopSalePolicy.Request request,
                    out string blockedLocaleKey))
            {
                if (!string.IsNullOrEmpty(blockedLocaleKey))
                    PopupMessageLocale(blockedLocaleKey);
                return;
            }
            Action accept = () => ConfirmShopSell(request);
            if (!request.RequiresConfirmation)
            {
                accept();
                return;
            }
            if (gameUi == null) return;
            GameObject window = gameUi.Open("questiondialog");
            if (window == null) return;
            Metin2QuestionDialogPanel panel =
                window.GetComponent<Metin2QuestionDialogPanel>() ?? window.AddComponent<Metin2QuestionDialogPanel>();
            shopQuestionDialog = panel;
            panel.Open(request.Prompt, accept);
        }

        private async void ConfirmShopSell(Metin2NpcShopSalePolicy.Request request)
        {
            shopQuestionDialog = null;
            if (!CanActMainInstance()) return;
            if (!Metin2NpcShopSalePolicy.IsStillValid(playerState, request) || request.Cell > byte.MaxValue)
                return;
            try
            {
                Debug.Log($"METIN2_SHOP_SELL_CONFIRMED cell={request.Cell} count={request.Count} vnum={request.Vnum}");
                // uishop.py:348 plays on the client-side send, rather than waiting for the
                // authoritative inventory/gold update packet.
                Metin2SoundManager.Instance.PlaySound("sound/ui/money.wav");
                if (gameSession != null) await gameSession.SendShopSellAsync(1, (ushort)request.Cell, (ushort)request.Count, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private Metin2EquipmentRuntimeBinder localEquipmentBinder;

        private void ApplyLocalMount(uint mountVnum)
        {
            localMountEntity?.SetMount(mountVnum);
            // Do not put the rider into the horse pose while its addressable race is still loading
            // (or genuinely missing).  The mount entity raises MountPresentationChanged when the
            // prefab and race data are ready; until then the character remains safely on foot.
            bool presented = mountVnum != 0 && localMountEntity != null && localMountEntity.MountInstance != null;
            localEquipmentBinder?.SetMounted(presented);
            localController?.SetMounted(presented);
        }

        private void OnLocalMountPresentationChanged(bool available)
        {
            bool mountedPresentation = available && localMountEntity != null &&
                                       localMountEntity.MountVnum != 0;
            localEquipmentBinder?.SetMounted(mountedPresentation);
            // The controller also subscribes directly, but SetMounted is idempotent and keeping
            // both presentation consumers on this lifecycle edge prevents weapon/socket drift.
            localController?.SetMounted(mountedPresentation);
        }

        private void ApplyLocalActorSpeeds(ushort moveSpeed, ushort attackSpeed)
        {
            localController?.ApplyServerActorSpeeds(moveSpeed, attackSpeed);
            // The mounted presentation is a separate Metin2NetworkEntity with its own Animator.
            // Initialising it at 100 and only updating the rider made a 150/200-speed character
            // travel normally while the mount legs kept playing at 1.0x: the reported slow-motion
            // effect. Native CActorInstance forwards SetMoveSpeed/SetAttackSpeed to the horse too.
            localMountEntity?.ApplyActorSpeeds(moveSpeed, attackSpeed);
        }

        private void ApplyLocalTextTail()
        {
            if (localMountEntity == null || entityWorld == null) return;
            string name = string.IsNullOrEmpty(entityWorld.LocalDisplayName)
                ? mainCharacterName : entityWorld.LocalDisplayName;
            uint level = entityWorld.LocalLevel;
            if (level == 0 && selectedCharacterIndex >= 0 && currentCharacters.Characters != null &&
                selectedCharacterIndex < currentCharacters.Characters.Length)
                level = currentCharacters.Characters[selectedCharacterIndex].Level;
            localMountEntity.SetTextTailMetadata(name, level, entityWorld.LocalAlignmentRaw,
                entityWorld.LocalIdentityKnown ? entityWorld.LocalEmpire : localEmpire,
                entityWorld.LocalGuildId, entityWorld.ResolveGuildName(entityWorld.LocalGuildId),
                entityWorld.LocalLanguage);
            localMountEntity.SetClientPlatform(entityWorld.LocalPlatform);
            entityWorld.ApplyLocalTextTailCombatState(localMountEntity);
        }

        private void AppendGameOptionInfo(string text) => chatWindow?.AppendChat(Metin2ChatWindow.ChatTypeInfo, text);

        private void RefreshGameOptionTargetBoard()
        {
            if (combatController != null && entityWorld != null &&
                entityWorld.TryGetEntity(combatController.TargetVid, out Metin2NetworkEntity target) && target.Type == 6)
                OpenTargetBoard(target.Vid);
        }

        /// <summary>Applies local actor parts; PART_RUNE is index 7 in Auriga's extended array.</summary>
        private void ApplyLocalParts(ushort[] parts)
        {
            // Editor Stop / process exit need not pass through ClearGamePhaseState. Persist
            // authoritative equipment when it arrives so the next select preview has a weapon.
            if (entityWorld != null)
                Metin2CharacterPreviewState.Save(mainCharacterName, mainCharacterRace, parts,
                    KnownMountPreviewRace(), KnownPetPreviewRace());
            if(parts!=null&&parts.Length>6&&localController!=null&&mainCharacterRace>=0&&mainCharacterRace<8)
            {
                var costumeEffects=localController.GetComponent<Metin2CostumeEffectAppearance>()??localController.gameObject.AddComponent<Metin2CostumeEffectAppearance>();
                costumeEffects.Apply(parts[5],parts[6],(byte)mainCharacterRace,localEquipmentBinder!=null&&localEquipmentBinder.IsMounted);
            }
            if (parts != null && parts.Length > 4 && localController != null && mainCharacterRace >= 0 && mainCharacterRace < 8)
            {
                var sash = localController.GetComponent<Metin2SashAppearance>() ?? localController.gameObject.AddComponent<Metin2SashAppearance>();
                sash.Apply(parts[4], (byte)mainCharacterRace, localEquipmentBinder != null && localEquipmentBinder.IsMounted);
            }
            if (parts != null && parts.Length > 7 && localController != null)
            {
                Metin2RuneCharacterEffect rune = localController.GetComponent<Metin2RuneCharacterEffect>() ??
                                                  localController.gameObject.AddComponent<Metin2RuneCharacterEffect>();
                rune.Apply(parts[7]);
            }
            if (localEquipmentBinder == null || parts == null || parts.Length < 4) return;
            localEquipmentBinder.SetWeaponPart(parts[1]);
            localEquipmentBinder.SetHairPart(parts[3]);
        }

        private Metin2TargetBoardPanel targetBoard;
        private void ClearTargetFromBoard() => combatController?.SetTarget(0);
        /// <summary>RecvDuelStartPacket closes TargetBoard before refreshing PC text tails.</summary>
        private void HandleDuelStartedTargetBoard(Metin2GamePacketCodec.DuelStart _)
            => targetBoard?.Close();
        /// <summary>The local player's empire, from GC_MAIN_CHARACTER - player.IsSameEmpire's left side.</summary>
        private byte localEmpire;

        /// <summary>
        /// game.py splits the two cases before the board ever sees them (:824-856): a PC goes through
        /// SetPCTargetBoard and gets buttons, a monster through SetHPTargetBoard and gets a gauge.
        /// ECharTypes 6 is PC; everything else here is a monster, stone or NPC.
        /// </summary>
        private void OpenTargetBoard(uint vid)
        {
            if (targetBoard == null) return;
            if (vid == 0 || entityWorld == null ||
                !entityWorld.TryGetEntity(vid, out Metin2NetworkEntity entity) || entity == null)
            { targetBoard.Close(); return; }
            // CPythonNonPlayer::GetMonsterName - the localized mob_proto name is the authoritative
            // label; the VID fallback stays only for races missing from the table.
            string label = entity.Type==6 ? entity.DisplayName : Metin2MobNames.TryGetName(entity.Race, out string mobName) ? mobName
                : string.IsNullOrEmpty(entity.DisplayName) ? entity.Vid.ToString() : entity.DisplayName;
            if (entity.Type == 6)
            {
                if (!Metin2ClientSettings.Current.ViewTargetBoard && entity.Empire != 0 &&
                    localEmpire != 0 && entity.Empire != localEmpire) { targetBoard.Close(); return; }
                // Never open a whisper to a VID/race placeholder while GC name
                // data is still arriving. PC names are not mob_proto names.
                if(string.IsNullOrWhiteSpace(label)){targetBoard.Close();return;}
                targetBoard.OpenPlayer(vid, label);
            }
            else
            {
                // OpenCharacterMenu explicitly accepts BUILDING but TargetBoard.Open immediately
                // hides it (uitarget.py:172-176). Monsters never use this path.
                targetBoard.Close();
            }
        }

        /// <summary>
        /// PythonNetworkStreamPhaseGame.cpp::RecvTargetPacket (2501-2532). Monster/stone HP is
        /// server-authoritative and every GC_TARGET refreshes the same board. PC/building replies do
        /// not replace an already-open matching PC menu; a missing/dead actor closes the board.
        /// </summary>
        private void HandleServerTarget(uint vid, byte hpPercent)
        {
            if (targetBoard == null) return;
            if (vid == 0 || entityWorld == null ||
                !entityWorld.TryGetEntity(vid, out Metin2NetworkEntity entity) || entity == null || entity.IsDead)
            {
                targetBoard.Close();
                return;
            }

            if (entity.Type == 6 || entity.Type == 5)
            {
                if (targetBoard.TargetVid != vid) targetBoard.Close();
                return;
            }

            string fallbackName = Metin2MobNames.TryGetName(entity.Race, out string mobName) ? mobName
                : string.IsNullOrEmpty(entity.DisplayName) ? entity.Vid.ToString() : entity.DisplayName;
            string label = Metin2MobNames.FormatTargetName(entity.Race, fallbackName, localeText);
            targetBoard.OpenMonster(vid, label, hpPercent / 100f);
            // One line per HP update packet - the busiest trace left in a pack fight.
            if (Metin2.Runtime.Metin2Diagnostics.Verbose)
                Debug.Log($"METIN2_TARGET_HP vid={vid} race={entity.Race} hp={hpPercent} label={label}");
        }
        private void HandleServerTargetHealth(uint vid,byte percent,long current,long maximum)
        {
            if(targetBoard==null||entityWorld==null||!entityWorld.TryGetEntity(vid,out var entity)||entity==null||entity.IsDead)return;
            if(entity.Type==6)
            {
                bool fighting=(pvpState!=null&&((pvpState.DuelMode==Metin2PvpState.DuelStart&&pvpState.IsDuelParticipant(vid))||pvpState.IsFightPair(entityWorld.LocalVid,vid)))||(advancedDuel!=null&&advancedDuel.LiveVisible&&advancedDuel.OpponentVid==vid);
                if(fighting)targetBoard.OpenPlayerHealth(vid,entity.DisplayName,percent/100f,current,maximum);
                else if(targetBoard.TargetVid==vid&&targetBoard.IsHealthPresentation)targetBoard.OpenPlayer(vid,entity.DisplayName);
            }
            if(targetBoard.TargetVid==vid)targetBoard.SetHealthValues(current,maximum);
        }

        /// <summary>
        /// CPythonPlayer::NotifyCharacterDead clears the selected VID. Despawn is covered as the
        /// same invariant: a board may never outlive the actor it describes.
        /// </summary>
        private void HandleTargetEntityInvalidated(uint vid)
        {
            locallyHitMetins.Remove(vid);
            if (vid == 0) return;
            if (combatController != null && combatController.TargetVid == vid)
                combatController.SetTarget(0);
            if (targetBoard != null && targetBoard.TargetVid == vid)
                targetBoard.Close();
        }

        /// <summary>
        /// Each button's packet, from uitarget.py's own handlers (:324-370). The ones the original
        /// sends as chat commands are sent as chat commands here too - that IS the protocol for them.
        /// </summary>
        private async void OnTargetBoardButton(string key, uint vid, string targetName)
        {
            try
            {
                if (gameSession == null) return;
                switch (key)
                {
                    case Metin2TargetBoardPanel.Whisper: OpenWhisperForTarget(targetName); break;
                    case Metin2TargetBoardPanel.Exchange:
                        if (CanActMainInstance()) await gameSession.SendExchangeStartAsync(vid, lifetime.Token);
                        break;
                    case Metin2TargetBoardPanel.Fight:
                    case Metin2TargetBoardPanel.AcceptFight:
                    case Metin2TargetBoardPanel.Avenge:
                        EnsureAdvancedDuel();
                        await gameSession.SendChatAsync(ChatTypeTalking, $"/pvp_advanced {vid}", lifetime.Token); break;
                    // uitarget.py OnAppendToMessenger uses CG_MESSENGER subheader ADD_BY_VID.
                    // ADD_BY_NAME belongs to the messenger window's text input, not this board.
                    case Metin2TargetBoardPanel.Friend: await gameSession.SendMessengerAddByVidAsync(vid, lifetime.Token); break;
                    case Metin2TargetBoardPanel.InviteParty: await gameSession.SendPartyInviteAsync(vid, lifetime.Token); break;
                    // netSendPartyExitPacket (PythonNetworkStreamModule.cpp:1112-1124) is not its own
                    // packet: leaving is removing YOURSELF, by your own party PID.
                    case Metin2TargetBoardPanel.LeaveParty:
                    {
                        Metin2PartyPanel panel = GetComponent<Metin2PartyPanel>();
                        if (panel != null && entityWorld != null && panel.TryGetPid(entityWorld.LocalVid, out uint ownPid))
                            await gameSession.SendPartyRemoveAsync(ownPid, lifetime.Token);
                        break;
                    }
                    case Metin2TargetBoardPanel.Exclude:
                    {
                        // uitarget.py passes the VID into the Python wrapper, but CG_PARTY_REMOVE
                        // carries PID. Resolve the same PartyMemberVIDToPID mapping before sending.
                        Metin2PartyPanel panel = GetComponent<Metin2PartyPanel>();
                        if (panel != null && panel.TryGetPid(vid, out uint targetPid))
                            await gameSession.SendPartyRemoveAsync(targetPid, lifetime.Token);
                        break;
                    }
                    case Metin2TargetBoardPanel.InviteGuild: await gameSession.SendGuildAddMemberAsync(vid, lifetime.Token); break;
                    case Metin2TargetBoardPanel.RequestEnterParty: await gameSession.SendChatAsync(ChatTypeTalking, $"/party_request {vid}", lifetime.Token); break;
                    case Metin2TargetBoardPanel.EmotionAllow: await gameSession.SendChatAsync(ChatTypeTalking, $"/emotion_allow {vid}", lifetime.Token); break;
                    case Metin2TargetBoardPanel.ViewEquipment: await gameSession.SendChatAsync(ChatTypeTalking, $"/view_equip {vid}", lifetime.Token); break;
                    case Metin2TargetBoardPanel.Dismount: await gameSession.SendChatAsync(ChatTypeTalking, "/unmount", lifetime.Token); break;
                }
                Debug.Log($"METIN2_TARGET_BOARD_ACTION key={key} vid={vid} name={targetName}");
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        private async void SendNpcClick(uint vid)
        {
            // The shop, the dialogue and the warp all begin with this packet, so when nothing
            // opens the first question is whether it was sent at all.
            try
            {
                if (offlineShopCustomerPanel != null && offlineShopCustomerPanel.IsOpen &&
                    offlineShopCustomerPanel.OwnerVid != 0 && offlineShopCustomerPanel.OwnerVid != vid &&
                    offlineShopWorldSigns != null && offlineShopWorldSigns.TryGet(vid, out _))
                {
                    offlineShopCustomerPanel.CloseLocal(false);
                    // The legacy CG_OFFLINESHOP (57) header is HEADER_CG_OPENSHOP on this server, so
                    // sending it desynchronized the stream and the server closed the connection. The
                    // Auriga close-board (CG_NEW_OFFLINESHOP 119 sub 25) is the packet the customer
                    // window's own close button sends.
                    if (gameSession != null) await gameSession.SendAurigaOfflineShopCloseBoardAsync(lifetime.Token);
                }
                Debug.Log($"METIN2_NPC_CLICK vid={vid}");
                if (gameSession != null) await gameSession.SendNpcClickAsync(vid, lifetime.Token);
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private async void SendItemPickup(uint vid) { if (!CanActMainInstance()) return; try { if (gameSession != null) await gameSession.SendItemPickupAsync(vid, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private bool HasEquippedFishingRod()
        {
            if (localController != null && localController.IsMounted) return false;
            uint vnum = 0;
            // Character parts are authoritative and can arrive before the inventory replay.
            // The attack button must therefore use the same effective weapon as the renderer.
            if (localEquipmentBinder != null && localEquipmentBinder.AuthoritativeWeaponPartKnown)
                vnum = localEquipmentBinder.AuthoritativeWeaponPart;
            else if (playerState != null && playerState.TryGetEquipment(4, out Metin2GamePacketCodec.ItemData item))
                vnum = item.Vnum;
            if (vnum != 0 && itemCatalog != null && itemCatalog.TryGet(vnum, out Metin2ItemCatalog.Entry entry))
                return entry.itemType == Metin2ItemConstants.TypeRod;
            return localController?.GetComponentInChildren<Metin2CharacterMotionSet>(true)?.CurrentMode ==
                Metin2MotionMode.Fishing;
        }
        private async void SendFishing(byte direction) { try { if (gameSession != null) await gameSession.SendFishingAsync(direction, lifetime.Token); } catch (Exception exception) { ShowOperationFailure(exception); } }
        private void PickUpClosestItem() => groundItemWorld?.RequestClosest();
        private async void HandleWarp(Metin2GamePacketCodec.WarpDestination warp)
        {
            // CPythonNetworkStream::RecvWarpPacket does not locally reposition the
            // character.  It first switches its TCP descriptor to warp.lAddr/wPort
            // (PythonNetworkStreamPhaseGame.cpp:1000-1011), then DirectEnter receives
            // a new authoritative main-character/world burst from that core.  Treating
            // this packet as an ordinary local teleport mixed two map cores and caused
            // the active dungeon prefab to be destroyed as the player took a few steps.
            string host = FormatAddress(warp.Address);
            Debug.Log($"METIN2_WARP_RECONNECT_REQUIRED x={warp.X} y={warp.Y} host={host} port={warp.Port}");
            if (gameTransitionInProgress || selectedCharacterIndex < 0) return;
            gameUi?.BeginLootTeleport();
            try
            {
                await EnterCharacterCoreAsync(selectedCharacterIndex, host, warp.Port, "warp", lifetime.Token);
                gameUi?.CompleteLootTeleport(mainCharacterName);
            }
            catch (Exception exception) { gameUi?.CancelLootTeleport(); FailGameTransition(exception); }
        }

        /// <summary>
        /// Mirrors CPythonNetworkStream::RecvPhasePacket (PythonNetworkStream.cpp:609-647).
        ///
        /// The server completes /phase_select with GC_PHASE(SELECT), and /logout with
        /// GC_PHASE(CLOSE).  These used to be consumed silently by Metin2GameRuntime, after which a
        /// socket close could even enter the generic reconnect path.  Marking the game offline
        /// before stopping the receiver makes a racing ReceiveFailed harmless and restores the
        /// original screen transition.
        /// </summary>
        private void HandleGamePhase(byte phase)
        {
            intentionalExitCommand = null;
            if (gameSession != null) Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"BOOTSTRAP_GC_PHASE phase={phase}");
            Debug.Log($"METIN2_GAME_PHASE phase={phase}");
            switch (phase)
            {
                case Metin2ProtocolCodec.PhaseSelect:
                    ClearQuickSlotsForCharacterChange();
                    LeaveLiveGamePhase(disposeSession: false);
                    SetStatusLocale(currentCharacters.Characters == null ? "UNITY_CREATE_CHARACTER" : "UNITY_SELECT_CHARACTER");
                    ShowCharacters(currentCharacters);
                    return;
                case Metin2ProtocolCodec.PhaseClose:
                case Metin2ProtocolCodec.PhaseLogin:
                    ClearQuickSlotsForCharacterChange();
                    LeaveLiveGamePhase(disposeSession: true);
                    currentCharacters = default;
                    accountLoginKey = 0;
                    SetStatusLocale("UNITY_READY");
                    if (originalUiActive) originalUi.ShowLogin();
                    return;
                // PHASE_DEAD/PHASE_GAME are carried as part of the normal death/restart lifecycle;
                // their presentation is driven by GC_DEAD and the authoritative state packets.
                case Metin2ProtocolCodec.PhaseDead:
                case Metin2ProtocolCodec.PhaseGame:
                case Metin2ProtocolCodec.PhaseLoading:
                case Metin2ProtocolCodec.PhaseHandshake:
                    return;
                default:
                    Debug.LogWarning($"METIN2_GAME_PHASE unhandled={phase}");
                    return;
            }
        }

        private void ClearQuickSlotsForCharacterChange()
        {
            Array.Clear(quickSlotState, 0, quickSlotState.Length);
            gameUi?.TaskBarPanel?.QuickSlots?.ClearAllRegistrations();
            cursorAttachment?.Deattach();
        }

        private void LeaveLiveGamePhase(bool disposeSession)
        {
            gameUi?.CancelLootTeleport();
            // Set this first: ReceiveFailed can be raised later in the same Update after PHASE_CLOSE.
            inGame = false;
            gameTransitionInProgress = false;
            TearDownStep("leaveHideWindows", () => gameUi?.HideOpenWindows(null));
            TearDownStep("leaveClearPhase", ClearGamePhaseState);
            TearDownStep("leaveGuildMarkStore", Metin2GuildMarkStore.Clear);
            TearDownStep("leaveHideLoading", () => loadingScreen?.Hide());
            TearDownStep("leaveGameUiVisibility", ApplyGameUiVisibility);
            CloseCharacterDialogs();
            selectedCharacterIndex = -1;
            activeGameHost = string.Empty;
            activeGamePort = 0;
            mainCharacterName = string.Empty;
            mainCharacterJob = string.Empty;
            mainCharacterRace = -1;
            mainCharacterSkillGroup = 1;
            localEmpire = 0;
            if (disposeSession)
            {
                TearDownStep("leaveDisposeSession", () => { gameSession?.Dispose(); gameSession = null; });
                TearDownStep("leaveReleaseSessionAssets", ReleaseSessionAssetCaches);
            }
        }

        private void RequestApplicationQuit()
        {
            Debug.Log("METIN2_APPLICATION_QUIT serverCommand=quit");
            ApplicationQuitRequested?.Invoke();
#if UNITY_EDITOR
            // Application.Quit is deliberately ignored by the Editor. Interactive play must still
            // behave like the standalone client, while command-line validators stay alive long
            // enough to report their assertions.
            if (!Application.isBatchMode) UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }

        private void HandleReceiveFailure(Exception failure)
        {
            if (!inGame || gameTransitionInProgress) return;
            if (intentionalExitCommand != null && Time.unscaledTime <= intentionalExitDeadline)
            {
                string command = intentionalExitCommand;
                intentionalExitCommand = null;
                LeaveLiveGamePhase(disposeSession: true);
                if (command == "/phase_select" && currentCharacters.Characters != null)
                    ShowCharacters(currentCharacters);
                else if (command == "/quit") RequestApplicationQuit();
                else if (originalUiActive) originalUi.ShowLogin();
                SetStatus(string.Empty);
                return;
            }
            if (failure is System.IO.EndOfStreamException)
            {
                // A clean TCP EOF is how this server also ends a kicked session. It has no
                // network-error packet and must not be presented as a failed connection.
                LeaveLiveGamePhase(disposeSession: true);
                currentCharacters = default;
                accountLoginKey = 0;
                if (originalUiActive) originalUi.ShowLogin();
                SetStatusLocale("UNITY_SERVER_CLOSED");
                return;
            }
            // CPythonNetworkStream never keeps the old world alive after the game descriptor dies:
            // phase teardown returns to login. The former automatic DirectEnter reconnect mixed a
            // stale receive loop/world with a fresh socket and manifested as map retention/jitter.
            Debug.LogWarning($"METIN2_REMOTE_DISCONNECT error={failure.GetType().Name} message={failure.Message}");
            if (gameSession != null)
            {
                Metin2NetworkDiagnostics.Event(gameSession.DiagnosticSessionId, $"BOOTSTRAP_RECEIVE_FAILURE type={failure.GetType().Name} message={failure.Message}");
                Metin2NetworkDiagnostics.Flush("BOOTSTRAP_RECEIVE_FAILURE");
            }
            FailGameTransition(failure);
        }

        private string intentionalExitCommand;
        private float intentionalExitDeadline;
        private void MarkIntentionalExit(string message)
        {
            string command = message == null ? string.Empty : message.Trim();
            if (command != "/phase_select" && command != "/logout" && command != "/quit") return;
            intentionalExitCommand = command;
            intentionalExitDeadline = Time.unscaledTime + 20f;
        }

        private void HandleMainCharacter(Metin2GamePacketCodec.MainCharacter main)
        {
            mainCharacterName = main.Name;
            mobileUiRoot?.SetCharacterName(mainCharacterName);
            MessengerPanelOrNull()?.Configure(localeText, mainCharacterName);
            targetBoard?.RefreshFromMessengerState();
            // GC_GUILD can land before this packet; the roster is matched to the local player by
            // name, so hand the name to a panel that was already built without it.
            GuildPanelOrNull()?.SetLocalCharacterName(mainCharacterName);
            targetBoard?.RefreshFromGuildState();
            localEmpire = main.Empire;
            entityWorld?.SetLocalTextTailEmpire(localEmpire);
            combatController?.SetLocalCombatIdentity(localEmpire,
                GuildPanelOrNull()?.GuildId ?? 0u);
            // InstanceBase.h:1119 RaceToJob: race % 4. The skill group comes straight from the packet
            // and selects which of the two skill tables the character window shows.
            mainCharacterJob = SkillJobs[main.Race % SkillJobs.Length];
            mainCharacterRace = main.Race;
            toolTipController?.SetCharacterRace(mainCharacterRace);
            mainCharacterSkillGroup = main.SkillGroup < 1 ? 1 : main.SkillGroup;
            sessionState?.ApplySkillGroup(main.SkillGroup);
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?
                .BindSkills(skillCatalog, mainCharacterJob, mainCharacterSkillGroup, null, null, null, localEmpire);
            // The surrounding-entity broadcast includes the player's own character; without this the
            // entity world would spawn a second, server-driven copy next to the controlled one.
            if (entityWorld != null) entityWorld.LocalVid = main.Vid;
            if (localController != null) localController.LocalVid = main.Vid;
            if (combatController != null) combatController.LocalVid = main.Vid;
            if (pvpState != null) pvpState.LocalVid = main.Vid;
            // Party packets are allowed to arrive before GC_MAIN_CHARACTER.  Once the local VID is
            // authoritative, replay the party-derived leader affordances (EXP mode and member menu).
            partyPanel?.Refresh();
            // game.py OnGameOver (game.py:814-818): the dead main character closes the target board
            // and opens the restart dialog (interfacemodule.py:960-962, uirestart.py).
            if (entityWorld != null)
            {
                entityWorld.LocalPlayerDied -= OnLocalPlayerDied; entityWorld.LocalPlayerDied += OnLocalPlayerDied;
            }
            // The player's own affect flags arrive on its character packets, which the entity world
            // otherwise ignores because there is no Metin2NetworkEntity for the locally driven actor.
            if (entityWorld != null && localController != null) entityWorld.LocalPlayerRoot = localController.transform;
            ApplyLocalTextTail();
            damageTextTail?.SetLocal(main.Vid, localController == null ? null : localController.transform);
            gameUi?.GetPanel<Metin2CharacterWindowPanel>("characterwindow")?.SetIdentity(mainCharacterName, string.Empty);
            // Original game.py: net.GetFieldMusicFileName() -> snd.FadeInMusic("BGM/" + fieldMusic)
            // A login window BGM (login_window.mp3) futását enterworld után meg kell szüntetni,
            // de a loginok.wav UI hang marad - ezért itt FadeInMusic field-re.
            try
            {
                string bgmName = main.BgmName;
                float bgmVol = main.BgmVolume;
                if (!string.IsNullOrWhiteSpace(bgmName))
                {
                    // Szerver által küldött field BGM (pl. desert.mp3, m2bg.mp3)
                    Metin2SoundManager.Instance.FadeInMusic("BGM/" + bgmName, 0.02f);
                    Metin2SoundManager.Instance.SetMusicVolume(bgmVol > 0.001f ? bgmVol : 1f);
                    Metin2ClientSettings.Current.MusicFile = System.IO.Path.GetFileName(bgmName);
                }
                else
                {
                    // Ha szerver nem küldött BGM-et, ne tartsuk meg a login_window.mp3-et
                    // Original fallback: musicInfo.fieldMusic = METIN2THEMA (m2bg.mp3)
                    string fallback = "BGM/m2bg.mp3";
                    // Próbáljuk a lastplay.inf-et is, mint eredeti LoadLastPlayFieldMusic
                    try
                    {
                        string lastPlay = System.IO.Path.Combine(Application.dataPath, "../Client/BGM/lastplay.inf");
                        if (System.IO.File.Exists(lastPlay))
                        {
                            string last = System.IO.File.ReadAllText(lastPlay).Trim();
                            if (!string.IsNullOrWhiteSpace(last)) fallback = "BGM/" + System.IO.Path.GetFileName(last);
                        }
                    } catch {}
                    Metin2SoundManager.Instance.FadeInMusic(fallback, 0.02f);
                    Metin2ClientSettings.Current.MusicFile = System.IO.Path.GetFileName(fallback);
                }
            }
            catch (Exception ex) { Debug.LogWarning("METIN2_BGM_APPLY " + ex.Message); }

            if (localController == null) return;
            try { Vector3 local = worldRuntime.LoadAt(main.X, main.Y); RebindMapWindowAssets(); local.y = worldRuntime.GroundHeight(local); localController.PlaceOnGround(local); localController.gameObject.name = "LocalPlayer_" + main.Name; }
            catch (InvalidOperationException)
            {
                // Out-of-range coordinates mean the packet was parsed at the wrong offset or the
                // stream was misframed. The header, the name and the raw field bytes identify which.
                Debug.LogWarning(string.Format(
                    "METIN2_MAIN_CHARACTER_BAD_POSITION header={0} vid={1} race={2} name='{3}' bgm='{4}' x={5} y={6} z={7} empire={8} skillGroup={9}",
                    main.Header, main.Vid, main.Race, main.Name, main.BgmName, main.X, main.Y, main.Z, main.Empire, main.SkillGroup));
            }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }

        /// <summary>
        /// Reports where the player, the camera and the loaded map actually are. The map renders in the
        /// Scene view but not in the Game view, so the question is whether the two use the same
        /// coordinate space and whether the follow camera ends up inside or below the terrain.
        /// </summary>
        private void LogWorldDiagnostics(GameObject player)
        {
            Camera camera = Camera.main;
            Debug.Log(string.Format("METIN2_WORLD player={0} camera={1} mapLoaded={2} mapName={3}",
                player == null ? "null" : player.transform.position.ToString("F2"),
                camera == null ? "null" : camera.transform.position.ToString("F2"),
                worldRuntime != null && worldRuntime.HasMap,
                worldRuntime == null ? "-" : worldRuntime.CurrentMapName));

            if (worldRuntime == null || player == null) return;
            Vector3 probe = player.transform.position;
            var terrains = worldRuntime.GetComponentsInChildren<Terrain>(true);
            Debug.Log("METIN2_WORLD terrains=" + terrains.Length + " probe=" + probe.ToString("F2"));

            // GroundHeight picks the terrain whose local bounds contain the query point. Report which
            // tile that is and what it samples, because a returned height of exactly 0 means no tile
            // matched and the player was left under the surface.
            Terrain match = null;
            foreach (Terrain terrain in terrains)
            {
                if (terrain.terrainData == null) continue;
                Vector3 local = terrain.transform.InverseTransformPoint(probe);
                bool contains = terrain.terrainData.bounds.Contains(local);
                if (contains && match == null) match = terrain;
                Debug.Log(string.Format("METIN2_WORLD tile {0} pos={1} size={2} local={3} contains={4} sample={5:F2}",
                    terrain.name, terrain.transform.position.ToString("F1"),
                    terrain.terrainData.size.ToString("F1"), local.ToString("F1"), contains,
                    terrain.SampleHeight(probe) + terrain.transform.position.y));
            }
            Debug.Log("METIN2_WORLD matchedTile=" + (match == null ? "NONE" : match.name) +
                      " groundHeight=" + worldRuntime.GroundHeight(probe).ToString("F2"));

            var renderers = worldRuntime.GetComponentsInChildren<Renderer>(true);
            int enabled = 0;
            var bounds = new Bounds(worldRuntime.transform.position, Vector3.zero);
            foreach (Renderer renderer in renderers)
            {
                if (!renderer.enabled) continue;
                if (enabled == 0) bounds = renderer.bounds; else bounds.Encapsulate(renderer.bounds);
                enabled++;
            }
            Debug.Log(string.Format("METIN2_WORLD renderers={0}/{1} boundsCenter={2} boundsSize={3} cameraInside={4}",
                enabled, renderers.Length, bounds.center.ToString("F1"), bounds.size.ToString("F1"),
                camera != null && bounds.Contains(camera.transform.position)));
        }

        /// <summary>
        /// TSimplePlayerInformation.lAddr holds the result of inet_addr, so the four address bytes are
        /// already in wire order inside the little-endian DWORD.
        /// </summary>
        private static string FormatAddress(uint address)
        {
            if (address == 0) return string.Empty;
            return string.Format("{0}.{1}.{2}.{3}", address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xFF, (address >> 24) & 0xFF);
        }

        /// <summary>
        /// The login packets carry a four byte language code. When the original login screen is
        /// active its selector is authoritative, matching locale_select.py in the classic client.
        /// </summary>
        private string ActiveLanguage => Metin2Language.Code;

        private void SetStatus(string value)
        {
            statusLocaleKey = null;
            if (statusText != null) statusText.text = value;
            if (originalUiActive) originalUi.SetStatus(value);
        }
        private void ShowOperationFailure(Exception exception)
        {
            if (exception is OperationCanceledException) return;
            Debug.LogException(exception);
            string key = exception is TimeoutException ? "UNITY_CLIENT_TIMEOUT"
                : exception is System.Net.Sockets.SocketException || exception is System.IO.IOException
                    ? "UNITY_CLIENT_NETWORK_FAILED" : "UNITY_CLIENT_OPERATION_FAILED";
            if (Metin2Language.TryGet(exception.Message, out _)) key = exception.Message;
            SetStatusLocale(key);
            if(creationFrontend!=null&&(creationFrontend.IsCreateVisible||creationFrontend.IsEmpireVisible))creationFrontend.ShowError(Metin2Language.Get(key));
        }

        private string statusLocaleKey;
        private void SetStatusLocale(string key)
        {
            SetStatus(Metin2Language.Get(key));
            statusLocaleKey = key;
        }

        private void RefreshLocalizedStatus(string code)
        {
            if (statusLocaleKey != null) SetStatusLocale(statusLocaleKey);
            if (createCharacterPanel != null) RefreshCreateLabels();
        }

        private async void LoginFromOriginalUi()
        {
            try { await LoginAsync(originalUi.Account, originalUi.Password, lifetime.Token); }
            catch (OperationCanceledException) { SetStatusLocale("UNITY_CANCELLED"); }
            catch (Exception exception) { ShowOperationFailure(exception); }
        }
        private static uint[] CreateClientKey()
        {
            byte[] bytes = new byte[16]; using (RandomNumberGenerator random = RandomNumberGenerator.Create()) random.GetBytes(bytes);
            return new[] { BitConverter.ToUInt32(bytes, 0), BitConverter.ToUInt32(bytes, 4), BitConverter.ToUInt32(bytes, 8), BitConverter.ToUInt32(bytes, 12) };
        }

        private static RectTransform CreateRect(string name, Transform parent, Vector2 size, Vector2 position)
        {
            var go = new GameObject(name, typeof(RectTransform)); var rect = (RectTransform)go.transform; rect.SetParent(parent, false);
            rect.anchorMin = rect.anchorMax = new Vector2(0.5f, 0.5f); rect.sizeDelta = size; rect.anchoredPosition = position; return rect;
        }
        private static Text CreateText(Transform parent, Font font, string value, int size, Vector2 position, Vector2 dimensions)
        {
            RectTransform rect = CreateRect("Text", parent, dimensions, position); Text text = rect.gameObject.AddComponent<Text>();
            text.font = font; text.fontSize = size; text.alignment = TextAnchor.MiddleCenter; text.color = Color.white; text.text = value; return text;
        }
        private static InputField CreateInput(Transform parent, Font font, string placeholder, Vector2 position, bool password)
        {
            RectTransform rect = CreateRect(placeholder, parent, new Vector2(340, 38), position); rect.gameObject.AddComponent<Image>().color = new Color(0.15f, 0.17f, 0.2f, 1);
            InputField input = rect.gameObject.AddComponent<InputField>(); Text value = CreateText(rect, font, "", 16, Vector2.zero, new Vector2(320, 34)); value.alignment = TextAnchor.MiddleLeft;
            Text hint = CreateText(rect, font, placeholder, 16, Vector2.zero, new Vector2(320, 34)); hint.alignment = TextAnchor.MiddleLeft; hint.color = Color.gray;
            input.textComponent = value; input.placeholder = hint; if (password) input.contentType = InputField.ContentType.Password; return input;
        }
        private static Text CreateLocaleText(Transform parent, Font font, string key, int size, Vector2 position, Vector2 dimensions)
        {
            Text label = CreateText(parent, font, Metin2Language.Get(key), size, position, dimensions);
            label.gameObject.AddComponent<Metin2LocalizedLabel>().Key = key;
            return label;
        }
        private static void BindLocaleText(Text text, string key, Func<object[]> arguments = null)
        {
            if (text == null) return;
            Metin2LocalizedLabel label = text.GetComponent<Metin2LocalizedLabel>();
            if (label == null) label = text.gameObject.AddComponent<Metin2LocalizedLabel>();
            if (arguments == null) label.Key = key;
            else label.ConfigureFormat(key, arguments);
        }
        private static InputField CreateLocaleInput(Transform parent, Font font, string key, Vector2 position, bool password)
        {
            InputField input = CreateInput(parent, font, Metin2Language.Get(key), position, password);
            input.placeholder.gameObject.AddComponent<Metin2LocalizedLabel>().Key = key;
            return input;
        }
        private static Button CreateLocaleButton(Transform parent, Font font, string key, Vector2 position, UnityEngine.Events.UnityAction action)
            => CreateLocaleButton(parent, font, key, position, action, new Vector2(340, 30));
        private static Button CreateLocaleButton(Transform parent, Font font, string key, Vector2 position, UnityEngine.Events.UnityAction action, Vector2 size)
        {
            Button button = CreateButton(parent, font, Metin2Language.Get(key), position, action, size);
            button.GetComponentInChildren<Text>().gameObject.AddComponent<Metin2LocalizedLabel>().Key = key;
            return button;
        }
        private static Button CreateButton(Transform parent, Font font, string label, Vector2 position, UnityEngine.Events.UnityAction action)
            => CreateButton(parent, font, label, position, action, new Vector2(340, 30));
        private static Button CreateButton(Transform parent, Font font, string label, Vector2 position, UnityEngine.Events.UnityAction action, Vector2 size)
        {
            RectTransform rect = CreateRect(label, parent, size, position); Image image = rect.gameObject.AddComponent<Image>(); image.color = new Color(0.22f, 0.32f, 0.48f, 1);
            Button button = rect.gameObject.AddComponent<Button>(); button.targetGraphic = image; button.onClick.AddListener(action); CreateText(rect, font, label, 15, Vector2.zero, rect.sizeDelta); return button;
        }

        private void OnDestroy()
        {
            clientCameraBinding?.Dispose();
            clientCameraBinding = null;
            Metin2Language.Changed -= RefreshLocalizedStatus;
            Metin2ClientSettings.Current.Changed -= RefreshGameOptionTargetBoard;
            StopGuildMarkTransfer();
            // Never Clear(): the main-thread guard can fire during the play-mode transition.
            Metin2GuildMarkStore.ClearForTeardown();
            lifetime?.Cancel(); lifetime?.Dispose(); lifetime = null; gameSession?.Dispose(); gameSession = null;
            ReleaseSessionAssetCaches();
        }
    }
}
