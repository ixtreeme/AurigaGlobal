using System;
using System.Collections.Generic;

namespace Metin2.Runtime.Network
{
    /// <summary>Reassembles decrypted GC stream chunks into packed protocol packets.</summary>
    public sealed class Metin2PacketFramer
    {
        public const int SourceCharacterUpdateSize = 268;
        // Auriga's packed GC19 ends with language at offset 267. Live login captures
        // place the next GC17 at offset 268; consuming 272 eats its four-byte header.
        public const int LiveCharacterUpdateSize = SourceCharacterUpdateSize;
        public readonly struct Definition
        {
            public readonly int FixedSize; public readonly int SizeOffset; public readonly bool Size32;
            public readonly Func<IReadOnlyList<byte>, int> Measure;
            public bool Dynamic => SizeOffset >= 0;
            public bool Measured => Measure != null;
            private Definition(int fixedSize, int sizeOffset, Func<IReadOnlyList<byte>, int> measure = null, bool size32 = false)
            { FixedSize = fixedSize; SizeOffset = sizeOffset; Measure = measure; Size32 = size32; }
            public static Definition Fixed(int size) => new Definition(size, -1);
            public static Definition DynamicAt(int minimumSize, int sizeOffset = 1) => new Definition(minimumSize, sizeOffset);
            /// <summary>Like <see cref="DynamicAt"/> but the length field is a 32 bit int, as the
            /// Auriga GC_NEW_OFFLINESHOP envelope declares it under __ENABLE_LARGE_DYNAMIC_PACKET__.</summary>
            public static Definition DynamicAt32(int minimumSize, int sizeOffset = 1) => new Definition(minimumSize, sizeOffset, null, true);
            /// <summary>A packet whose real length has to be derived from its own payload.</summary>
            public static Definition Computed(int minimumSize, Func<IReadOnlyList<byte>, int> measure) =>
                new Definition(minimumSize, -1, measure);
        }

        private readonly Dictionary<byte, Definition> definitions = new Dictionary<byte, Definition>();
        private readonly List<byte> buffered = new List<byte>(4096);
        public int MaxPacketSize { get; set; } = ushort.MaxValue;
        public int BufferedBytes => buffered.Count;

        public Metin2PacketFramer Register(byte header, Definition definition) { definitions[header] = definition; return this; }

        /// <summary>
        /// Optional sink for framing traces. A desync is only ever explainable from the raw bytes:
        /// the declared length of one packet has to be compared against what the payload actually
        /// contains, and the history of header/size pairs alone cannot show that.
        /// </summary>
        public Action<string> Trace { get; set; }

        /// <summary>Headers whose bytes are dumped when <see cref="Trace"/> is set.</summary>
        public readonly HashSet<byte> TraceHeaders = new HashSet<byte>();

        /// <summary>GUILD_SUBHEADER_GC_SKILL_INFO - the 13th entry of the subheader enum.</summary>
        private const byte GuildSubheaderSkillInfo = 12;

        /// <summary>
        /// GC_GUILD is { BYTE header; WORD size; BYTE subheader; } plus a per-subheader payload.
        /// The declared size is authoritative for every subheader except SKILL_INFO, where the server
        /// overstates it by exactly one byte:
        ///
        /// <code>
        /// // guild.cpp:1396 CGuild::SendSkillInfoPacket
        /// pack.size = sizeof(pack) + 6 + GUILD_SKILL_COUNT;   // 4 + 6 + 12 = 22
        /// d->BufferedPacket(&amp;pack, sizeof(pack));             //  4
        /// d->BufferedPacket(&amp;m_data.skill_point, 1);          // +1
        /// d->BufferedPacket(&amp;m_data.abySkill, GUILD_SKILL_COUNT); // +12
        /// d->BufferedPacket(&amp;m_data.power, 2);                // +2
        /// d->Packet(&amp;m_data.max_power, 2);                    // +2  = 21 written
        /// </code>
        ///
        /// The literal should be 5 (1 + 2 + 2), not 6. The original client never noticed because
        /// PythonNetworkStreamPhaseGame.cpp:3649 reads the four fields one by one and ignores the
        /// size field entirely; a length-driven framer cannot, so the correction is applied here.
        /// Trusting the declared size swallows the next packet's header byte and desyncs the stream.
        /// </summary>
        public static int MeasureGuild(IReadOnlyList<byte> buffer)
            => CorrectedGuildSize(buffer[1] | buffer[2] << 8, buffer[3]);

