using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Metin2.Runtime.Mobile;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

namespace Metin2.Runtime.Network
{
    /// <summary>
    /// The two Auriga inventory windows the base layout has no view for: the mount bag
    /// (EWindows::MOUNT_INVENTORY = 5, 12x16 = 192 cells, c_Mount_Inventory_Count) and the extra
    /// inventory (EWindows::EXTRA_INVENTORY = 8, 45 per page, 48 pages in 6 categories x 8 tabs).
    ///
    /// The imported original windows (Metin2Ui_mountinventory / Metin2Ui_extrainventory) are bound
    /// when they exist - uimountinventory.py and uiextrainventory.py cell for cell - and the runtime
    /// grid remains only as the fallback for harnesses that build the component without a prefab.
    /// It is not a second inventory system: the cells read the shared <see cref="Metin2PlayerState"/>
    /// item model, the icons come from <see cref="Metin2ItemCatalog"/>, and every interaction is
    /// surfaced as an event the bootstrap forwards to the existing mount / item-move writers.
    /// </summary>
    public sealed class Metin2AuxiliaryInventoryPanel : MonoBehaviour
    {
        public enum Kind { Mount, Extra, Belt }

        private Metin2PlayerState state;
        private Metin2ItemCatalog catalog;
        private Metin2ToolTipController toolTips;
        private Metin2CursorAttachment cursor;
        private Kind kind;
        private byte window;
        private int beltStartCell;
        private int pageSize = 45, pageCount = 1, page, category, categoryCount = 1;
        private int tabsPerCategory = 1;
        private RectTransform grid;
        private Text pageLabel;
        private Button[] cells = Array.Empty<Button>();
        private Image[] icons = Array.Empty<Image>();
        private Image[] searchGlows = Array.Empty<Image>();
        private Image[] activateEffects = Array.Empty<Image>();
        private bool[] activatedSlots = Array.Empty<bool>();
        private int[] activatedSlotCells = Array.Empty<int>();
        private Metin2UiSpriteCatalog uiSprites;
        private float searchHighlightUntil;
        private Text[] counts = Array.Empty<Text>();
        private Button[] inventoryTabs = Array.Empty<Button>();
        private Button[] categoryTabs = Array.Empty<Button>();
        private readonly List<GameObject> runtimeCells = new List<GameObject>();
        private InputField searchInput;
        private Button searchButton;
        private readonly HashSet<int> searchHighlights = new HashSet<int>();
        private int selectedCell = -1;
        private RectTransform mobileActions;
        private Button mobileUseButton, mobileSplitButton;
        // constinfo.py: the exact active checklists. Commented-out source rows are intentionally
        // absent. Belt entries are +0 family representatives; ownership accepts any +0..+9.
        private static readonly uint[] BeltAllowedItems =
        {
            18000,18010,18020,18030,18040,18050,18060,18070,
            18080,18090,18100,18110,18120,18130,18140,18150
        };
        private static readonly uint[] MountAllowedItems = new uint[]
        {
            14590,14591,14592,14593,52040,60001,48421,49009,49049,71223,71224,71228,71251,
            71125,71126,71127,71166,71171,71176,71177,71221,71222,71252,71225,71226,71227,
            71255,71254,71233,71250,71128,23015,23016,71137,611502,611503,611504,611505,
            611506,611512,611515,611517,611518,611520,611521,611523,611524,611525,611527,
            611528,611530,611532,611533,611534,611535,611538,611540,611545,611546,611547,
            611549,611550,611551,611553,611554,611555,611556,611557,611558,611559,611563,
            611565,611568,611571,611573,611576,611577,611579,611581,611582,611583,611584,
            611585,611587,611589,611591,611592,611594,611598,611600,611602,611603,611605,
            611608,611610,611611,611612,611613,611615,611618,611623,611624,611631,611633,
            611637,611638,611640,611641,611654,611661,611586,611513,611666,611650,611653,
            611597,611655,611665,611626,611627,611652,611651,611531
        }.Concat(Enumerable.Range(630000,203).Select(v=>(uint)v)).ToArray();
        /// <summary>Authoritative constinfo.py mount whitelist, exposed to parity validators.</summary>
        public static IReadOnlyList<uint> MountAllowedItemVnums => MountAllowedItems;
        private readonly Dictionary<uint, Text> allowedItemRows = new Dictionary<uint, Text>();
        private RectTransform allowedItemContent;

        /// <summary>The absolute server cell of a local grid index. The mount and extra windows
        /// start at 0; the belt window's prefab names its cells with the absolute index (500..659),
        /// and its local index 0 is cell 500.</summary>
        private int CellFor(int localIndex)
            => (kind == Kind.Belt ? beltStartCell : 0) + GlobalPage * pageSize + localIndex;

        private bool TryGetCell(int cell, out Metin2GamePacketCodec.ItemData item)
        {
            if (kind == Kind.Mount) return state.TryGetMountItem((ushort)cell, out item);
            // Auriga keeps belt cells in the ordinary INVENTORY storage at the absolute
            // 500..659 cells. BELT_INVENTORY exists in EWindows, but CHARACTER::GetItem and the
            // original uiinventory.py move path both use INVENTORY for these cells.
            if (kind == Kind.Belt) return state.TryGetItem(1, (ushort)cell, out item);
            return state.TryGetExtraItem((ushort)cell, out item);
        }

        /// <summary>window, cell, item; a click the bootstrap turns into a move / attach.</summary>
        public event Action<byte, ushort, Metin2GamePacketCodec.ItemData> SlotActivated;
        /// <summary>window, cell; an attached item was dropped on the cell.</summary>
        public event Action<byte, ushort> SlotDropRequested;
        /// <summary>window, cell, carried count, screen position; the item was released over the
        /// world, so the original drop/destroy confirmation must open (game.py:1347 __PutItem).</summary>
        public event Action<byte, ushort, int, Vector2> WorldReleaseRequested;
        /// <summary>window, cell, use count; original UseItemSlot (right click/double click).
        /// uiinventory.py:882-888 uses the whole stack on Ctrl, otherwise exactly one.</summary>
        public event Action<byte, ushort, int> UseRequested;
        /// <summary>window, cell, stack size; Shift+click asked for the pick-amount split dialog.
        /// uiextrainventory.py:764-770.</summary>
        public event Action<byte, ushort, int> SplitRequested;
        /// <summary>Original extra-inventory Safebox button chat route.</summary>
        public event Action<string> ChatRequested;
        public event Action<string> ItemLinkRequested;
        public event Action<byte, ushort> SlotHovered;
        public event Action HoverExited;
        public event Action CloseRequested;
        /// <summary>Original row/i-button action: open this item on the wiki origin page.</summary>
        public event Action<uint> WikiItemRequested;

        public int Page => GlobalPage;
        public int GlobalPage => category * tabsPerCategory + page;
        public int PageCount => pageCount;
        public int Category => category;
        public int SlotCount => cells.Length;
        public byte Window => window;
        /// <summary>Deterministic gate accessor: the cell currently paints an item icon.</summary>
        public bool IsSlotVisible(int localIndex)
            => localIndex >= 0 && localIndex < icons.Length && icons[localIndex] != null &&
               icons[localIndex].enabled && icons[localIndex].sprite != null;
        public bool IsOpen => gameObject.activeSelf;
        /// <summary>True while this panel's own item is held on the shared cursor (the page/category
        /// tabs switch on hover while this is set, like Metin2InventoryPanel.HasInventoryDrag).</summary>
        public bool HasAuxiliaryDrag => cursor != null && cursor.IsAttached && ReferenceEquals(cursor.Owner, this);
        public bool UsesImportedWindow { get; private set; }
        public int AllowedItemCount => allowedItemRows.Count;
        public bool HasSearchControls => kind == Kind.Extra && searchInput != null && searchButton != null;
        public int HighlightedSlotCount => searchHighlights.Count;
        public bool IsAllowedItemOwned(uint vnum)
            => allowedItemRows.TryGetValue(vnum, out Text row) && row != null && row.color.g > row.color.r;

