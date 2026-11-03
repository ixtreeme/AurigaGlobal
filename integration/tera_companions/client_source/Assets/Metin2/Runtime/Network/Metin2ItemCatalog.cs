using System;
using System.Collections.Generic;
using UnityEngine;

namespace Metin2.Runtime.Network
{
    [CreateAssetMenu(menuName = "Metin2/Network Item Catalog")]
    public sealed class Metin2ItemCatalog : ScriptableObject
    {
        /// <summary>CItemData::ITEM_LIMIT_MAX_NUM (GameLib/ItemData.h:15), what item.LIMIT_MAX_NUM is.</summary>
        public const int LimitCount = 2;
        /// <summary>CItemData::ITEM_APPLY_MAX_NUM (ItemData.h:18) = item.ITEM_APPLY_MAX_NUM.</summary>
        public const int ApplyCount = 5;
        /// <summary>CItemData::ITEM_VALUES_MAX_NUM (ItemData.h:16) - item.GetValue(0..5).</summary>
        public const int ValueCount = 6;

        /// <summary>
        /// constinfo.py:59 TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE. The file sets it twice - 0 at
        /// :46 and 10 at :59 - and the later assignment is the one that reaches
        /// app.SetTwoHandedWeaponAttSpeedDecreaseValue (:156). PythonItemModule.cpp:256-259 subtracts
        /// it from APPLY_ATT_SPEED on two-handed weapons before the tooltip ever sees the value.
        /// </summary>
        public const int TwoHandedAttackSpeedDecrease = 10;

        /// <summary>
        /// <c>size</c> is the inventory height in cells (1..3) taken from <c>item_proto.size</c>.
        /// PythonItemModule.cpp:111 <c>itemGetItemSize</c> returns <c>(1, GetSize())</c>, so an item is
        /// always one column wide and only its height varies; a two-cell item at cell C also covers
        /// C + 5, and a three-cell item C + 5 and C + 10.
        ///
        /// The numeric fields below are the raw <c>item_proto</c> row, extracted from the server dump
        /// by <c>Migration/Tools/extract_item_size.py</c>. root/uitooltip.py reads exactly these through
        /// <c>item.GetValue / GetLimit / GetAffect / GetItemType / GetItemSubType / IsAntiFlag</c>, so
        /// they are stored in the same shape rather than pre-interpreted.
        /// </summary>
        [Serializable]
        public struct Entry
        {
            public uint vnum;
            public string name, description, type, iconPath, modelPath;
            public Sprite icon;
            // uirune.py RefreshItemSlot does not display the transparent base icon for the six
            // outer Sharingan runes.  It uses <vnum>_1.tga while inactive and <vnum>_2.tga while
            // active through SetCoverButton.  Keep those authored states in the catalog so the
            // Unity socket renderer can reproduce the same rule without runtime AssetDatabase use.
            public Sprite runeInactiveIcon, runeActiveIcon;
            /// <summary>Converted item model used by the ground-item instance manager.</summary>
            public GameObject model;
            public byte size;

            /// <summary>CItemData::EItemType (ItemData.h:22) - item.GetItemType().</summary>
            public int itemType;
            /// <summary>Weapon / armour / use sub type - item.GetItemSubType().</summary>
            public int subType;
            /// <summary>EItemAntiFlag bit field (ItemData.h:189) - item.IsAntiFlag(mask).</summary>
            public int antiFlag;
            /// <summary>EItemFlag bit field (ItemData.h:203) - item.IsFlag(mask).</summary>
            public int itemFlag;
            /// <summary>EItemWearableFlag bit field (ItemData.h:245) - item.IsWearableFlag(mask).</summary>
            public int wearFlag;

            /// <summary>
            /// Adjacent client-proto DWORDs. SQL/server calls them dwGold/dwShopBuyPrice; the
            /// binary-compatible client calls them dwIBuyItemPrice/dwISellItemPrice.
            /// </summary>
            public int buyItemPrice, sellItemPrice;

            public int limitType0, limitValue0, limitType1, limitValue1;
            public int applyType0, applyValue0, applyType1, applyValue1, applyType2, applyValue2;
            public int applyType3, applyValue3, applyType4, applyValue4;
            public int rarity, extraApplyType0, extraApplyValue0, extraApplyType1, extraApplyValue1;
            public int value0, value1, value2, value3, value4, value5;
            /// <summary>item_proto.bSpecular, the 0..100 sphere-map mask multiplier.</summary>
            public int specular;
            /// <summary>
            /// item_proto.bGainSocketPct. CItemData::GetSocketCount returns this byte and
            /// CInstanceBase::__GetRefinedEffect adds it to vnum % 10.
            /// </summary>
            public int socketPct;

            /// <summary>item.GetValue(index) - PythonItemModule.cpp:264.</summary>
            public int GetValue(int index)
            {
                switch (index)
                {
                    case 0: return value0;
                    case 1: return value1;
                    case 2: return value2;
                    case 3: return value3;
                    case 4: return value4;
                    case 5: return value5;
                    default: return 0;
                }
            }

