using System;
using System.Text;

namespace Metin2.Runtime.Network
{
    /// <summary>Fixed-layout Auriga application packets carried by the TLS stream.</summary>
    public static class Metin2ProtocolCodec
    {
        public const int CharacterSlotCount = 4;
        public const int CharacterListWireSize = 1 + CharacterSlotCount * (65 + 4 + 13) + 8;
        // Auriga-Global header values (client Packet.h + server common/Headers/Header_CG.h,
        // Header_GC.h). The previous base client used a different, older header set.
        public const byte HeaderCgLogin3 = 44;
        public const byte HeaderCgLogin2 = 109;
        public const byte HeaderCgCharacterCreate = 25;
        public const byte HeaderCgCharacterDelete = 5;
        public const byte HeaderCgCharacterSelect = 6;
        public const byte HeaderCgCharacterChangeName = 106;
        public const byte HeaderCgEnterGame = 10;
        // Header_CG.h: HEADER_CG_ATTACK = 49 and HEADER_CG_USE_SKILL = 42. The base client used 2
        // and 52; sending the base values made the server treat the attack/skill packet as an
        // unknown header and close the connection the moment a mob was hit.
        public const byte HeaderCgAttack = 49;
        public const byte HeaderCgUseSkill = 42;
        public const byte HeaderCgHandshake = 0xf0;
        public const byte HeaderCgTimeSync = 0xed;
        public const byte HeaderCgPong = 0xef;
        public const byte HeaderCgClientVersion = 0xfd;
        public const byte HeaderCgKeyAgreement = 0xec;
        public const byte HeaderGcKeyAgreementCompleted = 0xeb;
        public const byte HeaderGcHandshakeCompleted = 0xeb;
        public const byte HeaderGcKeyAgreement = 0xec;
        public const byte HeaderGcPhase = 0xee;
        public const byte HeaderGcHandshake = 0xf0;
        public const byte HeaderGcTimeSync = 0xed;
        public const byte HeaderGcBindUdp = 0xef;
        public const byte HeaderGcPing = 44;

        public const byte PhaseClose = 0;
        public const byte PhaseHandshake = 1;
        public const byte PhaseLogin = 2;
        public const byte PhaseSelect = 3;
        public const byte PhaseLoading = 4;
        public const byte PhaseGame = 5;
        public const byte PhaseDead = 6;
        public const byte PhaseAuth = 10;

        public const int LoginMaxLength = 30;
        public const int PasswordMaxLength = 16;
        public const int HwidLength = 64;
        // Auriga TPacketCGLogin3: header + name[31] + pwd[17] + adwClientKey[4] + hwid[65].
        public const int Login3Size = 1 + LoginMaxLength + 1 + PasswordMaxLength + 1 + 16 + HwidLength + 1;
        // Auriga TPacketCGLogin2: header + name[31] + dwLoginKey + adwClientKey[4].
        public const int Login2Size = 1 + LoginMaxLength + 1 + 4 + 16;
        public const int HandshakeSize = 13;
        public const int ClientVersionSize = 67;
        public const int BindUdpSize = 7;
        public const int KeyAgreementSize = 261;
        public const byte InitialClientSequence = 0xaf;

        /// <summary>
        /// The version string the Auriga game core compares against the DESC's client version in
        /// <c>CInputLogin::Entergame</c> (input_login.cpp:739-752). The official EUROPE client sends
        /// "20251204" in CG_CLIENT_VERSION2 (PythonNetworkStreamPhaseGame.cpp SendClientVersionPacket),
        /// and the server's own default is the same (config.cpp:87 <c>g_stClientVersion = "20251204"</c>).
        /// A mismatch triggers <c>DelayedDisconnect(0)</c> immediately after CG_ENTERGAME: the client
        /// receives the whole loading burst, then an unexplained EOF instead of PHASE_GAME.
        /// </summary>
        public const string ExpectedClientVersion = "20251204";
        // common/length.h GOLD_MAX is an exclusive server-side currency limit.
        public const long GoldLimitExclusive = 999000000000L;

        public static byte[] WriteLogin3(string login, string password, uint[] clientKey, string hwid)
        {
            if (clientKey == null || clientKey.Length != 4) throw new ArgumentException("LOGIN3 requires four client key words", nameof(clientKey));
            var packet = new byte[Login3Size];
            packet[0] = HeaderCgLogin3;
            WriteFixedAscii(packet, 1, LoginMaxLength + 1, login, nameof(login));
            WriteFixedAscii(packet, 32, PasswordMaxLength + 1, password, nameof(password));
            for (int index = 0; index < 4; index++) WriteUInt32(packet, 49 + index * 4, clientKey[index]);
            WriteFixedAscii(packet, 65, HwidLength + 1, hwid ?? string.Empty, nameof(hwid));
            return packet;
        }

        public static byte[] WriteSequencedLogin3(string login, string password, uint[] clientKey, string hwid, byte sequence = InitialClientSequence)
        {
            byte[] body = WriteLogin3(login, password, clientKey, hwid);
            Array.Resize(ref body, body.Length + 1);
            body[body.Length - 1] = sequence;
            return body;
        }

        public static byte[] WriteSequencedLogin2(string login, uint loginKey, uint[] clientKey, byte sequence = InitialClientSequence)
        {
            if (clientKey == null || clientKey.Length != 4) throw new ArgumentException("LOGIN2 requires four client key words", nameof(clientKey));
            var packet = new byte[Login2Size + 1];
            packet[0] = HeaderCgLogin2;
            WriteFixedAscii(packet, 1, LoginMaxLength + 1, login, nameof(login));
            WriteUInt32(packet, 32, loginKey);
            for (int index = 0; index < 4; index++) WriteUInt32(packet, 36 + index * 4, clientKey[index]);
            packet[Login2Size] = sequence;
            return packet;
        }