        public void BindToolTips(Metin2ToolTipController controller) => toolTips = controller;

        /// <summary>The shared drag cursor, so a slot click can pick the item up (original
        /// SelectItemSlot) and a click on another slot can drop it.</summary>
        public void BindCursor(Metin2CursorAttachment attachment) => cursor = attachment;
        public void ClearSelection(){selectedCell=-1;Refresh();}

        private Metin2QuickSlotBar.SlotType AttachType
            => kind == Kind.Mount ? Metin2QuickSlotBar.SlotType.MountInventory
             : kind == Kind.Belt ? Metin2QuickSlotBar.SlotType.BeltInventory
             : Metin2QuickSlotBar.SlotType.ExtraInventory;

        /// <summary>Builds the cells. Mount: 12 columns, 16 rows, one page. Extra: 5 columns,
        /// 9 rows (c_Extra_Inventory_Page_Size), 48 pages in 6 categories of 8 tabs. Belt
        /// (GameType.h:203-208, EWindows::BELT_INVENTORY = 7): 10 columns, 16 rows, cells
        /// 500..659, one page.</summary>
        public void Configure(Metin2PlayerState playerState, Metin2ItemCatalog itemCatalog, Kind inventoryKind)
        {
            if (state != null) state.ItemChanged -= OnItemChanged;
            state = playerState; catalog = itemCatalog; kind = inventoryKind;
            if (state != null) state.ItemChanged += OnItemChanged;
            if (kind == Kind.Mount)
            {
                window = 5; pageSize = 192; pageCount = 1; categoryCount = 1; tabsPerCategory = 1;
            }
            else if (kind == Kind.Belt)
            {
                window = 7; pageSize = 160; pageCount = 1; categoryCount = 1; tabsPerCategory = 1;
                beltStartCell = Metin2PlayerState.BeltInventoryStartCell;
            }
            else
            {
                window = 8; pageSize = 45; pageCount = 48; categoryCount = 6; tabsPerCategory = 8;
            }
            if (BindImportedWindow()) { BuildExtraControls(); BuildAllowedItemList(); Refresh(); return; }
            BuildRuntimeGrid();
            BuildExtraControls();
            BuildAllowedItemList();
            Refresh();
        }

        private void OnItemChanged(Metin2GamePacketCodec.ItemData item)
        {
            // Belt items are ordinary INVENTORY items in Auriga (cells 500..659), not
            // EWindows::BELT_INVENTORY packets.  The old panel only repainted on Open(), so the
            // authoritative GC_ITEM_SET/DEL was invisible until the player reopened the bag.
            if (kind == Kind.Belt)
            {
                if (item.Window == 1 && item.Cell >= beltStartCell && item.Cell < beltStartCell + pageSize) Refresh();
                return;
            }
            if (item.Window == window) Refresh();
        }

        private void OnDestroy()
        {
            if (state != null) state.ItemChanged -= OnItemChanged;
        }

        /// <summary>
        /// Binds the imported original window when the caller opened one: the uiscript layout
        /// carries Slot_N cells (192 for the mount bag, 45 for an extra page) plus the
        /// Inventory_Tab_XX / Cat_XX selectors.
        /// </summary>
        private bool BindImportedWindow()
        {
            var views = new Dictionary<int, Button>();
            var iconViews = new Dictionary<int, Image>();
            var glowViews = new Dictionary<int, Image>();
            var effectViews = new Dictionary<int, Image>();
            var countViews = new Dictionary<int, Text>();
            foreach (Transform child in GetComponentsInChildren<Transform>(true))
            {
                if (child.name.StartsWith("Slot_", StringComparison.Ordinal) &&
                    int.TryParse(child.name.Substring(5), NumberStyles.Integer, CultureInfo.InvariantCulture, out int index))
                {
                    // The belt prefab names its cells Slot_500..Slot_659; every other grid is
                    // local (Slot_0..). The view dictionary is keyed by local index either way.
                    int local = kind == Kind.Belt ? index - beltStartCell : index;
                    if (local < 0) continue;
                    Button button = child.GetComponent<Button>() ?? child.gameObject.AddComponent<Button>();
                    Image background = child.GetComponent<Image>();
                    if (background != null) button.targetGraphic = background;
                    button.onClick.RemoveAllListeners();
                    int captured = local;
                    button.onClick.AddListener(() => Activate(captured));
                    Metin2AuxiliaryInventorySlotInput input = child.GetComponent<Metin2AuxiliaryInventorySlotInput>() ??
                        child.gameObject.AddComponent<Metin2AuxiliaryInventorySlotInput>();
                    input.Initialize(this, local);
                    views[local] = button;
                    iconViews[local] = FindChild<Image>(child, "Icon");
                    glowViews[local] = CreateSearchGlow(child);
                    effectViews[local] = CreateActivateEffect(child);
                    countViews[local] = FindChild<Text>(child, "Count");
                }
                else if (child.name.StartsWith("Inventory_Tab_", StringComparison.Ordinal) &&
                    int.TryParse(child.name.Substring("Inventory_Tab_".Length), NumberStyles.Integer, CultureInfo.InvariantCulture, out int tab))
                {
                    Button button = child.GetComponent<Button>();
                    if (button == null) continue;
                    button.onClick.RemoveAllListeners();
                    int captured = tab - 1;
                    button.onClick.AddListener(() => SetLocalPage(captured));
                    // Dragging an item over the tab switches the page, exactly like the normal
                    // inventory's Metin2InventoryPageDropTarget does for the I/II tabs.
                    Metin2AuxiliaryPageDropTarget pageTarget = child.GetComponent<Metin2AuxiliaryPageDropTarget>() ??
                        child.gameObject.AddComponent<Metin2AuxiliaryPageDropTarget>();
                    pageTarget.Initialize(this, captured, false);
                    if (inventoryTabs.Length < captured + 1) Array.Resize(ref inventoryTabs, captured + 1);
                    inventoryTabs[captured] = button;
                }
                else if (child.name.StartsWith("Cat_", StringComparison.Ordinal) &&
                    int.TryParse(child.name.Substring("Cat_".Length), NumberStyles.Integer, CultureInfo.InvariantCulture, out int cat))
                {
                    Button button = child.GetComponent<Button>();
                    if (button == null) continue;
                    button.onClick.RemoveAllListeners();
                    int captured = cat;
                    button.onClick.AddListener(() => SetCategory(captured));
                    Metin2AuxiliaryPageDropTarget categoryTarget = child.GetComponent<Metin2AuxiliaryPageDropTarget>() ??
                        child.gameObject.AddComponent<Metin2AuxiliaryPageDropTarget>();
                    categoryTarget.Initialize(this, captured, true);
                    if (categoryTabs.Length < captured + 1) Array.Resize(ref categoryTabs, captured + 1);
                    categoryTabs[captured] = button;
                }
            }
            if (views.Count == 0) return false;
            int max = 0;
            foreach (int key in views.Keys) if (key > max) max = key;
            int total = Mathf.Max(max + 1, Mathf.Min(pageSize, views.Count));
            var cellButtons = new Button[total]; var cellIcons = new Image[total];
            var cellGlows = new Image[total]; var cellCounts = new Text[total];
            var cellEffects = new Image[total];
            for (int index = 0; index < total; index++)
            {
                views.TryGetValue(index, out cellButtons[index]);
                iconViews.TryGetValue(index, out cellIcons[index]);
                glowViews.TryGetValue(index, out cellGlows[index]);
                effectViews.TryGetValue(index, out cellEffects[index]);
                countViews.TryGetValue(index, out cellCounts[index]);
            }
            cells = cellButtons; icons = cellIcons; searchGlows = cellGlows; counts = cellCounts;
            activateEffects = cellEffects;
            activatedSlots = new bool[total]; activatedSlotCells = new int[total];
            UsesImportedWindow = true;
            // beltinventorywindow.py / mountinventory.py name the authored X after the window
            // (BeltInventoryCloseButton / MountInventoryCloseButton) and the importer's TitleBar
            // also carries a plain CloseButton. Every one of them must close the window; the old
            // single lookup bound only the first and left the visible X inert.
            foreach (Button candidate in GetComponentsInChildren<Button>(true))
            {
                string name = candidate.gameObject.name;
                if (name != "CloseButton" && !name.EndsWith("CloseButton", StringComparison.Ordinal)) continue;
                candidate.onClick.RemoveAllListeners();
                candidate.onClick.AddListener(() => CloseRequested?.Invoke());
            }
            Button refresh = Find<Button>(transform, "RefreshButton");
            if (refresh != null) { refresh.onClick.RemoveAllListeners(); refresh.onClick.AddListener(Refresh); }
            // uiscript/beltinventorywindow.py: ExpandBtn -> OpenInventory (reveals the grid),
            // MinimizeBtn -> CloseInventory (collapses it). The Unity window is always expanded, so
            // expand is idempotent and minimize maps to the same close the X button raises.
            Button expand = Find<Button>(transform, "ExpandBtn");
            if (expand != null) { expand.onClick.RemoveAllListeners(); expand.onClick.AddListener(Refresh); }
            Button minimize = Find<Button>(transform, "MinimizeBtn");
            if (minimize != null) { minimize.onClick.RemoveAllListeners(); minimize.onClick.AddListener(() => CloseRequested?.Invoke()); }
            Button safebox = Find<Button>(transform, "Safebox_cat");
            if (safebox != null)
            {
                safebox.onClick.RemoveAllListeners();
                safebox.onClick.AddListener(() => ChatRequested?.Invoke("/click_safebox"));
            }
            // uiextrainventory.py:1045-1051 ClickMallButton -> "/click_mall".
            Button mall = Find<Button>(transform, "Mall_cat");
            if (mall != null)
            {
                mall.onClick.RemoveAllListeners();
                mall.onClick.AddListener(() => ChatRequested?.Invoke("/click_mall"));
            }
            // uiextrainventory.py:92-95 hides every lock cover on load. The imported prefab keeps
            // the nine cover_open_/cover_close_ rows active and raycastable over the grid, which is
            // what made the rows below the first unclickable. Unity has no locked-extra state, so
            // the authored default (hidden) is the correct state here.
            HideLockedRowCovers();
            Debug.Log($"METIN2_AUX_INVENTORY_IMPORTED kind={kind} slots={views.Count} tabs={inventoryTabs.Length} cats={categoryTabs.Length}");
            return true;
        }