        /// <summary>
        /// The one implementation of the SKILL_INFO correction described above.
        ///
        /// The framer trims the byte and <c>Metin2GamePacketCodec.ReadGuild</c> has to expect the
        /// trimmed length, or the two disagree by exactly one byte and every guild skill-info
        /// packet is rejected as a malformed envelope. Keeping the rule in one place is what stops
        /// that pair from drifting apart again.
        /// </summary>
        public static int CorrectedGuildSize(int declaredSize, byte subheader)
            => subheader == GuildSubheaderSkillInfo ? declaredSize - 1 : declaredSize;

        /// <summary>
        /// packet_quest_info with __QUEST_RENEWAL__: the server writes the full 107 byte struct
        /// (header, size=107, index, c_index, flag, ...) and then appends the optional sections
        /// selected by the flag without ever updating the transmitted size field, so the real
        /// length is 107 plus the section sizes. The flag lives at offset 7 in this build.
        /// </summary>
        public static int MeasureQuestInfo(IReadOnlyList<byte> buffer)
        {
            const int headerSize = 107;
            byte flag = buffer[7];
            int size = headerSize;
            if ((flag & 1 << 0) != 0) size += 1;      // QUEST_SEND_ISBEGIN
            if ((flag & 1 << 1) != 0) size += 30 + 1; // QUEST_SEND_TITLE
            if ((flag & 1 << 2) != 0) size += 16 + 1; // QUEST_SEND_CLOCK_NAME
            if ((flag & 1 << 3) != 0) size += 4;      // QUEST_SEND_CLOCK_VALUE
            if ((flag & 1 << 4) != 0) size += 16 + 1; // QUEST_SEND_COUNTER_NAME
            if ((flag & 1 << 5) != 0) size += 4;      // QUEST_SEND_COUNTER_VALUE
            if ((flag & 1 << 6) != 0) size += 24 + 1; // QUEST_SEND_ICON_FILE
            return size;
        }

        /// <summary>
        /// GC_BATTLE_PASS_OPEN is not a conventional WORD-sized dynamic packet. Auriga writes
        /// <c>wSize = sizeof(header) + missionBytes</c> and then appends a second block whose byte
        /// length is carried independently in <c>wRewardSize</c>. The original C++ receiver first
        /// consumes wSize and then consumes wRewardSize. Framing only wSize leaves the reward item
        /// bytes in the stream, where the first item-vnum byte is misread as the next GC header.
        /// </summary>
        public static int MeasureBattlePassOpen(IReadOnlyList<byte> buffer)
            => (buffer[1] | buffer[2] << 8) + (buffer[3] | buffer[4] << 8);

        /// <summary>
        /// GC_ITEMSHOP (93). The full catalogue packet declares its size correctly, but the
        /// no-catalogue sentinel does not: char_manager.cpp:2270 writes
        /// <c>p.size = sizeof(TPacketGCItemShop) + 1 + 4 + 4 = 14</c> and then also writes the
        /// 4-byte categoryTotalSize(9999), so 18 bytes hit the wire. The original client reads the
        /// fields one by one and never looks at the size; a length-driven framer must correct it or
        /// the trailing 4 bytes (0f 27 00 00) become the next packet's "header 15" and the stream
        /// is aborted. The 93/14 disconnect after closing and reopening the shop is exactly this.
        /// </summary>
        public static int MeasureItemShop(IReadOnlyList<byte> buffer)
        {
            int declared = buffer[1] | buffer[2] << 8 | buffer[3] << 16 | buffer[4] << 24;
            if (declared == 14 && buffer.Count > 5 && buffer[5] == 0) return 18; // ITEMSHOP_LOAD sentinel
            return declared;
        }