        /// <summary>
        /// Stable hardware-derived identifier matching the original client's HWID chain
        /// (device id + the same salt, SHA-256, Base64). Sent in TPacketCGLogin3.hwid.
        /// </summary>
        public static string ComputeHwid(string deviceUniqueIdentifier)
        {
            string chain = (deviceUniqueIdentifier ?? string.Empty) + "ASfds<W!k(T0=;d%Tc!k";
            using (var sha = System.Security.Cryptography.SHA256.Create())
            {
                byte[] hash = sha.ComputeHash(Encoding.UTF8.GetBytes(chain));
                return Convert.ToBase64String(hash);
            }
        }

        /// <summary>
        /// Auriga's packet_info.cpp ignores the legacy bSeq flag and the server reads exactly the
        /// registered struct size, so the placeholder byte the writers append must not reach the wire.
        /// </summary>
        public static byte[] TrimPacketSequence(byte[] packet)
        {
            if (packet == null || packet.Length <= 1) return packet;
            var trimmed = new byte[packet.Length - 1];
            Buffer.BlockCopy(packet, 0, trimmed, 0, trimmed.Length);
            return trimmed;
        }

        public static byte[] WriteMountFlight(byte action,int altitude,uint time,byte sequence)
        {
            var p=new byte[11];p[0]=225;p[1]=action;WriteUInt32(p,2,unchecked((uint)altitude));WriteUInt32(p,6,time);p[10]=sequence;return p;
        }
        public static byte[] WriteMove(byte function, byte argument, byte rotation, int x, int y, uint time, uint duration, byte sequence)
        {
            // Current TPacketCGMove: BYTE header/func/arg, float rotation, int32 x/y,
            // uint32 time/duration = 23 bytes. One legacy sequence placeholder is trimmed
            // by the session before transmission. WAIT/attack/combo/skill do not interpolate.
            var packet = new byte[Metin2MovementTiming.MoveWireSize + 1];
            packet[0] = 7;
            packet[1] = function;
            packet[2] = argument;
            byte[] rotationBytes = BitConverter.GetBytes((float)rotation);
            Buffer.BlockCopy(rotationBytes, 0, packet, 3, rotationBytes.Length);
            WriteUInt32(packet, 7, unchecked((uint)x));
            WriteUInt32(packet, 11, unchecked((uint)y));
            WriteUInt32(packet, 15, time);
            WriteUInt32(packet, 19, function == 1 ? duration : 0);
            packet[23] = sequence;
            return packet;
        }

        /// <summary>
        /// CG_SYNC_POSITION: packed header/WORD size and one VID/X/Y element. wSize excludes the
        /// trailing sequence byte, exactly as FlushVictimList sends the native packet followed by
        /// SendSequence(). Coordinates are already converted to global map centimetres.
        /// </summary>
        public static byte[] WriteSyncPosition(uint victimVid, int x, int y, byte sequence)
        {
            var packet = new byte[16];
            packet[0] = 8; packet[1] = 15; packet[2] = 0;
            WriteUInt32(packet, 3, victimVid);
            WriteUInt32(packet, 7, unchecked((uint)x));
            WriteUInt32(packet, 11, unchecked((uint)y));
            packet[15] = sequence;
            return packet;
        }

        public static byte[] WriteAttack(byte type, uint targetVid, byte processCrc, byte fileCrc, byte sequence)
        {
            var packet = new byte[9]; packet[0] = HeaderCgAttack; packet[1] = type; WriteUInt32(packet, 2, targetVid);
            packet[6] = processCrc; packet[7] = fileCrc; packet[8] = sequence; return packet;
        }

        /// <summary>
        /// CG_SHOOT (Packet.h:82). TPacketCGShoot is header + bType, and bType is
        /// CActorInstance::m_kCurMotNode.uSkill - 0 for a plain bow shot.
        ///
        /// This is the ENTIRE damage path of a bow attack. CPythonPlayerEventHandler's
        /// CNormalBowAttack_FlyEventHandler_AutoClear::OnShoot (PythonPlayerEventHandler.cpp:241-245)
        /// sends it from ProcessMotionEventFly, the same motion event that spawns the arrow; a bow
        /// never sends CG_ATTACK at all.
        /// </summary>
        public static byte[] WriteShoot(byte type, byte sequence) => new[] { (byte)54, type, sequence };

        /// <summary>
        /// CG_FLY_TARGETING (Packet.h:79). TPacketCGFlyTargeting is header + target VID + the
        /// target's fly position in GLOBAL map coordinates, sent when a motion that needs a fly
        /// target starts (ActorInstanceMotion.cpp:371-377).
        /// </summary>
        public static byte[] WriteFlyTargeting(uint targetVid, int x, int y, byte sequence)
        {
            var packet = new byte[14]; packet[0] = 51; WriteUInt32(packet, 1, targetVid);
            WriteUInt32(packet, 5, unchecked((uint)x)); WriteUInt32(packet, 9, unchecked((uint)y));
            packet[13] = sequence; return packet;
        }