        private void HideLockedRowCovers()
        {
            foreach (Transform child in GetComponentsInChildren<Transform>(true))
            {
                string name = child.name;
                if (!name.StartsWith("cover_open_", StringComparison.Ordinal) &&
                    !name.StartsWith("cover_close_", StringComparison.Ordinal)) continue;
                child.gameObject.SetActive(false);
            }
        }

        /// <summary>uiextrainventory.py::__InitExtraInventorySearch, source coordinates intact.</summary>
        private void BuildExtraControls()
        {
            if (kind != Kind.Extra || searchInput != null) return;
            // uiextrainventory.py __InitExtraInventorySearch positions every control on the
            // ExtraInventory window itself (SetParent(self)). The panel component lives on the
            // full-canvas generated carrier, so the search must be parented to the authored window
            // or it lands on the screen edge instead of the board's bottom.
            Transform authored = FindTransform(transform, "ExtraInventory");
            RectTransform root = authored as RectTransform ?? transform as RectTransform;
            if (root == null) return;
            Metin2UiSpriteCatalog sprites = Metin2UiSpriteCatalog.Load();
            Sprite fieldSprite = sprites != null ? sprites.Get("parameter_slot_05") : null;

            // uiextrainventory.py:156-165 searchEdit at (34,456) inside the 100x18 field at (30,454).
            GameObject inputObject = new GameObject("ExtraSearchInput", typeof(RectTransform), typeof(Image), typeof(InputField));
            RectTransform inputRect = (RectTransform)inputObject.transform; inputRect.SetParent(root, false);
            inputRect.anchorMin = inputRect.anchorMax = inputRect.pivot = new Vector2(0f, 1f);
            inputRect.anchoredPosition = new Vector2(30f, -454f); inputRect.sizeDelta = new Vector2(100f, 18f);
            Image inputImage = inputObject.GetComponent<Image>(); inputImage.sprite = fieldSprite; inputImage.type = Image.Type.Sliced;
            searchInput = inputObject.GetComponent<InputField>(); searchInput.characterLimit = 20;
            searchInput.textComponent = CreateSearchText(inputObject.transform, "Text", string.Empty, TextAnchor.MiddleLeft);
            Text placeholder = CreateSearchText(inputObject.transform, "Placeholder", string.Empty, TextAnchor.MiddleLeft);
            placeholder.color = new Color(.55f, .8f, .55f, 1f); searchInput.placeholder = placeholder;
            searchInput.onEndEdit.AddListener(_ => SubmitSearch());

            // 2026-09-29 live: no separate Itemshop / Bezár buttons. A small magnifier at the right
            // edge of the input starts the search. The original extra-inventory icons
            // (new_messenger/btn_1.png) are missing from the shipped pack, so the staged original
            // switchbot search triple is used (search_attr_01..03.sub).
            var iconObject = new GameObject("ExtraSearchButton", typeof(RectTransform), typeof(Image), typeof(Button));
            RectTransform iconRect = (RectTransform)iconObject.transform; iconRect.SetParent(root, false);
            iconRect.anchorMin = iconRect.anchorMax = iconRect.pivot = new Vector2(0f, 1f);
            iconRect.anchoredPosition = new Vector2(112f, -453f); iconRect.sizeDelta = new Vector2(18f, 18f);
            Image iconImage = iconObject.GetComponent<Image>();
            iconImage.sprite = sprites != null ? sprites.Get("switchbot_search_01") : null;
            iconImage.preserveAspect = true;
            searchButton = iconObject.GetComponent<Button>();
            searchButton.targetGraphic = iconImage;
            if (sprites != null)
            {
                searchButton.transition = Selectable.Transition.SpriteSwap;
                searchButton.spriteState = new SpriteState
                {
                    highlightedSprite = sprites.Get("switchbot_search_02"),
                    pressedSprite = sprites.Get("switchbot_search_03"),
                    selectedSprite = sprites.Get("switchbot_search_02")
                };
            }
            searchButton.onClick.AddListener(SubmitSearch);
            iconRect.SetAsLastSibling();
        }

        /// <summary>Pulsing highlight overlay for search hits (above the icon, never eats clicks).</summary>
        private static Image CreateSearchGlow(Transform slot)
        {
            var glowObject = new GameObject("SearchGlow", typeof(RectTransform), typeof(Image));
            RectTransform rect = (RectTransform)glowObject.transform;
            rect.SetParent(slot, false);
            rect.anchorMin = Vector2.zero; rect.anchorMax = Vector2.one;
            rect.offsetMin = Vector2.zero; rect.offsetMax = Vector2.zero;
            Image glow = glowObject.GetComponent<Image>();
            glow.color = new Color(.25f, 1f, .2f, 0f);
            glow.raycastTarget = false;
            glowObject.SetActive(false);
            rect.SetAsLastSibling();
            return glow;
        }

