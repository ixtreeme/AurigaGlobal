using System;
using UnityEngine;

namespace Metin2.Runtime.Network
{
    public sealed class Metin2PlayerController : MonoBehaviour
    {
#if UNITY_STANDALONE_WIN
        private void Awake()=>NotifyTailControllerStructure();
        private void OnEnable()=>NotifyTailControllerStructure();
        private void NotifyTailControllerStructure()=>GetComponent<Metin2ActorTextTailView>()?.MarkTopControllerDirty();
#endif
        public bool RaidImmobilized { get; private set; }
        public void SetRaidImmobilized(bool value)
        {
            if (RaidImmobilized == value) return;
            RaidImmobilized = value;
            if (value) { hasMoveDestination = false; externalInput = Vector2.zero; moving = false; motions?.StopIntoWait(mounted); }
        }
        public readonly struct MoveCommand
        {
            public readonly byte Function, Argument, Rotation; public readonly int X, Y; public readonly uint Time, Duration;
            public MoveCommand(byte function, byte argument, byte rotation, int x, int y, uint time, uint duration) { Function = function; Argument = argument; Rotation = rotation; X = x; Y = y; Time = time; Duration = duration; }
        }
        // Fallback only.  Normal physical locomotion resolves from the authored RUN/WALK motion,
        // matching CActorInstance::__AccumulationMovement.  These values remain for an incomplete
        // generated prefab whose motion definition is genuinely unavailable.
        [SerializeField] private float moveSpeed = 6f;
        [SerializeField] private float walkSpeed = 3f;
        private bool walkMode;
        private bool mounted;
        private Metin2NetworkEntity mountPresentation;
        private float movingSkillEndsAt;
#if UNITY_EDITOR || DEVELOPMENT_BUILD
        private float nextBridgeBlockReport;
#endif
        private ushort serverMoveSpeed = 100;
        private ushort serverAttackSpeed = 100;
        private float lastResolvedBaseMoveSpeed;
        private string lastMoveSpeedSource = "fallback";
        public bool IsWalkMode => walkMode;
        public bool IsMounted => mounted;
        Metin2MountFlight mountFlight;
        public Metin2MountFlight MountFlight=>mountFlight!=null?mountFlight:(mountFlight=GetComponent<Metin2MountFlight>());
        public bool IsFlying=>MountFlight!=null&&MountFlight.Flying;
        public GameObject ActiveMountInstance => mounted && mountPresentation != null ? mountPresentation.MountInstance : null;
        public ushort ServerMoveSpeed => serverMoveSpeed;
        public ushort ServerAttackSpeed => serverAttackSpeed;
        public float CurrentMoveSpeed
        {
            get
            {
                float baseSpeed = ResolveAuthoredLocomotionSpeed(out string source);
                lastResolvedBaseMoveSpeed = baseSpeed;
                lastMoveSpeedSource = source;
                return baseSpeed * serverMoveSpeed / 100f;
            }
        }
        public float LastResolvedBaseMoveSpeed => lastResolvedBaseMoveSpeed;
        public string LastMoveSpeedSource => lastMoveSpeedSource;
        public bool IsMovingSkillActive => mounted && Time.time < movingSkillEndsAt;

        private float ResolveAuthoredLocomotionSpeed(out string source)
        {
            // The server uses the mount race's GENERAL RUN/WALK accumulation when mounted.  Resolve
            // that definition directly from the generated race-data catalog so different mounts are
            // not all forced through the old 6 m/s player constant.
            if (mounted && mountPresentation != null && mountPresentation.MountVnum != 0)
            {
                // IMPORTANT: never load Metin2RaceDataCatalog here. That monolithic Resources asset
                // hard-references every race motion and AnimationClip. The first mounted movement
                // used to hit that Resources.Load synchronously from CurrentMoveSpeed, freezing the
                // render thread for several seconds exactly when the rider first pressed forward.
                //
                // Metin2RaceDataBinder.TryGetEntry is the runtime parity path used by actor spawning:
                // it resolves only the requested race through Metin2RaceDataIndex/Metin2RaceDataAsset
                // and caches that single asset. This restores the original client's per-race lazy load
                // granularity and keeps mounted locomotion speed sourced from the mount's own RUN/WALK.
                uint mountVnum = mountPresentation.MountVnum;
                if (Metin2RaceDataBinder.TryGetEntry(mountVnum, out Metin2RaceDataCatalog.Entry mount) &&
                    mount != null)
                {
                    Metin2MotionDefinition definition = mount.DefinitionFor(walkMode
                        ? Metin2RaceDataCatalog.MotionWalk
                        : Metin2RaceDataCatalog.MotionRun);
                    if (Metin2CharacterMotionSet.TryGetDefinitionTravelSpeed(definition, out float mountedSpeed))
                    {
                        source = "mount-motion:" + mountVnum;
                        // Broken/community mount clips sometimes contain no useful root travel, or
                        // only a fraction of an ordinary mount stride. A mount must not become
                        // slower than the unmounted player merely because that optional authoring
                        // channel is incomplete. Preserve every faster authored mount and floor the
                        // outliers at the measured lower edge of Auriga's valid mount family.
                        return Mathf.Max(mountedSpeed, walkMode ? 3.75f : 7.5f);
                    }

                    source = "mount-reference:" + mountVnum;
                    return walkMode ? 3.75f : 7.5f;
                }
            }

            if (motions != null && motions.TryGetLocomotionSpeed(walkMode, out float authoredSpeed))
            {
                source = "actor-motion:" + motions.CurrentMode + ":" + motions.LastLocomotionSpeedSource;
                return authoredSpeed;
            }

            if (!walkMode)
            {
                // Last-resort player RUN fallback.  This is not a tuned Unity speed: it is the
                // original shipped run.msa contract (300 cm over 0.666667 s) at MOV_SPEED 100.
                // TryGetLocomotionSpeed above normally improves this further with the active mode's
                // real clip duration, so weapon-mode runs retain their source-specific timing.
                source = "original-run-reference";
                return Metin2CharacterMotionSet.OriginalPlayerGeneralRunMetresPerSecond;
            }

            source = "fallback-walk";
            return walkSpeed;
        }

        /// <summary>
        /// The local player is a logical wrapper while the visual is reparented below the horse's
        /// saddle. Keep locomotion on the wrapper and forward only the presentation state to the
        /// horse actor, matching CActorInstance::__SetMotion's rider/horse pair.
        /// </summary>
        public void BindMountPresentation(Metin2NetworkEntity value)
        {
            if (mountPresentation != null)
                mountPresentation.MountPresentationChanged -= OnMountPresentationChanged;
            mountPresentation = value;
            if (mountPresentation != null)
                mountPresentation.MountPresentationChanged += OnMountPresentationChanged;
            mountPresentation?.ApplyActorSpeeds(serverMoveSpeed, serverAttackSpeed);
        }

        private void OnMountPresentationChanged(bool available)
            => SetMounted(available && mountPresentation != null && mountPresentation.MountVnum != 0);

        /// <summary>
        /// Applies the bMovingSpeed/bAttackSpeed pair from the local actor's character ADD/UPDATE.
        /// The native client divides each by 100 for actor playback and forwards both to the horse.
        /// </summary>
        public void ApplyServerActorSpeeds(ushort move, ushort attack)
        {
            serverMoveSpeed = move;
            serverAttackSpeed = attack;
            motions?.SetActorSpeeds(move, attack);
            mountPresentation?.ApplyActorSpeeds(move, attack);
        }