        public static byte[] WriteUseSkill(uint skillVnum, uint targetVid, byte sequence) => WriteHeaderTwoUInt32(HeaderCgUseSkill, skillVnum, targetVid, sequence);
        public static byte[] WriteTarget(uint targetVid, byte sequence) => WriteHeaderUInt32(61, targetVid, sequence);
        public static byte[] WriteItemPickup(uint itemVid, byte sequence) => WriteHeaderUInt32(15, itemVid, sequence);
        /// <summary>CG_FISHING: the classic client quantises the heading to five-degree units.</summary>
        public static byte[] WriteFishing(byte direction, byte sequence) => new[] { (byte)82, direction, sequence };

        public static byte[] WriteItemUse(byte window, ushort cell, byte sequence)
        {
            var packet = new byte[5]; packet[0] = 11; WriteItemPosition(packet, 1, window, cell); packet[4] = sequence; return packet;
        }

        public static byte[] WriteItemMove(byte fromWindow, ushort fromCell, byte toWindow, ushort toCell, int count, byte sequence)
        {
            // Packed TPacketCGItemMove is 11 bytes on the wire: header + two TItemPos values + int32
            // count. Sending count as one byte makes the server consume three bytes from the following
            // packet and deterministically desynchronizes the CG stream.
            if (count < 0) throw new ArgumentOutOfRangeException(nameof(count));
            var packet = new byte[12]; packet[0] = 13; WriteItemPosition(packet, 1, fromWindow, fromCell);
            WriteItemPosition(packet, 4, toWindow, toCell); WriteUInt32(packet, 7, unchecked((uint)count));
            packet[11] = sequence; return packet;
        }

        /// <summary>CG_ITEM_DROP2: packed header + TItemPos + gold (zero for an item) + count.</summary>
        public static byte[] WriteItemDrop(byte window, ushort cell, long gold, int count, byte sequence)
        {
            if (gold < 0 || count < 0) throw new ArgumentOutOfRangeException(gold < 0 ? nameof(gold) : nameof(count));
            // Auriga ENABLE_NEW_STACK_LIMIT: header + TItemPos + int64 gold + int32 count = 16.
            var packet = new byte[17]; packet[0] = 20; WriteItemPosition(packet, 1, window, cell);
            WriteInt64(packet, 4, gold); WriteUInt32(packet, 12, unchecked((uint)count));
            packet[16] = sequence; return packet;
        }

        /// <summary>CG_GIVE_ITEM. The target server uses VID and TItemPos; count stays present for
        /// ABI compatibility even though CInputMain::ItemGive delegates the actual quantity check to GiveItem.</summary>
        public static byte[] WriteGiveItem(uint targetVid, byte window, ushort cell, int count, byte sequence)
        {
            if (count < 0) throw new ArgumentOutOfRangeException(nameof(count));
            // Auriga ENABLE_NEW_STACK_LIMIT widens byItemCount to int32.
            var packet = new byte[13]; packet[0] = 83; WriteUInt32(packet, 1, targetVid); WriteItemPosition(packet, 5, window, cell);
            WriteUInt32(packet, 8, unchecked((uint)count)); packet[12] = sequence; return packet;
        }
        /// <summary>CG_SCRIPT_SELECT_ITEM: selection is the authoritative inventory cell, zero cancels.</summary>
        public static byte[] WriteScriptSelectItem(uint selection, byte sequence)
        { var packet = new byte[6]; packet[0] = 114; WriteUInt32(packet, 1, selection); packet[5] = sequence; return packet; }

        public static byte[] WriteSafeboxCheckin(uint safePosition, byte fromWindow, ushort fromCell, byte sequence)
            => WriteStorageCheckoutOrCheckin(70, safePosition, fromWindow, fromCell, sequence);
        public static byte[] WriteSafeboxCheckout(uint safePosition, byte toWindow, ushort toCell, byte sequence)
            => WriteStorageCheckoutOrCheckin(71, safePosition, toWindow, toCell, sequence);
        public static byte[] WriteMallCheckout(uint mallPosition, byte toWindow, ushort toCell, byte sequence)
            => WriteStorageCheckoutOrCheckin(69, mallPosition, toWindow, toCell, sequence);
        public static byte[] WriteSafeboxMove(ushort fromPosition, ushort toPosition, byte sequence)
        {
            var packet = new byte[12]; packet[0] = 77; WriteItemPosition(packet, 1, 3, fromPosition);
            WriteItemPosition(packet, 4, 3, toPosition); WriteUInt32(packet, 7, 0); packet[11] = sequence; return packet;
        }
        private static byte[] WriteStorageCheckoutOrCheckin(byte header, uint storagePosition, byte otherWindow, ushort otherCell, byte sequence)
        {
            var packet = new byte[9]; packet[0] = header; WriteUInt32(packet, 1, storagePosition);
            WriteItemPosition(packet, 5, otherWindow, otherCell); packet[8] = sequence; return packet;
        }

        public static byte[] WriteItemUseToItem(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell, byte sequence)
        {
            var packet = new byte[8]; packet[0] = 60; WriteItemPosition(packet, 1, sourceWindow, sourceCell);
            WriteItemPosition(packet, 4, targetWindow, targetCell); packet[7] = sequence; return packet;
        }