            /// <summary>item.GetLimit(index) - PythonItemModule.cpp:225, a (type, value) pair.</summary>
            public void GetLimit(int index, out int limitType, out int limitValue)
            {
                switch (index)
                {
                    case 0: limitType = limitType0; limitValue = limitValue0; return;
                    case 1: limitType = limitType1; limitValue = limitValue1; return;
                    default: limitType = 0; limitValue = 0; return;
                }
            }

            /// <summary>
            /// item.GetAffect(index) - PythonItemModule.cpp:242, including the two-handed weapon
            /// correction at :256-259 that the client applies before returning the pair.
            /// </summary>
            public void GetAffect(int index, out int affectType, out int affectValue)
            {
                switch (index)
                {
                    case 0: affectType = applyType0; affectValue = applyValue0; break;
                    case 1: affectType = applyType1; affectValue = applyValue1; break;
                    case 2: affectType = applyType2; affectValue = applyValue2; break;
                    case 3: affectType = applyType3; affectValue = applyValue3; break;
                    case 4: affectType = applyType4; affectValue = applyValue4; break;
                    default: affectType = 0; affectValue = 0; return;
                }
                if (affectType == Metin2ItemConstants.ApplyAttackSpeed &&
                    itemType == Metin2ItemConstants.TypeWeapon &&
                    subType == Metin2ItemConstants.WeaponTwoHanded)
                    affectValue -= TwoHandedAttackSpeedDecrease;
            }

            /// <summary>item.IsAntiFlag(mask) - PythonItemModule.cpp itemIsAntiFlag.</summary>
            public bool IsAntiFlag(int mask) => (antiFlag & mask) != 0;

            /// <summary>item.IsFlag(mask), including ITEM_FLAG_COUNT_PER_1GOLD.</summary>
            public bool IsFlag(int mask) => (itemFlag & mask) != 0;

            /// <summary>item.IsWearableFlag(mask).</summary>
            public bool IsWearableFlag(int mask) => (wearFlag & mask) != 0;
        }

        public Entry[] entries = Array.Empty<Entry>();
        private Dictionary<uint, Entry> lookup;
        private Dictionary<uint, Metin2.Runtime.WeaponDefinition> toolDefinitions;

        /// <summary>Rods and picks have item-list models but are absent from the weapon-only
        /// equipment catalog. Attach their converted models through the normal hand socket.</summary>
        public bool TryGetToolDefinition(uint vnum, out Metin2.Runtime.WeaponDefinition definition)
        {
            definition = null;
            if (!TryGet(vnum, out Entry item) ||
                (item.itemType != Metin2ItemConstants.TypeRod && item.itemType != Metin2ItemConstants.TypePick) ||
                item.model == null) return false;
            if (toolDefinitions == null) toolDefinitions = new Dictionary<uint, Metin2.Runtime.WeaponDefinition>();
            if (toolDefinitions.TryGetValue(vnum, out definition) && definition != null) return true;
            definition = ScriptableObject.CreateInstance<Metin2.Runtime.WeaponDefinition>();
            definition.name = "Tool_" + vnum;
            definition.hideFlags = HideFlags.DontSave;
            definition.staticModel = item.model;
            definition.weaponType = Metin2.Runtime.Metin2WeaponType.None;
            toolDefinitions[vnum] = definition;
            return true;
        }

        /// <summary>CItemData::ITEM_TYPE_USE (GameLib/ItemData.h:27).</summary>
        public const int ItemTypeUse = 3;
        /// <summary>CItemData::ITEM_TYPE_METIN (ItemData.h) - the stones that socket into gear.</summary>
        public const int ItemTypeMetin = 10;
        /// <summary>EUseSubTypes: POTION 0, TALISMAN 1, TUNING 2 (ItemData.h:111-116).</summary>
        public const int UseSubTypeTuning = 2;

        /// <summary>
        /// itemIsRefineScroll (PythonItemModule.cpp:346-368): an ITEM_TYPE_USE item whose subtype is
        /// USE_TUNING. __DropSrcItemToDestItemInInventory (uiinventory.py:803-810) tests this before
        /// anything else, and takes it to the refine window instead of moving the item.
        /// </summary>
        public bool IsRefineScroll(uint vnum)
            => TryGet(vnum, out Entry entry) && entry.itemType == ItemTypeUse && entry.subType == UseSubTypeTuning;