        /// <summary>
        /// CInstanceBase::NEW_UseSkill's moving-skill branch (InstanceBaseBattle.cpp:321-327).
        /// Skill 137 keeps m_isGoing true and calls StartWalking before the rider's ONCE motion is
        /// installed. The horse therefore gallops, while the rider's wildattack clip supplies the
        /// blows. This is intentionally separate from ordinary held input: a locked normal skill
        /// must still stop movement.
        /// </summary>
        public void BeginMovingSkill(float duration)
        {
            if (!mounted || duration <= 0f) return;
            movingSkillEndsAt = Time.time + duration;
            mountPresentation?.ApplyMountMovingSkill(true);
        }

        public void CancelMovingSkill()
        {
            movingSkillEndsAt = 0f;
            mountPresentation?.ApplyMountMovingSkill(false);
        }

        /// <summary>Deterministic source-parity hook; runtime calls the same step from ApplyInput.</summary>
        public void AdvanceMovingSkillForValidation(float seconds)
        {
            if (!mounted || seconds <= 0f) return;
            MoveWithWorld(AdvancingHeading * (CurrentMoveSpeed * seconds));
        }
        public void SetMounted(bool value)
        {
            // GC_CHARACTER_UPDATE repeats the current mount VNUM when only an affect flag changes.
            // Treating that unchanged value as a mount transition replayed wait/run and immediately
            // replaced the ONCE motion of every self-buff.  The original actor update path does not
            // refresh mount presentation for an unchanged mount state.
            if (mounted == value) return;
            if (!value && movingSkillEndsAt > 0f) CancelMovingSkill();
            mounted = value;
            if (motions != null)
            {
                if (moving) { if (walkMode) motions.PlayWalk(); else motions.PlayMove(mounted); }
                else motions.PlayIdle(mounted);
            }
            // Rider attack/combo owns the mount's animation through MotionStarted; locomotion
            // must not overwrite that state every frame just because auto-attack holds direction.
            if(motions==null || !motions.IsLocked ||
                (motions.CurrentMotionType!=Metin2CharacterMotionSet.MotionType.Attack &&
                 motions.CurrentMotionType!=Metin2CharacterMotionSet.MotionType.Combo))
                mountPresentation?.ApplyMountLocomotion(moving, walkMode);
        }

        /// <summary>CActorInstance::SetWalkMode/SetRunMode, driven only by GC_WALK_MODE.</summary>
        public void SetWalkMode(bool enabled)
        {
            if (walkMode == enabled) return;
            walkMode = enabled;
            if (motions != null && motions.IsMoveMotion)
            {
                if (walkMode) motions.PlayWalk(); else motions.PlayMove(mounted);
            }
            mountPresentation?.ApplyMountLocomotion(moving, walkMode);
        }
        /// <summary>
        /// Yaw correction between the mesh's own forward axis and Unity's convention.
        ///
        /// Quaternion.LookRotation aims the transform's +Z along the movement direction, but the
        /// converted Metin2 meshes face their local +X. Without the correction the character walks
        /// forward while looking right, back while looking left, and so on - a constant 90 degrees.
        /// </summary>
        [SerializeField] private float modelYawOffset = -90f;
        /// <summary>
        /// The fixup between the logical heading and the model's own forward axis. Effects anchored
        /// to the actor root inherit it, and the original's mc_pmatLocal for a detached effect is the
        /// plain world matrix, so whether this belongs in an effect frame is a measurable question -
        /// Metin2MotionEffectBridge reports it next to the frame it actually built.
        /// </summary>
        public float ModelYawOffset => modelYawOffset;
        [SerializeField] private Transform cameraTransform;
        private float facingDegrees;
        private bool moving; private uint nextMoveNotify, nextWaitingNotify; private Vector3 lastWaitingNotifyPosition; private Metin2CharacterMotionSet motions;
        private bool movementOriginKnown;
        private int movementOriginX, movementOriginY;
        private Metin2.Runtime.Metin2WorldRuntime world;
        public event Action<MoveCommand> MoveRequested;
        public Func<uint> TimeProvider { get; set; }
        public int SentCommandCount { get; private set; }
        /// <summary>Server-facing heading in classic five-degree units, as used by CG_FISHING.</summary>
        /// <summary>
        /// The true movement heading in degrees. transform.rotation carries modelYawOffset on top of
        /// it because the converted meshes face their local +X, so it cannot be used as a direction.
        /// </summary>
        /// <summary>
        /// Moves by a delta through the same world rules held input goes through: a blocked
        /// destination is refused and the height comes from the terrain.
        ///
        /// Motion-driven travel needs this as much as input does. Adding the delta straight to the
        /// transform - which is what the accumulation did - walked the character through hillsides
        /// and into blocked zones, because nothing consulted the map on that path.
        /// </summary>
        /// <summary>
        /// CInstanceBase::BlendRotation's default (InstanceBase.h:732) and CActorInstance's attack
        /// default (ActorInstance.h:368-369) are both 0.1 s. Turning is never instant in the
        /// original - it is interpolated from the current heading to the new one over this window.
        /// </summary>
        private const float RotationBlendSeconds = 0.1f;

        private float rotationBegin, rotationEnd, rotationBeginTime, rotationBlend;
        private float advancingDegrees;

        /// <summary>True while a turn is still being interpolated.</summary>
        public bool IsTurning => rotationBlend > 0f && Time.time < rotationBeginTime + rotationBlend;

        /// <summary>
        /// CActorInstance::BlendRotation (ActorInstanceRotation.cpp:26-40): remembers where the turn
        /// started, where it is going and when, and lets RotationProcess walk between them.
        /// </summary>
        private void BlendRotation(float degrees, float blendSeconds = RotationBlendSeconds)
        {
            if (rotationBlend > 0f && Mathf.Abs(Mathf.DeltaAngle(rotationEnd, degrees)) < 0.01f) return;
            rotationBegin = facingDegrees;
            rotationEnd = degrees;
            rotationBeginTime = Time.time;
            rotationBlend = blendSeconds;
        }

        /// <summary>
        /// CActorInstance::RotationProcess (ActorInstanceRotation.cpp:50-73). Runs every frame
        /// whatever motion is playing, which is why the character can be turned through a swing.
        /// </summary>
        private void RotationProcess()
        {
            if (rotationBlend <= 0f) return;
            float progress = (Time.time - rotationBeginTime) / rotationBlend;
            facingDegrees = progress < 1f
                ? Mathf.LerpAngle(rotationBegin, rotationEnd, progress)
                : rotationEnd;
            // ActorInstanceRotation.cpp:55-59 updates m_fAdvancingRotation only while the body is
            // inside the interpolation window. NEW_UseSkill has already primed it with the target
            // rotation before the motion starts; subsequent frames then follow the visible turn.
            if (progress < 1f) advancingDegrees = facingDegrees;
            transform.rotation = Quaternion.Euler(0f, facingDegrees + modelYawOffset, 0f);
            if (progress >= 1f) rotationBlend = 0f;
        }

        /// <summary>
        /// CActorInstance::LookAt (ActorInstanceRotation.cpp:86-89) - turn to face a world position.
        ///
        /// The blend is 0.3 s here, not the 0.1 s a movement or attack turn uses: LookAt passes its
        /// own constant, and this is the path a skill takes to face its target
        /// (CPythonPlayer::__UseSkill -> NEW_LookAtDestInstance -> LookAt -> BlendRotation(rot, 0.3f)).
        /// </summary>
        public void LookAt(Vector3 worldPosition)
        {
            Vector3 direction = worldPosition - transform.position;
            direction.y = 0f;
            if (direction.sqrMagnitude < 0.0001f) return;
            BlendRotation(Quaternion.LookRotation(direction, Vector3.up).eulerAngles.y, 0.3f);
        }