        public static byte[] WriteChat(byte type, string message, byte sequence)
        {
            if (message == null) throw new ArgumentNullException(nameof(message));
            // The wire is single-byte text in the locale's code page, not UTF-8: the server stores
            // and re-broadcasts the bytes untouched, so a UTF-8 "ő" arrived back as two garbage
            // characters. Same decoder the receive side has used since the quest-title fix.
            byte[] text = Metin2EmoticonEffects.TryGetWireMarker(message, out byte[] emoticonBytes)
                ? emoticonBytes : Metin2GamePacketCodec.GameText.GetBytes(message);
            int wireSize = 5 + text.Length;
            if (wireSize > ushort.MaxValue) throw new ArgumentOutOfRangeException(nameof(message));
            var packet = new byte[wireSize + 1]; packet[0] = 3; packet[1] = (byte)wireSize; packet[2] = (byte)(wireSize >> 8); packet[3] = type;
            // TPacketCGChat carries the NUL terminator right after the text (Packet.h:637-642);
            // writing it over the last character truncated every message by one.
            Buffer.BlockCopy(text, 0, packet, 4, text.Length); packet[4 + text.Length] = 0; packet[wireSize] = sequence; return packet;
        }

        public static byte[] WriteWhisper(string recipient, string message, byte sequence)
        {
            if (string.IsNullOrWhiteSpace(recipient)) throw new ArgumentNullException(nameof(recipient));
            if (message == null) throw new ArgumentNullException(nameof(message));
            byte[] name = Metin2GamePacketCodec.GameText.GetBytes(recipient);
            byte[] text = Metin2GamePacketCodec.GameText.GetBytes(message);
            if (name.Length >= 25 || text.Length >= 255) throw new ArgumentOutOfRangeException(name.Length >= 25 ? nameof(recipient) : nameof(message));
            int wireSize = 28 + text.Length + 1;
            var packet = new byte[wireSize + 1]; packet[0] = 19; packet[1] = (byte)wireSize; packet[2] = (byte)(wireSize >> 8);
            Buffer.BlockCopy(name, 0, packet, 3, name.Length); Buffer.BlockCopy(text, 0, packet, 28, text.Length);
            packet[wireSize - 1] = 0; packet[wireSize] = sequence; return packet;
        }