        // A stream position can only go wrong if a previous packet was consumed with the wrong length,
        // so the header that fails is rarely the culprit. The last few accepted packets identify the
        // one whose registered size is too short or too long.
        private readonly (byte Header, int Size, long Offset)[] history = new (byte, int, long)[40];
        private int historyCount;
        /// <summary>Stream offset of the next byte to frame, for locating the desync in a capture.</summary>
        private long streamOffset;
        /// <summary>
        /// First bytes of the most recently accepted packet. A wrong registered length shows up here:
        /// the trailing bytes of the previous packet are what the next header was read from.
        /// </summary>
        private readonly byte[] lastPacketHead = new byte[24];
        private int lastPacketHeadLength;
        // Capture the complete last actor-update candidate, not only the final bogus packet
        // header. Keep 32 following bytes so a live/source ABI mismatch can be diagnosed without
        // guessing lengths from padding or treating an actor VID as a new header.
        private readonly List<byte> lastActorUpdateCapture = new List<byte>(304);
        private long lastActorUpdateOffset = -1;
        private int lastActorUpdateFramedSize;
        private const int ActorUpdateCaptureLimit = 304;

        private void Record(byte header, int size, IReadOnlyList<byte> packet)
        {
            for (int index = history.Length - 1; index > 0; index--) history[index] = history[index - 1];
            history[0] = (header, size, streamOffset);
            if (historyCount < history.Length) historyCount++;
            streamOffset += size;

            lastPacketHeadLength = Math.Min(lastPacketHead.Length, packet.Count);
            for (int index = 0; index < lastPacketHeadLength; index++) lastPacketHead[index] = packet[index];
        }

        private static string Hex(IReadOnlyList<byte> bytes, int start, int count)
        {
            var builder = new System.Text.StringBuilder();
            int end = Math.Min(bytes.Count, start + count);
            for (int index = start; index < end; index++)
            {
                if (index > start) builder.Append(' ');
                builder.Append(bytes[index].ToString("x2"));
            }
            return builder.ToString();
        }

        private string History()
        {
            var builder = new System.Text.StringBuilder(" | at stream offset ").Append(streamOffset);
            builder.Append(" | next 48 bytes: ").Append(Hex(buffered, 0, 48));
            builder.Append(" | last accepted packet head: ").Append(Hex(lastPacketHead, 0, lastPacketHeadLength));
            builder.Append(" | preceding packets (newest first, header/size@offset):");
            for (int index = 0; index < historyCount; index++)
                builder.Append(' ').Append(history[index].Header).Append('/').Append(history[index].Size)
                       .Append('@').Append(history[index].Offset);
            builder.Append(" | buffered=").Append(buffered.Count);
            if (lastActorUpdateOffset >= 0)
                builder.Append(" | GC19 candidate@=").Append(lastActorUpdateOffset)
                    .Append(" source_size=268 framed_size=").Append(lastActorUpdateFramedSize).Append(" raw=")
                    .Append(Hex(lastActorUpdateCapture, 0, lastActorUpdateCapture.Count));
            return builder.ToString();
        }

