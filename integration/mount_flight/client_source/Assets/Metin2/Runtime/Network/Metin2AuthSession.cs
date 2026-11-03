using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Tasks;

namespace Metin2.Runtime.Network
{
    /// <summary>
    /// Auriga TLS handshake and LOGIN3/LOGIN2 transport.
    ///
    /// The game endpoint is serviced by a single background receive pump from the moment the
    /// TLS PHASE_LOGIN packet arrives. This is mandatory, not an optimisation: the server
    /// descriptor runs <c>ping_event</c> in every phase, and that event closes the descriptor when
    /// the previous GC_PING was not answered with a CG_PONG. It also emits a GC_HANDSHAKE
    /// on every cycle which must be answered with CG_TIME_SYNC carrying the same handshake token.
    /// Leaving the socket unread while the player looks at the character selection screen therefore
    /// terminates the connection, which is what produced the "Metin2 game server closed the
    /// connection" failure on character select.
    /// </summary>
    public sealed class Metin2AuthSession : IDisposable
    {
        public enum SessionState { Disconnected, Connecting, TlsHandshake, Handshake, Login, Auth, Authenticated, Failed }
        public event Action<byte,ushort,byte,ushort> ItemMoveSending;

        private Metin2TlsTransport transport;
        private readonly Metin2TlsOptions tlsOptions;
        private bool disposed;

        public Metin2AuthSession(Metin2TlsOptions options = null)
        {
            tlsOptions = (options ?? Metin2TlsTransport.DefaultOptions).Snapshot();
        }
        private int sequenceIndex;
        private readonly int diagnosticSessionId = Metin2NetworkDiagnostics.CreateSession("created");
        private string diagnosticRole = "unknown";
        private uint synchronizedServerTime; private int synchronizedLocalTick;
        private readonly Queue<byte[]> deferredPackets = new Queue<byte[]>();
        private readonly Metin2PacketFramer gameFramer = Metin2PacketFramer.CreateGameCoreFramer();

        /// <summary>
        /// Turns on raw framing traces for the given headers. Used to locate a stream desync: the
        /// declared length of every traced packet can then be checked against its own payload.
        /// </summary>
        public void TraceFraming(Action<string> sink, params byte[] headers)
        {
            gameFramer.Trace = sink;
            gameFramer.TraceHeaders.Clear();
            foreach (byte header in headers ?? Array.Empty<byte>()) gameFramer.TraceHeaders.Add(header);
        }

        private readonly object inboundGate = new object();
        private readonly Queue<byte[]> inbound = new Queue<byte[]>();
        private readonly SemaphoreSlim inboundSignal = new SemaphoreSlim(0);
        private readonly SemaphoreSlim sendGate = new SemaphoreSlim(1, 1);
        private int diagnosticPendingSends;
        private CancellationTokenSource pumpLifetime;
        private Task pumpTask;
        private Exception pumpFault;

        public SessionState State { get; private set; } = SessionState.Disconnected;
        /// <summary>Hardware id sent in TPacketCGLogin3. Must be primed on the Unity main thread.</summary>
        public static string Hwid { get; set; }
        /// <summary>Number of GC_PING packets answered with a CG_PONG by the keepalive pump.</summary>
        public int PongCount { get; private set; }
        /// <summary>Number of GC_HANDSHAKE packets answered with a CG_TIME_SYNC by the keepalive pump.</summary>
        public int TimeSyncCount { get; private set; }
        /// <summary>True while the background receive pump owns the socket.</summary>
        public bool IsPumping => pumpTask != null && !pumpTask.IsCompleted;
        internal int DiagnosticSessionId => diagnosticSessionId;

        public async Task ConnectAsync(string host, int port, CancellationToken cancellationToken)
        {
            await ConnectCoreAsync(host, port, Metin2ProtocolCodec.PhaseAuth, cancellationToken).ConfigureAwait(false);
        }

        public async Task ConnectGameEndpointAsync(string host, int port, CancellationToken cancellationToken)
        {
            await ConnectCoreAsync(host, port, Metin2ProtocolCodec.PhaseLogin, cancellationToken).ConfigureAwait(false);
        }