        public static byte[] WriteMessengerAddByVid(uint vid, byte sequence) => WriteHeaderSubUInt32(67, 0, vid, sequence);
        public static byte[] WriteMessengerAddByName(string name, byte sequence) => WriteMessengerName(1, name, sequence);
        public static byte[] WriteMessengerRemove(string key, byte sequence) => WriteMessengerName(2, key, sequence);
        private static byte[] WriteMessengerName(byte subHeader, string name, byte sequence)
        {
            if (string.IsNullOrWhiteSpace(name)) throw new ArgumentNullException(nameof(name));
            // TPacketCGMessenger (2 bytes) is followed by CHARACTER_NAME_MAX_LEN (24) bytes,
            // then the transport sequence byte.  The original client reserves the final byte of
            // the fixed name field for NUL, so at most 23 encoded bytes may be copied.
            byte[] encoded = Metin2GamePacketCodec.GameText.GetBytes(name); if (encoded.Length >= 24) throw new ArgumentOutOfRangeException(nameof(name));
            var packet = new byte[27]; packet[0] = 67; packet[1] = subHeader; Buffer.BlockCopy(encoded, 0, packet, 2, encoded.Length); packet[26] = sequence; return packet;
        }
        private static byte[] WriteHeaderSubUInt32(byte header, byte subHeader, uint value, byte sequence)
        { var packet = new byte[7]; packet[0] = header; packet[1] = subHeader; WriteUInt32(packet, 2, value); packet[6] = sequence; return packet; }
        public static byte[] WritePartyInvite(uint vid, byte sequence) => WriteHeaderUInt32(72,vid,sequence);
        public static byte[] WritePartyInviteAnswer(uint leaderPid,bool accept,byte sequence)
        { var p=new byte[7];p[0]=73;WriteUInt32(p,1,leaderPid);p[5]=accept?(byte)1:(byte)0;p[6]=sequence;return p; }
        public static byte[] WritePartyRemove(uint pid,byte sequence)=>WriteHeaderUInt32(74,pid,sequence);
        public static byte[] WritePartySetState(uint vid,byte state,bool flag,byte sequence)
        { var p=new byte[8];p[0]=75;WriteUInt32(p,1,vid);p[5]=state;p[6]=flag?(byte)1:(byte)0;p[7]=sequence;return p; }
        public static byte[] WritePartyUseSkill(byte skill,uint targetVid,byte sequence)
        { var p=new byte[7];p[0]=76;p[1]=skill;WriteUInt32(p,2,targetVid);p[6]=sequence;return p; }
        public static byte[] WritePartyParameter(byte mode,byte sequence)=>new byte[]{78,mode,sequence};
        public static byte[] WriteGuildInviteAnswer(uint guildId,bool accept,byte sequence)
        {var p=new byte[8];p[0]=80;p[1]=11;WriteUInt32(p,2,guildId);p[6]=accept?(byte)1:(byte)0;p[7]=sequence;return p;}
        // TPacketCGAnswerMakeGuild: header + char guild_name[GUILD_NAME_MAX_LEN + 1],
        // followed by the encrypted-stream sequence byte used by this client transport.
        public static byte[] WriteGuildCreate(string name, byte sequence)
        { byte[] text=Metin2GamePacketCodec.GameText.GetBytes(name??string.Empty); if(text.Length==0||text.Length>12)throw new ArgumentOutOfRangeException(nameof(name)); var p=new byte[15];p[0]=81;Buffer.BlockCopy(text,0,p,1,text.Length);p[14]=sequence;return p; }
        public static byte[] WriteGuildAddMember(uint vid,byte sequence)=>WriteHeaderSubUInt32(80,0,vid,sequence);
        public static byte[] WriteGuildRemoveMember(uint pid,byte sequence)=>WriteHeaderSubUInt32(80,1,pid,sequence);
        /// <summary>
        /// SendGuildPostCommentPacket (PythonNetworkStreamPhaseGame.cpp:3220): header, subheader,
        /// then <c>bySize = strlen(text) + 1</c> followed by <c>bySize</c> bytes - the string
        /// INCLUDING its terminating NUL.
        ///
        /// The port declared the +1 but wrote only the characters, so the server read one byte past
        /// the message and swallowed the sequence byte. From that point the stream was misaligned
        /// and the connection was dropped: posting anything on the guild board disconnected.
        /// </summary>
        public static byte[] WriteGuildPostComment(string text,byte sequence)
        {if(string.IsNullOrEmpty(text))throw new ArgumentNullException(nameof(text));byte[] b=Metin2GamePacketCodec.GameText.GetBytes(text);if(b.Length>49)throw new ArgumentOutOfRangeException(nameof(text));var p=new byte[5+b.Length];p[0]=80;p[1]=5;p[2]=(byte)(b.Length+1);Buffer.BlockCopy(b,0,p,3,b.Length);p[3+b.Length]=0;p[p.Length-1]=sequence;return p;}
        public static byte[] WriteGuildDeleteComment(uint id,byte sequence)=>WriteHeaderSubUInt32(80,6,id,sequence);
        public static byte[] WriteGuildRefreshComments(byte sequence)=>new byte[]{80,7,sequence};
        public static byte[] WriteGuildUseSkill(uint skillId,uint targetVid,byte sequence){var p=new byte[11];p[0]=80;p[1]=9;WriteUInt32(p,2,skillId);WriteUInt32(p,6,targetVid);p[10]=sequence;return p;}
        public static byte[] WriteGuildChargePower(uint money,byte sequence)=>WriteHeaderSubUInt32(80,12,money,sequence);
        public static byte[] WriteGuildChangeGradeName(byte grade,string name,byte sequence){byte[] b=Metin2GamePacketCodec.GameText.GetBytes(name??"");if(b.Length>8)throw new ArgumentOutOfRangeException(nameof(name));var p=new byte[13];p[0]=80;p[1]=2;p[2]=grade;Buffer.BlockCopy(b,0,p,3,b.Length);p[12]=sequence;return p;}
        public static byte[] WriteGuildChangeGradeAuthority(byte grade,byte authority,byte sequence)=>new byte[]{80,3,grade,authority,sequence};
        public static byte[] WriteGuildOffer(uint experience,byte sequence)=>WriteHeaderSubUInt32(80,4,experience,sequence);
        public static byte[] WriteGuildChangeMemberGrade(uint pid,byte grade,byte sequence){var p=new byte[8];p[0]=80;p[1]=8;WriteUInt32(p,2,pid);p[6]=grade;p[7]=sequence;return p;}
        public static byte[] WriteGuildChangeMemberGeneral(uint pid,bool general,byte sequence){var p=new byte[8];p[0]=80;p[1]=10;WriteUInt32(p,2,pid);p[6]=general?(byte)1:(byte)0;p[7]=sequence;return p;}
        public static byte[] WriteGuildDeposit(uint money,byte sequence)=>WriteHeaderSubUInt32(80,13,money,sequence);
        public static byte[] WriteGuildWithdraw(uint money,byte sequence)=>WriteHeaderSubUInt32(80,14,money,sequence);
        public static byte[] WriteExchange(byte sub,long arg1,byte displayPosition,byte window,ushort cell,byte sequence)
        {
            // Packed Auriga TPacketCGExchange: header(1), sub(1), int64 arg1(8), BYTE arg2(1),
            // TItemPos(3). The old base-client layout used a 32-bit arg1 and was four bytes short.
            var p=new byte[15];p[0]=27;p[1]=sub;WriteInt64(p,2,arg1);p[10]=displayPosition;
            WriteItemPosition(p,11,window,cell);p[14]=sequence;return p;
        }
        public static byte[] WriteExchangeStart(uint vid,byte sequence)=>WriteExchange(0,vid,0,0,0,sequence);
        public static byte[] WriteExchangeItemAdd(byte window,ushort cell,byte display,byte sequence)=>WriteExchange(1,0,display,window,cell,sequence);
        public static byte[] WriteExchangeItemDelete(byte display,byte sequence)=>WriteExchange(2,display,0,0,0,sequence);
        public static byte[] WriteExchangeMoney(long money,byte sequence)
        {
            if (money <= 0 || money >= GoldLimitExclusive) throw new ArgumentOutOfRangeException(nameof(money));
            return WriteExchange(3,money,0,0,0,sequence);
        }
        public static byte[] WriteExchangeAccept(byte sequence)=>WriteExchange(4,0,0,0,0,sequence);
        public static byte[] WriteExchangeCancel(byte sequence)=>WriteExchange(5,0,0,0,0,sequence);

        /// <summary>One packed Auriga <c>TShopItemTable</c> entry sent after TPacketCGMyShop.</summary>
        public readonly struct PrivateShopItem
        {
            public readonly byte Window; public readonly ushort Cell; public readonly long Price; public readonly byte DisplayPosition;
            public PrivateShopItem(byte window, ushort cell, long price, byte displayPosition)
            { Window = window; Cell = cell; Price = price; DisplayPosition = displayPosition; }
        }