        /// <summary>
        /// uiinventory.py __DropSrcItemToDestItemInInventory (:790-819): an item dropped on another
        /// item is USED on it when it is a treasure key (item.IsKey), carries ITEM_FLAG_APPLICABLE,
        /// or its use type is one of USE_TYPE_TUPLE (:480). Everything else is a plain move. The
        /// bonus adders (USE_ADD_ATTRIBUTE / USE_ADD_ATTRIBUTE2 / USE_CHANGE_ATTRIBUTE) and the
        /// socket/accessory/belt/ring put-in items all fall in the tuple.
        /// </summary>
        public static bool IsUseToItemSource(in Entry entry)
        {
            if (entry.itemType == Metin2ItemConstants.TypeTreasureKey) return true;
            if ((entry.itemFlag & Metin2ItemConstants.ItemFlagApplicable) != 0) return true;
            if (entry.itemType != ItemTypeUse) return false;
            switch (entry.subType)
            {
                case Metin2ItemConstants.UseCleanSocket:
                case Metin2ItemConstants.UseChangeAttribute:
                case Metin2ItemConstants.UseAddAttribute:
                case Metin2ItemConstants.UseAddAccessorySocket:
                case Metin2ItemConstants.UsePutIntoAccessorySocket:
                case Metin2ItemConstants.UseAddAttribute2:
                case Metin2ItemConstants.UsePutIntoBeltSocket:
                case Metin2ItemConstants.UsePutIntoRingSocket:
                case Metin2ItemConstants.UseChangeCostumeAttr:
                case Metin2ItemConstants.UseResetCostumeAttr:
                // ATTR_LOCK: GetUseTypeString maps ADD_LOCK to USE_ADD_ATTRIBUTE and both
                // CHANGE_LOCK and DELETE_LOCK to USE_CHANGE_ATTRIBUTE (ItemData.cpp:336-343).
                case Metin2ItemConstants.UseAddAttributeLock:
                case Metin2ItemConstants.UseChangeAttributeLock:
                case Metin2ItemConstants.UseDeleteAttributeLock:
                // ENABLE_ATTR_COSTUMES: 70402/70403/70404 have their own explicit branches in
                // uiinventory.py:2366-2376 and must reach the costume as item-on-item use.
                case Metin2ItemConstants.UseAddAttrCostume1:
                case Metin2ItemConstants.UseAddAttrCostume2:
                case Metin2ItemConstants.UseRemoveAttrCostume:
                    return true;
                default:
                    return false;
            }
        }

        /// <summary>CItemData::IsEquipment (ItemData.cpp): weapon or armour only.</summary>
        public static bool IsEquipment(in Entry entry)
            => entry.itemType == Metin2ItemConstants.TypeWeapon || entry.itemType == Metin2ItemConstants.TypeArmor;

        /// <summary>constinfo.py:695-710 IS_AUTO_POTION: HP 72725/72726, SP 72729/72730.</summary>
        public static bool IsAutoPotion(uint vnum)
            => vnum == 72725 || vnum == 72726 || vnum == 72729 || vnum == 72730;
        /// <summary>constinfo.py:712-716 IS_INFINITE_AUTO_POTION: 72726 / 72730.</summary>
        public static bool IsInfiniteAutoPotion(uint vnum) => vnum == 72726 || vnum == 72730;
        /// <summary>constinfo.py:745-749 IS_PET_SEAL (NEW_PET_SYSTEM): 55701..55711. Summoned
        /// state is socket[0].</summary>
        public static bool IsNewPetSeal(uint vnum) => vnum >= 55701 && vnum <= 55711;
        /// <summary>constinfo.py:751-765 IS_PET_SEAL_OLD: the classic pet boxes. Summoned state is
        /// socket[2] (CPetActor::Summon/Unsummon, PetSystem.cpp:142/232). The 53290-53297 block is
        /// the Auriga dragon pets (53293 Dragon Epic, 53294 Dragon Legend); 53001-53005 are the
        /// original Phoenix/reindeer/Ice-Phoenix seals (CMobVnumHelper::IsPetUsingPetSystem,
        /// VnumHelper.h:17-19) plus Baby Azrael, which the shipped constinfo range missed.</summary>
        public static bool IsOldPetSeal(uint vnum)
            => vnum == 38200 || vnum == 38201 || (vnum >= 53001 && vnum <= 53283) ||
               vnum == 48301 || vnum == 48311 || vnum == 48321 || vnum == 49010 || vnum == 49050 ||
               (vnum >= 60101 && vnum <= 60104) || (vnum >= 53290 && vnum <= 53297) ||
               (vnum >= 631000 && vnum <= 631068);

        public bool TryGet(uint vnum, out Entry entry)
        {
            if (lookup == null)
            {
                lookup = new Dictionary<uint, Entry>(entries.Length);
                foreach (Entry value in entries) lookup[value.vnum] = value;
            }
            if (lookup.TryGetValue(vnum, out entry))
            {
                if (Application.isPlaying)
                {
                    if(Metin2Language.TryGetItemText(vnum,out string name,out string description))
                    { entry.name=name;entry.description=description; }
                    else
                    { entry.name=vnum.ToString(Metin2Language.Culture);entry.description=string.Empty; }
                }
                Metin2WoodcuttingAssets.OverrideItem(ref entry);
                Metin2HerbalismAssets.OverrideItem(ref entry);
                return true;
            }
            // The original hu item_list omits 71031 even though item_proto, item_names and
            // itemdesc all define it. Its artwork was also omitted from the pack; use the
            // adjacent Dragon God item artwork so item-shop cards remain identifiable.
            if (vnum == 71031 && lookup.TryGetValue(71030, out entry))
            {
                entry.vnum = vnum;
                Metin2Language.TryGetItemText(vnum, out entry.name, out entry.description);
                entry.subType = 10;
                return true;
            }
            return false;
        }

        private void OnEnable() { lookup = null; toolDefinitions = null; }
    }
}