        /// <summary>EmotionProcess sets both current and advancing rotation immediately after the
        /// actors have reached their paired positions.</summary>
        public void FaceImmediately(Vector3 worldPosition)
        {
            Vector3 direction = worldPosition - transform.position;
            direction.y = 0f;
            if (direction.sqrMagnitude < 0.0001f) return;
            facingDegrees = Quaternion.LookRotation(direction, Vector3.up).eulerAngles.y;
            advancingDegrees = facingDegrees;
            rotationBlend = 0f;
            transform.rotation = Quaternion.Euler(0f, facingDegrees + modelYawOffset, 0f);
        }

        [SerializeField] private Metin2NetworkEntityWorld entityWorld;
        /// <summary>The local player's vid, so the blocking test skips the player itself.</summary>
        public uint LocalVid { get; set; }
        public int AuthoritativeReviveCount { get; private set; }
        public void BindEntityWorld(Metin2NetworkEntityWorld value)
        {
            if (entityWorld != null) entityWorld.LocalLifeStateChanged -= OnLocalLifeStateChanged;
            entityWorld = value;
            if (entityWorld != null) entityWorld.LocalLifeStateChanged += OnLocalLifeStateChanged;
        }

        private void OnDestroy()
        {
#if UNITY_STANDALONE_WIN
            NotifyTailControllerStructure();
#endif
            if (entityWorld != null) entityWorld.LocalLifeStateChanged -= OnLocalLifeStateChanged;
            if (mountPresentation != null)
                mountPresentation.MountPresentationChanged -= OnMountPresentationChanged;
        }

        private void OnLocalLifeStateChanged(bool dead)
        {
            if (dead) return;

            // CActorInstance::Revive clears the real-dead/walk/stun state immediately.  On the
            // restart path the native actor is then refreshed in WAIT while being recreated.  Our
            // local actor persists across that packet edge, so explicitly replace the held DEAD
            // pose here; waiting for moving != wantsMove made the first key press appear to revive it.
            moving = false;
            hasMoveDestination = false;
            externalInput = Vector2.zero;
            syncBlend = 0f;
            CancelMovingSkill();
            if (motions == null) motions = GetComponentInChildren<Metin2CharacterMotionSet>(true);
            motions?.ClearLock();
            motions?.PlayIdle(mounted);
            mountPresentation?.ApplyMountLocomotion(false, walkMode);
            AuthoritativeReviveCount++;
            Metin2NetworkDiagnostics.Event(unchecked((int)LocalVid),
                $"LOCAL_REVIVE_PRESENTATION wait=1 mounted={(mounted ? 1 : 0)}");
        }

        /// <summary>
        /// CActorInstance::__Push (ActorInstanceSync.cpp:7-51), reached from GC_SYNC_POSITION through
        /// SyncActor -> NEW_SyncPixelPosition -> TEMP_Push.
        ///
        /// This is how the original stops you walking through a monster or an NPC. The client
        /// deliberately does NOT block against them
        /// (CPythonCharacterManager::AdjustCollisionWithOtherObjects skips IsPC, IsNPC and IsEnemy):
        /// the server refuses the move and sends the position back, and the client eases the actor
        /// there. Ignoring the packet for the local player - which is what the port did - is exactly
        /// why they could be walked through.
        ///
        /// Two details from the original that matter. The correction is BLENDED over
        /// SetBlendingPosition's default 1.0 s rather than snapped, so it reads as being stopped
        /// rather than teleported. And the path is walked first in a hundred steps: if any of them
        /// collides, the whole correction is dropped rather than dragging the actor through a wall.
        /// </summary>
        public void SyncPosition(Vector3 target)
        {
            Vector3 from = transform.position;
            Vector3 to = new Vector3(target.x, from.y, target.z);
            Vector3 delta = to - from;
            if (delta.sqrMagnitude < 0.0001f) { syncBlend = 0f; return; }

            const int steps = 100;
            for (int step = 1; step <= steps; step++)
            {
                Vector3 sample = from + delta * (step / (float)steps);
                if (world != null && world.IsBlocked(sample)) { syncBlend = 0f; return; }
            }

            syncFrom = from;
            syncTo = to;
            syncBeganAt = Time.time;
            syncBlend = SyncBlendSeconds;
        }

        /// <summary>
        /// Applies the authoritative position carried by GC_CHARACTER_ADD for the main actor.
        ///
        /// CNetworkActorManager::AppendActor does NOT ignore the main actor: it deletes/recreates
        /// the visible main instance from SCreateData.m_lPosX/m_lPosY (except the explicit mount
        /// transition override).  The Unity port keeps a separate locally-driven object, so dropping
        /// this position let the local prediction continue after the server rejected a too-far MOVE.
        /// That caused every following MOVE to trip the server's 25 m gate and Show/ViewReencode the
        /// whole view again.
        ///
        /// A CHARACTER_ADD reinsert is stronger than GC_SYNC_POSITION's one-second blend; mirror the
        /// recreated native instance by placing the logical root immediately and ending the old move.
        /// Held keyboard input may start a fresh move on the following frame.
        /// </summary>
        public void ApplyAuthoritativeCharacterAdd(Vector3 groundPosition)
        {
            Vector3 before = transform.position;
            syncBlend = 0f;
            hasMoveDestination = false;
            externalInput = Vector2.zero;
            moving = false;
            if (world != null && world.HasMap) groundPosition.y = world.GroundHeight(groundPosition);
            PlaceOnGround(groundPosition);
            lastWaitingNotifyPosition = transform.position;
            nextWaitingNotify = 0;
            motions?.StopIntoWait(mounted);
            mountPresentation?.ApplyMountLocomotion(false, walkMode);
            Debug.Log($"METIN2_LOCAL_ADD_CORRECTION metres={Vector3.Distance(before, transform.position):F2} " +
                      $"from=({before.x:F2},{before.z:F2}) to=({transform.position.x:F2},{transform.position.z:F2})");
        }

        /// <summary>SetBlendingPosition's default (ActorInstance.h:364).</summary>
        private const float SyncBlendSeconds = 1f;
        private Vector3 syncFrom, syncTo;
        private float syncBeganAt, syncBlend;

        /// <summary>Eases an accepted correction into place; held input cancels it, as moving again does.</summary>
        private void SyncProcess()
        {
            if (syncBlend <= 0f) return;
            float progress = (Time.time - syncBeganAt) / syncBlend;
            Vector3 eased = Vector3.Lerp(syncFrom, syncTo, Mathf.Clamp01(progress));
            if (world != null)
            {
                world.StreamForLocalPlayer(eased);
                eased.y = world.GroundHeight(eased) - footOffset;
            }
            else eased.y = transform.position.y;
            transform.position = eased;
            if (progress >= 1f) syncBlend = 0f;
        }

        /// <summary>
        /// CheckAdvancing for this actor: refuses a step into another character, or slides along it.
        /// A skill in progress narrows the test to doors, which is the original's own exception.
        /// </summary>
        private bool TryAdvance(Vector3 from, Vector3 to, out Vector3 adjusted)
        {
            adjusted = to;
            if (entityWorld == null) return true;
            // IsUsingSkill, not "any locked motion". IsLocked is also true through a normal swing,
            // and using it here narrowed the test to doors during every attack - which is why mobs
            // could be walked through again while hitting them. The native's door exception applies
            // to skills only (InstanceBaseBattle.cpp:521).
            // Both halves are needed. CurrentMotionSkill names the skill but is only cleared by the
            // next ordinary swing, so on its own it stayed set after a cast and made everything
            // walkable. IsLocked says a motion is still running but is equally true through a normal
            // attack. IsUsingSkill is the conjunction: a skill was cast AND its motion is still
            // playing.
            bool usingSkill = combat != null && combat.CurrentMotionSkill != 0
                              && motions != null && motions.IsLocked;
            // Ordinary NEW_MoveToDirection has already entered a TYPE_MOVE motion by the time
            // CheckAdvancing runs, so native TestActorCollision uses BODY spheres here.
            return entityWorld.TryAdvance(from, to, LocalVid, CollisionProfile,
                Metin2.Runtime.Metin2CollisionProfile.CollisionTypeBody, usingSkill, out adjusted);
        }