        /// <summary>
        /// CG_MYSHOP (55): header + char sign[SHOP_SIGN_MAX_LEN + 1] + count + packed stock,
        /// followed by the encrypted transport sequence byte.  Keep the fixed 33-byte name field:
        /// changing it shifts every stock item and makes the server reject the packet.
        /// </summary>
        public static byte[] WritePrivateShop(string sign, System.Collections.Generic.IReadOnlyList<PrivateShopItem> items, byte sequence)
        {
            if (string.IsNullOrWhiteSpace(sign)) throw new ArgumentException("A private shop needs a name.", nameof(sign));
            if (items == null || items.Count == 0 || items.Count > 120) throw new ArgumentOutOfRangeException(nameof(items));
            byte[] name = Metin2GamePacketCodec.GameText.GetBytes(sign);
            if (name.Length > 32) throw new ArgumentOutOfRangeException(nameof(sign));
            // KASMIR_PAKET_SYSTEM makes the fixed header 40 bytes. TShopItemTable is 60 bytes in
            // this build: vnum/count (zero for inventory-backed stock), five price pairs, int64
            // price, TItemPos and display_pos.
            var packet = new byte[40 + items.Count * 60 + 1]; packet[0] = 55;
            Buffer.BlockCopy(name, 0, packet, 1, name.Length); packet[34] = (byte)items.Count;
            // dwKasmirNpc and bKasmirBaslik stay zero until the Unity UI exposes those choices.
            for (int i = 0, offset = 40; i < items.Count; i++, offset += 60)
            {
                PrivateShopItem item = items[i];
                if (item.Price <= 0 || item.Price >= GoldLimitExclusive) throw new ArgumentOutOfRangeException(nameof(items), "Shop price must fit the server gold limit.");
                WriteInt64(packet, offset + 48, item.Price);
                WriteItemPosition(packet, offset + 56, item.Window, item.Cell);
                packet[offset + 59] = item.DisplayPosition;
            }
            packet[packet.Length - 1] = sequence; return packet;
        }

        public static byte[] WriteHandshake(uint handshake, uint time, int delta)
        {
            var packet = new byte[HandshakeSize];
            packet[0] = HeaderCgHandshake;
            WriteUInt32(packet, 1, handshake);
            WriteUInt32(packet, 5, time);
            WriteUInt32(packet, 9, unchecked((uint)delta));
            return packet;
        }

        /// <summary>Sequenced CG_PONG. The server ping event closes the descriptor when no pong arrives before the next cycle.</summary>
        public static byte[] WritePong(byte sequence) => new[] { HeaderCgPong, sequence };

        /// <summary>
        /// Sequenced CG_TIME_SYNC reply to a GC_HANDSHAKE received after the handshake phase.
        /// Mirrors CPythonNetworkStream::RecvHandshakePacket: the handshake token is echoed unchanged,
        /// dwTime becomes dwTime + 2 * lDelta and lDelta is reset to zero. A wrong token closes the descriptor.
        /// </summary>
        public static byte[] WriteTimeSync(uint handshake, uint time, int delta, byte sequence)
        {
            var packet = new byte[HandshakeSize + 1];
            packet[0] = HeaderCgTimeSync;
            WriteUInt32(packet, 1, handshake);
            WriteUInt32(packet, 5, unchecked(time + (uint)(delta * 2)));
            WriteUInt32(packet, 9, 0);
            packet[HandshakeSize] = sequence;
            return packet;
        }

        /// <summary>Sequenced CG_CLIENT_VERSION (TPacketCGClientVersion, 67 packed bytes) sent right after GC_MAIN_CHARACTER.</summary>
        public static byte[] WriteClientVersion(string filename, string timestamp, byte sequence)
        {
            var packet = new byte[ClientVersionSize + 1];
            packet[0] = HeaderCgClientVersion;
            WriteFixedAscii(packet, 1, 33, filename, nameof(filename));
            WriteFixedAscii(packet, 34, 33, timestamp, nameof(timestamp));
            packet[ClientVersionSize] = sequence;
            return packet;
        }

        public static byte[] WriteCharacterSelect(byte index, byte sequence)
        {
            ValidateCharacterSlot(index);
            return new[] { HeaderCgCharacterSelect, index, sequence };
        }
        public static byte[] WriteEnterGame(byte sequence) => new[] { HeaderCgEnterGame, sequence };

        public static byte[] WriteSelectEmpire(byte empire, byte sequence)
        {
            if(empire<1||empire>3)throw new ArgumentOutOfRangeException(nameof(empire));
            return new[]{(byte)90,empire,sequence};
        }

        public static byte[] WriteCharacterCreate(byte index, string name, byte race, byte shape, byte sequence)
        {
            ValidateCharacterSlot(index);
            if (race >= 8) throw new ArgumentOutOfRangeException(nameof(race), "Target server supports player race indices 0..7");
            if (shape > 1) throw new ArgumentOutOfRangeException(nameof(shape), "Target server supports shape indices 0..1");
            // Packed TPacketCGPlayerCreate is 34 bytes; the server derives initial stats from the race/job table.
            var packet = new byte[35];
            packet[0] = HeaderCgCharacterCreate; packet[1] = index;
            WriteFixedAscii(packet, 2, 25, name, nameof(name));
            packet[27] = race; packet[28] = 0; // WORD job/race
            packet[29] = shape;
            packet[30] = packet[31] = packet[32] = packet[33] = 0; // ignored CON/INT/STR/DEX input
            packet[34] = sequence;
            return packet;
        }

        public static byte[] WriteCharacterCreate(byte index,string name,byte race,byte shape,byte skillGroup,ushort hairAppearance,byte sequence)
        {
            if(skillGroup<1||skillGroup>2)throw new ArgumentOutOfRangeException(nameof(skillGroup));
            if(!Metin2CreationChoices.ValidHair(hairAppearance))throw new ArgumentOutOfRangeException(nameof(hairAppearance));
            var packet=WriteCharacterCreate(index,name,race,shape,sequence);
            // Versioned use of the four ignored stat bytes: packed ABI and sequence stay unchanged.
            packet[30]=Metin2CreationChoices.Marker;packet[31]=skillGroup;
            packet[32]=(byte)hairAppearance;packet[33]=(byte)(hairAppearance>>8);return packet;
        }