        public IReadOnlyList<byte[]> Feed(ReadOnlySpan<byte> decryptedChunk)
        {
            if (lastActorUpdateOffset >= 0 && lastActorUpdateCapture.Count < ActorUpdateCaptureLimit)
            {
                int count = Math.Min(decryptedChunk.Length, ActorUpdateCaptureLimit - lastActorUpdateCapture.Count);
                for (int index = 0; index < count; index++) lastActorUpdateCapture.Add(decryptedChunk[index]);
            }
            for (int i = 0; i < decryptedChunk.Length; i++) buffered.Add(decryptedChunk[i]);
            var result = new List<byte[]>();
            while (buffered.Count > 0)
            {
                byte header = buffered[0];
                if (header == 0)
                {
                    // Zero bytes are cipher-block padding, not packets. They advance the stream
                    // position but are absent from the history, so they are traced separately -
                    // a padding byte eaten in the wrong place is itself a way to lose a byte.
                    if (Trace != null) Trace($"METIN2_FRAME pad@{streamOffset}");
                    buffered.RemoveAt(0); streamOffset++; result.Add(new byte[] { 0 }); continue;
                }
                if (!definitions.TryGetValue(header, out Definition definition))
                {
                    string failure = "Unknown GC packet header " + header + History();
                    // Into the trace as well, so the failure and the bytes that led to it end up in
                    // one file instead of having to be correlated across two places.
                    Trace?.Invoke("METIN2_FRAME FAILURE " + failure);
                    throw new InvalidOperationException(failure);
                }
                int size = definition.FixedSize;
                if (definition.Measured)
                {
                    if (buffered.Count < definition.FixedSize) break;
                    size = definition.Measure(buffered);
                    if (size < definition.FixedSize)
                        throw new InvalidOperationException("Invalid computed packet size " + size + " for header " + header + History());
                }
                else if (definition.Dynamic)
                {
                    int width = definition.Size32 ? 4 : 2;
                    if (buffered.Count < definition.SizeOffset + width) break;
                    // Keep widened operands and explicit branches. Unity2022
                    // IL2CPP inferred a byte for the former ternary's shared
                    // stack slot, truncating473 to217 in the generated C++.
                    int low=buffered[definition.SizeOffset];
                    int high=buffered[definition.SizeOffset+1];
                    size=low|(high<<8);
                    if(definition.Size32)
                    {
                        int third=buffered[definition.SizeOffset+2];
                        int fourth=buffered[definition.SizeOffset+3];
                        size=unchecked(size|(third<<16)|(fourth<<24));
                    }
                    if (size < definition.FixedSize) throw new InvalidOperationException("Invalid dynamic packet size " + size + " for header " + header);
                }
                if (size > MaxPacketSize) throw new InvalidOperationException("GC packet exceeds configured maximum: " + size);
                if (buffered.Count < size) break;
                if (header == 19)
                {
                    lastActorUpdateOffset = streamOffset;
                    lastActorUpdateFramedSize = size;
                    lastActorUpdateCapture.Clear();
                    int count = Math.Min(buffered.Count, ActorUpdateCaptureLimit);
                    for (int index = 0; index < count; index++) lastActorUpdateCapture.Add(buffered[index]);
                }
                byte[] packet = buffered.GetRange(0, size).ToArray(); buffered.RemoveRange(0, size); result.Add(packet);
                if (Trace != null && TraceHeaders.Contains(header))
                    Trace($"METIN2_FRAME {header}/{size}@{streamOffset} bytes={Hex(packet, 0, Math.Min(size, 64))}");
                Record(header, size, packet);
            }
            return result;
        }

        public static Metin2PacketFramer CreateLoginFramer() => new Metin2PacketFramer()
            .Register(7, Definition.Fixed(10)).Register(6, Definition.Fixed(249)).Register(32, Definition.Fixed(Metin2ProtocolCodec.CharacterListWireSize))
            .Register(8, Definition.Fixed(67)).Register(9, Definition.Fixed(2))
            .Register(10, Definition.Fixed(2)).Register(11, Definition.Fixed(1))
            .Register(90, Definition.Fixed(2)).Register(238, Definition.Fixed(2)).Register(44, Definition.Fixed(1))
            // DESC::ping_event fires in every phase and emits GC_PING plus DESC::SendHandshake. Without these three
            // definitions the framer aborts on the first keepalive burst, the client stops answering and the server
            // closes the descriptor on the next ping cycle ("no pong").
            .Register(240, Definition.Fixed(13))   // GC_HANDSHAKE  - TPacketGCHandshake (BYTE + DWORD + DWORD + long)
            .Register(237, Definition.Fixed(1))    // GC_TIME_SYNC  - bare header, DESC::HandshakeProcess acknowledgement
            .Register(239, Definition.Fixed(7));   // GC_BINDUDP    - TPacketGCBindUDP (BYTE + DWORD + WORD)

        public static Metin2PacketFramer CreateGameCoreFramer() => CreateGameCoreFramer(false);