        /// <summary>PythonSlotWindow.cpp:613-645 CSlotWindow::ActivateSlot. The animated highlight
        /// drawn over an active slot (13 frames, size-dependent folder).</summary>
        private static Image CreateActivateEffect(Transform slot)
        {
            if (slot == null) return null;
            var effectObject = new GameObject("SlotActiveEffect", typeof(RectTransform), typeof(Image));
            RectTransform rect = (RectTransform)effectObject.transform;
            rect.SetParent(slot, false);
            rect.anchorMin = Vector2.zero; rect.anchorMax = Vector2.one;
            rect.offsetMin = Vector2.zero; rect.offsetMax = Vector2.zero;
            Image effect = effectObject.GetComponent<Image>();
            effect.raycastTarget = false;
            effectObject.SetActive(false);
            rect.SetAsLastSibling();
            return effect;
        }

        private const int SlotActiveFrameCount = 13;
        // CAniImageBox defaults to m_byDelay=4 client frames (PythonWindow.cpp:1900-1904).
        private const float SlotActiveFrameSeconds = 4f / 60f;

        /// <summary>2026-09-29 live: search hits pulse (about 2 Hz) so they stand out clearly. The
        /// active-slot highlight advances its 13-frame animation on the same tick.</summary>
        private void Update()
        {
            if (kind != Kind.Extra || searchGlows.Length == 0) return;
            bool searchActive = searchHighlights.Count != 0 && Time.unscaledTime < searchHighlightUntil;
            if (!searchActive && searchHighlights.Count != 0)
            {
                foreach (var glow in searchGlows) if (glow != null) glow.gameObject.SetActive(false);
                searchHighlights.Clear();
            }
            if (searchActive)
            {
                float alpha = .18f + .22f * (0.5f + 0.5f * Mathf.Sin(Time.unscaledTime * 8f));
                Color color = new Color(.25f, 1f, .2f, alpha);
                for (int index = 0; index < searchGlows.Length; index++)
                    if (searchGlows[index] != null && searchGlows[index].gameObject.activeSelf)
                        searchGlows[index].color = color;
            }
            if (activateEffects.Length == 0) return;
            int frame = Mathf.FloorToInt(Time.unscaledTime / SlotActiveFrameSeconds) % SlotActiveFrameCount;
            string suffix = frame.ToString("00", CultureInfo.InvariantCulture);
            Metin2UiSpriteCatalog sprites = uiSprites != null ? uiSprites : uiSprites = Metin2UiSpriteCatalog.Load();
            if (sprites == null) return;
            for (int index = 0; index < activateEffects.Length && index < activatedSlots.Length; index++)
            {
                if (!activatedSlots[index] || activateEffects[index] == null) continue;
                activateEffects[index].sprite = sprites.Get(
                    "slot_active_" + activatedSlotCells[index] + "_" + suffix);
                activateEffects[index].enabled = activateEffects[index].sprite != null;
            }
        }

        private static Text CreateSearchText(Transform parent, string name, string value, TextAnchor alignment)
        {
            GameObject child = new GameObject(name, typeof(RectTransform), typeof(Text));
            RectTransform rect = (RectTransform)child.transform; rect.SetParent(parent, false);
            rect.anchorMin = Vector2.zero; rect.anchorMax = Vector2.one; rect.offsetMin = new Vector2(4f, 0f); rect.offsetMax = new Vector2(-4f, 0f);
            Text text = child.GetComponent<Text>(); text.font = Metin2.Runtime.Metin2UiFont.Get(); text.fontSize = 12;
            text.alignment = alignment; text.color = Color.white; text.text = value; text.raycastTarget = false;
            return text;
        }