        public Metin2.Runtime.Metin2CollisionProfile CollisionProfile
        {
            get
            {
                if (collisionProfile == null)
                    collisionProfile = GetComponentInChildren<Metin2.Runtime.Metin2CollisionProfile>(true);
                return collisionProfile;
            }
        }
        private Metin2.Runtime.Metin2CollisionProfile collisionProfile;

        /// <summary>
        /// The player's original MSM COLLISION_TYPE_BODY footprint.
        /// Renderer bounds are not a movement collision source: equipped weapons, armour and an
        /// outstretched animation can make them several times wider than the authored body sphere.
        /// </summary>
        private float BodyRadius
        {
            get
            {
                if (bodyRadiusResolved) return bodyRadius;
                bodyRadiusResolved = true;
                Metin2.Runtime.Metin2CollisionProfile profile =
                    GetComponentInChildren<Metin2.Runtime.Metin2CollisionProfile>(true);
                if (profile != null && profile.TryGetPlanarRadius(
                        Metin2.Runtime.Metin2CollisionProfile.CollisionTypeBody, transform.position, out float authored))
                    bodyRadius = authored;
                return bodyRadius;
            }
        }
        private float bodyRadius;
        private bool bodyRadiusResolved;

        [SerializeField] private Metin2CombatController combat;
        public void BindCombat(Metin2CombatController value) => combat = value;

        public void MoveWithWorld(Vector3 delta)
        {
            // A caravan defender may animate attacks, but root travel must not
            // invoke terrain grounding and pull the actor through the deck.
            var caravan=GetComponent<Metin2CaravanRiderAttachment>();if(caravan!=null&&caravan.LocksTranslation)return;
            Vector3 from = transform.position;
            Vector3 candidate = from + delta;
            world?.StreamForLocalPlayer(candidate);
            if (world != null && world.IsBlocked(candidate)) return;

            bool usingSkill = combat != null && combat.CurrentMotionSkill != 0 && motions != null && motions.IsLocked;
            int collisionType = motions != null &&
                (motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Wait ||
                 motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Attack ||
                 motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Combo ||
                 motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Skill)
                ? Metin2.Runtime.Metin2CollisionProfile.CollisionTypeDefending
                : Metin2.Runtime.Metin2CollisionProfile.CollisionTypeBody;
            if (entityWorld != null && !entityWorld.TryAdvance(from, candidate, LocalVid, CollisionProfile,
                    collisionType, usingSkill, out candidate)) return;
            if (world != null && !IsFlying) candidate.y = world.GroundHeight(candidate) - footOffset;
            transform.position = candidate;
        }

        /// <summary>
        /// CActorInstance::ProcessMotionEventWarp.  A warp moves the logical actor, never the
        /// animated mesh child; otherwise the camera/world continue following the old root while
        /// only the Ninja model appears at the target.  Background blocking and ground placement
        /// are applied before committing the teleport.
        /// </summary>
        public bool ApplySkillWarp(Vector3 destination, Vector3 lookAt)
        {
            Vector3 candidate = new Vector3(destination.x, transform.position.y, destination.z);
            bool accepted = world == null || !world.IsBlocked(candidate);
            if (accepted)
            {
                world?.StreamForLocalPlayer(candidate);
                if (world != null && !IsFlying) candidate.y = world.GroundHeight(candidate) - footOffset;
                transform.position = candidate;
                hasMoveDestination = false;
                externalInput = Vector2.zero;
                syncBlend = 0f;
            }

            LookAt(lookAt);
            // __OnWarp -> CPythonPlayerEventHandler::OnWarp sends FUNC_WAIT from the post-warp
            // logical position/rotation even when IsBlock refused the positional step.  This keeps
            // the server and remote clients on the same root that the local camera follows.
            EmitAction(0, 0);
            return accepted;
        }

        public Vector3 Heading
        {
            get
            {
                // Taken from the live rotation, not from facingDegrees. That field only moves while
                // movement input is held, so a swing thrown while standing - or after turning -
                // stepped off towards wherever the character last walked. The native reads the
                // actor's current rotation for the same purpose (NEW_Attack calls GetRotation()).
                Vector3 forward = transform.rotation * Quaternion.Euler(0f, -modelYawOffset, 0f) * Vector3.forward;
                forward.y = 0f;
                return forward.sqrMagnitude > 0.0001f ? forward.normalized : Vector3.forward;
            }
        }

        /// <summary>
        /// CActorInstance::GetAdvancingRotation expressed as a Unity heading. Motion accumulation
        /// must use this value; the visible transform includes the converted model yaw offset and
        /// the current body heading is not necessarily the direction primed by NEW_UseSkill.
        /// </summary>
        public Vector3 AdvancingHeading => Quaternion.Euler(0f, advancingDegrees, 0f) * Vector3.forward;

        /// <summary>
        /// InstanceBaseBattle.cpp:336-339: immediately before InterceptOnceMotion, NEW_UseSkill
        /// copies GetTargetRotation() into m_fAdvancingRotation. This is deliberately separate from
        /// LookAt's 0.3 s visual blend.
        /// </summary>
        public void CaptureSkillAdvancingRotation()
        {
            advancingDegrees = rotationBlend > 0f ? rotationEnd : facingDegrees;
        }

        /// <summary>
        /// Native CG_MOVE.bRot (five-degree units). The packet/world basis is reflected relative to
        /// Unity, so this is the inverse of Metin2CoordinateSpace.ActorYaw, not Unity yaw / 5.
        /// Moving remote actors face their travel vector and hid the old error; WAIT exposes bRot.
        /// </summary>
        public byte FacingDirection => (byte)Mathf.FloorToInt(
            Metin2CoordinateSpace.SourceActorYaw(facingDegrees) / 5f);

        private float footOffset;

        /// <summary>
        /// How far the actor was sunk so its mesh soles meet the ground. Measured once at spawn -
        /// re-measuring later gives a different answer because equipment changes which renderer the
        /// heuristic picks (0.14 with armour against 0.99 at spawn, measured) - so anything that has
        /// to undo the sink, such as bone-anchored effects, must read this stored value.
        /// </summary>
        public float FootOffset => footOffset;
        /// <summary>The native actor pixel position; the Unity root is sunk only to ground the mesh.</summary>
        public Vector3 LogicalGroundPosition => transform.position + Vector3.up * footOffset;

        public void Initialize(Vector3 position, Transform cameraTarget = null)
        {
            cameraTransform = cameraTarget; moving = false; nextMoveNotify = 0;
            advancingDegrees = facingDegrees;
            motions = GetComponentInChildren<Metin2CharacterMotionSet>(true);
            motions?.SetActorSpeeds(serverMoveSpeed, serverAttackSpeed);
            motions?.PlayIdle(false);
            // Hands a motion's own baked travel to the transform - see the note on the component.
            if (GetComponent<Metin2.Runtime.Metin2MotionRootTravel>() == null)
                gameObject.AddComponent<Metin2.Runtime.Metin2MotionRootTravel>();
            // Measured before the first placement: the converted meshes sit about 0.95 m above their
            // transform origin, so putting the origin on the terrain would leave the character floating.
            transform.position = position;
            footOffset = Metin2.Runtime.Metin2Units.MeasureFootOffset(gameObject);
            // Sink offset diagnostic: it has to stay near the body's ~1.0 m no matter which
            // hair/armour/weapon was just equipped, or the character ends up under the terrain.
            Debug.Log($"METIN2_FOOT_OFFSET value={footOffset:F3}");
            PlaceOnGround(position);
            lastWaitingNotifyPosition = transform.position;
            nextWaitingNotify = 0;
        }