        /// <summary>The source and live Auriga profiles use the same packed actor-update ABI.
        /// The profile argument is retained for existing callers and regression tests.</summary>
        public static Metin2PacketFramer CreateGameCoreFramer(bool sourceActorUpdateAbi) => ConfigureGameCoreFramer(CreateLoginFramer()
            .Register(1, Definition.Fixed(38))      // character add (Auriga: +bool transname)
            .Register(2, Definition.Fixed(5))       // character delete
            .Register(3, Definition.Fixed(27))      // character move (Auriga: +float bRot)
            .Register(4, Definition.DynamicAt(9))   // chat (Auriga: header, WORD size, type, vid, empire + text)
            .Register(5, Definition.DynamicAt(3))   // synchronized positions
            .Register(12, Definition.Fixed(10))     // actor attack
            .Register(13, Definition.Fixed(5))      // stun
            .Register(14, Definition.Fixed(5))      // dead
            .Register(16, Definition.Fixed(2041))   // all player points (Auriga: int64 points[255])
            .Register(17, Definition.Fixed(25))     // point change (Auriga: int header + int64 amount/value)
            .Register(18, Definition.Fixed(7))      // actor movement speed
            .Register(19, Definition.Fixed(sourceActorUpdateAbi ? SourceCharacterUpdateSize : LiveCharacterUpdateSize))
            // GC_ITEM_DEL does not use the 2 byte TPacketGCItemDel. char_item.cpp:424 sends
            // TPacketGCItemDelDeprecated instead: BYTE header + TItemPos(3) + DWORD vnum + BYTE count
            // + long alSockets[3] + TPlayerItemAttribute aAttr[7], where TPlayerItemAttribute is
            // BYTE + short = 3 bytes. That is 1+3+4+1+12+21 = 42, and consuming only 2 desynchronised
            // the stream every time an item was removed.
            .Register(20, Definition.Fixed(50))     // item delete (Auriga: int count + ATTR_LOCK)
            .Register(21, Definition.Fixed(59))     // item set (Auriga: int count + ATTR_LOCK)
            .Register(22, Definition.Fixed(16))     // item-use presentation
            .Register(25, Definition.Fixed(46))     // item update (Auriga: int count + ATTR_LOCK)
            .Register(26, Definition.Fixed(21))     // ground item add
            .Register(27, Definition.Fixed(5))      // ground item delete
            .Register(28, Definition.Fixed(5))      // quickslot add (Auriga: TQuickslot 3 bytes)
            .Register(29, Definition.Fixed(2))      // quickslot delete
            .Register(30, Definition.Fixed(3))      // quickslot swap
            .Register(31, Definition.Fixed(30))     // ground-item ownership name
            .Register(34, Definition.DynamicAt(29)) // whisper (Auriga: header, WORD size, type, name[25] + text)
            .Register(36, Definition.Fixed(11))     // actor motion
            .Register(38, Definition.DynamicAt(4))  // shop envelope
            .Register(39, Definition.Fixed(39))     // shop sign + offline border style
            .Register(40, Definition.DynamicAt(3))  // duel list
            .Register(41, Definition.Fixed(10))     // pvp state
            .Register(42, Definition.Fixed(59))     // exchange state (Auriga: arg4 + ATTR_LOCK)
            .Register(43, Definition.Fixed(6))      // character position
            .Register(45, Definition.DynamicAt(6))  // quest script
            .Register(46, Definition.Fixed(74))     // quest confirmation
            .Register(47, Definition.DynamicAt(4))  // offline shop envelope
            .Register(48, Definition.Fixed(39))     // offline shop sign
            .Register(49, Definition.Fixed(49))     // offline shop management panel
            .Register(50, Definition.Fixed(9609))   // 80-slot P2P offline shop snapshot
            .Register(51, Definition.Fixed(39))     // changed offline shop sign
            .Register(52, Definition.Fixed(39))     // P2P changed offline shop sign
            .Register(53, Definition.Fixed(42))     // offline shop sale notification
            .Register(58, Definition.Fixed(17))     // target info (__SEND_TARGET_INFO__, DWORD count)
            .Register(61, Definition.Fixed(18))     // legacy mount state
            .Register(62, Definition.Fixed(9))      // ownership
            .Register(63, Definition.Fixed(23))     // target (Auriga: decimal HP + element fields)
            .Register(65, Definition.Fixed(15))     // warp
            .Register(69, Definition.Fixed(17))     // add fly targeting
            .Register(70, Definition.Fixed(10))     // create fly
            .Register(71, Definition.Fixed(17))     // fly targeting
            .Register(72, Definition.Fixed(256))    // legacy skill levels (header + 255 bytes)
            .Register(73, Definition.Fixed(2))      // skill cooldown ended early
            .Register(74, Definition.DynamicAt(4))  // messenger
            .Register(75, Definition.Computed(4, MeasureGuild))  // guild - see MeasureGuild
            .Register(76, Definition.Fixed(256))    // header + 255 absolute skill levels
            .Register(77, Definition.Fixed(5))      // party invite
            .Register(78, Definition.Fixed(30))     // party add
            .Register(79, Definition.Fixed(21))     // party update
            .Register(80, Definition.Fixed(5))      // party remove
            // GC_QUEST_INFO carries a size field that the server never fixes up. questpc.cpp
            // PC::SendQuestInfoPakcet writes the full 107 byte struct first (header, size=107,
            // index, c_index, flag, ...) and then appends the optional sections while only the
            // local copy of qi.size is incremented, so the transmitted size stays 107 no matter
            // how much payload follows. The real length is reconstructed from the flag byte at
            // offset 7, using the section sizes from packet.h: ISBEGIN 1, TITLE 31, CLOCK_NAME 17,
            // CLOCK_VALUE 4, COUNTER_NAME 17, COUNTER_VALUE 4, ICON_FILE 25.
            .Register(81, Definition.Computed(107, MeasureQuestInfo))
            .Register(82, Definition.Fixed(1))      // request make guild
            .Register(83, Definition.Fixed(2))      // party parameter
            .Register(84, Definition.Fixed(5))      // legacy safebox money change
            .Register(85, Definition.Fixed(59))     // safebox item set (Auriga: int count + ATTR_LOCK)
            .Register(86, Definition.Fixed(3))      // safebox item delete (Auriga: WORD pos)
            .Register(87, Definition.Fixed(1))      // safebox wrong password
            .Register(88, Definition.Fixed(2))      // safebox size
            .Register(89, Definition.Fixed(7))      // fishing state
            .Register(91, Definition.Fixed(9))      // party link
            .Register(92, Definition.Fixed(9))      // party unlink
            .Register(93, Definition.Computed(6, MeasureItemShop)) // in-game item shop; the sentinel size is corrected
            .Register(95, Definition.Fixed(64))     // legacy refine information (without type byte)
            .Register(99, Definition.Fixed(948))    // equipment view (Auriga: 23 equips)
            .Register(106, Definition.Fixed(9))     // server time (Auriga: uint64 time)
            .Register(107, Definition.Fixed(30))    // character name change
            .Register(110, Definition.DynamicAt(4)) // dungeon envelope
            .Register(111, Definition.Fixed(6))     // walk/run mode
            .Register(112, Definition.Fixed(2))     // skill group
            .Register(113, Definition.Fixed(46))    // main character
            .Register(114, Definition.Fixed(6))     // special effect
            .Register(115, Definition.DynamicAt(5))// NPC position list
            .Register(119, Definition.Fixed(64))    // live Auriga: BYTE position, int64 cost
            .Register(121, Definition.Fixed(2))     // channel
            .Register(122, Definition.Fixed(2))     // mall open
            .Register(123, Definition.Fixed(13))    // quest target update
            .Register(124, Definition.Fixed(5))     // quest target delete
            .Register(125, Definition.Fixed(43))    // quest target create
            .Register(126, Definition.Fixed(22))    // affect add
            .Register(127, Definition.Fixed(6))     // affect remove
            .Register(128, Definition.Fixed(59))    // mall item set (Auriga: int count + ATTR_LOCK)
            .Register(129, Definition.Fixed(3))     // mall item delete (Auriga: WORD pos)
            .Register(130, Definition.DynamicAt(3)) // land list
            .Register(131, Definition.Fixed(27))    // lover info
            .Register(132, Definition.Fixed(2))     // love point
            .Register(133, Definition.DynamicAt(7)) // guild symbol data
            .Register(134, Definition.Fixed(10))    // mining motion
            .Register(135, Definition.Fixed(10))    // damage
            .Register(136, Definition.Fixed(285))   // character additional info (Auriga: skill-color grid + language)
            .Register(137, Definition.Fixed(71))    // main character + BGM
            .Register(138, Definition.Fixed(75))    // main character + BGM + volume
            .Register(140, Definition.Fixed(2))     // request change language (sent on every enter game)
            .Register(141, Definition.Fixed(29))    // recv target language (TPacketRecvLang)
            .Register(160, Definition.Computed(5, MeasureBattlePassOpen)) // mission frame + separately-sized reward tail
            .Register(161, Definition.Fixed(6))     // battle pass update (mission type + progress)
            .Register(162, Definition.DynamicAt(4)) // battle pass ranking (WORD size at offset 1)
            .Register(142, Definition.DynamicAt(4)) // in-game item shop (legacy header)
            .Register(147, Definition.DynamicAt32(5)) // event manager (every enter game, uint32 size)
            .Register(152, Definition.DynamicAt(7)) // hybridcrypt keys (legacy)
            .Register(153, Definition.DynamicAt(7)) // hybridcrypt map SDB (legacy)
            .Register(155, Definition.DynamicAt(8, 6)) // chat new (every enter game, WORD size at offset 6)
            .Register(156, Definition.Fixed(97))    // private shop search row (legacy)
            .Register(157, Definition.Fixed(97))    // new private shop search row (legacy)
            .Register(171, Definition.DynamicAt32(7)) // switchbot (every enter game, int size at offset 1)
            .Register(208, Definition.Fixed(133))   // specific effect path
            .Register(209, Definition.Fixed(5))     // dragon soul refine result
            .Register(211, Definition.DynamicAt(4)) // in-game wiki (WORD size at offset 1)
            // GC_NEW_OFFLINESHOP uses __ENABLE_LARGE_DYNAMIC_PACKET__ on both sides, so the
            // envelope is { BYTE header; int wSize; BYTE subHeader; } = 6 bytes and wSize is the
            // full packet length. The payload is subheader specific (offlineshop::TShopInfo and
            // friends), so only the declared length can frame it.
            .Register(214, Definition.DynamicAt32(6, 1)) // Auriga offline shop envelope (int32 size)
            .Register(218, Definition.Fixed(2092))  // ranking table (header + 51 * TPacketGCList(41))
            .Register(215, Definition.Fixed(23))    // acce window (SPacketAcce: header..dwMaxAbs)
            .Register(217, Definition.Fixed(179))   // cube renewal (header, subheader, TInfoDateCubeRenewal(177))
            .Register(223, Definition.Fixed(12))    // new fishing (SPacketFishingNew)
            .Register(222, Definition.DynamicAt(5)) // boss position (every enter game, WORD size)
            .Register(225, Definition.Fixed(11)) // mount flight snapshot
            .Register(224, Definition.DynamicAt(7)) // mount inventory (WORD size)
            .Register(246, Definition.Fixed(93))    // item on title name update (every enter game)
            .Register(249, Definition.Fixed(13))    // fake shop sign
            .Register(250, Definition.Fixed(2049))  // leaderboard data
            .Register(251, Definition.Fixed(2049))  // leaderboard news
            .Register(252, Definition.Fixed(2049))); // leaderboard guild

        /// <summary>GC_NEW_OFFLINESHOP declares a 32 bit wSize, and a full shop list is
        /// 10 + 81 per shop, so a busy server can legitimately exceed the 64 KB default guard.</summary>
        private static Metin2PacketFramer ConfigureGameCoreFramer(Metin2PacketFramer framer)
        {
            framer.MaxPacketSize = 1 << 20;
            return framer;
        }
    }
}