        public void SubmitSearch()
        {
            searchHighlightUntil=Time.unscaledTime+2.4f;
            if (kind != Kind.Extra || searchInput == null) return;
            string needle = (searchInput.text ?? string.Empty).Trim();
            searchHighlights.Clear();
            if (needle.Length == 0) { Refresh(); return; }
            for (int cell = 0; cell < pageCount * pageSize; cell++)
            {
                if (!state.TryGetExtraItem((ushort)cell, out Metin2GamePacketCodec.ItemData item) || item.Vnum == 0 ||
                    catalog == null || !catalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) || string.IsNullOrEmpty(entry.name)) continue;
                if (entry.name.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) >= 0) searchHighlights.Add(cell);
            }
            int first = int.MaxValue; foreach (int cell in searchHighlights) if (cell < first) first = cell;
            if (first != int.MaxValue) SetPage(first / pageSize); else Refresh();
        }

        public int SearchForDiagnostics(string text)
        {
            if (searchInput == null) return -1;
            searchInput.text = text ?? string.Empty; SubmitSearch(); return searchHighlights.Count;
        }

        public void ClearSearch()
        {
            if (searchInput != null) searchInput.text = string.Empty;
            searchHighlights.Clear(); Refresh();
        }

        private void BuildRuntimeGrid()
        {
            if (grid != null) return;
            grid = transform as RectTransform;
            if (grid == null) return;
            int columns = kind == Kind.Mount ? 12 : kind == Kind.Belt ? 10 : 5;
            int rows = kind == Kind.Mount ? 16 : kind == Kind.Belt ? 16 : 9;
            var layout = grid.GetComponent<GridLayoutGroup>();
            if (layout == null) layout = grid.gameObject.AddComponent<GridLayoutGroup>();
            float cell = kind == Kind.Mount ? 30f : kind == Kind.Belt ? 32f : 40f;
            layout.cellSize = new Vector2(cell, cell);
            layout.spacing = new Vector2(2f, 2f);
            layout.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
            layout.constraintCount = columns;
            int total = columns * rows;
            var cellButtons = new Button[total]; var cellIcons = new Image[total];
            var cellGlows = new Image[total]; var cellCounts = new Text[total];
            var cellEffects = new Image[total];
            for (int index = 0; index < total; index++)
            {
                var slot = new GameObject("AuxSlot_" + index, typeof(RectTransform), typeof(Image), typeof(Button));
                RectTransform rect = (RectTransform)slot.transform; rect.SetParent(grid, false);
                Image background = slot.GetComponent<Image>(); background.color = new Color(.08f, .08f, .1f, .85f);
                Button button = slot.GetComponent<Button>(); button.targetGraphic = background;
                int captured = index;
                button.onClick.AddListener(() => Activate(captured));
                var iconObject = new GameObject("Icon", typeof(RectTransform), typeof(Image)); iconObject.transform.SetParent(slot.transform, false);
                RectTransform iconRect = (RectTransform)iconObject.transform; iconRect.anchorMin = Vector2.zero; iconRect.anchorMax = Vector2.one;
                iconRect.offsetMin = new Vector2(2f, 2f); iconRect.offsetMax = new Vector2(-2f, -2f);
                Image icon = iconObject.GetComponent<Image>(); icon.preserveAspect = true; icon.raycastTarget = false;
                var countObject = new GameObject("Count", typeof(RectTransform), typeof(Text)); countObject.transform.SetParent(slot.transform, false);
                RectTransform countRect = (RectTransform)countObject.transform; countRect.anchorMin = Vector2.zero; countRect.anchorMax = Vector2.one;
                countRect.offsetMin = new Vector2(1f, 0f); countRect.offsetMax = new Vector2(-1f, 0f);
                Text count = countObject.GetComponent<Text>(); count.alignment = TextAnchor.LowerRight;
                count.font = Metin2.Runtime.Metin2UiFont.Get(); count.fontSize = kind == Kind.Mount ? 10 : 12;
                count.color = Color.white; count.raycastTarget = false;
                Metin2AuxiliaryInventorySlotInput input = slot.AddComponent<Metin2AuxiliaryInventorySlotInput>();
                input.Initialize(this, index);
                Image glow = CreateSearchGlow(slot.transform);
                Image effect = CreateActivateEffect(slot.transform);
                cellButtons[index] = button; cellIcons[index] = icon; cellCounts[index] = count; cellGlows[index] = glow;
                cellEffects[index] = effect;
                runtimeCells.Add(slot);
            }
            cells = cellButtons; icons = cellIcons; counts = cellCounts; searchGlows = cellGlows;
            activateEffects = cellEffects;
            activatedSlots = new bool[total]; activatedSlotCells = new int[total];
            if (pageCount > 1)
            {
                var labelObject = new GameObject("AuxPage", typeof(RectTransform), typeof(Text));
                RectTransform labelRect = (RectTransform)labelObject.transform; labelRect.SetParent(grid.parent, false);
                labelRect.anchorMin = labelRect.anchorMax = new Vector2(.5f, 0f);
                labelRect.anchoredPosition = new Vector2(0f, -18f); labelRect.sizeDelta = new Vector2(160f, 20f);
                pageLabel = labelObject.GetComponent<Text>(); pageLabel.font = Metin2.Runtime.Metin2UiFont.Get();
                pageLabel.fontSize = 12; pageLabel.alignment = TextAnchor.MiddleCenter; pageLabel.color = Color.white;
            }
        }

        /// <summary>uiextrainventory.py: GetInventoryPageIndex + page/category selectors.</summary>
        public void SetLocalPage(int value)
        {
            page = Mathf.Clamp(value, 0, tabsPerCategory - 1);
            selectedCell = -1;
            Refresh();
        }

        public void SetCategory(int value)
        {
            category = Mathf.Clamp(value, 0, categoryCount - 1);
            selectedCell = -1;
            Refresh();
        }

        /// <summary>Global page 0..47 for the extra inventory, 0 for the mount bag.</summary>
        public void SetPage(int value)
        {
            int global = Mathf.Clamp(value, 0, pageCount - 1);
            page = global % tabsPerCategory;
            category = global / tabsPerCategory;
            selectedCell = -1;
            Refresh();
        }

        public void NextPage() => SetPage(GlobalPage + 1);
        public void PreviousPage() => SetPage(GlobalPage - 1);

        /// <summary>uiextrainventory.py:615-639 - active new potions use socket[1], auto-potions
        /// socket[0] (constinfo.IS_AUTO_POTION vnums). The extra inventory has no pet-seal
        /// activation in the original.</summary>
        private static bool IsSlotActivated(in Metin2GamePacketCodec.ItemData item, in Metin2ItemCatalog.Entry definition)
        {
            if (definition.subType == Metin2ItemConstants.UseNewPotion)
                return item.Sockets != null && item.Sockets.Length > 1 && item.Sockets[1] != 0;
            if (Metin2ItemCatalog.IsAutoPotion(item.Vnum))
                return item.Sockets != null && item.Sockets.Length > 0 && item.Sockets[0] != 0;
            return false;
        }

        public void Refresh()
        {
            if (cells.Length == 0 || state == null) return;
            for (int index = 0; index < cells.Length; index++)
            {
                bool has = TryGetCell(CellFor(index), out Metin2GamePacketCodec.ItemData item);
                Metin2ItemCatalog.Entry entry = default;
                bool visible = has && catalog != null && catalog.TryGet(item.Vnum, out entry) && entry.icon != null;
                if (icons[index] == null) continue;
                icons[index].sprite = visible ? entry.icon : null; icons[index].enabled = visible;
                bool highlighted = kind == Kind.Extra && searchHighlights.Contains(CellFor(index));
                if (cells[index] != null && cells[index].targetGraphic != null)
                {
                    bool selected = kind == Kind.Extra && CellFor(index) == selectedCell;
                    cells[index].targetGraphic.color = selected ? new Color(1f, .85f, .4f, 1f)
                        : highlighted ? new Color(.2f, .8f, 0f, 1f)
                        : Color.white;
                }
                // 2026-09-29 live: the plain tint was barely visible on the dark slot art; a pulsing
                // overlay marks the found slots without hiding the icon.
                if (searchGlows.Length > index && searchGlows[index] != null)
                    searchGlows[index].gameObject.SetActive(highlighted);
                // uiextrainventory.py:615-639 ActivateSlot: active auto-potions (socket[0]) and new
                // potions (socket[1]) keep the animated highlight; the search highlight wins.
                bool activated = visible && !highlighted && IsSlotActivated(item, entry);
                if (activateEffects.Length > index && activateEffects[index] != null)
                {
                    activateEffects[index].gameObject.SetActive(activated);
                    if (activated)
                    {
                        int size = Mathf.Clamp(entry.size, 1, 3);
                        RectTransform rect = activateEffects[index].rectTransform;
                        rect.anchorMin = new Vector2(0f, 1f); rect.anchorMax = new Vector2(1f, 1f);
                        rect.pivot = new Vector2(.5f, 1f);
                        float height = rect.parent is RectTransform parent && parent.rect.height > 1f ? parent.rect.height : 32f;
                        rect.offsetMin = new Vector2(0f, -height * size); rect.offsetMax = Vector2.zero;
                        rect.SetAsLastSibling();
                    }
                    else activateEffects[index].sprite = null;
                }
                if (index < activatedSlots.Length)
                {
                    activatedSlots[index] = activated;
                    activatedSlotCells[index] = Mathf.Clamp(entry.size, 1, 3);
                }
                if (counts[index] != null)
                    counts[index].text = has && item.Count > 1 ? item.Count.ToString(CultureInfo.InvariantCulture) : string.Empty;
            }
            if (pageLabel != null) pageLabel.text = (GlobalPage + 1).ToString(CultureInfo.InvariantCulture) + " / " + pageCount.ToString(CultureInfo.InvariantCulture);
            for (int index = 0; index < inventoryTabs.Length; index++)
                if (inventoryTabs[index] != null) inventoryTabs[index].interactable = index != page;
            for (int index = 0; index < categoryTabs.Length; index++)
                if (categoryTabs[index] != null) categoryTabs[index].interactable = index != category;
            RefreshAllowedItemList();
            RefreshMobileActions();
        }

        private void BuildAllowedItemList()
        {
            if (kind == Kind.Extra || allowedItemContent != null) return;
            Transform authored = FindTransform(transform, "MountInventoryList");
            RectTransform viewport = authored as RectTransform;
            if (viewport == null) return;
            if (viewport.GetComponent<RectMask2D>() == null) viewport.gameObject.AddComponent<RectMask2D>();
            ScrollRect scroll = viewport.GetComponent<ScrollRect>() ?? viewport.gameObject.AddComponent<ScrollRect>();
            scroll.horizontal = false; scroll.vertical = true; scroll.movementType = ScrollRect.MovementType.Clamped;
            GameObject contentObject = new GameObject("AllowedItemContent", typeof(RectTransform),
                typeof(VerticalLayoutGroup), typeof(ContentSizeFitter));
            allowedItemContent = (RectTransform)contentObject.transform;
            allowedItemContent.SetParent(viewport, false);
            allowedItemContent.anchorMin = new Vector2(0f, 1f); allowedItemContent.anchorMax = new Vector2(1f, 1f);
            allowedItemContent.pivot = new Vector2(.5f, 1f); allowedItemContent.anchoredPosition = Vector2.zero;
            allowedItemContent.sizeDelta = Vector2.zero;
            VerticalLayoutGroup layout = contentObject.GetComponent<VerticalLayoutGroup>();
            layout.spacing = 0f; layout.childControlHeight = true; layout.childControlWidth = true;
            layout.childForceExpandHeight = false; layout.childForceExpandWidth = true;
            ContentSizeFitter fitter = contentObject.GetComponent<ContentSizeFitter>();
            fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
            scroll.viewport = viewport; scroll.content = allowedItemContent;
            uint[] source = kind == Kind.Mount ? MountAllowedItems : BeltAllowedItems;
            Font font = Metin2.Runtime.Metin2UiFont.Get();
            foreach (uint vnum in source)
            {
                GameObject rowObject = new GameObject("AllowedItem_" + vnum, typeof(RectTransform),
                    typeof(Image), typeof(Button), typeof(LayoutElement));
                rowObject.transform.SetParent(allowedItemContent, false);
                rowObject.GetComponent<LayoutElement>().preferredHeight = 16f;
                Image background = rowObject.GetComponent<Image>(); background.color = new Color(0f, 0f, 0f, 0f);
                Button rowButton = rowObject.GetComponent<Button>(); rowButton.targetGraphic = background;
                uint captured = vnum; rowButton.onClick.AddListener(() => WikiItemRequested?.Invoke(captured));
                GameObject labelObject = new GameObject("Name", typeof(RectTransform), typeof(Text));
                RectTransform labelRect = (RectTransform)labelObject.transform; labelRect.SetParent(rowObject.transform, false);
                labelRect.anchorMin = Vector2.zero; labelRect.anchorMax = Vector2.one;
                labelRect.offsetMin = new Vector2(4f, 0f); labelRect.offsetMax = new Vector2(-24f, 0f);
                Text label = labelObject.GetComponent<Text>(); label.font = font; label.fontSize = 11;
                label.alignment = TextAnchor.MiddleLeft; label.raycastTarget = false;
                string itemName = catalog != null && catalog.TryGet(vnum, out Metin2ItemCatalog.Entry entry)
                    ? entry.name : vnum.ToString(CultureInfo.InvariantCulture);
                if (kind == Kind.Belt && itemName != null && itemName.EndsWith("+0", StringComparison.Ordinal))
                    itemName = itemName.Substring(0, itemName.Length - 2).TrimEnd();
                label.text = itemName; allowedItemRows[vnum] = label;
                GameObject infoObject = new GameObject("Info", typeof(RectTransform), typeof(Image), typeof(Button));
                RectTransform infoRect = (RectTransform)infoObject.transform; infoRect.SetParent(rowObject.transform, false);
                infoRect.anchorMin = infoRect.anchorMax = new Vector2(1f, .5f); infoRect.pivot = new Vector2(1f, .5f);
                infoRect.anchoredPosition = new Vector2(-2f, 0f); infoRect.sizeDelta = new Vector2(20f, 15f);
                Image infoImage = infoObject.GetComponent<Image>();
                // uimountinventory.py:37-40 / uiinventory.py belt list: the row's wiki control is the
                // authored xsmall button. The old brown placeholder rectangle is not original art.
                Metin2UiSpriteCatalog infoSprites = Metin2UiSpriteCatalog.Load();
                Sprite infoUp = infoSprites != null ? infoSprites.Get("xsmall_button_01") : null;
                if (infoUp != null)
                {
                    infoImage.sprite = infoUp;
                    infoRect.sizeDelta = new Vector2(infoUp.rect.width, infoUp.rect.height);
                    Button infoButton = infoObject.GetComponent<Button>();
                    infoButton.transition = Selectable.Transition.SpriteSwap;
                    SpriteState state = infoButton.spriteState;
                    state.highlightedSprite = infoSprites.Get("xsmall_button_02");
                    state.pressedSprite = infoSprites.Get("xsmall_button_03");
                    infoButton.spriteState = state;
                }
                else infoImage.color = new Color(.22f, .18f, .12f, .95f);
                infoObject.GetComponent<Button>().onClick.AddListener(() => WikiItemRequested?.Invoke(captured));
                GameObject glyphObject = new GameObject("Glyph", typeof(RectTransform), typeof(Text));
                RectTransform glyphRect = (RectTransform)glyphObject.transform; glyphRect.SetParent(infoRect, false);
                glyphRect.anchorMin = Vector2.zero; glyphRect.anchorMax = Vector2.one; glyphRect.offsetMin = glyphRect.offsetMax = Vector2.zero;
                Text glyph = glyphObject.GetComponent<Text>(); glyph.font = font; glyph.fontSize = 11;
                glyph.alignment = TextAnchor.MiddleCenter; glyph.text = "i"; glyph.color = Color.white; glyph.raycastTarget = false;
            }
        }

        private void RefreshAllowedItemList()
        {
            if (allowedItemRows.Count == 0 || state == null) return;
            var owned = new HashSet<uint>();
            int first = kind == Kind.Belt ? beltStartCell : 0;
            int count = kind == Kind.Belt ? 160 : 192;
            for (int index = 0; index < count; index++)
                if (TryGetCell(first + index, out Metin2GamePacketCodec.ItemData item) && item.Vnum != 0)
                    owned.Add(item.Vnum);
            foreach (KeyValuePair<uint, Text> pair in allowedItemRows)
            {
                bool has = kind == Kind.Belt
                    ? HasBeltFamily(owned, pair.Key)
                    : owned.Contains(pair.Key);
                pair.Value.color = has ? new Color(.2f, .8f, .2f, 1f) : new Color(.9f, .2f, .2f, 1f);
            }
        }

        private static bool HasBeltFamily(HashSet<uint> owned, uint baseVnum)
        {
            for (uint offset = 0; offset < 10; offset++) if (owned.Contains(baseVnum + offset)) return true;
            return false;
        }

        public bool RequestItemLink(int index)
        {
            if(state==null||catalog==null||index<0||index>=cells.Length||!TryGetCell(CellFor(index),out var item)||
                item.Vnum==0||!catalog.TryGet(item.Vnum,out var entry))return false;
            ItemLinkRequested?.Invoke(Metin2ChatItemLink.BuildItemLink(item,entry.name));return true;
        }

        public void Activate(int index)
        {
            if(Input.GetKey(KeyCode.LeftAlt)||Input.GetKey(KeyCode.RightAlt)) { RequestItemLink(index);return; }
            if (state == null || index < 0 || index >= cells.Length) return;
            int cell = CellFor(index);
            bool has = TryGetCell(cell, out Metin2GamePacketCodec.ItemData item);
            // Original uimountinventory.py:149 / uiextrainventory SelectItemSlot: a carried item
            // dropped on a cell moves it, a click on the carried item's own cell releases it, and
            // a click on a filled cell with an empty cursor picks it up.
            if (cursor != null && cursor.IsAttached)
            {
                if (cursor.Type == AttachType && cursor.SlotNumber == cell) { cursor.Deattach(); return; }
                Drop(index);
                return;
            }
            if (!has || item.Vnum == 0)
            {
                // The normal inventory's SelectEmptySlot clears the current selection; the touch
                // action strip must disappear with it.
                if (kind == Kind.Extra && selectedCell >= 0) { selectedCell = -1; Refresh(); }
                return;
            }
            // uiextrainventory.py:764-770 (uiinventory.py:771 on the normal inventory): Shift opens
            // the pick-amount dialog instead of picking the whole stack up.
            if ((cursor == null || !cursor.IsAttached) && Input.GetKey(KeyCode.LeftShift) && item.Count > 1)
            {
                SplitRequested?.Invoke(window, (ushort)cell, item.Count);
                return;
            }
            // 2026-09-29 live feedback: behave like the normal inventory. A left click selects the
            // item and the touch action bar (UNITY_ITEM_EQUIP / UNITY_ITEM_USE / UNITY_ITEM_SPLIT)
            // offers the real actions; a drag still picks the stack up through BeginSlotDrag.
            if (kind == Kind.Extra)
            {
                selectedCell = cell;
                Refresh();
                return;
            }
            if (catalog != null && catalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) && entry.icon != null)
                cursor?.Attach(this, AttachType, cell, item.Vnum, entry.icon, item.Count);
            SlotActivated?.Invoke(window, (ushort)cell, item);
        }

        /// <summary>
        /// Drag start on an auxiliary cell (uiextrainventory.py SelectItemSlot AttachObject). The
        /// plain click opens the item menu, so the attachment for a move starts here.
        /// </summary>
        public void BeginSlotDrag(int index)
        {
            if(Input.GetKey(KeyCode.LeftAlt)||Input.GetKey(KeyCode.RightAlt))return;
            if (cursor == null || index < 0 || index >= cells.Length) return;
            if (cursor.IsAttached && ReferenceEquals(cursor.Owner, this)) return;
            int cell = CellFor(index);
            if (!TryGetCell(cell, out Metin2GamePacketCodec.ItemData item) || item.Vnum == 0) return;
            if (catalog == null || !catalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) || entry.icon == null) return;
            cursor.Attach(this, AttachType, cell, item.Vnum, entry.icon, item.Count);
            Metin2SoundManager.Instance.PlaySound("sound/ui/pick.wav");
        }

        /// <summary>
        /// game.py:1347 __PutItem / __DropItem: a carried item released over nothing (or the world
        /// touch area) opens the drop confirmation. The bootstrap maps the UI window to the wire.
        /// </summary>
        public void CancelSlotDrag(Vector2 screenPosition, bool releasedOutsideUi)
        {
            if (releasedOutsideUi && cursor != null && cursor.IsAttached && ReferenceEquals(cursor.Owner, this) &&
                cursor.SlotNumber >= 0)
                WorldReleaseRequested?.Invoke(window, (ushort)cursor.SlotNumber,
                    Mathf.Clamp(cursor.Count, 1, 50000), screenPosition);
            cursor?.Deattach();
            selectedCell = -1;
            Refresh();
        }

        /// <summary>
        /// uiextrainventory.py OnPickItem (:476-492): the amount chosen in the pick dialog is what
        /// the shared cursor carries, so the following move sends exactly that many.
        /// </summary>
        public bool AttachSplit(ushort cell, int requestedCount)
        {
            if (cursor == null || cursor.IsAttached || requestedCount <= 0) return false;
            if (!TryGetCell(cell, out Metin2GamePacketCodec.ItemData item) || item.Count <= 1) return false;
            if (catalog == null || !catalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry entry) || entry.icon == null) return false;
            selectedCell = -1;
            cursor.Attach(this, AttachType, cell, item.Vnum, entry.icon, Mathf.Clamp(requestedCount, 1, item.Count));
            Refresh();
            return true;
        }

        /// <summary>
        /// The carried stack size a cross-window move must send (uiextrainventory.py
        /// SelectEmptySlot/SelectItemSlot use mouseController.GetAttachedItemCount). The handlers
        /// used to hard-code 1, which moved a single item out of a ten-item stack.
        /// </summary>
        public static int MoveCountFor(Metin2CursorAttachment attachment, int fallback = 1)
            => Mathf.Clamp(attachment != null && attachment.IsAttached && attachment.Count > 0
                ? attachment.Count : fallback, 1, 50000);

        // ---------------------------------------------------------------- touch item actions

        /// <summary>
        /// Touch item action strip, copied from Metin2InventoryPanel.ConfigureMobilePresentation /
        /// RefreshMobileActions so the extra inventory behaves exactly like the normal inventory:
        /// a small dark panel under the selected item with the real actions (equip/use/split).
        /// The previous full-screen dim layer is gone; nothing covers the game.
        /// </summary>
        public void ConfigureMobilePresentation()
        {
            if (kind != Kind.Extra) return;
            if (mobileActions == null) BuildMobileActions();
            RefreshMobileActions();
        }

        private void BuildMobileActions()
        {
            Transform slotHost = FindTransform(transform, "ItemSlot");
            RectTransform host = slotHost != null ? slotHost.parent as RectTransform : transform as RectTransform;
            if (host == null) return;
            Transform existing = host.Find("MobileItemActions");
            if (existing != null)
            {
                mobileActions = existing as RectTransform;
                mobileUseButton = FindButton(existing, "Use");
                mobileSplitButton = FindButton(existing, "Split");
                return;
            }
            var panelObject = new GameObject("MobileItemActions", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
            mobileActions = (RectTransform)panelObject.transform;
            mobileActions.SetParent(host, false);
            mobileActions.anchorMin = mobileActions.anchorMax = new Vector2(.5f, .5f);
            mobileActions.pivot = new Vector2(.5f, 1f);
            mobileActions.sizeDelta = new Vector2(104f, 64f);
            Image background = panelObject.GetComponent<Image>();
            background.color = new Color(.025f, .025f, .025f, .94f);
            background.raycastTarget = true;
            mobileUseButton = CreateMobileActionButton(mobileActions, "Use", "UNITY_ITEM_USE", -2f, UseSelectedItem);
            mobileSplitButton = CreateMobileActionButton(mobileActions, "Split", "UNITY_ITEM_SPLIT", -33f, SplitSelectedStack);
            mobileActions.SetAsLastSibling();
        }

        private static Button FindButton(Transform root, string name)
        {
            Transform found = root != null ? root.Find(name) : null;
            return found != null ? found.GetComponent<Button>() : null;
        }

        private static Button CreateMobileActionButton(RectTransform parent, string name, string label, float y,
            UnityEngine.Events.UnityAction onClick)
        {
            var buttonObject = new GameObject(name, typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(Button));
            RectTransform rect = (RectTransform)buttonObject.transform;
            rect.SetParent(parent, false);
            rect.anchorMin = rect.anchorMax = new Vector2(.5f, 1f);
            rect.pivot = new Vector2(.5f, 1f);
            rect.anchoredPosition = new Vector2(0f, y);
            rect.sizeDelta = new Vector2(98f, 28f);
            Image image = buttonObject.GetComponent<Image>();
            image.color = new Color(.15f, .13f, .1f, .98f);
            Button button = buttonObject.GetComponent<Button>();
            button.targetGraphic = image;
            button.onClick.AddListener(onClick);
            var textObject = new GameObject("Label", typeof(RectTransform), typeof(CanvasRenderer), typeof(Text));
            RectTransform textRect = (RectTransform)textObject.transform;
            textRect.SetParent(rect, false);
            textRect.anchorMin = Vector2.zero;
            textRect.anchorMax = Vector2.one;
            textRect.offsetMin = textRect.offsetMax = Vector2.zero;
            Text text = textObject.GetComponent<Text>();
            text.font = Metin2.Runtime.Metin2UiFont.Get();
            text.fontSize = 15;
            text.alignment = TextAnchor.MiddleCenter;
            text.color = Color.white;
            text.raycastTarget = false;
            text.text = label;
            textObject.AddComponent<Metin2LocalizedLabel>().Key = label;
            return button;
        }

        private void UseSelectedItem()
        {
            int cell = selectedCell;
            if (cell < 0 || !TryGetCell(cell, out Metin2GamePacketCodec.ItemData item) || item.Vnum == 0) return;
            selectedCell = -1;
            UseRequested?.Invoke(window, (ushort)cell, 1);
            Refresh();
        }

        private void SplitSelectedStack()
        {
            int cell = selectedCell;
            if (cell < 0 || !TryGetCell(cell, out Metin2GamePacketCodec.ItemData item) || item.Count <= 1) return;
            selectedCell = -1;
            SplitRequested?.Invoke(window, (ushort)cell, item.Count);
            Refresh();
        }

        private void RefreshMobileActions()
        {
            if (mobileActions == null) return;
            if (kind != Kind.Extra || selectedCell < 0 || (cursor != null && cursor.IsAttached) ||
                !TryGetCell(selectedCell, out Metin2GamePacketCodec.ItemData item) || item.Vnum == 0 ||
                catalog == null || !catalog.TryGet(item.Vnum, out Metin2ItemCatalog.Entry definition))
            {
                mobileActions.gameObject.SetActive(false);
                return;
            }
            bool wearable = IsWearable(definition.itemType);
            bool usable = wearable || IsUsable(definition);
            // Live rule: split only for a real stack (count > 1 and the item is stackable), and
            // use/equip only when the item can actually be used or worn.
            bool stackable = item.Count > 1 && definition.IsFlag(Metin2ItemConstants.ItemFlagStackable);
            mobileActions.gameObject.SetActive(usable || stackable);
            if (mobileUseButton != null) mobileUseButton.gameObject.SetActive(usable);
            if (mobileUseButton != null)
            {
                Text label = mobileUseButton.GetComponentInChildren<Text>(true);
                if (label != null)
                {
                    Metin2LocalizedLabel localized = label.GetComponent<Metin2LocalizedLabel>();
                    if (localized == null) localized = label.gameObject.AddComponent<Metin2LocalizedLabel>();
                    localized.Key = wearable ? "UNITY_ITEM_EQUIP" : "UNITY_ITEM_USE";
                }
            }
            if (mobileSplitButton != null) mobileSplitButton.gameObject.SetActive(stackable);
            mobileActions.sizeDelta = new Vector2(104f, stackable && usable ? 64f : 32f);
            PositionMobileActions(selectedCell);
            mobileActions.SetAsLastSibling();
        }

        private void PositionMobileActions(int cell)
        {
            RectTransform host = mobileActions.parent as RectTransform;
            if (host == null) return;
            RectTransform slot = null;
            for (int index = 0; index < cells.Length; index++)
                if (CellFor(index) == cell) { slot = cells[index] != null ? cells[index].transform as RectTransform : null; break; }
            if (slot == null) return;
            var corners = new Vector3[4];
            slot.GetWorldCorners(corners);
            Vector3 bottomCenter = (corners[0] + corners[3]) * .5f;
            Vector2 point = host.InverseTransformPoint(bottomCenter);
            float half = mobileActions.rect.width * .5f;
            point.x = Mathf.Clamp(point.x, host.rect.xMin + half, host.rect.xMax - half);
            point.y -= 4f;
            mobileActions.localPosition = new Vector3(point.x, point.y, 0f);
        }

        /// <summary>Metin2InventoryPanel.IsWearable - the types that equip instead of use.</summary>
        private static bool IsWearable(int itemType)
        {
            switch (itemType)
            {
                case Metin2ItemConstants.TypeWeapon:
                case Metin2ItemConstants.TypeArmor:
                case Metin2ItemConstants.TypeRod:
                case Metin2ItemConstants.TypePick:
                case Metin2ItemConstants.TypeCostume:
                case Metin2ItemConstants.TypeRing:
                case Metin2ItemConstants.TypeBelt:
                    return true;
                default:
                    return false;
            }
        }

        /// <summary>Usable/openable types: USE (potions, chests, adders), QUEST and the box types.
        /// ITEM_TYPE_TREASURE_BOX = 20 and ITEM_TYPE_GIFTBOX = 23 (ItemData.h:134-137).</summary>
        private static bool IsUsable(in Metin2ItemCatalog.Entry definition)
        {
            switch (definition.itemType)
            {
                case Metin2ItemConstants.TypeUse:
                case Metin2ItemConstants.TypeQuest:
                case 20:
                case 23:
                    return true;
                default:
                    return false;
            }
        }

        /// <summary>Drop of an attached item: the bootstrap maps it to checkin / item move.</summary>
        public void Drop(int index)
        {
            if (index < 0 || index >= cells.Length) return;
            selectedCell = -1;
            SlotDropRequested?.Invoke(window, (ushort)CellFor(index));
            Refresh();
        }

        public void Use(int index)
        {
            if (index < 0 || index >= cells.Length) return;
            int cell = CellFor(index);
            if (!TryGetCell(cell, out Metin2GamePacketCodec.ItemData item) || item.Vnum == 0) return;
            selectedCell = -1;
            cursor?.Deattach();
            // uiextrainventory.py:974-984 UseItemSlot: Ctrl uses the whole stack, otherwise one.
            bool useAll = Input.GetKey(KeyCode.LeftControl) || Input.GetKey(KeyCode.RightControl);
            UseRequested?.Invoke(window, (ushort)cell, useAll ? Mathf.Max(1, item.Count) : 1);
            Refresh();
        }

        public void Hover(int index)
        {
            int cell = CellFor(index);
            Metin2GamePacketCodec.ItemData item = default;
            bool has = state != null && TryGetCell(cell, out item);
            if (has && item.Vnum != 0) {
#if UNITY_ANDROID || UNITY_IOS
                toolTips?.ShowInventoryItemAttached(item, FindTransform(transform, "ExtraInventory") as RectTransform ?? transform as RectTransform);
#else
                toolTips?.ShowInventoryItem(item);
#endif
                SlotHovered?.Invoke(window, (ushort)cell); }
            else { toolTips?.HideAll(); HoverExited?.Invoke(); }
        }

        public void HoverOut()
        {
            toolTips?.HideAll();
            HoverExited?.Invoke();
        }

        private static T Find<T>(Transform root, string name) where T : Component
        {
            foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
                if (string.Equals(child.name, name, StringComparison.Ordinal)) return child.GetComponent<T>();
            return null;
        }

        private static T FindChild<T>(Transform root, string name) where T : Component
        {
            foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
                if (child != root && string.Equals(child.name, name, StringComparison.Ordinal)) return child.GetComponent<T>();
            return null;
        }

        private static Transform FindTransform(Transform root, string name)
        {
            foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
                if (string.Equals(child.name, name, StringComparison.Ordinal)) return child;
            return null;
        }
    }

    /// <summary>Switches extra-inventory pages (and categories) while one of its items is held over
    /// a tab, mirroring Metin2InventoryPageDropTarget on the normal bag's I/II tabs.</summary>
    public sealed class Metin2AuxiliaryPageDropTarget : MonoBehaviour, IPointerEnterHandler, IDropHandler
    {
        private Metin2AuxiliaryInventoryPanel panel;
        private int value;
        private bool category;

        public void Initialize(Metin2AuxiliaryInventoryPanel owner, int index, bool isCategory)
        { panel = owner; value = index; category = isCategory; }

        private void Apply()
        {
            if (panel == null || !panel.HasAuxiliaryDrag) return;
            if (category) panel.SetCategory(value); else panel.SetLocalPage(value);
        }

        public void OnPointerEnter(PointerEventData eventData) => Apply();
        public void OnDrop(PointerEventData eventData) => Apply();
    }

    /// <summary>Pointer bridge for the imported and runtime auxiliary grids.</summary>
    public sealed class Metin2AuxiliaryInventorySlotInput : MonoBehaviour, IPointerClickHandler,
        IPointerEnterHandler, IPointerExitHandler, IDropHandler, IBeginDragHandler, IDragHandler, IEndDragHandler
    {
        private Metin2AuxiliaryInventoryPanel panel;
        private int index;
        public void Initialize(Metin2AuxiliaryInventoryPanel owner, int slotIndex) { panel = owner; index = slotIndex; }
        public void OnPointerClick(PointerEventData eventData)
        {
            if(eventData.button==PointerEventData.InputButton.Left&&(Input.GetKey(KeyCode.LeftAlt)||Input.GetKey(KeyCode.RightAlt)))return;
            // Button.onClick owns the single left-click pickup/drop. Calling Activate here as well
            // used to execute it twice (attach, then immediately detach), which made every auxiliary
            // inventory item appear unclickable. Right click / double click are UseItemSlot.
            if (eventData.button == PointerEventData.InputButton.Right || eventData.clickCount >= 2)
                panel?.Use(index);
        }
        public void OnPointerEnter(PointerEventData eventData) => panel?.Hover(index);
        public void OnPointerExit(PointerEventData eventData) => panel?.HoverOut();
        public void OnDrop(PointerEventData eventData) => panel?.Drop(index);
        public void OnBeginDrag(PointerEventData eventData)
        {
            panel?.BeginSlotDrag(index);
        }
        public void OnDrag(PointerEventData eventData) { }
        public void OnEndDrag(PointerEventData eventData)
        {
            GameObject hit = eventData.pointerCurrentRaycast.gameObject;
            bool releasedOnWorld = hit == null || hit.GetComponentInParent<Metin2MobileCameraTouchArea>() != null;
            panel?.CancelSlotDrag(eventData.position, releasedOnWorld);
        }
    }
}