        private async Task ConnectCoreAsync(string host, int port, byte expectedPhase, CancellationToken cancellationToken)
        {
            if (disposed) throw new ObjectDisposedException(nameof(Metin2AuthSession));
            if (State != SessionState.Disconnected) throw new InvalidOperationException("Session is already in use");
            bool auth = expectedPhase == Metin2ProtocolCodec.PhaseAuth;
            diagnosticRole = auth ? "auth" : "game";
            Metin2NetworkDiagnostics.Endpoint(diagnosticSessionId, diagnosticRole, host, port);
            try
            {
                State = SessionState.Connecting;
                Metin2NetworkDiagnostics.State(diagnosticSessionId, State.ToString());
                State = SessionState.TlsHandshake;
                transport = await Metin2TlsTransport.ConnectAsync(host, port, cancellationToken, tlsOptions).ConfigureAwait(false);
                Metin2NetworkDiagnostics.Connected(diagnosticSessionId, diagnosticRole, "TLS", host + ":" + port);
                using (var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
                {
                    deadline.CancelAfter(tlsOptions.HandshakeTimeoutMilliseconds);
                    CancellationToken token = deadline.Token;
                    State = SessionState.Handshake;
                    Metin2NetworkDiagnostics.State(diagnosticSessionId, State.ToString());
                    // LoginServer starts directly with GC_HANDSHAKE. Only a game descriptor
                    // sends PHASE_HANDSHAKE first; neither endpoint performs legacy DH2.
                    if (!auth)
                    {
                        byte[] initialPhase = await ReadExactAsync(2, token).ConfigureAwait(false);
                        if (!Metin2ProtocolCodec.TryReadPhase(initialPhase, out byte phase) || phase != Metin2ProtocolCodec.PhaseHandshake)
                            throw new InvalidDataException("Expected game PHASE_HANDSHAKE.");
                    }
                    bool completed = false, challengeAnswered = false;
                    for (int retry = 0; retry < 34 && !completed; retry++)
                    {
                        byte header = (await ReadExactAsync(1, token).ConfigureAwait(false))[0];
                        if (header == Metin2ProtocolCodec.HeaderGcHandshake)
                        {
                            byte[] tail = await ReadExactAsync(12, token).ConfigureAwait(false);
                            uint challenge = Metin2ProtocolCodec.ReadUInt32(tail, 0);
                            uint time = Metin2ProtocolCodec.ReadUInt32(tail, 4);
                            int delta = unchecked((int)Metin2ProtocolCodec.ReadUInt32(tail, 8));
                            if (delta < 0) throw new InvalidDataException("Negative handshake delta.");
                            synchronizedServerTime = unchecked(time + (uint)delta * 2);
                            synchronizedLocalTick = Environment.TickCount;
                            await WriteAsync(Metin2ProtocolCodec.WriteHandshake(challenge, synchronizedServerTime, 0), token).ConfigureAwait(false);
                            challengeAnswered = true;
                        }
                        else if (header == Metin2ProtocolCodec.HeaderGcHandshakeCompleted)
                        {
                            if (!challengeAnswered) throw new InvalidDataException("Handshake completion arrived before a challenge.");
                            await ReadExactAsync(3, token).ConfigureAwait(false);
                            completed = true;
                        }
                        else throw new InvalidDataException("Unexpected TLS handshake packet " + header);
                    }
                    if (!completed) throw new InvalidDataException("The server did not complete the application handshake.");
                    if (!auth)
                    {
                        byte[] phase = await ReadExactAsync(2, token).ConfigureAwait(false);
                        if (!Metin2ProtocolCodec.TryReadPhase(phase, out byte value) || value != Metin2ProtocolCodec.PhaseLogin)
                            throw new InvalidDataException("Expected game PHASE_LOGIN.");
                    }
                    State = auth ? SessionState.Auth : SessionState.Login;
                    Metin2NetworkDiagnostics.State(diagnosticSessionId, State.ToString());
                }
                if (!auth) StartPump();
            }
            catch (OperationCanceledException exception) when (!cancellationToken.IsCancellationRequested)
            {
                State = SessionState.Failed;
                DisposeTransport();
                throw new TimeoutException("The server application handshake timed out.", exception);
            }
            catch (Exception exception)
            {
                State = SessionState.Failed;
                Metin2NetworkDiagnostics.State(diagnosticSessionId, State.ToString());
                Metin2NetworkDiagnostics.Fault(diagnosticSessionId, "ConnectCoreAsync", exception);
                DisposeTransport();
                throw;
            }
        }

        // ---------------------------------------------------------------- keepalive pump

        private void StartPump()
        {
            if (pumpTask != null) return;
            pumpLifetime = new CancellationTokenSource();
            Metin2NetworkDiagnostics.Event(diagnosticSessionId, "PUMP_START");
            pumpTask = Task.Run(() => PumpAsync(pumpLifetime.Token));
        }

        private async Task PumpAsync(CancellationToken token)
        {
            var wire = new byte[8192];
            try
            {
                while (!token.IsCancellationRequested)
                {
                    Metin2NetworkDiagnostics.TransportReadBegin(diagnosticSessionId, "PumpAsync", wire.Length);
                    int recvStart = Environment.TickCount;
                    int read = await transport.ReadAsync(wire, 0, wire.Length, token).ConfigureAwait(false);
                    int recvElapsed = unchecked(Environment.TickCount - recvStart);
                    Metin2NetworkDiagnostics.TransportReadOk(diagnosticSessionId, "PumpAsync", read, recvElapsed);
                    if (read == 0)
                    {
                        Metin2NetworkDiagnostics.Eof(diagnosticSessionId, "PumpAsync.ReadAsync");
                        throw new EndOfStreamException("Metin2 game server closed the connection");
                    }
                    Metin2NetworkDiagnostics.RawReceive(read);
                    var chunk = new byte[read];
                    Buffer.BlockCopy(wire, 0, chunk, 0, read);
                    var framedPackets = new List<byte[]>(gameFramer.Feed(chunk));
                    Metin2NetworkDiagnostics.FramerResult(diagnosticSessionId, read, framedPackets.Count);
                    foreach (byte[] packet in framedPackets)
                    {
                        if (packet.Length == 0 || packet[0] == 0) continue;
                        Metin2NetworkDiagnostics.ReceivePacket(diagnosticSessionId, packet);
                        if (packet[0] == Metin2ProtocolCodec.HeaderGcPing)
                        {
                            await SendSequencedAsync(Metin2ProtocolCodec.WritePong, token).ConfigureAwait(false);
                            PongCount++;
                            continue;
                        }
                        if (packet[0] == Metin2ProtocolCodec.HeaderGcHandshake)
                        {
                            uint handshake = Metin2ProtocolCodec.ReadUInt32(packet, 1);
                            uint serverTime = Metin2ProtocolCodec.ReadUInt32(packet, 5);
                            int delta = unchecked((int)Metin2ProtocolCodec.ReadUInt32(packet, 9));
                            synchronizedServerTime = unchecked(serverTime + (uint)delta);
                            synchronizedLocalTick = Environment.TickCount;
                            await SendSequencedAsync(sequence => Metin2ProtocolCodec.WriteTimeSync(handshake, serverTime, delta, sequence), token).ConfigureAwait(false);
                            TimeSyncCount++;
                            continue;
                        }
                        // Bare GC_TIME_SYNC is DESC::HandshakeProcess acknowledging our clock; nothing to do.
                        if (packet[0] == Metin2ProtocolCodec.HeaderGcTimeSync && packet.Length == 1) continue;
                        // The legacy client sends these variable packets to CEterPackManager to update
                        // encrypted archive keys/SDB data. Unity consumes already-imported assets, so
                        // there is no Eter archive manager to update; keeping them out of game-runtime
                        // dispatch preserves the packet boundary without inventing a gameplay action.
                        if (packet[0] == 152 || packet[0] == 153) continue;
                        lock (inboundGate) inbound.Enqueue(packet);
                        inboundSignal.Release();
                    }
                }
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                Metin2NetworkDiagnostics.Event(diagnosticSessionId, "PUMP_CANCELLED");
            }
            catch (Exception exception)
            {
                pumpFault = exception;
                Metin2NetworkDiagnostics.Fault(diagnosticSessionId, "PumpAsync", exception);
            }
            finally
            {
                Metin2NetworkDiagnostics.Event(diagnosticSessionId, "PUMP_STOP");
                inboundSignal.Release();
            }
        }

        private async Task<byte[]> ReceiveFramedPacketAsync(CancellationToken token)
        {
            while (true)
            {
                lock (inboundGate) { if (inbound.Count > 0) return inbound.Dequeue(); }
                if (disposed) throw new ObjectDisposedException(nameof(Metin2AuthSession));
                Exception fault = pumpFault;
                if (fault != null)
                {
                    inboundSignal.Release();
                    ExceptionDispatchInfo.Capture(fault).Throw();
                }
                await inboundSignal.WaitAsync(token).ConfigureAwait(false);
            }
        }

        // ---------------------------------------------------------------- login / select

        public Task SendLoginByKeyAsync(string login, uint loginKey, uint[] clientKey, string language, CancellationToken cancellationToken)
        {
            if (State != SessionState.Login) throw new InvalidOperationException("Session is not in game login phase");
            // Auriga TPacketCGLogin2 has no language field; language is negotiated by CG_CHANGE_LANGUAGE.
            return SendSequencedAsync(sequence => Metin2ProtocolCodec.WriteSequencedLogin2(login, loginKey, clientKey, sequence), cancellationToken);
        }

        public async Task<GameLoginResult> ReceiveGameLoginAsync(CancellationToken cancellationToken)
        {
            if (State != SessionState.Login) throw new InvalidOperationException("Session is not in game login phase");
            byte empire = 0; Metin2ProtocolCodec.CharacterList list = default; bool hasList = false;
            for (int packetIndex = 0; packetIndex < 32 && !hasList; packetIndex++)
            {
                byte[] packet = await ReceiveFramedPacketAsync(cancellationToken).ConfigureAwait(false);
                switch (packet[0])
                {
                    case 7:
                        return new GameLoginResult(false, Encoding.ASCII.GetString(packet, 1, packet.Length - 1).TrimEnd('\0'), empire, default);
                    case 90:
                        empire = packet[1];
                        break;
                    case Metin2ProtocolCodec.HeaderGcPhase:
                        if (!Metin2ProtocolCodec.TryReadPhase(packet, out byte phase) || phase != Metin2ProtocolCodec.PhaseSelect)
                            throw new InvalidDataException("Expected PHASE_SELECT, received " + packet[1]);
                        break;
                    case 6:
                    case 32:
                        list = Metin2ProtocolCodec.ReadCharacterList(packet); hasList = true; break;
                    default:
                        deferredPackets.Enqueue(packet); break;
                }
            }
            if (!hasList) throw new InvalidDataException("Character list was not received");
            State = SessionState.Authenticated;
            return new GameLoginResult(true, "OK", empire, list);
        }

        public Task SendCharacterSelectAsync(byte index, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteCharacterSelect(index, sequence), token);

        public async Task<GameLoginResult> SelectEmpireAsync(byte empire,CancellationToken token)
        {
            if(State!=SessionState.Authenticated)throw new InvalidOperationException("Empire selection requires character-select phase");
            if(empire<1||empire>3)throw new ArgumentOutOfRangeException(nameof(empire));
            await SendGameAsync(sequence=>Metin2ProtocolCodec.WriteSelectEmpire(empire,sequence),token).ConfigureAwait(false);
            return await ReceiveEmpireSelectionAsync(empire,token).ConfigureAwait(false);
        }

        private async Task<GameLoginResult> ReceiveEmpireSelectionAsync(byte empire,CancellationToken token)
        {
            byte acknowledged=0;
            while(true)
            {
                byte[] packet=await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                if(packet[0]==90){acknowledged=packet[1];continue;}
                if(packet[0]==6||packet[0]==32)
                {
                    if(acknowledged!=empire)throw new InvalidDataException("Empire selection was not acknowledged by server");
                    return new GameLoginResult(true,"OK",acknowledged,Metin2ProtocolCodec.ReadCharacterList(packet));
                }
                if(packet[0]==Metin2ProtocolCodec.HeaderGcPhase&&packet[1]!=Metin2ProtocolCodec.PhaseSelect)throw new InvalidDataException("Server left character-select phase during empire selection");
                deferredPackets.Enqueue(packet);
            }
        }

        public async Task<Metin2ProtocolCodec.CharacterCreateResult> CreateCharacterAsync(byte index, string name, byte race, byte shape, CancellationToken token,byte skillGroup=0,ushort hairAppearance=0)
        {
            await SendGameAsync(sequence => skillGroup==0?Metin2ProtocolCodec.WriteCharacterCreate(index,name,race,shape,sequence):Metin2ProtocolCodec.WriteCharacterCreate(index,name,race,shape,skillGroup,hairAppearance,sequence), token).ConfigureAwait(false);
            while (true)
            {
                byte[] packet = await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                if (packet[0] == 8) return Metin2ProtocolCodec.ReadCharacterCreateSuccess(packet);
                if (packet[0] == 9) return new Metin2ProtocolCodec.CharacterCreateResult(false, index, Metin2ProtocolCodec.ReadCharacterCreateFailure(packet), default);
                deferredPackets.Enqueue(packet);
            }
        }

        public async Task<CharacterDeleteResult> DeleteCharacterAsync(byte index, string privateCode, CancellationToken token)
        {
            await SendGameAsync(sequence => Metin2ProtocolCodec.WriteCharacterDelete(index, privateCode, sequence), token).ConfigureAwait(false);
            while (true)
            {
                byte[] packet = await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                if (packet[0] == 10) return new CharacterDeleteResult(true, Metin2ProtocolCodec.ReadCharacterDeleteSuccess(packet), false);
                if (packet[0] == 11) return new CharacterDeleteResult(false, index, true);
                deferredPackets.Enqueue(packet);
            }
        }

        public async Task<CharacterNameChangeResult> ChangeCharacterNameAsync(byte index, string name, CancellationToken token)
        {
            await SendGameAsync(sequence => Metin2ProtocolCodec.WriteCharacterChangeName(index, name, sequence), token).ConfigureAwait(false);
            while (true)
            {
                byte[] packet = await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                if (packet[0] == 107)
                {
                    Metin2ProtocolCodec.CharacterNameChange change = Metin2ProtocolCodec.ReadCharacterNameChange(packet);
                    return new CharacterNameChangeResult(true, index, 0, change);
                }
                // The target server uses the same generic failure response as character creation.
                if (packet[0] == 9) return new CharacterNameChangeResult(false, index, Metin2ProtocolCodec.ReadCharacterCreateFailure(packet), default);
                deferredPackets.Enqueue(packet);
            }
        }

        /// <summary>
        /// Consumes packets until the requested GC_PHASE arrives. Every packet seen on the way is
        /// deferred, so the loading burst (points, items, quickslots, skills, main character, ...)
        /// is replayed to the game runtime instead of being dropped.
        /// </summary>
        public async Task WaitForGamePhaseAsync(byte expectedPhase, CancellationToken token)
        {
            if (State != SessionState.Authenticated) throw new InvalidOperationException("Character list login has not completed");
            while (true)
            {
                byte[] packet = await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                if (packet[0] == Metin2ProtocolCodec.HeaderGcPhase)
                {
                    if (!Metin2ProtocolCodec.TryReadPhase(packet, out byte phase)) throw new InvalidDataException("Invalid GC_PHASE packet");
                    if (phase == expectedPhase) return;
                    if (phase == Metin2ProtocolCodec.PhaseClose) throw new InvalidDataException("Server moved the descriptor to PHASE_CLOSE");
                    throw new InvalidDataException("Expected game phase " + expectedPhase + ", received " + phase);
                }
                deferredPackets.Enqueue(packet);
            }
        }

        /// <summary>
        /// Consumes the loading burst until GC_MAIN_CHARACTER (113 / 137 / 138) arrives, exactly like
        /// CPythonNetworkStream::LoadingPhase. The main-character packet is deferred as well so the
        /// game runtime still receives it once the receive loop takes over.
        /// </summary>
        public async Task<byte[]> WaitForMainCharacterAsync(CancellationToken token)
        {
            if (State != SessionState.Authenticated) throw new InvalidOperationException("Character list login has not completed");
            while (true)
            {
                byte[] packet = await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
                deferredPackets.Enqueue(packet);
                if (packet[0] == 113 || packet[0] == 137 || packet[0] == 138) return packet;
                if (packet[0] == Metin2ProtocolCodec.HeaderGcPhase &&
                    Metin2ProtocolCodec.TryReadPhase(packet, out byte phase) && phase == Metin2ProtocolCodec.PhaseClose)
                    throw new InvalidDataException("Server moved the descriptor to PHASE_CLOSE while loading");
            }
        }

        public Task SendClientVersionAsync(string filename, string timestamp, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteClientVersion(filename, timestamp, sequence), token);

        public Task SendEnterGameAsync(CancellationToken token) => SendGameAsync(Metin2ProtocolCodec.WriteEnterGame, token);

        public Task SendMountFlightAsync(byte action,int altitude,uint time,CancellationToken token)=>
            SendGameAsync(s=>Metin2ProtocolCodec.WriteMountFlight(action,altitude,time,s),token);
        public Task SendMoveAsync(byte function, byte argument, byte rotation, int x, int y, uint time, uint duration, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteMove(function, argument, rotation, x, y, time, duration, sequence), token);
        public Task SendSyncPositionAsync(uint victimVid, int x, int y, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteSyncPosition(victimVid, x, y, sequence), token);
        public Task SendAttackAsync(byte type, uint targetVid, byte processCrc, byte fileCrc, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteAttack(type, targetVid, processCrc, fileCrc, sequence), token);
        public Task SendShootAsync(byte type, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteShoot(type, sequence), token);
        public Task SendFlyTargetingAsync(uint targetVid, int x, int y, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteFlyTargeting(targetVid, x, y, sequence), token);
        public Task SendUseSkillAsync(uint skillVnum, uint targetVid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteUseSkill(skillVnum, targetVid, sequence), token);
        public Task SendTargetAsync(uint targetVid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteTarget(targetVid, sequence), token);
        public Task SendChatAsync(byte type, string message, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteChat(type, message, sequence), token);
        public Task SendWhisperAsync(string recipient, string message, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteWhisper(recipient, message, sequence), token);
        public Task SendMessengerAddByNameAsync(string name, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteMessengerAddByName(name, sequence), token);
        public Task SendMessengerAddByVidAsync(uint vid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteMessengerAddByVid(vid, sequence), token);
        public Task SendMessengerRemoveAsync(string name, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteMessengerRemove(name, sequence), token);
        public Task SendPartyInviteAnswerAsync(uint leaderPid,bool accept,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartyInviteAnswer(leaderPid,accept,s),token);
        public Task SendPartyInviteAsync(uint vid,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartyInvite(vid,s),token);
        public Task SendPartyRemoveAsync(uint pid,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartyRemove(pid,s),token);
        public Task SendPartySetStateAsync(uint vid,byte state,bool flag,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartySetState(vid,state,flag,s),token);
        public Task SendPartyUseSkillAsync(byte skill,uint targetVid,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartyUseSkill(skill,targetVid,s),token);
        public Task SendPartyParameterAsync(byte mode,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WritePartyParameter(mode,s),token);
        public Task SendGuildInviteAnswerAsync(uint guildId,bool accept,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildInviteAnswer(guildId,accept,s),token);
        public Task SendGuildCreateAsync(string name,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildCreate(name,s),token);
        public Task SendGuildAddMemberAsync(uint vid,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildAddMember(vid,s),token);
        public Task SendGuildRemoveMemberAsync(uint pid,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildRemoveMember(pid,s),token);
        public Task SendGuildPostCommentAsync(string text,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildPostComment(text,s),token);
        public Task SendGuildDeleteCommentAsync(uint id,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildDeleteComment(id,s),token);
        public Task SendGuildRefreshCommentsAsync(CancellationToken token)=>SendGameAsync(Metin2ProtocolCodec.WriteGuildRefreshComments,token);
        public Task SendGuildUseSkillAsync(uint skill,uint target,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildUseSkill(skill,target,s),token);
        public Task SendGuildChargePowerAsync(uint money,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildChargePower(money,s),token);
        public Task SendGuildChangeGradeNameAsync(byte grade,string name,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildChangeGradeName(grade,name,s),token);
        public Task SendGuildChangeGradeAuthorityAsync(byte grade,byte authority,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildChangeGradeAuthority(grade,authority,s),token);
        public Task SendGuildOfferAsync(uint experience,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildOffer(experience,s),token);
        public Task SendGuildChangeMemberGradeAsync(uint pid,byte grade,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildChangeMemberGrade(pid,grade,s),token);
        public Task SendGuildChangeMemberGeneralAsync(uint pid,bool general,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildChangeMemberGeneral(pid,general,s),token);
        public Task SendGuildDepositAsync(uint money,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildDeposit(money,s),token);
        public Task SendGuildWithdrawAsync(uint money,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteGuildWithdraw(money,s),token);
        public Task SendItemUseAsync(byte window, ushort cell, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteItemUse(window, cell, sequence), token);
        public Task SendItemMoveAsync(byte fromWindow, ushort fromCell, byte toWindow, ushort toCell, int count, CancellationToken token)
        {
            ItemMoveSending?.Invoke(fromWindow,fromCell,toWindow,toCell);
            return SendGameAsync(sequence => Metin2ProtocolCodec.WriteItemMove(fromWindow, fromCell, toWindow, toCell, count, sequence), token);
        }
        public Task SendItemDropAsync(byte window, ushort cell, uint gold, int count, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteItemDrop(window, cell, gold, count, sequence), token);
        public Task SendGiveItemAsync(uint targetVid, byte window, ushort cell, int count, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteGiveItem(targetVid, window, cell, count, sequence), token);
        public Task SendItemUseToItemAsync(byte sourceWindow, ushort sourceCell, byte targetWindow, ushort targetCell, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteItemUseToItem(sourceWindow, sourceCell, targetWindow, targetCell, sequence), token);
        public Task SendItemPickupAsync(uint itemVid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteItemPickup(itemVid, sequence), token);
        public Task SendFishingAsync(byte direction, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteFishing(direction, sequence), token);
        public Task SendShopEndAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteShopEnd, token);
        public Task SendShopBuyAsync(byte displayPosition, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteShopBuy(displayPosition, sequence), token);
        public Task SendShopSellAsync(byte window, ushort cell, ushort count, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteShopSell(window, cell, count, sequence), token);
        public Task SendShopBuyMultipleAsync(byte displayPosition, byte count, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteShopBuyMultiple(displayPosition, count, sequence), token);
        public Task SendPrivateShopAsync(string sign, System.Collections.Generic.IReadOnlyList<Metin2ProtocolCodec.PrivateShopItem> items, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WritePrivateShop(sign, items, sequence), token);
        public Task SendExchangeItemAddAsync(byte window, ushort cell, byte displayPosition, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteExchangeItemAdd(window, cell, displayPosition, sequence), token);
        public Task SendExchangeItemDeleteAsync(byte displayPosition, CancellationToken token) =>
            SendGameAsync(sequence => Metin2ProtocolCodec.WriteExchangeItemDelete(displayPosition, sequence), token);
        public Task SendExchangeStartAsync(uint vid, CancellationToken token) => SendGameAsync(sequence => Metin2ProtocolCodec.WriteExchangeStart(vid, sequence), token);
        public Task SendExchangeGoldAsync(long gold, CancellationToken token) => SendGameAsync(sequence => Metin2ProtocolCodec.WriteExchangeMoney(gold, sequence), token);
        public Task SendExchangeAcceptAsync(CancellationToken token) => SendGameAsync(Metin2ProtocolCodec.WriteExchangeAccept, token);
        public Task SendExchangeCancelAsync(CancellationToken token) => SendGameAsync(Metin2ProtocolCodec.WriteExchangeCancel, token);
        public Task SendSafeboxCheckinAsync(uint safePosition, byte fromWindow, ushort fromCell, CancellationToken token) => SendGameAsync(s => Metin2ProtocolCodec.WriteSafeboxCheckin(safePosition, fromWindow, fromCell, s), token);
        public Task SendSafeboxCheckoutAsync(uint safePosition, byte toWindow, ushort toCell, CancellationToken token) => SendGameAsync(s => Metin2ProtocolCodec.WriteSafeboxCheckout(safePosition, toWindow, toCell, s), token);
        public Task SendMallCheckoutAsync(uint mallPosition, byte toWindow, ushort toCell, CancellationToken token) => SendGameAsync(s => Metin2ProtocolCodec.WriteMallCheckout(mallPosition, toWindow, toCell, s), token);
        public Task SendSafeboxMoveAsync(ushort fromPosition, ushort toPosition, CancellationToken token) => SendGameAsync(s => Metin2ProtocolCodec.WriteSafeboxMove(fromPosition, toPosition, s), token);
        public Task SendNpcClickAsync(uint vid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteNpcClick(vid, sequence), token);
        public Task SendScriptAnswerAsync(byte answer, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteScriptAnswer(answer, sequence), token);
        public Task SendScriptButtonAsync(uint questIndex, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteScriptButton(questIndex, sequence), token);
        public Task SendQuestInputAsync(string value, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteQuestInput(value, sequence), token);
        public Task SendScriptSelectItemAsync(uint selection, CancellationToken token) => SendGameAsync(sequence => Metin2ProtocolCodec.WriteScriptSelectItem(selection, sequence), token);
        public Task SendQuestConfirmAsync(byte answer, uint requestPid, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteQuestConfirm(answer, requestPid, sequence), token);
        public Task SendRefineAsync(ushort position, byte type, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteRefine(position, type, sequence), token);
        // Auriga has no client-to-server item shop header at all: uiitemshop.py drives the shop
        // through the /ishop chat command (cmd.cpp do_ishop). The legacy 87 writers stay in the
        // codec for the old build but must never reach this server - packet_info.cpp does not
        // register header 87, so the descriptor would close on the unknown header.
        // uiitemshop.py:754 sends the cached UPDATE_TIME so the server can answer with the 9999
        // "no data" marker when nothing changed, and :774 sends /ishop log for the history window.
        public Task SendItemShopOpenAsync(int updateTime,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteChat(0,"/ishop data "+updateTime,s),token);
        public Task SendItemShopBuyAsync(uint id,ushort count,CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteChat(0,"/ishop buy "+id+" "+count,s),token);
        public Task SendItemShopLogAsync(CancellationToken token)=>SendGameAsync(s=>Metin2ProtocolCodec.WriteChat(0,"/ishop log",s),token);
        /// <summary>The Auriga shop pushes the limited count with ITEMSHOP_LOAD, so no request is needed.</summary>
        public Task SendItemShopRequestCountAsync(ushort id,CancellationToken token)=>Task.CompletedTask;
        public Task SendQuickSlotAddAsync(byte globalPosition, byte type, ushort target, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteQuickSlotAdd(globalPosition, type, target, sequence), token);
        public Task SendQuickSlotDeleteAsync(byte globalPosition, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteQuickSlotDelete(globalPosition, sequence), token);
        public Task SendQuickSlotSwapAsync(byte globalFrom, byte globalTo, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteQuickSlotSwap(globalFrom, globalTo, sequence), token);
        public Task SendOfflineShopEndAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopEnd, token);
        public Task SendOfflineShopBuyAsync(byte position, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteOfflineShopBuy(position, sequence), token);
        public Task SendOfflineShopAddAsync(byte window, ushort cell, byte displayPosition, long price, CancellationToken token) =>
            SendGameAsync(sequence => Metin2GamePacketCodec.WriteOfflineShopAdd(window, cell, displayPosition, price, sequence), token);
        public Task SendOfflineShopRemoveAsync(byte position, CancellationToken token) => SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopRemove(position, s), token);
        public Task SendOfflineShopDestroyAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopDestroy, token);
        public Task SendOfflineShopRefreshAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopRefresh, token);
        public Task SendOfflineShopRefreshMoneyAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopRefreshMoney, token);
        public Task SendOfflineShopWithdrawAsync(long money, CancellationToken token) => SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopWithdraw(money, s), token);
        public Task SendOfflineShopRefreshUnsoldAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopRefreshUnsold, token);
        public Task SendOfflineShopTakeAsync(byte position, CancellationToken token) => SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopTake(position, s), token);
        public Task SendOfflineShopCheckAsync(CancellationToken token) => SendGameAsync(Metin2GamePacketCodec.WriteOfflineShopCheck, token);
        public Task SendOfflineShopChangePriceAsync(byte position, long price, CancellationToken token) => SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopChangePrice(position, price, s), token);
        public Task SendOfflineShopChangeNameAsync(string name, CancellationToken token) => SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopChangeName(name, s), token);
        public Task SendOfflineShopBuildAsync(string name, byte npcStyle, byte boardStyle,
            System.Collections.Generic.IReadOnlyList<Metin2GamePacketCodec.OfflineShopBuildItem> items, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteOfflineShopBuild(name, npcStyle, boardStyle, items, s), token);

        // ---- Auriga CG_NEW_OFFLINESHOP (119): the authoritative offline-shop protocol. The
        // legacy 57-subheader writers above belong to the old build and are kept only as rollback. ----

        public Task SendAurigaOfflineShopCreateAsync(string name, uint duration, uint kasmirNpc,
            System.Collections.Generic.IReadOnlyList<Metin2GamePacketCodec.AurigaShopPositionItem> items, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopCreate(name, duration, kasmirNpc, items, s), token);
        public Task SendAurigaOfflineShopChangeNameAsync(string name, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopChangeName(name, s), token);
        public Task SendAurigaOfflineShopForceCloseAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopForceClose, token);
        public Task SendAurigaOfflineShopRequestListAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopRequestList, token);
        public Task SendAurigaOfflineShopOpenAsync(uint ownerId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopOpen(ownerId, s), token);
        public Task SendAurigaOfflineShopOpenOwnerAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopOpenOwner, token);
        public Task SendAurigaOfflineShopBuyAsync(uint ownerId, uint itemId, bool isSearch, long totalPriceSeen, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopBuy(ownerId, itemId, isSearch, totalPriceSeen, s), token);
        public Task SendAurigaOfflineShopAddItemAsync(byte window, ushort cell, long price, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopAddItem(window, cell, price, s), token);
        public Task SendAurigaOfflineShopRemoveItemAsync(uint itemId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopRemoveItem(itemId, s), token);
        public Task SendAurigaOfflineShopEditItemAsync(uint itemId, long price, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopEditItem(itemId, price, s), token);
        public Task SendAurigaOfflineShopFilterRequestAsync(byte type, byte subType, string name, long priceStart, long priceEnd,
            int levelStart, int levelEnd, uint wearFlag, Metin2GamePacketCodec.ItemAttribute[] attributes, int rarity, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopFilterRequest(type, subType, name, priceStart, priceEnd, levelStart, levelEnd, wearFlag, attributes, rarity, s), token);
        public Task SendAurigaOfflineShopOfferCreateAsync(uint ownerId, uint itemId, long price, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopOfferCreate(ownerId, itemId, price, s), token);
        public Task SendAurigaOfflineShopOfferAcceptAsync(uint offerId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopOfferAccept(offerId, s), token);
        public Task SendAurigaOfflineShopOfferCancelAsync(uint offerId, uint ownerId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopOfferCancel(offerId, ownerId, s), token);
        public Task SendAurigaOfflineShopOfferListRequestAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopOfferListRequest, token);
        public Task SendAurigaOfflineShopSafeboxOpenAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopSafeboxOpen, token);
        public Task SendAurigaOfflineShopSafeboxGetItemAsync(uint itemId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopSafeboxGetItem(itemId, s), token);
        public Task SendAurigaOfflineShopSafeboxGetValutesAsync(long yang, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopSafeboxGetValutes(yang, s), token);
        public Task SendAurigaOfflineShopSafeboxCloseAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopSafeboxClose, token);
        public Task SendAurigaOfflineShopAuctionListRequestAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionListRequest, token);
        public Task SendAurigaOfflineShopAuctionOpenAsync(uint ownerId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionOpen(ownerId, s), token);
        public Task SendAurigaOfflineShopAuctionOpenMyAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionOpenMy, token);
        public Task SendAurigaOfflineShopAuctionCreateAsync(uint duration, byte window, ushort cell, long initPrice, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionCreate(duration, window, cell, initPrice, s), token);
        public Task SendAurigaOfflineShopAuctionAddOfferAsync(uint ownerId, long price, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionAddOffer(ownerId, price, s), token);
        public Task SendAurigaOfflineShopAuctionExitFromAsync(uint ownerId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopAuctionExitFrom(ownerId, s), token);
        public Task SendAurigaOfflineShopCloseBoardAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopCloseBoard, token);
        public Task SendAurigaOfflineShopClickEntityAsync(uint shopVid, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAurigaOfflineShopClickEntity(shopVid, s), token);
        public Task SendAurigaOfflineShopCloseMyAuctionAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteAurigaOfflineShopCloseMyAuction, token);

        // ---- Remaining client-sendable Auriga CG headers ----

        public Task SendLoginAsync(string login, string password, CancellationToken token) =>
            SendSequencedAsync(s => Metin2GamePacketCodec.WriteLogin(login, password, s), token);
        public Task SendItemDropGroundAsync(byte window, ushort cell, long elk, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteItemDropGround(window, cell, elk, s), token);
        public Task SendItemDestroyAsync(byte window, ushort cell, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteItemDestroy(window, cell, s), token);
        public Task SendItemDivisionAsync(byte window, ushort cell, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteItemDivision(window, cell, s), token);
        public Task SendInventoryExpansionAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteInventoryExpansion, token);
        public Task SendCharacterPositionAsync(byte position, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteCharacterPosition(position, s), token);
        // Wire = header + action (2 bytes); the writer's trailing placeholder is trimmed by
        // SendSequencedAsync like every other writer (Packet.h:3836-3840).
        public Task SendBattlePassActionAsync(byte action, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteBattlePassAction(action, s), token);
        public Task SendSkillColorAsync(byte slot, uint color1, uint color2, uint color3, uint color4, uint color5, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteSkillColor(slot, color1, color2, color3, color4, color5, s), token);
        public Task SendOpenShopAsync(int shopId, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteOpenShop(shopId, s), token);
        public Task SendTargetInfoLoadAsync(uint vid, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteTargetInfoLoad(vid, s), token);
        public Task SendWikiRequestAsync(ulong returnId, uint vnum, bool isMob, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteWikiRequest(returnId, vnum, isMob, s), token);
        public Task SendWarpRequestAsync(CancellationToken token) =>
            SendGameAsync(Metin2GamePacketCodec.WriteWarpRequest, token);
        public Task SendMapTeleporterAsync(int mapCode, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteMapTeleporter(mapCode, s), token);
        public Task SendChangeLanguageAsync(byte language, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteChangeLanguage(language, s), token);
        public Task SendRequestLanguageAsync(string targetName, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteRequestLanguage(targetName, s), token);
        public Task SendMountInventoryCheckinAsync(byte window, ushort cell, ushort mountPosition, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteMountInventoryCheckin(window, cell, mountPosition, s), token);
        public Task SendMountInventoryCheckoutAsync(ushort mountPosition, byte window, ushort cell, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteMountInventoryCheckout(mountPosition, window, cell, s), token);
        public Task SendMountInventoryItemMoveAsync(ushort mountPosition, ushort destination, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteMountInventoryItemMove(mountPosition, destination, s), token);
        public Task SendPetSetNameAsync(string name, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WritePetSetName(name, s), token);
        public Task SendSwitchbotStartAsync(byte slot, byte[] alternatives, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteSwitchbotStart(slot, alternatives, s), token);
        public Task SendSwitchbotStopAsync(byte slot, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteSwitchbotStop(slot, s), token);
        public Task SendDragonSoulRefineAllAsync(byte subheader, byte type, byte grade, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteDragonSoulRefineAll(subheader, type, grade, s), token);
        public Task SendAcceAsync(byte subheader, bool window, uint price, byte position, byte itemWindow, ushort itemCell,
            uint itemVnum, uint minAbs, uint maxAbs, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteAcce(subheader, window, price, position, itemWindow, itemCell, itemVnum, minAbs, maxAbs, s), token);
        public Task SendCubeRenewalAsync(byte subheader, uint indexItem, uint countItem, uint indexItemImprove, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteCubeRenewal(subheader, indexItem, countItem, indexItemImprove, s), token);
        public Task SendFishingNewAsync(byte subheader, uint vid, int direction, byte need, byte count, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteFishingNew(subheader, vid, direction, need, count, s), token);
        public Task SendWheelDestinyAsync(byte option, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteWheelDestiny(option, s), token);
        public Task SendWhisperAdminAsync(string text, string language, int color, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteWhisperAdmin(text, language, color, s), token);
        public Task SendClientVersion2Async(string filename, string timestamp, CancellationToken token) =>
            SendGameAsync(s => Metin2GamePacketCodec.WriteClientVersion2(filename, timestamp, s), token);

        public async Task<byte[]> ReceiveGamePacketAsync(CancellationToken token)
        {
            if (State != SessionState.Authenticated) throw new InvalidOperationException("Character list login has not completed");
            if (deferredPackets.Count > 0) return deferredPackets.Dequeue();
            return await ReceiveFramedPacketAsync(token).ConfigureAwait(false);
        }

        /// <summary>
        /// Takes the loading/game packets deliberately buffered by the phase waits. The caller is
        /// Unity's main thread, so map, actor and equipment state can be built before world reveal.
        /// </summary>
        public byte[][] DrainDeferredGamePackets()
        {
            byte[][] result = deferredPackets.ToArray();
            deferredPackets.Clear();
            return result;
        }

        private Task SendGameAsync(Func<byte, byte[]> build, CancellationToken token)
        {
            if (State != SessionState.Login && State != SessionState.Authenticated) throw new InvalidOperationException("Session is not connected to the game endpoint");
            return SendSequencedAsync(build, token);
        }

        /// <summary>Sends one of packet_info.cpp's sequence=false game packets without advancing
        /// the sequence table. ITEMSHOP_NEW is the only current gameplay caller.</summary>
        private async Task SendGameUnsequencedAsync(byte[] packet, CancellationToken token)
        {
            if (State != SessionState.Login && State != SessionState.Authenticated)
                throw new InvalidOperationException("Session is not connected to the game endpoint");
            int pending = Interlocked.Increment(ref diagnosticPendingSends);
            int gateStart = Environment.TickCount;
            if (pending >= 4) Metin2NetworkDiagnostics.Event(diagnosticSessionId, $"SEND_GATE_QUEUE depth={pending} kind=unsequenced");
            await sendGate.WaitAsync(token).ConfigureAwait(false);
            int gateWait = unchecked(Environment.TickCount - gateStart);
            if (gateWait >= 25) Metin2NetworkDiagnostics.Event(diagnosticSessionId, $"SEND_GATE_WAIT ms={gateWait} depth={pending} kind=unsequenced");
            try
            {
                Metin2NetworkDiagnostics.SendPacket(diagnosticSessionId, packet, false, -1, 0);
                byte[] wire = (byte[])packet.Clone();

                Metin2NetworkDiagnostics.TransportSendBegin(diagnosticSessionId, "SendGameUnsequencedAsync", packet[0], wire.Length, true, -1);
                int sendStart = Environment.TickCount;
                await transport.WriteAsync(wire, 0, wire.Length, token).ConfigureAwait(false);
                int sendElapsed = unchecked(Environment.TickCount - sendStart);
                Metin2NetworkDiagnostics.TransportSendOk(diagnosticSessionId, "SendGameUnsequencedAsync", packet[0], wire.Length, sendElapsed);
            }
            catch (Exception exception)
            {
                Metin2NetworkDiagnostics.Fault(diagnosticSessionId, "SendGameUnsequencedAsync", exception);
                throw;
            }
            finally
            {
                sendGate.Release();
                Interlocked.Decrement(ref diagnosticPendingSends);
            }
        }

        /// <summary>
        /// Builds and writes a sequenced packet while holding the send gate. The sequence byte is
        /// allocated inside the gate so the value the server reads always matches the arrival order;
        /// the keepalive pump and the gameplay code send from different threads.
        /// </summary>
        private async Task SendSequencedAsync(Func<byte, byte[]> build, CancellationToken token)
        {
            int pending = Interlocked.Increment(ref diagnosticPendingSends);
            int gateStart = Environment.TickCount;
            if (pending >= 4) Metin2NetworkDiagnostics.Event(diagnosticSessionId, $"SEND_GATE_QUEUE depth={pending} kind=sequenced");
            await sendGate.WaitAsync(token).ConfigureAwait(false);
            int gateWait = unchecked(Environment.TickCount - gateStart);
            if (gateWait >= 25) Metin2NetworkDiagnostics.Event(diagnosticSessionId, $"SEND_GATE_WAIT ms={gateWait} depth={pending} kind=sequenced");
            try
            {
                int sequenceSlot = sequenceIndex;
                byte sequence = Metin2ClientSequence.At(sequenceIndex++);
                byte[] packet = Metin2ProtocolCodec.TrimPacketSequence(build(sequence));
                Metin2NetworkDiagnostics.SendPacket(diagnosticSessionId, packet, true, sequenceSlot, sequence);
                byte[] wire = (byte[])packet.Clone();

                Metin2NetworkDiagnostics.TransportSendBegin(diagnosticSessionId, "SendSequencedAsync", packet[0], wire.Length, true, sequenceSlot);
                int sendStart = Environment.TickCount;
                await transport.WriteAsync(wire, 0, wire.Length, token).ConfigureAwait(false);
                int sendElapsed = unchecked(Environment.TickCount - sendStart);
                Metin2NetworkDiagnostics.TransportSendOk(diagnosticSessionId, "SendSequencedAsync", packet[0], wire.Length, sendElapsed);
            }
            catch (Exception exception)
            {
                Metin2NetworkDiagnostics.Fault(diagnosticSessionId, "SendSequencedAsync", exception);
                throw;
            }
            finally
            {
                sendGate.Release();
                Interlocked.Decrement(ref diagnosticPendingSends);
            }
        }

        // ---------------------------------------------------------------- auth endpoint

        public async Task<LoginResult> LoginAsync(string login, string password, uint[] clientKey, string language, CancellationToken cancellationToken)
        {
            if (State != SessionState.Auth) throw new InvalidOperationException("Session is not in auth phase");
            string hwid = Hwid ?? Metin2ProtocolCodec.ComputeHwid(Environment.MachineName);
            using (var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
            {
                deadline.CancelAfter(tlsOptions.AuthTimeoutMilliseconds);
                CancellationToken token = deadline.Token;
                try
                {
                    // LOGIN3 is 130 bytes with HWID; the writer's sequence placeholder is removed.
                    await SendSequencedAsync(sequence => Metin2ProtocolCodec.WriteSequencedLogin3(login, password, clientKey, hwid, sequence), token).ConfigureAwait(false);
                    byte header = (await ReadExactAsync(1, token).ConfigureAwait(false))[0];
                    if (header == 7)
                    {
                        byte[] tail = await ReadExactAsync(9, token).ConfigureAwait(false);
                        return new LoginResult(false, 0, Encoding.ASCII.GetString(tail).TrimEnd('\0'));
                    }
                    if (header != 150) throw new InvalidDataException("Unexpected LoginServer response " + header);
                    byte[] result = await ReadExactAsync(5, token).ConfigureAwait(false);
                    uint key = Metin2ProtocolCodec.ReadUInt32(result, 0);
                    if (result[4] > 1 || (result[4] == 1 && key == 0)) throw new InvalidDataException("Invalid auth success response.");
                    bool success = result[4] == 1;
                    if (success) State = SessionState.Authenticated;
                    return new LoginResult(success, key, success ? "OK" : "AUTH_FAILED");
                }
                catch (OperationCanceledException exception) when (!cancellationToken.IsCancellationRequested)
                {
                    State = SessionState.Failed; DisposeTransport();
                    throw new TimeoutException("LoginServer authentication timed out.", exception);
                }
                catch
                {
                    State = SessionState.Failed;
                    DisposeTransport();
                    throw;
                }
            }
        }

        private async Task<byte[]> ReadExactAsync(int length, CancellationToken token)
        {
            var data = new byte[length]; int offset = 0;
            while (offset < length)
            {
                Metin2NetworkDiagnostics.TransportReadBegin(diagnosticSessionId, "ReadExactAsync", length - offset);
                int recvStart = Environment.TickCount;
                int read = await transport.ReadAsync(data, offset, length - offset, token).ConfigureAwait(false);
                int recvElapsed = unchecked(Environment.TickCount - recvStart);
                Metin2NetworkDiagnostics.TransportReadOk(diagnosticSessionId, "ReadExactAsync", read, recvElapsed);
                if (read == 0)
                {
                    Metin2NetworkDiagnostics.Eof(diagnosticSessionId, "ReadExactAsync");
                    throw new EndOfStreamException("Metin2 server closed the connection");
                }
                Metin2NetworkDiagnostics.RawReceive(read);
                offset += read;
            }
            return data;
        }

        private async Task WriteAsync(byte[] data, CancellationToken token)
        {
            try
            {
                if (data != null && data.Length > 0)
                    Metin2NetworkDiagnostics.Event(diagnosticSessionId, $"RAW_CG header={data[0]} len={data.Length} TLS=1");
                byte[] wire = data;
                byte header = data != null && data.Length > 0 ? data[0] : (byte)0;
                Metin2NetworkDiagnostics.TransportSendBegin(diagnosticSessionId, "WriteAsync", header, wire?.Length ?? 0, true, -1);
                int sendStart = Environment.TickCount;
                await transport.WriteAsync(wire, 0, wire.Length, token).ConfigureAwait(false);
                int sendElapsed = unchecked(Environment.TickCount - sendStart);
                Metin2NetworkDiagnostics.TransportSendOk(diagnosticSessionId, "WriteAsync", header, wire.Length, sendElapsed);
            }
            catch (Exception exception)
            {
                Metin2NetworkDiagnostics.Fault(diagnosticSessionId, "WriteAsync", exception);
                throw;
            }
        }

        private static void WriteUInt16(byte[] data, int offset, ushort value) { data[offset] = (byte)value; data[offset + 1] = (byte)(value >> 8); }
        public byte NextSequence() => Metin2ClientSequence.At(sequenceIndex++);
        public uint ServerTimeMilliseconds => unchecked(synchronizedServerTime + (uint)(Environment.TickCount - synchronizedLocalTick));
        public void Dispose()
        {
            if (disposed) return;
            disposed = true;
            Metin2NetworkDiagnostics.DisposeCalled(diagnosticSessionId, State.ToString());
            if (diagnosticRole == "game") Metin2NetworkDiagnostics.Flush("GAME_SESSION_DISPOSE");
            DisposeTransport();
            State = SessionState.Disconnected;
            Metin2NetworkDiagnostics.State(diagnosticSessionId, State.ToString());
        }

        private void DisposeTransport()
        {
            Metin2NetworkDiagnostics.Event(diagnosticSessionId, "TRANSPORT_DISPOSE_BEGIN");
            try { pumpLifetime?.Cancel(); } catch { }
            pumpLifetime?.Dispose(); pumpLifetime = null; pumpTask = null;
            transport?.Dispose();
            Metin2NetworkDiagnostics.Event(diagnosticSessionId, "TRANSPORT_DISPOSE_END");
        }

        public readonly struct LoginResult
        {
            public readonly bool Success; public readonly uint LoginKey; public readonly string Status;
            public LoginResult(bool success, uint loginKey, string status) { Success = success; LoginKey = loginKey; Status = status; }
        }

        public readonly struct GameLoginResult
        {
            public readonly bool Success; public readonly string Status; public readonly byte Empire; public readonly Metin2ProtocolCodec.CharacterList CharacterList;
            public GameLoginResult(bool success, string status, byte empire, Metin2ProtocolCodec.CharacterList characterList)
            { Success = success; Status = status; Empire = empire; CharacterList = characterList; }
        }

        public readonly struct CharacterDeleteResult
        {
            public readonly bool Success; public readonly byte Index; public readonly bool WrongPrivateCode;
            public CharacterDeleteResult(bool success, byte index, bool wrongPrivateCode)
            { Success = success; Index = index; WrongPrivateCode = wrongPrivateCode; }
        }

        public readonly struct CharacterNameChangeResult
        {
            public readonly bool Success; public readonly byte Index; public readonly byte FailureType; public readonly Metin2ProtocolCodec.CharacterNameChange Change;
            public CharacterNameChangeResult(bool success, byte index, byte failureType, Metin2ProtocolCodec.CharacterNameChange change)
            { Success = success; Index = index; FailureType = failureType; Change = change; }
        }
    }
}