        /// <summary>Places the character so its soles rest on the given ground position.</summary>
        public void PlaceOnGround(Vector3 groundPosition)
        {
            groundPosition.y -= footOffset;
            transform.position = groundPosition;
            ResetMovementOrigin();
        }
        public void BindWorld(Metin2.Runtime.Metin2WorldRuntime value) { world = value; movementOriginKnown = false; }
        private Vector2 externalInput;
        private bool hasMoveDestination;
        private Vector3 moveDestination;

        /// <summary>CPythonPlayer constructor: SetMovableGroundDistance(40.0f), converted to metres.</summary>
        public const float MovableGroundDistance = 0.4f;
        public bool HasMoveDestination => hasMoveDestination;
        public Vector3 MoveDestination => moveDestination;

        /// <summary>
        /// CInstanceBase::NEW_MoveToDestPixelPositionDirection. The target survives mouse release and
        /// is consumed only when reached, blocked or superseded by direct keyboard/mouse steering.
        /// </summary>
        public bool MoveToDestination(Vector3 groundPosition)
        {
            Vector3 flat = groundPosition - transform.position;
            flat.y = 0f;
            if (flat.magnitude < MovableGroundDistance) return false;
            if (world != null)
            {
                groundPosition.y = world.GroundHeight(groundPosition) - footOffset;
                if (world.IsBlocked(groundPosition)) return false;
            }
            moveDestination = groundPosition;
            hasMoveDestination = true;
            externalInput = Vector2.zero;
            return true;
        }

        public void CancelMoveDestination()
        {
            hasMoveDestination = false;
            externalInput = Vector2.zero;
        }

        public void StopBeforeWoodcutting()
        {
            CancelMoveDestination();
            if(motions==null)motions=GetComponentInChildren<Metin2CharacterMotionSet>(true);
            if(motions!=null && motions.IsMoveMotion)motions.PlayIdle(mounted);
            // Publish the final predicted position BEFORE the NPC-click request.
            // Otherwise the next WAIT moves the server actor after chopping began.
            EmitAction(0, 0);
            moving = false;
            lastWaitingNotifyPosition = transform.position;
        }

        /// <summary>
        /// Movement requested by the mouse. game.py drives the keyboard directly while the mouse
        /// walk mode feeds a screen-space direction, so both sources are combined here.
        /// </summary>
        public void ApplyExternalInput(Vector2 direction)
        {
            externalInput = direction;
            if (direction.sqrMagnitude > .0001f) hasMoveDestination = false;
        }

        private Vector2 DestinationInput()
        {
            Vector3 direction = moveDestination - transform.position;
            direction.y = 0f;
            if (direction.sqrMagnitude < .0004f) { hasMoveDestination = false; return Vector2.zero; }
            direction.Normalize();
            Vector3 forward = cameraTransform != null
                ? Vector3.ProjectOnPlane(cameraTransform.forward, Vector3.up).normalized : Vector3.forward;
            Vector3 right = cameraTransform != null
                ? Vector3.ProjectOnPlane(cameraTransform.right, Vector3.up).normalized : Vector3.right;
            return new Vector2(Vector3.Dot(direction, right), Vector3.Dot(direction, forward));
        }

        /// <summary>Deterministic destination tick used by the parity gate without synthesising OS input.</summary>
        public void AdvanceMoveDestination(float deltaTime, uint serverTime)
        {
            ApplyInput(hasMoveDestination ? DestinationInput() : Vector2.zero, deltaTime, serverTime);
        }

        private void Update()
        {
            long sample = Metin2.Runtime.Metin2FrameProfiler.Begin();
            UpdatePlayer();
            Metin2.Runtime.Metin2FrameProfiler.End(Metin2.Runtime.Metin2FrameProfiler.SlotPlayer, sample);
        }