        public static byte[] WriteCharacterDelete(byte index, string privateCode, byte sequence)
        {
            ValidateCharacterSlot(index);
            var packet = new byte[11];
            packet[0] = HeaderCgCharacterDelete; packet[1] = index;
            WriteFixedAscii(packet, 2, 8, privateCode, nameof(privateCode));
            packet[10] = sequence;
            return packet;
        }

        /// <summary>CG_CHANGE_NAME: slot, 24-character ASCII name and the stream sequence.</summary>
        public static byte[] WriteCharacterChangeName(byte index, string name, byte sequence)
        {
            ValidateCharacterSlot(index);
            var packet = new byte[28];
            packet[0] = HeaderCgCharacterChangeName; packet[1] = index;
            WriteFixedAscii(packet, 2, 25, name, nameof(name));
            packet[27] = sequence;
            return packet;
        }

        private static void ValidateCharacterSlot(byte index)
        {
            if (index >= CharacterSlotCount) throw new ArgumentOutOfRangeException(nameof(index), "Target server supports character slots 0..3");
        }

        public static bool TryReadPhase(ReadOnlySpan<byte> packet, out byte phase)
        {
            phase = 0;
            if (packet.Length < 2 || packet[0] != HeaderGcPhase) return false;
            phase = packet[1];
            return true;
        }

        public static bool TryReadKeyAgreementHeader(ReadOnlySpan<byte> packet, out ushort agreedLength, out ushort dataLength)
        {
            agreedLength = dataLength = 0;
            if (packet.Length < KeyAgreementSize || packet[0] != HeaderGcKeyAgreement) return false;
            agreedLength = ReadUInt16(packet, 1);
            dataLength = ReadUInt16(packet, 3);
            return dataLength <= 256;
        }

        public static uint ReadUInt32(ReadOnlySpan<byte> source, int offset)
        {
            if (offset < 0 || source.Length - offset < 4) throw new ArgumentOutOfRangeException(nameof(offset));
            return (uint)(source[offset] | source[offset + 1] << 8 | source[offset + 2] << 16 | source[offset + 3] << 24);
        }

        public static CharacterList ReadCharacterList(ReadOnlySpan<byte> packet)
        {
            // Auriga TPacketGCLoginSuccess (HEADER_GC_LOGIN_SUCCESS_NEWSLOT = 32):
            //   header + players[4] + guild_id[4] + guild_name[4][13] + handle + random_key.
            // TSimplePlayer is 65 bytes: id(4) name(25) job(1) level(1) playMinutes(4) st/ht/dx/iq(4)
            // mainPart(2) changeName(1) hairPart(2) accePart(2) dummy(4) x(4) y(4) addr(4) port(2) skillGroup(1).
            const int playerCount = CharacterSlotCount;
            const int playerSize = 65;
            if (packet.Length != CharacterListWireSize || packet[0] != 32) throw new ArgumentException("Expected packed four-slot LOGIN_SUCCESS (337 bytes)", nameof(packet));
            var characters = new CharacterSummary[playerCount];
            for (int index = 0; index < playerCount; index++)
            {
                int guildIds = 1 + playerCount * playerSize;
                int guildNames = guildIds + playerCount * 4;
                characters[index] = ReadCharacterSummary(packet, 1 + index * playerSize).WithGuild(
                    ReadUInt32(packet, guildIds + index * 4), ReadFixedAscii(packet, guildNames + index * 13, 13));
            }
            int suffix = 1 + playerCount * playerSize + playerCount * 4 + playerCount * 13;
            return new CharacterList(characters, ReadUInt32(packet, suffix), ReadUInt32(packet, suffix + 4));
        }

        public static CharacterCreateResult ReadCharacterCreateSuccess(ReadOnlySpan<byte> packet)
        {
            // Auriga TPacketGCPlayerCreateSuccess: header + index + TSimplePlayer(65).
            if (packet.Length != 67 || packet[0] != 8) throw new ArgumentException("Expected packed CHARACTER_CREATE_SUCCESS", nameof(packet));
            return new CharacterCreateResult(true, packet[1], 0, ReadCharacterSummary(packet, 2));
        }

        public static byte ReadCharacterCreateFailure(ReadOnlySpan<byte> packet)
        {
            if (packet.Length != 2 || packet[0] != 9) throw new ArgumentException("Expected packed CHARACTER_CREATE_FAILURE", nameof(packet));
            return packet[1];
        }

        public static byte ReadCharacterDeleteSuccess(ReadOnlySpan<byte> packet)
        {
            if (packet.Length != 2 || packet[0] != 10) throw new ArgumentException("Expected packed CHARACTER_DELETE_SUCCESS", nameof(packet));
            return packet[1];
        }

        public static CharacterNameChange ReadCharacterNameChange(ReadOnlySpan<byte> packet)
        {
            if (packet.Length != 30 || packet[0] != 107) throw new ArgumentException("Expected packed CHARACTER_NAME_CHANGE", nameof(packet));
            return new CharacterNameChange(ReadUInt32(packet, 1), ReadFixedAscii(packet, 5, 25));
        }