        private void UpdatePlayer()
        {
            // game.py __BuildKeyDict binds both WASD and the arrow keys to the same move handlers.
            // A focused chat line takes the keyboard before any of that: the window manager routes
            // keys to the CEditLine, so the move handlers never fire while typing. The mouse walk
            // (externalInput) is unaffected and keeps working, exactly as in the client.
            bool typing = Metin2KeyboardFocus.IsTextEntryActive;
            // constInfo.SET_ITEM_QUESTION_DIALOG_STATUS: while a quest dialog is open the
            // character cannot move (PythonPlayerInput.cpp __CanMove → CInstanceBase::CanMove).
            bool dialogLocked = Metin2QuestDialogPanel.IsDialogLocked;
            float horizontal = typing || dialogLocked ? 0f
                : (Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow) ? 1f : 0f)
                - (Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow) ? 1f : 0f);
            float vertical = typing || dialogLocked ? 0f
                : (Input.GetKey(KeyCode.W) || Input.GetKey(KeyCode.UpArrow) ? 1f : 0f)
                - (Input.GetKey(KeyCode.S) || Input.GetKey(KeyCode.DownArrow) ? 1f : 0f);
            Vector2 input = new Vector2(horizontal, vertical);
            if (input.sqrMagnitude > .0001f) hasMoveDestination = false;
            else if (externalInput.sqrMagnitude > .0001f) input = externalInput;
            else if (hasMoveDestination) input = DestinationInput();
            ApplyInput(input, Time.deltaTime, TimeProvider != null ? TimeProvider() : unchecked((uint)Environment.TickCount));
        }
        public void ApplyInput(Vector2 input, float deltaTime, uint serverTime)
        {
            // Streaming follows the local actor only. GroundHeight is intentionally a pure query,
            // because remote entities, camera probes and quest markers ask it about other points.
            world?.StreamForLocalPlayer(transform.position);
            // Advanced first and unconditionally, as CInstanceBase::Update runs RotationProcess
            // before anything else can return early. A turn asked for on the frame the swing started
            // must keep interpolating while the actor is locked.
            RotationProcess();
            SyncProcess();
            if (!movementOriginKnown) ResetMovementOrigin();
            // CActorInstance::CanAct (ActorInstanceBattle.cpp:18-35): IsDead() blocks every input -
            // movement, attack, skills. The dead motion is a hold (CurrentMotionProcess returns on
            // IsDead, ActorInstanceMotion.cpp:159-160), so the body stays down until the server
            // revives the character and a CHARACTER_UPDATE without the dead bit clears the gate.
            bool dead = entityWorld != null && (entityWorld.LocalStateFlags & 1) != 0;
            if (dead)
            {
                MountFlight?.Land();
                if (moving) { Emit(0, 0, serverTime); moving = false; }
                hasMoveDestination = false; externalInput = Vector2.zero;
                if (motions != null && motions.CurrentMotionType != Metin2CharacterMotionSet.MotionType.Die && !motions.PlayDead())
                    motions.PlayIdle(false);
                return;
            }
            if(MountFlight!=null)
            {
                bool allow=!RaidImmobilized&&!Metin2KeyboardFocus.IsTextEntryActive&&!Metin2QuestDialogPanel.IsDialogLocked;
                int axis=allow?((Input.GetKey(KeyCode.LeftShift)||Input.GetKey(KeyCode.RightShift)?1:0)-(Input.GetKey(KeyCode.LeftControl)||Input.GetKey(KeyCode.RightControl)?1:0)):0;
                MountFlight.Tick(axis,deltaTime,serverTime);
            }
            if (RaidImmobilized) { hasMoveDestination = false; externalInput = Vector2.zero; input = Vector2.zero; }
            input = Vector2.ClampMagnitude(input, 1f); bool wantsMove = input.sqrMagnitude > .0001f;

            // ActEmotion / ActDualEmotion disables ordinary movement state changes until the ONCE
            // motion is finished. Without this, held input restarted RUN on the very next Update
            // and remote clients saw only the first frame of the emotion.
            if (wantsMove && motions != null)
            {
                GetComponent<Metin2WoodcuttingMotion>()?.CancelForMovement(motions);
                GetComponent<Metin2HerbalismMotion>()?.Stop();
            }
            if (motions != null && motions.IsEventMotion)
            {
                if (moving) { Emit(0, 0, serverTime); moving = false; }
                hasMoveDestination = false;
                return;
            }

            // A swing stops the walk. CInstanceBase::NEW_SetAttackKeyState
            // (InstanceBaseBattle.cpp:369-372) does exactly this before starting the motion:
            //
            //     if (IsWalking()) EndWalking();
            //     m_isGoing = FALSE;
            //
            // The forward travel during an attack is not held input - it is baked into the clip and
            // handed to the transform by Metin2MotionRootTravel. Letting input keep pushing at run
            // speed on top of it is what made the character slide instead of step.
            //
            // Deliberately NOT calling PlayIdle here: that would restart the clip and cancel the
            // swing. The motion is left alone and only the movement is suppressed; when the lock
            // expires the block below sees moving != wantsMove and resumes the run by itself.
            if (motions != null && motions.IsLocked)
            {
                if (moving) { Emit(0, 0, serverTime); moving = false; }

                // Walking ends but STEERING does not. The two are separate in the original and I had
                // conflated them: NEW_Attack calls EndWalking() and SetAdvancingRotation(fDirRot)
                // (InstanceBaseBattle.cpp:369-372, ActorInstanceBattle.cpp:479-481), and what that
                // second call fixes is the direction the ACCUMULATION travels - m_fAdvancingRotation,
                // read by __AccumulationMovement. The actor's own facing keeps being blended, because
                // RotationProcess() runs every frame from CInstanceBase::Update (:1879) whatever
                // motion is playing.
                //
                // So the swing steps along the heading it started on while the body can still be
                // turned, which is what the original does and what removing this broke.
                if (wantsMove)
                {
                    Vector3 turnForward = cameraTransform != null
                        ? Vector3.ProjectOnPlane(cameraTransform.forward, Vector3.up).normalized : Vector3.forward;
                    Vector3 turnRight = cameraTransform != null
                        ? Vector3.ProjectOnPlane(cameraTransform.right, Vector3.up).normalized : Vector3.right;
                    Vector3 turnDirection = (turnRight * input.x + turnForward * input.y).normalized;
                    if (turnDirection.sqrMagnitude > 0.0001f)
                        BlendRotation(Quaternion.LookRotation(turnDirection, Vector3.up).eulerAngles.y);
                }

                // CPythonSkill marks only VNUM 137 as MOVING_SKILL. NEW_UseSkill starts walking and
                // leaves m_isGoing TRUE for the entire locked wildattack, while direction input is
                // still allowed to change m_fAdvancingRotation (InstanceBaseMovement.cpp:298-301).
                // Advance the logical wrapper so its child horse and saddle-attached rider travel
                // as one actor. The ordinary lock path above remains stationary for every other
                // attack/skill.
                if (IsMovingSkillActive)
                    AdvanceMovingSkillForValidation(deltaTime);
                else if (movingSkillEndsAt > 0f)
                    CancelMovingSkill();
                return;
            }
            if (movingSkillEndsAt > 0f) CancelMovingSkill();
            if (wantsMove)
            {
                Vector3 forward = cameraTransform != null ? Vector3.ProjectOnPlane(cameraTransform.forward, Vector3.up).normalized : Vector3.forward;
                Vector3 right = cameraTransform != null ? Vector3.ProjectOnPlane(cameraTransform.right, Vector3.up).normalized : Vector3.right;
                Vector3 direction = (right * input.x + forward * input.y).normalized;
                float step = CurrentMoveSpeed * deltaTime;
                if (hasMoveDestination)
                {
                    Vector3 remaining = moveDestination - transform.position; remaining.y = 0f;
                    if (remaining.sqrMagnitude <= .0004f) { hasMoveDestination = false; step = 0f; }
                    else { direction = remaining.normalized; step = Mathf.Min(step, remaining.magnitude); }
                }
                Vector3 candidate = transform.position + direction * step;
                world?.StreamForLocalPlayer(candidate);
#if UNITY_EDITOR || DEVELOPMENT_BUILD
                string blockReason = null;
                bool mapBlocked = IsFlying ? !MountFlight.CanAdvance(candidate) : world != null && world.IsBlocked(candidate, out blockReason);
#else
                bool mapBlocked = IsFlying ? !MountFlight.CanAdvance(candidate) : world != null && world.IsBlocked(candidate);
#endif
                bool free = step > 0f && !mapBlocked &&
                            TryAdvance(transform.position, candidate, out candidate);
#if UNITY_EDITOR || DEVELOPMENT_BUILD
                Vector3 acceptedTravel = candidate - transform.position;
                acceptedTravel.y = 0f;
                if ((!free || acceptedTravel.sqrMagnitude < .000001f) && step > .001f &&
                    world != null && world.CurrentMapName == "natural_map" &&
                    Time.unscaledTime >= nextBridgeBlockReport)
                {
                    nextBridgeBlockReport = Time.unscaledTime + 2f;
                    Debug.LogWarning($"METIN2_NATURAL_MOVE_BLOCK from={transform.position:F4} to={candidate:F4} " +
                        $"ground={world.GroundHeight(candidate):F4} footOffset={footOffset:F4} " +
                        $"reason={(mapBlocked ? blockReason : "actor-collision")} " +
                        $"actors={entityWorld?.DescribeMovementObstacles(transform.position, candidate, LocalVid, CollisionProfile)}");
                }
#endif
                if (free)
                {
                    if (world != null && !IsFlying) candidate.y = world.GroundHeight(candidate) - footOffset;
                    transform.position = candidate;
                    if (hasMoveDestination)
                    {
                        Vector3 remaining = moveDestination - transform.position; remaining.y = 0f;
                        if (remaining.sqrMagnitude <= .0004f) hasMoveDestination = false;
                    }
                }
                else if (hasMoveDestination) hasMoveDestination = false;
                // The facing sent to the server is the true movement heading; the mesh offset is
                // purely visual and must not leak into the protocol.
                BlendRotation(Quaternion.LookRotation(direction, Vector3.up).eulerAngles.y);
                if (!moving || IsDue(serverTime, nextMoveNotify)) { Emit(1, 0, serverTime); nextMoveNotify = serverTime + 300; }
            }
            else if (moving)
            {
                Emit(0, 0, serverTime);
                lastWaitingNotifyPosition = transform.position;
                nextWaitingNotify = serverTime + 100;
            }
            else if (IsDue(serverTime, nextWaitingNotify))
            {
                // CPythonPlayerEventHandler::OnWaiting: motion events, sync pushes and other
                // non-keyboard displacement are still reported every 100 ms once they exceed one
                // original pixel. Without this the Unity actor can continue locally while the server
                // stays at the last key-move coordinate until its 25 m anti-teleport gate trips.
                Vector3 delta = transform.position - lastWaitingNotifyPosition; delta.y = 0f;
                if (delta.sqrMagnitude >= 0.0001f)
                {
                    Emit(0, 0, serverTime);
                    lastWaitingNotifyPosition = transform.position;
                }
                nextWaitingNotify = serverTime + 100;
            }
            if (moving != wantsMove)
            {
                // Play restarts the clip from zero, so every flip is a visible jolt in the legs. A
                // run that is held down should produce exactly one of these; a burst of them means
                // the input is dropping out for a frame.
                Metin2MotionDiagnostics.Log(
                    $"METIN2_MOVE_STATE moving={wantsMove} t={Time.time:0.00} input={input}");
                if (wantsMove)
                {
                    float resolvedSpeed = CurrentMoveSpeed;
                    Metin2NetworkDiagnostics.Event(0,
                        $"LOCAL_MOVE_SPEED source={LastMoveSpeedSource} base_mps={LastResolvedBaseMoveSpeed:F3} " +
                        $"server={serverMoveSpeed} effective_mps={resolvedSpeed:F3} walk={(walkMode ? 1 : 0)} mounted={(mounted ? 1 : 0)}");
                }
            }
            // CInstanceBase::NEW_MoveToDirection (InstanceBaseMovement.cpp:292-330) asks this EVERY
            // frame the direction key is held, and IsWalking() is not a flag - it resolves to
            // __IsMoveMotion(), "is a TYPE_MOVE motion playing right now"
            // (InstanceBaseMovement.cpp:194-197, ActorInstanceMotion.cpp:874-877):
            //
            //     if (!IsWalking())
            //         StartWalking();
            //
            // So anything that takes the Animator away from the run - a damage flinch, a weapon
            // change (ChangeWeapon calls RefreshState(NAME_WAIT, true), InstanceBase.cpp:2789), the
            // wait a finished swing falls back to - is picked up on the very next frame and the run
            // starts again by itself.
            //
            // This used to compare the wanted state against a bool, so an interruption that arrived
            // while the key stayed down was never repaired: `moving` still said true, the edge never
            // came, and the character slid forward in whatever pose the interruption left behind.
            // Being hit while running was the reported case; a weapon swap mid-run does the same.
            if (wantsMove)
            {
                if (motions != null && !motions.IsMoveMotion)
                {
                    if (walkMode) motions.PlayWalk(); else motions.PlayMove(mounted);
                }
            }
            // CInstanceBase::EndGoing (InstanceBaseMovement.cpp:336-343), which carries EndWalking's
            // own guard with it: a flinch that is still playing is not cut short by the walk ending.
            else if (moving) motions?.StopIntoWait(mounted);
            moving = wantsMove;
            mountPresentation?.ApplyMountLocomotion(moving, walkMode);
        }
        private void Emit(byte function, byte argument, uint time)
        {
            byte rotation = FacingDirection;
            PacketPosition(out int x, out int y);
            if (!movementOriginKnown) ResetMovementOrigin();
            uint duration = 0;
            if (function == 1)
            {
                float speed = CurrentMoveSpeed;
                if (float.IsNaN(speed) || float.IsInfinity(speed) || speed <= 0f) return;
                // Same authored RUN/WALK/mount speed and MOV_SPEED factor as local prediction.
                // Measure the actual transmitted segment, after collision and map conversion.
                duration = Metin2MovementTiming.DurationMilliseconds(movementOriginX, movementOriginY, x, y, speed);
            }
            var command = new MoveCommand(function, argument, rotation, x, y, time, duration);
            movementOriginX = x; movementOriginY = y;
            SentCommandCount++; MoveRequested?.Invoke(command);
        }

        private void PacketPosition(out int x, out int y)
        {
            if (world != null && world.HasMap) world.Current.ToGlobal(transform.position, out x, out y);
            else { x = Mathf.RoundToInt(transform.position.x * 100f); y = Mathf.RoundToInt(transform.position.z * 100f); }
        }

        private void ResetMovementOrigin()
        {
            PacketPosition(out movementOriginX, out movementOriginY); movementOriginKnown = true;
        }

        /// <summary>
        /// CPythonPlayerEventHandler::OnAttack/OnUseSkill use the same character-state packet as
        /// movement, independently from CG_ATTACK/CG_USE_SKILL.  Remote clients animate from this
        /// packet; the damage request alone is intentionally not a presentation command.
        /// </summary>
        public void EmitAction(byte function, byte argument)
        {
            uint time = TimeProvider != null ? TimeProvider() : unchecked((uint)Environment.TickCount);
            Emit(function, argument, time);
        }
        private static bool IsDue(uint now, uint deadline) => unchecked((int)(now - deadline)) >= 0;
    }

    public sealed class Metin2FollowCamera : MonoBehaviour
    {
        // game.py:76 holds the client's defaults in Metin2 units:
        //   cameraDistance = 1550, cameraPitch = 27, cameraRotation = 0, cameraHeight = 100
        // Converted with Metin2Units.CentimetreToMetre that is 15.5 m, 27 degrees and a 1 m target height.
        [SerializeField] private Transform target;
        [SerializeField] private float distance = 15.5f, targetHeight = 1f, yaw, pitch = 27f;
        [SerializeField] private float minDistance = 3f, maxDistance = 25f;
        // Camera.cpp initializes m_fTerrainCollisionRadius to 50 native centimetres.
        // ProcessTerrainCollision keeps the eye at least 2 * radius above the terrain.
        [SerializeField] private float terrainCollisionRadius = 0.5f;
        private Metin2.Runtime.Metin2WorldRuntime world;
        private Metin2PlayerController targetController;
        private float nextTerrainCollisionLog;
        private float cameraDistanceSaveAt = -1f;
        public Transform Target => target;
        public float Distance => distance;
        public float NormalizedDistance => Mathf.InverseLerp(minDistance, maxDistance, distance);
        /// <summary>systemSetting.SetCameraMode: short=1550, long=2500 Metin2 centimetres.</summary>
        public void SetOptionDistance(float metres) => distance = Mathf.Clamp(metres, minDistance, maxDistance);
        public void SetNormalizedDistance(float value)
        {
            distance = Mathf.Lerp(minDistance, maxDistance, Mathf.Clamp01(value));
            RememberDistance();
        }
        public float Yaw => yaw;
        public float Pitch => pitch;
        public Vector3 LastFocusPoint { get; private set; }
        public Vector3 LastDesiredEye { get; private set; }
        public float LastEyeGround { get; private set; } = float.NaN;
        public bool WasTerrainCorrected { get; private set; }
        public void Bind(Transform value)
        {
            target = value;
            targetController = target == null ? null : target.GetComponent<Metin2PlayerController>();
            Snap();
        }
        /// <summary>Transfer a cinematic eye into the persistent orbit state atomically.
        /// Orbit yaw describes the eye offset, not Camera.forward's opposite heading.</summary>
        public void AdoptWorldPose(Transform value,Vector3 eye)
        {
            target=value;targetController=target==null?null:target.GetComponent<Metin2PlayerController>();
            if(target==null)return;
            var focus=target.position+Vector3.up*((targetController!=null?targetController.FootOffset:0)+targetHeight);
            var offset=eye-focus;float horizontal=new Vector2(offset.x,offset.z).magnitude;
            if(offset.sqrMagnitude>.01f){yaw=Mathf.Atan2(offset.x,-offset.z)*Mathf.Rad2Deg;pitch=Mathf.Clamp(Mathf.Atan2(offset.y,horizontal)*Mathf.Rad2Deg,10,65);distance=Mathf.Clamp(offset.magnitude,minDistance,maxDistance);}
            Snap();
        }
        public void BindWorld(Metin2.Runtime.Metin2WorldRuntime value)
        {
            world = value;
            ApplyFullMapFarClip();
            Snap();
        }

        private Transform savedTarget;
        private Metin2PlayerController savedTargetController;
        private float savedDistance, savedPitch, savedYaw;
        private GameObject buildFocus;
        private bool inBuildView;

        /// <summary>
        /// uiguild.py BuildGuildBuildingWindow.Open / __LockCameraMoving:
        /// app.SetCameraSetting(x, -y, z, 3000, 0, 30) - a top-down event camera centred on the
        /// building site (30 m zoom, rotation 0, pitch 30). __UnlockCameraMoving restores the
        /// default follow camera. The previous view is saved so closing the window returns exactly.
        /// </summary>
        public void BeginGuildBuildView(Vector3 focusPoint)
        {
            if (!inBuildView)
            {
                savedTarget = target;
                savedTargetController = targetController;
                savedDistance = distance;
                savedPitch = pitch;
                savedYaw = yaw;
                inBuildView = true;
            }
            if (buildFocus == null) buildFocus = new GameObject("GuildBuildCameraFocus");
            buildFocus.transform.position = focusPoint;
            target = buildFocus.transform;
            targetController = null;
            distance = 30f;
            pitch = 30f;
            yaw = 0f;
            Snap();
        }

        public void UpdateGuildBuildFocus(Vector3 focusPoint)
        {
            if (!inBuildView) return;
            if (buildFocus == null) buildFocus = new GameObject("GuildBuildCameraFocus");
            buildFocus.transform.position = focusPoint;
        }

        /// <summary>uiguild.py __UnlockCameraMoving: app.SetDefaultCamera().</summary>
        public void EndGuildBuildView()
        {
            if (!inBuildView) return;
            inBuildView = false;
            target = savedTarget;
            targetController = savedTargetController;
            distance = savedDistance;
            pitch = savedPitch;
            yaw = savedYaw;
            if (buildFocus != null) Destroy(buildFocus);
            buildFocus = null;
            Snap();
        }

        /// <summary>
        /// Limited mode clips within the guaranteed loaded neighbourhood (including camera orbit
        /// allowance). Full map preserves the original map-diagonal far plane.
        /// </summary>
        private void ApplyFullMapFarClip()
        {
            Camera camera = GetComponent<Camera>();
            if (camera == null || world == null || !world.HasMap) return;
            float width = Mathf.Max(1, world.Current.width) * 256f;
            float height = Mathf.Max(1, world.Current.height) * 256f;
            float required = Mathf.Sqrt(width * width + height * height) + 256f;
            int radius=Metin2ClientSettings.Current.MapRenderChunks;
            float clip=radius<=0?Mathf.Max(1000f,required):Mathf.Min(Mathf.Max(1000f,required),RenderRangeFarClip(radius));
            if(!Mathf.Approximately(camera.farClipPlane,clip))camera.farClipPlane=clip;
        }
        public static float RenderRangeFarClip(int radius)=>Mathf.Max(128f,radius*256f-36f);

        /// <summary>
        /// Camera orbit. In the original this only happens while the camera mouse button is dragged
        /// or while Q / E / T are held, never from raw mouse movement.
        /// </summary>
        public void Orbit(float yawDelta, float pitchDelta)
        {
            yaw += yawDelta;
            pitch = Mathf.Clamp(pitch + pitchDelta, 10f, 65f);
        }

        /// <summary>PythonApplicationEvent.cpp OnMouseWheel: the wheel is bound to zoom alone.</summary>
        public void Zoom(float delta)
        {
            float next = Mathf.Clamp(distance + delta, minDistance, maxDistance);
            if (Mathf.Approximately(next, distance)) return;
            distance = next;
            RememberDistance();
        }

        private void RememberDistance()
        {
            PlayerPrefs.SetFloat(Metin2ClientSettings.CameraDistancePreference, distance);
            cameraDistanceSaveAt = Time.unscaledTime + .5f;
        }

        private void FlushDistance()
        {
            if (cameraDistanceSaveAt < 0f) return;
            PlayerPrefs.Save();
            cameraDistanceSaveAt = -1f;
        }

        private void OnDisable() => FlushDistance();
        private void OnApplicationPause(bool paused) { if (paused) FlushDistance(); }
        private void OnApplicationQuit() => FlushDistance();

        private void LateUpdate()
        {
            ApplyFullMapFarClip();
            Metin2.Runtime.Metin2NativeParticleSystem.FlushStoneBatches();
            if (cameraDistanceSaveAt >= 0f && Time.unscaledTime >= cameraDistanceSaveAt)
                FlushDistance();
            if (target == null) return;
            Snap();
#if UNITY_STANDALONE_WIN
            Metin2.Runtime.Metin2ActorPresentationScheduler.Tick(GetComponent<Camera>(),target);
#endif
        }
        /// <summary>
        /// CGraphicBase::SetPositionCamera places the eye purely by orbiting the target at
        /// (distance, pitch, rotation); the only vertical offset is the camera's target height.
        /// The previous version added the orbit pitch AND a separate world-up height term, so the
        /// eye sat far too high and lowering the pitch barely moved it.
        /// </summary>
        private void Snap()
        {
            if (target == null) return;
            // CPythonApplication::SetCenterPosition follows the native actor's logical pixel
            // position, which is on the terrain, and then adds GetTargetHeight (100 cm by default).
            // Our converted mesh needs its GameObject root sunk by FootOffset so its soles meet that
            // same terrain.  Add the stored offset back for camera purposes; otherwise the camera
            // focuses on the artificial, underground Unity root instead of ground + targetHeight.
            float logicalGroundOffset = targetController == null ? 0f : targetController.FootOffset;
            Vector3 focus = target.position + Vector3.up * (logicalGroundOffset + targetHeight);
            float pitchRadians = pitch * Mathf.Deg2Rad;
            float yawRadians = yaw * Mathf.Deg2Rad;
            float horizontal = distance * Mathf.Cos(pitchRadians);
            Vector3 offset = new Vector3(horizontal * Mathf.Sin(yawRadians),
                                         distance * Mathf.Sin(pitchRadians),
                                         -horizontal * Mathf.Cos(yawRadians));
            Vector3 eye = focus + offset;
            LastFocusPoint = focus;
            LastDesiredEye = eye;
            LastEyeGround = float.NaN;
            WasTerrainCorrected = false;

            // CCamera::Update -> ProcessTerrainCollision. The native camera uses a 50 cm terrain
            // collision radius and lifts the eye to the collision point + 2 * radius. Without this
            // the orbit camera can sit underneath a rising terrain tile even while the player itself
            // is correctly grounded. Unity Terrain is one-sided from below, so the symptom looks as
            // if the whole map is floating above the character.
            if (world != null && world.HasMap)
            {
                float ground = world.GroundHeight(eye);
                LastEyeGround = ground;
                float minimumEyeY = ground + terrainCollisionRadius * 2f;
                if (eye.y < minimumEyeY)
                {
                    float wantedY = eye.y;
                    eye.y = minimumEyeY;
                    WasTerrainCorrected = true;
                    if (Time.unscaledTime >= nextTerrainCollisionLog)
                    {
                        nextTerrainCollisionLog = Time.unscaledTime + 1f;
                        Debug.Log($"METIN2_CAMERA_TERRAIN_COLLISION desiredY={wantedY:F2} ground={ground:F2} correctedY={eye.y:F2}");
                    }
                }
            }

            transform.position = eye;
            transform.LookAt(focus);
        }
    }
}