        private static CharacterSummary ReadCharacterSummary(ReadOnlySpan<byte> packet, int offset)
        {
            if (offset < 0 || packet.Length - offset < 65) throw new ArgumentOutOfRangeException(nameof(offset));
            return new CharacterSummary(
                ReadUInt32(packet, offset), ReadFixedAscii(packet, offset + 4, 25), packet[offset + 29], packet[offset + 30],
                ReadUInt32(packet, offset + 31), ReadUInt16(packet, offset + 39), ReadUInt16(packet, offset + 42),
                unchecked((int)ReadUInt32(packet, offset + 50)), unchecked((int)ReadUInt32(packet, offset + 54)),
                ReadUInt32(packet, offset + 58), ReadUInt16(packet, offset + 62), packet[offset + 64], packet[offset + 41] != 0,
                packet[offset + 35],packet[offset + 36],packet[offset + 37],packet[offset + 38]);
        }

        public readonly struct CharacterSummary
        {
            public readonly uint Id; public readonly string Name; public readonly byte Job; public readonly byte Level;
            public readonly uint PlayMinutes; public readonly ushort MainPart; public readonly ushort HairPart;
            public readonly int X; public readonly int Y; public readonly uint Address; public readonly ushort Port; public readonly byte SkillGroup;
            public readonly bool ChangeName;
            public readonly byte Strength,Health,Dexterity,Intelligence;
            public readonly uint GuildId;
            public readonly string GuildName;
            public CharacterSummary(uint id, string name, byte job, byte level, uint playMinutes, ushort mainPart, ushort hairPart, int x, int y, uint address, ushort port, byte skillGroup, bool changeName = false,byte strength=0,byte health=0,byte dexterity=0,byte intelligence=0,uint guildId=0,string guildName=null)
            { Id = id; Name = name; Job = job; Level = level; PlayMinutes = playMinutes; MainPart = mainPart; HairPart = hairPart; X = x; Y = y; Address = address; Port = port; SkillGroup = skillGroup; ChangeName = changeName;Strength=strength;Health=health;Dexterity=dexterity;Intelligence=intelligence;GuildId=guildId;GuildName=guildName??string.Empty; }
            public CharacterSummary WithName(string name, bool changeName) => new CharacterSummary(Id, name, Job, Level, PlayMinutes, MainPart, HairPart, X, Y, Address, Port, SkillGroup, changeName,Strength,Health,Dexterity,Intelligence,GuildId,GuildName);
            public CharacterSummary WithGuild(uint id, string name) => new CharacterSummary(Id, Name, Job, Level, PlayMinutes, MainPart, HairPart, X, Y, Address, Port, SkillGroup, ChangeName,Strength,Health,Dexterity,Intelligence,id,name);
        }

        public readonly struct CharacterNameChange
        {
            public readonly uint Id; public readonly string Name;
            public CharacterNameChange(uint id, string name) { Id = id; Name = name; }
        }

        public readonly struct CharacterList
        {
            public readonly CharacterSummary[] Characters; public readonly uint Handle; public readonly uint RandomKey;
            public CharacterList(CharacterSummary[] characters, uint handle, uint randomKey) { Characters = characters; Handle = handle; RandomKey = randomKey; }
        }

        public readonly struct CharacterCreateResult
        {
            public readonly bool Success; public readonly byte Index; public readonly byte FailureType; public readonly CharacterSummary Character;
            public CharacterCreateResult(bool success, byte index, byte failureType, CharacterSummary character)
            { Success = success; Index = index; FailureType = failureType; Character = character; }
        }

        private static void WriteFixedAscii(byte[] target, int offset, int width, string value, string parameter)
        {
            if (value == null) throw new ArgumentNullException(parameter);
            byte[] bytes = Encoding.ASCII.GetBytes(value);
            if (bytes.Length >= width) throw new ArgumentOutOfRangeException(parameter, $"ASCII value must be at most {width - 1} bytes");
            Buffer.BlockCopy(bytes, 0, target, offset, bytes.Length);
        }

        private static void WriteUInt32(byte[] target, int offset, uint value)
        {
            target[offset] = (byte)value; target[offset + 1] = (byte)(value >> 8);
            target[offset + 2] = (byte)(value >> 16); target[offset + 3] = (byte)(value >> 24);
        }

        private static void WriteInt64(byte[] target, int offset, long value)
        {
            ulong bits = unchecked((ulong)value);
            for (int index = 0; index < 8; index++) target[offset + index] = (byte)(bits >> (index * 8));
        }

        private static byte[] WriteHeaderUInt32(byte header, uint value, byte sequence)
        {
            var packet = new byte[6]; packet[0] = header; WriteUInt32(packet, 1, value); packet[5] = sequence; return packet;
        }

        private static byte[] WriteHeaderTwoUInt32(byte header, uint first, uint second, byte sequence)
        {
            var packet = new byte[10]; packet[0] = header; WriteUInt32(packet, 1, first); WriteUInt32(packet, 5, second); packet[9] = sequence; return packet;
        }

        private static void WriteItemPosition(byte[] packet, int offset, byte window, ushort cell)
        {
            packet[offset] = window; packet[offset + 1] = (byte)cell; packet[offset + 2] = (byte)(cell >> 8);
        }

        private static ushort ReadUInt16(ReadOnlySpan<byte> source, int offset) => (ushort)(source[offset] | source[offset + 1] << 8);
        private static string ReadFixedAscii(ReadOnlySpan<byte> source, int offset, int width)
        {
            int length = 0; while (length < width && source[offset + length] != 0) length++;
            return Encoding.ASCII.GetString(source.Slice(offset, length));
        }
    }
}
