using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Metin2.Runtime;
using UnityEngine;
using UnityEngine.UI;

namespace Metin2.Runtime.Network
{
    public sealed partial class Metin2NetworkEntityWorld : MonoBehaviour
    {
        [SerializeField] private Metin2RaceCatalog raceCatalog;
        [SerializeField] private Metin2EquipmentCatalog equipmentCatalog;
        [SerializeField] private Metin2ItemCatalog itemCatalog;
        [SerializeField] private GameObject[] playerPrefabs;
        [SerializeField] private Transform entityRoot;
        [SerializeField] private Metin2.Runtime.Metin2WorldRuntime worldRuntime;
        public string CurrentMapName => worldRuntime != null ? worldRuntime.CurrentMapName : string.Empty;
        /// <summary>
        /// The handshake-synchronised millisecond clock used by GC_MOVE.dwTime. Native
        /// CInstanceBase::PushTCPState compares packet command time with
        /// ELTimer_GetServerFrameMSec; using Time.time here would put the two clocks in unrelated
        /// epochs and make the remote command queue either stall or drain immediately.
        /// </summary>
        public Func<uint> TimeProvider { private get; set; }
        internal uint ServerTimeMilliseconds => TimeProvider != null
            ? TimeProvider()
            : unchecked((uint)Environment.TickCount);
        /// <summary>
        /// Yaw correction between the converted mesh's forward axis and the direction of travel.
        ///
        /// Optional model-only correction after the original client's packet angle has been
        /// converted by Metin2CoordinateSpace.ActorYaw. Travel headings need no protocol conversion.
        /// </summary>
        [SerializeField] private float entityYawOffset;
        private readonly Dictionary<uint, Metin2NetworkEntity> entities = new Dictionary<uint, Metin2NetworkEntity>();
        private readonly Dictionary<uint, byte> clientPlatforms = new Dictionary<uint, byte>();
        private readonly Dictionary<uint, string> guildNames = new Dictionary<uint, string>();
        private readonly Dictionary<uint, byte> guildLevels = new Dictionary<uint, byte>();
        private readonly Dictionary<uint, string> itemOnTitlePrefixes = new Dictionary<uint, string>();
        private readonly Dictionary<uint, Vector2Int> mountBeltCounts = new Dictionary<uint, Vector2Int>();
        // A packet remains authoritative while its asset root is acquired.  Replacing/removing the
        // VID removes this record, so a completed Addressables operation cannot resurrect it.
        private sealed class DeferredSpawn
        {
            public Metin2GamePacketCodec.EntityAdd Data;
            public readonly List<byte[]> FollowingPackets = new List<byte[]>();
        }
        private readonly Dictionary<uint, DeferredSpawn> deferredSpawns = new Dictionary<uint, DeferredSpawn>();
        private CancellationTokenSource spawnLifetime = new CancellationTokenSource();
        private Metin2LocaleText textTailLocale;
        private Metin2PvpState textTailPvp;
        private Metin2PartyPanel textTailParty;
        private Metin2GuildPanel textTailGuild;
        private Metin2NetworkEntity localTextTailActor;
        public event Action<Metin2NetworkEntity> EntityRegistered;
        public event Action<uint> EntityRemoved;
        /// <summary>Attacker-authored final shove position, sent as CG_SYNC_POSITION.</summary>
        public event Action<uint, Vector3> PushPositionReady;
        /// <summary>
        /// GC_CHARACTER_DEAD for a remote actor. CPythonCharacterManager eventually forwards this
        /// to CPythonPlayer::NotifyCharacterDead, which clears the selected VID when the victim was
        /// the current target (PythonPlayer.cpp:502-507).
        /// </summary>
        public event Action<uint> EntityDied;
        /// <summary>GC_DEAD for the main character - the original OnGameOver hook
        /// (PythonNetworkStreamPhaseGame.cpp:1659-1686) that opens the restart dialog.</summary>
        public event Action LocalPlayerDied;
        /// <summary>
        /// Authoritative local ADD/UPDATE/death state edge.  The local actor is not stored in the
        /// entity dictionary, so its presentation controller needs this edge to perform the same
        /// immediate Revive -> WAIT transition as the native actor instead of waiting for input.
        /// </summary>
        public event Action<bool> LocalLifeStateChanged;
        public int EntityCount => entities.Count;
        /// <summary>Read-only enumeration used by CInstanceBase::NEW_GetFrontInstance parity.</summary>
        public IEnumerable<Metin2NetworkEntity> Entities => entities.Values;
        private int missingSources;
        public int MissingSourceCount => missingSources;
        public bool TryGetEntity(uint vid, out Metin2NetworkEntity entity) => entities.TryGetValue(vid, out entity);
        public bool IsActorPending(uint vid) => deferredSpawns.ContainsKey(vid);

        public void ApplyItemOnTitle(uint vid, string prefix)
        {
            itemOnTitlePrefixes[vid] = prefix ?? string.Empty;
            if (IsLocalPlayer(vid)) localTextTailActor?.SetItemOnTitlePrefix(prefix);
            else if (entities.TryGetValue(vid, out Metin2NetworkEntity entity))
                entity.SetItemOnTitlePrefix(prefix);
        }

        public void ApplyFakeShopSign(Metin2GamePacketCodec.FakeShopSign sign)
        {
            mountBeltCounts[sign.Vid] = new Vector2Int(sign.MountCount, sign.BeltCount);
            if (IsLocalPlayer(sign.Vid)) localTextTailActor?.SetMountBeltCount(sign.MountCount, sign.BeltCount);
            else if (entities.TryGetValue(sign.Vid, out Metin2NetworkEntity entity))
                entity.SetMountBeltCount(sign.MountCount, sign.BeltCount);
        }
        /// <summary>Registers an already-created authoritative actor (also used by deterministic proofs).</summary>
        public bool RegisterEntity(Metin2NetworkEntity entity)
        {
            if (entity == null || entity.Vid == 0) return false;
            entity.BindMotionWorld(this, worldRuntime);
            entity.BindTextTail(textTailLocale, LocalPlayerRoot, ResolveGuildName(entity.GuildId));
            entity.BindNameColorResolver(ResolveTextTailNameColor);
            entities[entity.Vid] = entity;
            ApplyStoredOverhead(entity.Vid, entity);
            EntityRegistered?.Invoke(entity);
            return true;
        }
        public bool TryGetActor(uint vid, out Transform actor)
        {
            if (entities.TryGetValue(vid, out Metin2NetworkEntity entity) && entity != null) { actor = entity.transform; return true; }
            actor = null; return false;
        }

        /// <summary>
        /// The actor for a VID, the local player included.
        ///
        /// TryGetActor structurally CANNOT see the local player: LocalVid's setter despawns the
        /// network copy so it does not shadow the locally driven one, so the player's own VID is
        /// absent from the entity map by design. The original has no such split -
        /// CPythonCharacterManager keys every instance by VID and the main character is registered
        /// under its own - so any packet naming both ends of an interaction has to resolve this way.
        ///
        /// GC_CREATE_FLY is what exposed it. The sura's Lang lelke sends the CASTER as StartVid, and
        /// the caster is the player, so the projectile was thrown away on arrival every single time.
        /// The same hole silently dropped any fly targeting aimed AT the player - a monster shooting
        /// an arrow at you resolves the player as its EndVid.
        /// </summary>
        public bool TryResolveActor(uint vid, out Transform actor)
        {
            if (TryGetActor(vid, out actor)) return true;
            if (vid != 0 && vid == LocalVid && LocalPlayerRoot != null) { actor = LocalPlayerRoot; return true; }
            actor = null; return false;
        }
        /// <summary>
        /// Returns the nearest enemy/stone for target-less combat. The original OnHit implicitly
        /// selects an intersected victim before sending its attack packet; this also gives skills a
        /// protocol target without requiring the unfinished mouse target workflow.
        /// </summary>
        public bool TryFindNearestAttackTarget(Vector3 origin, float maximumDistance, out uint vid)
        {
            vid = 0;
            float best = Mathf.Max(0f, maximumDistance);
            best *= best;
            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                if (pair.Value == null || pair.Value.IsDead || !IsUntargetedAttackType(pair.Value.Type)) continue;
                Vector3 delta = pair.Value.transform.position - origin; delta.y = 0f;
                float distance = delta.sqrMagnitude;
                if (distance >= best) continue;
                best = distance; vid = pair.Key;
            }
            return vid != 0;
        }
        /// <summary>
        /// One geometric attack test. <paramref name="fromTime"/>/<paramref name="toTime"/> restrict
        /// the evaluation to the part of the weapon path the motion has actually swept so far:
        /// CActorInstance::__AttackProcess runs every frame while the elapsed motion time is inside
        /// [AttackingStartTime, AttackingEndTime] (GameLib/ActorInstanceBattle.cpp:341-350), so the
        /// hit lands when the blade reaches the victim, not when the swing begins. Passing the full
        /// range reproduces the whole-swing sweep used by motions without carried sample times.
        /// </summary>
        public bool IsAttackHit(uint vid, Transform attacker, Metin2.Runtime.Metin2MotionDefinition motion,
            float fromTime = float.NegativeInfinity, float toTime = float.PositiveInfinity)
        {
            if (attacker == null || motion?.attackData?.hitData == null ||
                !entities.TryGetValue(vid, out Metin2NetworkEntity victim) || victim == null) return false;
            Metin2.Runtime.Metin2CollisionProfile profile = victim.CollisionProfile;
            if (profile == null) return false;
            Quaternion facing = ActorFacing(attacker);
            float scale = NativeActorScale(attacker);
            float attackRadius = 20f * scale; // c_fAttackRadius, ActorInstanceCollisionDetection.cpp:355
            foreach (Metin2.Runtime.Metin2MotionHitData hit in motion.attackData.hitData)
            {
                float[] samples = hit.hitPositions;
                if (samples == null) continue;
                for (int index = 0; index + 6 < samples.Length; index += 7)
                {
                    float sampleTime = samples[index];
                    if (sampleTime <= fromTime || sampleTime > toTime) continue;
                    Vector3 from = NativeAttackPoint(attacker, facing, scale, samples[index + 1], samples[index + 2], samples[index + 3]);
                    Vector3 to = NativeAttackPoint(attacker, facing, scale, samples[index + 4], samples[index + 5], samples[index + 6]);
                    foreach (Metin2.Runtime.Metin2CollisionSphere sphere in profile.Spheres)
                    {
                        if (sphere.collisionType != 3) continue; // COLLISION_TYPE_DEFENDING
                        Vector3 center = profile.SphereWorldCentre(sphere);
                        float defendingRadius = sphere.radius * MaxAbs(profile.transform.lossyScale);
                        if (DynamicZCylindersIntersect(from, to, attackRadius, center, defendingRadius)) return true;
                    }
                }
            }
            return false;
        }

        /// <summary>
        /// CActorInstance::AttackProcess checks every attackable instance, not only the selected
        /// target.  That distinction is essential while target selection is unavailable: a swing
        /// still produces one CG_ATTACK for every enemy/stone intersecting the MSA hit path.
        /// </summary>
        public uint[] CollectAttackHits(Transform attacker, Metin2.Runtime.Metin2MotionDefinition motion,
            uint preferredVid = 0, float fromTime = float.NegativeInfinity, float toTime = float.PositiveInfinity,
            Func<Metin2NetworkEntity, bool> canAttack = null)
        {
            var result = new List<uint>();
            float nearestGap = float.PositiveInfinity;
            uint nearestVid = 0;
            byte nearestType = byte.MaxValue;
            bool nearestHasProfile = false;
            if (preferredVid != 0 && entities.TryGetValue(preferredVid, out Metin2NetworkEntity preferred) &&
                preferred != null && preferred.CanPick && !preferred.IsDead && IsSelectableAttackType(preferred.Type) &&
                (canAttack == null || canAttack(preferred)) &&
                IsAttackHit(preferredVid, attacker, motion, fromTime, toTime))
                result.Add(preferredVid);
            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                if (pair.Key == preferredVid || pair.Value == null || pair.Value.IsDead ||
                    !(canAttack == null ? IsUntargetedAttackType(pair.Value.Type) : IsSelectableAttackType(pair.Value.Type)) ||
                    (canAttack != null && !canAttack(pair.Value))) continue;
                if (IsAttackHit(pair.Key, attacker, motion, fromTime, toTime)) result.Add(pair.Key);
                if (Metin2.Runtime.CombatProof.Enabled)
                {
                    float gap = ClosestAttackGap(attacker, motion, pair.Value, out bool hasProfile);
                    if (gap < nearestGap)
                    {
                        nearestGap = gap;
                        nearestVid = pair.Key;
                        nearestType = pair.Value.Type;
                        nearestHasProfile = hasProfile;
                    }
                }
            }
            if (Metin2.Runtime.CombatProof.Enabled)
                Debug.Log($"METIN2_COMBAT_PROOF_HIT_SCAN motion={motion?.stateName ?? "none"} " +
                          $"entities={entities.Count} hits={result.Count} nearest={nearestVid} type={nearestType} " +
                          $"profile={nearestHasProfile} gap={(float.IsPositiveInfinity(nearestGap) ? -1f : nearestGap):F3} " +
                          $"attacker={attacker?.position.ToString("F3") ?? "none"} yaw={(attacker == null ? 0f : attacker.eulerAngles.y):F1} " +
                          $"scale={(attacker == null ? Vector3.zero : attacker.lossyScale).ToString("F3")}");
            return result.ToArray();
        }

        /// <summary>
        /// Collision path for MOTION_EVENT_TYPE_SPECIAL_ATTACKING. Skill MSA files generally have
        /// no Group AttackingData; they open a timed splash sphere instead. Native coordinates are
        /// centimetres in the actor's x/y ground plane with z as height.
        /// </summary>
        public uint[] CollectSpecialAttackHits(Transform attacker, Metin2.Runtime.Metin2MotionEvent attackEvent,
            uint preferredVid = 0, Func<Metin2NetworkEntity, bool> canAttack = null)
        {
            var result = new List<uint>();
            if (attacker == null || attackEvent == null || attackEvent.type != 4 || attackEvent.radius <= 0f)
                return result.ToArray();
            if (preferredVid != 0 && entities.TryGetValue(preferredVid, out Metin2NetworkEntity preferred) &&
                preferred != null && preferred.CanPick && !preferred.IsDead && IsSelectableAttackType(preferred.Type) &&
                (canAttack == null || canAttack(preferred)) &&
                IsSpecialAttackHit(attacker, attackEvent, preferred))
                result.Add(preferredVid);
            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                if (pair.Key == preferredVid || pair.Value == null || pair.Value.IsDead ||
                    !(canAttack == null ? IsUntargetedAttackType(pair.Value.Type) : IsSelectableAttackType(pair.Value.Type)) ||
                    (canAttack != null && !canAttack(pair.Value))) continue;
                if (IsSpecialAttackHit(attacker, attackEvent, pair.Value)) result.Add(pair.Key);
            }
            return result.ToArray();
        }

        private static bool IsSpecialAttackHit(Transform attacker, Metin2.Runtime.Metin2MotionEvent attackEvent,
            Metin2NetworkEntity victim)
        {
            Metin2.Runtime.Metin2CollisionProfile profile = victim.CollisionProfile;
            if (profile == null) return false;
            float scale = NativeActorScale(attacker);
            Vector3 attackCenter = NativeAttackPoint(attacker, ActorFacing(attacker), scale,
                attackEvent.position.x, attackEvent.position.y, attackEvent.position.z);
            float attackRadius = attackEvent.radius * scale;
            foreach (Metin2.Runtime.Metin2CollisionSphere sphere in profile.Spheres)
            {
                if (sphere.collisionType != 3) continue;
                Vector3 defendingCenter = profile.SphereWorldCentre(sphere);
                float defendingRadius = sphere.radius * MaxAbs(profile.transform.lossyScale);
                if (DynamicZCylindersIntersect(attackCenter, attackCenter, attackRadius,
                        defendingCenter, defendingRadius)) return true;
            }
            return false;
        }

        /// <summary>
        /// Distance from the whole weapon path to the nearest attackable victim's defending sphere,
        /// negative while they overlap. Reported when a swing ends without a hit: a swing that sends
        /// nothing has to say why, or the next round is spent guessing between "no victim",
        /// "no collision profile" and "out of reach".
        /// </summary>
        public float NearestAttackGap(Transform attacker, Metin2.Runtime.Metin2MotionDefinition motion,
            out uint vid, out bool hasProfile)
        {
            vid = 0; hasProfile = false;
            float best = float.PositiveInfinity;
            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                if (pair.Value == null || pair.Value.IsDead || !IsUntargetedAttackType(pair.Value.Type)) continue;
                float gap = ClosestAttackGap(attacker, motion, pair.Value, out bool profile);
                if (gap >= best) continue;
                best = gap; vid = pair.Key; hasProfile = profile;
            }
            return best;
        }

        private static float ClosestAttackGap(Transform attacker, Metin2.Runtime.Metin2MotionDefinition motion,
            Metin2NetworkEntity victim, out bool hasProfile)
        {
            hasProfile = false;
            if (attacker == null || victim == null || motion?.attackData?.hitData == null)
                return float.PositiveInfinity;
            Metin2.Runtime.Metin2CollisionProfile profile = victim.CollisionProfile;
            if (profile == null) return float.PositiveInfinity;
            hasProfile = true;
            Quaternion facing = ActorFacing(attacker);
            float scale = NativeActorScale(attacker);
            float best = float.PositiveInfinity;
            foreach (Metin2.Runtime.Metin2MotionHitData hit in motion.attackData.hitData)
            {
                float[] samples = hit.hitPositions;
                if (samples == null) continue;
                for (int index = 0; index + 6 < samples.Length; index += 7)
                {
                    Vector3 from = NativeAttackPoint(attacker, facing, scale, samples[index + 1], samples[index + 2], samples[index + 3]);
                    Vector3 to = NativeAttackPoint(attacker, facing, scale, samples[index + 4], samples[index + 5], samples[index + 6]);
                    Vector2 a = new Vector2(from.x, from.z), b = new Vector2(to.x, to.z);
                    Vector2 segment = b - a;
                    foreach (Metin2.Runtime.Metin2CollisionSphere sphere in profile.Spheres)
                    {
                        if (sphere.collisionType != 3) continue;
                        Vector3 center = profile.SphereWorldCentre(sphere);
                        Vector2 c = new Vector2(center.x, center.z);
                        float t = segment.sqrMagnitude > 0.000001f
                            ? Mathf.Clamp01(Vector2.Dot(c - a, segment) / segment.sqrMagnitude) : 0f;
                        float radius = 20f * scale +
                                       sphere.radius * MaxAbs(profile.transform.lossyScale);
                        // Planar only, because the test it reports on is planar only. A gap that
                        // counted height would read "miss" on a swing the collision accepts.
                        best = Mathf.Min(best, Vector2.Distance(a + segment * t, c) - radius);
                    }
                }
            }
            return best;
        }

        /// <summary>
        /// CPythonCharacterManager::__NEW_Pick (UserInterface/PythonCharacterManager.cpp:738-790)
        /// walks the pickable instances ordered by camera distance and takes the first one whose
        /// bounding box the cursor intersects (CInstanceBase::IntersectBoundingBox). Actors here
        /// carry no physics colliders, so the same bounding volume is tested analytically rather
        /// than through Physics.Raycast - which is why clicking an actor never selected anything.
        /// </summary>
        private readonly List<Metin2NetworkEntity> pickCandidates = new List<Metin2NetworkEntity>();
        private readonly HashSet<uint> loggedPickBoundsFallback = new HashSet<uint>();

        /// <summary>
        /// CPythonCharacterManager::__NEW_Pick (PythonCharacterManager.cpp:737-800) and the list it
        /// works from, __UpdateSortPickedActorList (:678-706).
        ///
        /// The original never picks against a renderer volume. A living actor is collected by
        /// CActorInstance::IntersectDefendingSphere (ActorInstance.cpp:753-780) - the ray against
        /// each authored COLLISION_TYPE_DEFENDING sphere of its MSM - and only then does
        /// IntersectBoundingBox (CGraphicThingInstance::Intersect, a real mesh test) decide which of
        /// the collected actors wins; if none of them is crossed, the nearest collected one is taken.
        ///
        /// The port collected candidates from the union of every child Renderer's world bounds,
        /// which is a box around the model PLUS its attached effects - the selection glow, affect
        /// auras and a metin stone's aura - and, being axis-aligned and metres tall, is crossed by a
        /// camera ray aimed at ground several metres to the side. That is the oversized click radius
        /// on stones, bosses and ordinary mobs alike.
        /// </summary>
        public bool TryPickEntity(Ray ray, Vector3 eye, uint excludeVid, out uint vid)
        {
            vid = 0;
            pickCandidates.Clear();
            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                Metin2NetworkEntity entity = pair.Value;
                if (entity == null || pair.Key == excludeVid || !entity.CanPick) continue;
                // CanPickInstance's __IsInViewFrustum arm. IntersectDefendingSphere tests an
                // infinite LINE (it never checks the sign of t), so without this an actor standing
                // behind the camera is a legal candidate and can win the second pass.
                if (Vector3.Dot(entity.transform.position - ray.origin, ray.direction) <= 0f) continue;
                // Authored spheres when the actor has them; the model box only for actors whose MSM
                // carries no defending data, so nothing becomes unpickable.
                bool collected;
                if (entity.TryIntersectDefendingSpheres(ray, out bool sphereHit)) collected = sphereHit;
                else
                {
                    collected = entity.IntersectModelBounds(ray);
                    // Once per actor: the model box is measurably looser than the authored spheres,
                    // so a race that lands here is a data gap worth seeing rather than a silent one.
                    if (loggedPickBoundsFallback.Add(pair.Key))
                        Debug.LogWarning($"METIN2_PICK_NO_DEFENDING_SPHERES vid={pair.Key} race={entity.Race} " +
                                         "hint=this MSM carries no COLLISION_TYPE_DEFENDING sphere, picking falls back to the model box");
                }
                // NPC defending spheres are combat data, often a single small foot sphere.
                // Talking/giving must also accept the visible body, not just that combat volume.
                if (!collected && entity.Type == 1) collected = entity.IntersectModelBounds(ray);
                if (collected) pickCandidates.Add(entity);
            }
            if (pickCandidates.Count == 0) return false;
            // __SortPickedActorList: nearest to the eye first.
            Vector3 eyePosition = eye;
            pickCandidates.Sort((left, right) =>
                (left.transform.position - eyePosition).sqrMagnitude
                    .CompareTo((right.transform.position - eyePosition).sqrMagnitude));
            // __NEW_Pick first pass: an actor the ray really crosses wins over a merely nearby one.
            foreach (Metin2NetworkEntity candidate in pickCandidates)
                if (candidate.IntersectModelBounds(ray)) { vid = candidate.Vid; return true; }
            // Second pass: nothing was crossed, so the nearest collected actor is picked.
            vid = pickCandidates[0].Vid;
            return true;
        }

        /// <summary>
        /// CInstanceBase::OnTargeted / OnUnselected attach and detach EFFECT_TARGET, registered in
        /// playersettingmodule.py:236 as d:/ymir work/effect/etc/click/click_glow_select.mse. Without
        /// it a selected actor looked identical to an unselected one.
        /// </summary>
        public bool ApplyTargetIndicator(uint vid)
        {
            if (targetIndicatorVid == vid && targetIndicator != null) return true;
            if (targetIndicator != null) Destroy(targetIndicator);
            targetIndicator = null; targetIndicatorVid = 0;
            if (vid == 0 || !entities.TryGetValue(vid, out Metin2NetworkEntity entity) ||
                entity == null || !entity.CanPick) return false;
            if (targetEffectCatalog == null)
                targetEffectCatalog = Resources.Load<Metin2.Runtime.Metin2EffectCatalog>("Metin2EffectCatalog");
            if (targetEffectCatalog == null ||
                !targetEffectCatalog.TryGet("effect/etc/click/click_glow_select.mse",
                    out Metin2.Runtime.Metin2EffectDefinition definition))
            {
                Debug.LogWarning("METIN2_TARGET_EFFECT_SOURCE_MISSING path=effect/etc/click/click_glow_select.mse");
                return false;
            }
            targetIndicator = Metin2.Runtime.Metin2EffectRuntime.Create(definition, entity.transform);
            Metin2.Runtime.Metin2ActorAttachedEffect.Attach(targetIndicator);
            targetIndicatorVid = vid;
            if (Metin2.Runtime.CombatProof.Enabled)
                Debug.Log($"METIN2_COMBAT_PROOF_TARGET_INDICATOR vid={vid}");
            return true;
        }

        private GameObject targetIndicator;
        private uint targetIndicatorVid;
        private Metin2.Runtime.Metin2EffectCatalog targetEffectCatalog;

        private static bool IsUntargetedAttackType(byte type) => type == 0 || type == 2; // enemy, stone
        private static bool IsSelectableAttackType(byte type) => IsUntargetedAttackType(type) || type == 6; // PC

        /// <summary>
        /// The actor's LOGICAL rotation - the one CActorInstance::__NormalAttackProcess rotates the
        /// weapon samples by. The native reads GetRotation(), which is the direction the actor faces;
        /// its models carry no yaw of their own
        /// (GameLib/ActorInstanceCollisionDetection.cpp:383-406).
        ///
        /// The converted PLAYER meshes do carry one. Metin2PlayerController writes
        /// facingDegrees + modelYawOffset (-90) into transform.rotation because those meshes face
        /// their local +X (Metin2PlayerController.cs:85), so the root's forward is ninety degrees off
        /// the heading. Using it here swept the blade past the character's SIDE instead of in front
        /// of them - the identical mistake Metin2ActorMotionEffects.BeginAccumulation already carries
        /// a note about, where a forward step went left. Monster and NPC roots are spawned with
        /// entityYawOffset 0 and need no correction, which is why only the local player's swings
        /// missed.
        /// </summary>
        private static Quaternion ActorFacing(Transform actor)
        {
            if (actor == null) return Quaternion.identity;
            Metin2PlayerController player = actor.GetComponentInParent<Metin2PlayerController>();
            if (player != null) return Quaternion.LookRotation(player.Heading, Vector3.up);
            Metin2NetworkEntity entity = actor.GetComponentInParent<Metin2NetworkEntity>();
            if (entity != null && Mathf.Abs(entity.ModelYawOffset) > 0.0001f)
                return actor.rotation * Quaternion.Euler(0f, -entity.ModelYawOffset, 0f);
            return actor.rotation;
        }

        /// <summary>
        /// One weapon-path sample in world space, built the way the native builds its attacking
        /// cylinder: rotate the sample around the vertical axis by the actor's rotation, then add the
        /// actor's position (ActorInstanceCollisionDetection.cpp:387-406). Native x/y is the ground
        /// plane and z is height. The combat actor is an unscaled network/controller root while its
        /// authored visual, motion and collision data live on a 0.01-scaled child. Using the outer
        /// root's lossyScale therefore turns a 150 cm weapon path into 150 Unity metres. Read the
        /// native-to-world scale from that authored child instead.
        /// </summary>
        private static Vector3 NativeAttackPoint(Transform attacker, Quaternion facing, float scale,
            float x, float y, float z)
        {
            Vector3 local = new Vector3(x, z, -y) * scale;
            return attacker.position + facing * local;
        }

        /// <summary>
        /// Scale of the hierarchy node that owns native Metin2 centimetre data.
        ///
        /// LocalPlayer_* and every network entity are deliberately unscaled controller roots. The
        /// converted character below them carries Metin2Units.CentimetreToMetre, and both MSM
        /// collision spheres and MSA weapon samples are expressed in the same native centimetres.
        /// Taking the controller root's scale was the 6.3 regression that let an empty-air swing hit
        /// mobs roughly one hundred times farther away. Collision data is the strongest source; a
        /// motion set is the production fallback. A synthetic actor with neither still uses the
        /// project's single native-unit conversion rather than silently restoring the 100x bug.
        /// </summary>
        internal static float NativeActorScale(Transform actor)
        {
            if (actor == null) return Metin2.Runtime.Metin2Units.CentimetreToMetre;
            Metin2.Runtime.Metin2CollisionProfile profile =
                actor.GetComponentInChildren<Metin2.Runtime.Metin2CollisionProfile>(true);
            if (profile != null)
            {
                float profileScale = MaxAbs(profile.transform.lossyScale);
                if (profileScale > 0.000001f) return profileScale;
            }
            Metin2CharacterMotionSet motions =
                actor.GetComponentInChildren<Metin2CharacterMotionSet>(true);
            if (motions != null)
            {
                float motionScale = MaxAbs(motions.transform.lossyScale);
                if (motionScale > 0.000001f) return motionScale;
            }
            return Metin2.Runtime.Metin2Units.CentimetreToMetre;
        }

        private static float MaxAbs(Vector3 value) => Mathf.Max(Mathf.Abs(value.x), Mathf.Abs(value.y), Mathf.Abs(value.z));

        /// <summary>
        /// DetectCollisionDynamicZCylinderVSDynamicZCylinder (GameLib/GameUtil.cpp:6-43). The name is
        /// literal: both cylinders have their z ZEROED before anything is measured (:11-15), so the
        /// test is purely on the ground plane and the AABB's z comparison at :35 can never reject.
        /// Height plays no part - a swing reaches a victim standing anywhere inside its planar reach.
        ///
        /// A vertical band check used to stand here. It is not in the original, and it can only
        /// remove hits, so it is gone.
        ///
        /// Remaining deviation, stated rather than hidden: the native's second cylinder is SWEPT
        /// (v3LastPosition to v3Position, the victim's own movement across the frame) and the test is
        /// segment against segment. This passes the victim's current centre as a zero-length segment,
        /// which is exact for a standing victim and slightly conservative for a moving one.
        /// </summary>
        private static bool DynamicZCylindersIntersect(Vector3 from, Vector3 to, float attackRadius,
            Vector3 center, float defendingRadius)
        {
            Vector2 a = new Vector2(from.x, from.z), b = new Vector2(to.x, to.z), c = new Vector2(center.x, center.z);
            Vector2 segment = b - a;
            float t = segment.sqrMagnitude > 0.000001f
                ? Mathf.Clamp01(Vector2.Dot(c - a, segment) / segment.sqrMagnitude) : 0f;
            float radius = attackRadius + defendingRadius;
            return (a + segment * t - c).sqrMagnitude <= radius * radius;
        }
        /// <summary>
        /// CActorInstance::__CanPushDestActor (ActorInstanceBattle.cpp:556-585). A hit only shoves the
        /// victim when the victim is shovable at all, and the exclusions are the point: buildings,
        /// doors, STONES, NPCs and huge races never move. A metin stone is a fixed-position actor and
        /// pushing one would be a visible desync against a server that never moved it.
        ///
        /// ECharTypes: 0 monster, 1 npc, 2 stone, 3 warp, 4 door, 5 building, 6 pc, 7 polymorph pc.
        /// The original also allows the push when the victim is stunned and otherwise requires the
        /// attacker to own it for under three seconds; ownership is modelled here through OwnerVid,
        /// and the stun flag through IsStunned.
        /// </summary>
        public bool CanPush(uint attackerVid, uint victimVid)
        {
            if (!entities.TryGetValue(victimVid, out Metin2NetworkEntity victim) || victim == null) return false;
            if (victim.Race == 2493) return false; // IS_HUGE_RACE in this client build.
            switch (victim.Type)
            {
                case 1: // NPC
                case 2: // STONE - the metin stones
                case 3: // WARP
                case 4: // DOOR
                case 5: // BUILDING
                    return false;
            }
            if (victim.IsStunned) return true;
            return victim.OwnerVid == attackerVid && victim.OwnerAge <= 3f;
        }

        /// <summary>
        /// The shove itself. CPhysicsObject::IncreaseExternalForce (PhysicsObject.cpp:53-90) gives the
        /// victim a velocity of force/mass along the attacker-to-victim direction and lets friction
        /// eat it over 0.02 s steps; with the original's mass 1 and friction 0.3 that settles at
        /// force * 0.02 / 0.3 centimetres. __PushCircle (:923-939) is where the direction comes from -
        /// attacker position to victim position, flattened.
        /// </summary>
        private readonly Dictionary<uint, float> lastPushSyncAt = new Dictionary<uint, float>();

        public bool ApplyExternalForce(uint attackerVid, uint victimVid, Vector3 attackOrigin, float force)
        {
            if (force <= 0f) return false;
            if (!CanPush(attackerVid, victimVid)) return false;
            if (!entities.TryGetValue(victimVid, out Metin2NetworkEntity victim) || victim == null) return false;
            // The server rejects syncs for the same victim inside 50 ms. Do not apply an
            // unsendable local shove: that would recreate the PvP position divergence.
            if (lastPushSyncAt.TryGetValue(victimVid, out float lastAt) && Time.time-lastAt<.06f) return false;
            // ActorInstanceBattle.cpp::__PushCircle uses NEW_GetAtkPixelPositionRef(), captured at
            // attack start. Using the attacker's live transform lets auto-follow cross the victim
            // during a long knock-down and flips the next shove back toward the player.
            Vector3 direction = ComputePushDirection(victim.transform.position, attackOrigin);
            direction.y = 0f;
            if (direction.sqrMagnitude < 0.000001f) return false;
            // The shove is a two-second ease-out over the integrated travel, not a jump. The former
            // closed form here (force * frameTime / friction) collapsed the whole 100-step series
            // into a single step and applied it instantly: about 7 cm for an authored force of 20
            // instead of the 6.5 m slide, teleported rather than slid, so the victim visibly just
            // fell over on the spot.
            if (!victim.BeginPush(direction, force)) return false;
            lastPushSyncAt[victimVid]=Time.time;
            PushPositionReady?.Invoke(victimVid,victim.PushSyncDestination);
            if (Metin2Diagnostics.Verbose)
                Debug.Log($"METIN2_COMBAT_PUSH attacker={attackerVid} victim={victimVid} type={victim.Type} force={force:F1}");
            return true;
        }

        public static Vector3 ComputePushDirection(Vector3 victimPosition, Vector3 attackOrigin)
        {
            Vector3 direction = victimPosition - attackOrigin;
            direction.y = 0f;
            return direction.sqrMagnitude > .000001f ? direction.normalized : Vector3.zero;
        }

        public bool ApplyDamage(uint vid)
        {
            if (!entities.TryGetValue(vid, out Metin2NetworkEntity entity) || entity == null) return false;
            entity.ApplyDamage(); return true;
        }

        /// <summary>
        /// CActorInstance::__ProcessDataAttackSuccess. This is called at local collision time; the
        /// later GC_DAMAGE remains responsible only for the authoritative number.
        /// </summary>
        public bool ApplyLocalHit(uint attackerVid, uint victimVid, Vector3 attackOrigin,
                                  int hittingType, float externalForce)
        {
            if (hittingType == 0 || !entities.TryGetValue(victimVid, out Metin2NetworkEntity victim) ||
                victim == null) return false;

            ApplyExternalForce(attackerVid, victimVid, attackOrigin, externalForce);
            Vector3 attackDirection = victim.transform.position - attackOrigin;
            attackDirection.y = 0f;
            bool fromBehind = attackDirection.sqrMagnitude > .000001f &&
                Vector3.Dot(attackDirection.normalized, victim.transform.forward.normalized) >= 0f;

            // Buildings never play a hit motion. Stones and doors use their existing non-falling
            // reaction path (__HitStone); neither may enter the knock-down chain.
            if (victim.Type == 5) return true;
            if (victim.Type == 2 || victim.Type == 4)
            {
                victim.ApplyDamage(fromBehind);
                return true;
            }

            return hittingType == 1
                ? victim.ApplyKnockDown(fromBehind)
                : victim.ApplyDamage(fromBehind);
        }
        public void Initialize(Metin2RaceCatalog catalog, Transform root = null, Metin2EquipmentCatalog equipment = null, GameObject[] players = null, Metin2ItemCatalog items = null) { raceCatalog = catalog; entityRoot = root; equipmentCatalog = equipment; playerPrefabs = players; itemCatalog = items; }
        public void BindWorld(Metin2.Runtime.Metin2WorldRuntime world)
        {
            worldRuntime = world;
            foreach (Metin2NetworkEntity entity in entities.Values)
                if (entity != null) entity.BindMotionWorld(this, worldRuntime);
        }

        public void BindTextTailLocale(Metin2LocaleText locale)
        {
            textTailLocale = locale;
            foreach (Metin2NetworkEntity entity in entities.Values)
                if (entity != null) entity.BindTextTail(locale, LocalPlayerRoot, ResolveGuildName(entity.GuildId));
        }
        private void OnEnable()
        {
            Metin2Language.Changed += RefreshLanguage;
            RefreshLanguage(null);
        }
        private void OnDisable() => Metin2Language.Changed -= RefreshLanguage;
        private void RefreshLanguage(string _)
        {
            foreach(Metin2NetworkEntity entity in entities.Values)
                if(entity!=null)entity.RefreshLocalizedText();
        }

        public void BindTextTailRelationships(Metin2PvpState pvp, Metin2PartyPanel party, Metin2GuildPanel guild)
        {
            if (textTailPvp != null) textTailPvp.Changed -= RefreshTextTailColors;
            textTailPvp = pvp; textTailParty = party; textTailGuild = guild;
            if (textTailPvp != null) textTailPvp.Changed += RefreshTextTailColors;
            RefreshTextTailColors();
        }

        public void SetLocalTextTailEmpire(byte empire)
        {
            LocalEmpire = empire;
            RefreshTextTailColors();
            LocalTextTailChanged?.Invoke();
        }

        public void RefreshTextTailColors()
        {
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.RefreshAllTailColors");
            try { RefreshTextTailColorsCore(); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); }
        }

        private void RefreshTextTailColorsCore()
        {
            foreach (Metin2NetworkEntity entity in entities.Values) entity?.RefreshClientOptionVisibility();
            localTextTailActor?.RefreshClientOptionVisibility();
        }

        public void ApplyLocalTextTailCombatState(Metin2NetworkEntity actor)
        {
            localTextTailActor = actor;
            if (actor == null) return;
            actor.BindNameColorResolver(ResolveTextTailNameColor);
            actor.ApplyStateFlags(LocalStateFlags);
            if (LocalVid != 0) ApplyStoredOverhead(LocalVid, actor);
        }

        private void ApplyStoredOverhead(uint vid, Metin2NetworkEntity actor)
        {
            if (actor == null) return;
            if (itemOnTitlePrefixes.TryGetValue(vid, out string prefix)) actor.SetItemOnTitlePrefix(prefix);
            if (mountBeltCounts.TryGetValue(vid, out Vector2Int counts)) actor.SetMountBeltCount(counts.x, counts.y);
        }

        private Color32 ResolveTextTailNameColor(Metin2NetworkEntity entity)
        {
            if (entity == null) return Color.white;
            if (entity.Type != 6 || entity.IsKiller)
                return entity.IsKiller && entity.Type == 6 ? new Color32(180, 100, 0, 255)
                    : Metin2ActorTextTailView.ResolveNameColor(entity.Type, entity.Empire,
                        Metin2ClientSettings.Current.NameColorMode);
            bool main = entity == localTextTailActor || (entity.Vid != 0 && entity.Vid == LocalVid);
            if (!main && LocalVid != 0)
            {
                if (textTailPvp != null && textTailPvp.DuelMode != Metin2PvpState.DuelNone)
                {
                    if (textTailPvp.DuelMode == Metin2PvpState.DuelStart &&
                        textTailPvp.IsDuelParticipant(entity.Vid)) return new Color32(238, 54, 223, 255);
                    return Metin2ActorTextTailView.ResolveNameColor(6, entity.Empire,
                        Metin2ClientSettings.Current.NameColorMode);
                }
                bool sameEmpire = entity.Empire == 0 || entity.HasAffectFlag(0) ||
                    (LocalAffectFlags & 1u) != 0 || LocalEmpire == entity.Empire;
                if (!sameEmpire) return new Color32(238, 54, 223, 255);
                if (textTailPvp != null && textTailPvp.IsFightPair(LocalVid, entity.Vid))
                    return new Color32(238, 54, 223, 255);
                if (textTailGuild != null && textTailGuild.IsAtWar(LocalGuildId, entity.GuildId))
                    return new Color32(238, 54, 223, 255);
            }
            if (textTailParty != null && textTailParty.IsMember(main ? LocalVid : entity.Vid))
                return new Color32(128, 192, 255, 255);
            return Metin2ActorTextTailView.ResolveNameColor(6, entity.Empire,
                Metin2ClientSettings.Current.NameColorMode);
        }

        /// <summary>GC_GUILD subheader 16 is CPythonGuild::RegisterGuildName's authoritative map.</summary>
        public void ApplyGuildMessage(Metin2GamePacketCodec.GuildMessage message)
        {
            if (message.SubHeader == 8 && message.Info.HasValue)
            {
                var info = message.Info.Value;
                guildNames[info.GuildId] = info.Name;
                guildLevels[info.GuildId] = info.Level;
            }
            else if (message.SubHeader == 10 && LocalGuildId != 0)
            {
                if(guildLevels.TryGetValue(LocalGuildId,out byte currentLevel)&&currentLevel==message.Value)return;
                guildLevels[LocalGuildId] = message.Value;
            }
            else if (message.SubHeader != 16) return;
            foreach (Metin2GamePacketCodec.GuildComment pair in message.Comments)
                if (pair.Id != 0 && !string.IsNullOrEmpty(pair.Author)) guildNames[pair.Id] = pair.Author;
            foreach (Metin2GamePacketCodec.GuildPair pair in message.Pairs)
                if (pair.First != 0) guildLevels[pair.First] = (byte)Mathf.Clamp((int)pair.Second, 0, 255);
            foreach (Metin2NetworkEntity entity in entities.Values)
                if (entity != null) entity.SetGuildName(ResolveGuildName(entity.GuildId));
            LocalTextTailChanged?.Invoke();
        }

        public string ResolveGuildName(uint guildId)
        {
            if (guildId == 0 || !guildNames.TryGetValue(guildId, out string value)) return string.Empty;
            int level = guildLevels.TryGetValue(guildId, out byte knownLevel) ? knownLevel : 0;
            return $"{value} (Lv{level})";
        }

        /// <summary>player.GetGuildName - the plain name without the text-tail level suffix.</summary>
        public string ResolveGuildNamePlain(uint guildId)
        {
            if (guildId == 0 || !guildNames.TryGetValue(guildId, out string value)) return string.Empty;
            return value;
        }

        /// <summary>chr.GetNameByVID for the emotion command: the actor's own display name.</summary>
        public string ResolveDisplayName(uint vid)
        {
            if (vid == 0) return string.Empty;
            if (IsLocalPlayer(vid)) return LocalDisplayName;
            return TryGetEntity(vid, out Metin2NetworkEntity entity) ? entity.DisplayName ?? string.Empty : string.Empty;
        }

        /// <summary>
        /// VID of the player's own character. The server includes it in the surrounding-entity
        /// broadcast like any other actor, so without this the local player is spawned twice: once by
        /// the bootstrap from GC_MAIN_CHARACTER, and once here as a plain remote actor standing in the
        /// same spot. The original client cannot hit this because CPythonCharacterManager keys every
        /// instance by VID and the main character is already registered under its own.
        /// </summary>
        public uint LocalVid
        {
            get => localVid;
            // Setting it also removes a duplicate that slipped in earlier, in case the add broadcast
            // ever arrives before GC_MAIN_CHARACTER is processed.
            set
            {
                bool hasAdditional = actorAdditional.TryGetValue(value, out var additional);
                bool hasUpdate = actorUpdates.TryGetValue(value, out var update);
                localVid = value;
                if (value == 0) return;
                if (clientPlatforms.TryGetValue(value, out byte platform)) LocalPlatform = platform;
                Despawn(value);
                // MAIN and the actor burst can reach the dispatcher in either order during a
                // warp. Promote metadata too, not just the VID: otherwise deleting the duplicate
                // also deletes its mount, parts and affect state until the next server update.
                if (hasAdditional) Additional(additional);
                if (hasUpdate) UpdateEntity(update);
                LocalTextTailChanged?.Invoke();
            }
        }
        private uint localVid;
        private readonly Dictionary<uint, Metin2GamePacketCodec.EntityAdditional> actorAdditional = new();
        private readonly Dictionary<uint, Metin2GamePacketCodec.EntityUpdate> actorUpdates = new();

        /// <summary>Drops the duplicate that would otherwise shadow the locally driven player.</summary>
        public bool IsLocalPlayer(uint vid) => LocalVid != 0 && vid == LocalVid;

        /// <summary>CPythonCharacterManager::Destroy / network actor manager reset.</summary>
        public void ClearWorld()
        {
            flightSnapshots.Clear();
            woodcutSnapshots.Clear();
            herbSnapshots.Clear();
            foreach(var corpse in fadingActors)if(corpse!=null)Destroy(corpse);
            fadingActors.Clear();
            spawnLifetime.Cancel(); spawnLifetime.Dispose(); spawnLifetime = new CancellationTokenSource();
            deferredSpawns.Clear();
            pendingSpawns.Clear(); pendingSpawnPackets.Clear(); cancelledVids.Clear();
            Metin2ActorTextTailView.ClearSpawnUiPool();
            raceFootOffsets.Clear();
#if UNITY_STANDALONE_WIN
            spawnTailHeights.Clear();
            ClearPreparedSpawnVisuals();
#endif
            clientPlatforms.Clear();
            uint[] vids = new uint[entities.Count]; entities.Keys.CopyTo(vids, 0);
            foreach (uint vid in vids) Despawn(vid);
            actorAdditional.Clear(); actorUpdates.Clear();
            if (targetIndicator != null) Destroy(targetIndicator);
            targetIndicator = null; targetIndicatorVid = 0;
            localVid = 0; LocalPlayerRoot = null; LocalPlatform = DevicePlatform; LocalStateFlags = 0; LocalAffectFlags = 0; LocalAffectFlagsChanged?.Invoke(0);
            LocalIdentityKnown = false; LocalMountVnum = 0; LocalGuildId = 0;
            LocalAlignment = 0; LocalAlignmentRaw = 0;
            LocalSkillColors = BuildEmptySkillColorTable();
            LocalSkillColorsChanged?.Invoke();
            localTextTailActor = null;
            LocalMoveSpeed = 100; LocalAttackSpeed = 100;
            LocalDisplayName = LocalLanguage = string.Empty; LocalLevel = 0; guildNames.Clear(); guildLevels.Clear();
            itemOnTitlePrefixes.Clear(); mountBeltCounts.Clear();
            LocalParts = Array.Empty<ushort>();
        }

        public bool HandlePacket(byte[] packet)
        {
            if (packet == null || packet.Length == 0) return false;
            // Every entity-followup packet handled below starts with header + VID. Keep its exact
            // bytes and order while the ADD waits for Addressables, then replay after registration.
            // Only packets handled by this entity world may be replayed through HandlePacket.
            // Shop-sign packets (39/48/51/52) must reach Metin2GameRuntime even while an
            // actor's visual dependencies are still loading; otherwise the sign is lost.
            if ((packet[0] == 3 || packet[0] == 12 || packet[0] == 13 ||
                 packet[0] == 14 || packet[0] == 18 || packet[0] == 19 ||
                 packet[0] == 36 || packet[0] == 136) && packet.Length >= 5)
            {
                uint pendingVid = (uint)(packet[1] | packet[2] << 8 | packet[3] << 16 | packet[4] << 24);
                if (deferredSpawns.TryGetValue(pendingVid, out DeferredSpawn pending))
                { pending.FollowingPackets.Add((byte[])packet.Clone()); return true; }
                if (pendingSpawnPackets.TryGetValue(pendingVid, out var queuedPackets))
                { queuedPackets.Add((byte[])packet.Clone()); return true; }
            }
            switch (packet[0])
            {
                case 1: QueueSpawn(Metin2GamePacketCodec.ReadEntityAdd(packet)); return true;
                case 2: {uint vid=Metin2GamePacketCodec.ReadEntityDelete(packet);flightSnapshots.Remove(vid);Despawn(vid,true);return true;}
                case 225: ApplyFlightPacket(packet);return true;
                case 3: Move(Metin2GamePacketCodec.ReadEntityMove(packet)); return true;
                case 12: Attack(Metin2GamePacketCodec.ReadEntityAttack(packet)); return true;
                case 13: Stun(Metin2GamePacketCodec.ReadEntityStun(packet)); return true;
                // GC_CHARACTER_DEAD: BYTE header + DWORD vid. The local player is not kept in the
                // remote entity dictionary, so preserve the dead gate explicitly there as well.
                case 14:
                {
                    if (packet.Length < 5) return true;
                    uint deadVid = (uint)(packet[1] | packet[2] << 8 | packet[3] << 16 | packet[4] << 24);
                    if (IsLocalPlayer(deadVid)) { ApplyLocalStateFlags((byte)(LocalStateFlags | 1)); LocalPlayerDied?.Invoke(); }
                    else if (entities.TryGetValue(deadVid, out Metin2NetworkEntity dead))
                    {
                        dead.ApplyDead();
                        EntityDied?.Invoke(deadVid);
                    }
                    return true;
                }
                case 19: UpdateEntity(Metin2GamePacketCodec.ReadEntityUpdate(packet)); return true;
                case 18: ChangeSpeed(Metin2GamePacketCodec.ReadEntitySpeed(packet)); return true;
                case 36: Motion(Metin2GamePacketCodec.ReadEntityMotion(packet)); return true;
                case 136: Additional(Metin2GamePacketCodec.ReadEntityAdditional(packet)); return true;
                case 43: ApplyPositionState(Metin2GamePacketCodec.ReadCharacterPosition(packet)); return true;
                default: return false;
            }
        }

        /// <summary>Profiler wrapper: the Instantiate + race-binding pipeline runs from packet
        /// handling, outside every Update loop, so a metin burst that spawns dozens of actors in
        /// one frame used to fall entirely into the unmeasured frame remainder.</summary>
        private void Spawn(Metin2GamePacketCodec.EntityAdd data, bool visualDependenciesAttempted = false)
        {
            long sample = Metin2.Runtime.Metin2FrameProfiler.Begin();
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.Spawn");
            try { SpawnBody(data, visualDependenciesAttempted); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); Metin2.Runtime.Metin2FrameProfiler.End(Metin2.Runtime.Metin2FrameProfiler.SlotEntitySpawn, sample); }
        }

        // A metin break enqueues dozens of GC_CREATE in the same network tick. Each remote actor
        // costs a full prefab Instantiate plus binding pipelines (tens of milliseconds each), which
        // is the one-frame stall. Spawns instead drain under a per-frame time budget, so the local
        // player and every already responsive system keep their frame and actors appear over the
        // following frames (the deferred Addressables path already had the same semantics).
        private const float SpawnBudgetMilliseconds = 4f;
        float lastSpawnQueuedAt,nextWarmCombatLookup;Transform warmPlayer;Metin2CombatController warmCombat;Vector3 warmPlayerPosition;
#if UNITY_STANDALONE_WIN
        static readonly bool cacheSpawnTailHeight=Metin2WindowsPerformance.Enabled("-benchmark-cached-spawn-height","-benchmark-disable-cached-spawn-height");
        // Only remote unmodified race instances. Clear on map reset; never cache a
        // player/equipment/mount anchor or retain a map asset beyond its lifetime.
        readonly Dictionary<(GameObject prefab,ushort race,byte type,Vector3 scale),float> spawnTailHeights=new();
        static readonly bool prepareSpawnVisuals=Array.IndexOf(Environment.GetCommandLineArgs(),"-benchmark-prepared-npc-visuals")>=0;
        sealed class PreparedVisual {public GameObject actor,view;public Metin2NetworkEntity entity;public Animator animator;public bool keepState;public Vector3 position,scale;public Quaternion rotation;}
        sealed class PreparedRace {public ushort race;public GameObject prefab;public readonly Queue<PreparedVisual> slots=new();}
        readonly Dictionary<ushort,PreparedRace> preparedRaces=new();
        readonly List<PreparedRace> preparedRaceOrder=new();
        Transform preparedVisualRoot;int preparedVisualCount,preparedRaceCursor;
        const int MaximumPreparedVisuals=64,MaximumPreparedPerRace=16;
        static readonly Unity.Profiling.ProfilerMarker preparedVisualHit=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.PreparedVisualHit");
        void ClearPreparedSpawnVisuals()
        {
            if(preparedVisualRoot!=null){preparedVisualRoot.gameObject.SetActive(false);if(Application.isPlaying)Destroy(preparedVisualRoot.gameObject);else DestroyImmediate(preparedVisualRoot.gameObject);}
            preparedVisualRoot=null;preparedRaces.Clear();preparedRaceOrder.Clear();preparedVisualCount=preparedRaceCursor=0;
        }
        PreparedVisual TakePreparedVisual(ushort race,byte type,GameObject prefab)
        {
            if(!prepareSpawnVisuals||type!=0||prefab.GetComponentInChildren<Metin2CharacterEquipment>(true)!=null)return null;
            if(!preparedRaces.TryGetValue(race,out var group))
            {
                var animator=prefab.GetComponentInChildren<Animator>(true);
                if(animator==null||animator.runtimeAnimatorController==null)return null;
                // Do not pre-run Unity AnimationEvents outside the actor's life.
                foreach(var clip in animator.runtimeAnimatorController.animationClips)if(clip!=null&&clip.events.Length!=0)return null;
                group=new PreparedRace{race=race,prefab=prefab};preparedRaces.Add(race,group);preparedRaceOrder.Add(group);
            }
            if(group.prefab!=prefab)return null;
            while(group.slots.Count>0)
            {
                var slot=group.slots.Dequeue();preparedVisualCount--;
                if(slot.actor!=null&&slot.view!=null&&slot.entity!=null)return slot;
            }
            return null;
        }
        void WarmPreparedSpawnVisuals(bool validation=false)
        {
            bool allowed=Application.isPlaying;
#if UNITY_EDITOR
            allowed|=validation;
#endif
            if(!prepareSpawnVisuals||!allowed||(!validation&&Time.unscaledDeltaTime>.025f)||preparedVisualCount>=MaximumPreparedVisuals||preparedRaceOrder.Count==0)return;
            for(int i=0;i<preparedRaceOrder.Count;i++)
            {
                var group=preparedRaceOrder[preparedRaceCursor++%preparedRaceOrder.Count];
                if(group.prefab==null||group.slots.Count>=MaximumPreparedPerRace)continue;
                if(preparedVisualRoot==null){var root=new GameObject("Prepared NPC Visuals");root.transform.SetParent(entityRoot!=null?entityRoot:transform,false);preparedVisualRoot=root.transform;}
                GameObject view=null,actor=null;
                try
                {
                    actor=new GameObject("PreparedActor_"+group.race);actor.transform.SetParent(preparedVisualRoot,false);
                    view=Instantiate(group.prefab,actor.transform);view.name="Prepared_"+group.race;
                    view.transform.localPosition=Vector3.zero;view.transform.localRotation=Quaternion.identity;
                    var animator=view.GetComponentInChildren<Animator>(true);
                    if(animator==null){Destroy(actor);return;}
                    InitializeSpawnPose(view,group.race);
                    // This wrapper has never received a network identity, name, state,
                    // effects or packets. Preparing it avoids expensive AddComponent /
                    // UI construction on the burst without recycling live actor state.
                    var entity=actor.AddComponent<Metin2NetworkEntity>();
                    entity.Initialize(0,group.race,0,100,100,null,null,view.transform,null);
                    entity.BindTextTail(null,null,string.Empty);
                    bool keep=animator.keepAnimatorStateOnDisable;animator.keepAnimatorStateOnDisable=true;
                    actor.SetActive(false);
                    group.slots.Enqueue(new PreparedVisual{actor=actor,view=view,entity=entity,animator=animator,keepState=keep,position=view.transform.localPosition,rotation=view.transform.localRotation,scale=view.transform.localScale});preparedVisualCount++;
                }
                catch{if(actor!=null)Destroy(actor);throw;}
                return; // one pristine visual per idle frame; never recycle a used actor
            }
        }
#endif
        static readonly Unity.Profiling.ProfilerMarker spawnDependencies=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.Dependencies");
        static readonly Unity.Profiling.ProfilerMarker spawnInstantiate=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.Instantiate");
        static readonly Unity.Profiling.ProfilerMarker spawnPose=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.InitialPose");
        static readonly Unity.Profiling.ProfilerMarker spawnEntitySetup=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.EntitySetup");
        static readonly Unity.Profiling.ProfilerMarker spawnInitialize=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.Initialize");
        static readonly Unity.Profiling.ProfilerMarker spawnTextTail=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.TextTail");
        static readonly Unity.Profiling.ProfilerMarker spawnHeightHit=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.HeightCacheHit");
        static readonly Unity.Profiling.ProfilerMarker spawnName=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.Name");
        static readonly Unity.Profiling.ProfilerMarker spawnRaceBinding=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.RaceBinding");
        static readonly Unity.Profiling.ProfilerMarker spawnFinalize=new Unity.Profiling.ProfilerMarker("Metin2.Spawn.Finalize");
        private readonly List<Metin2GamePacketCodec.EntityAdd> pendingSpawns = new List<Metin2GamePacketCodec.EntityAdd>();
        private readonly Dictionary<uint,List<byte[]>> pendingSpawnPackets = new Dictionary<uint,List<byte[]>>();
        private readonly HashSet<uint> cancelledVids = new HashSet<uint>();
        // Foot offset is a per-race constant of the imported template; measuring it per instance
        // re-ran GetComponentsInChildren + bounds for every spawn of the burst.
        private static readonly Dictionary<ushort, float> raceFootOffsets = new Dictionary<ushort, float>();

        private void QueueSpawn(Metin2GamePacketCodec.EntityAdd data)
        {
            lastSpawnQueuedAt=Time.realtimeSinceStartup;
            // Local-player bookkeeping must stay synchronous: ApplyAuthoritativeCharacterAdd and
            // the state/affect replay assume the ADD result is available in the same tick.
            if (IsLocalPlayer(data.Vid)) { Spawn(data); return; }
            cancelledVids.Remove(data.Vid);
            if(!pendingSpawnPackets.ContainsKey(data.Vid))pendingSpawnPackets[data.Vid]=new List<byte[]>();
            for (int index = 0; index < pendingSpawns.Count; index++)
                if (pendingSpawns[index].Vid == data.Vid) { pendingSpawns[index] = data; return; }
            pendingSpawns.Add(data);
        }

        private void Update()
        {
            bool idle=pendingSpawns.Count==0;
            DrainPendingSpawns();
#if UNITY_STANDALONE_WIN
            if(idle&&CanWarmSpawns())
            {
                // Never prepare a model and a widget tree in the same frame.
                if((Time.frameCount&1)==0)Metin2ActorTextTailView.WarmSpawnUiPool();
                else WarmPreparedSpawnVisuals();
            }
#endif
        }
#if UNITY_STANDALONE_WIN
        bool CanWarmSpawns()
        {
            if(!prepareSpawnVisuals&&!Metin2ActorTextTailView.SpawnUiPooling)return false;
            if(Time.realtimeSinceStartup-lastSpawnQueuedAt<2f||Time.unscaledDeltaTime>.025f)return false;
#if DEVELOPMENT_BUILD || UNITY_EDITOR || METIN2_BENCHMARK_BUILD
            if(Metin2WindowsProfileCapture.Sampling)return false;
#endif
            if(LocalPlayerRoot!=warmPlayer){warmPlayer=LocalPlayerRoot;warmCombat=null;nextWarmCombatLookup=0;warmPlayerPosition=warmPlayer!=null?warmPlayer.position:Vector3.zero;}
            if(warmPlayer==null)return false;
            bool moved=(warmPlayer.position-warmPlayerPosition).sqrMagnitude>.0001f;warmPlayerPosition=warmPlayer.position;
            if(moved)return false;
            if(warmCombat==null&&Time.realtimeSinceStartup>=nextWarmCombatLookup)
            {nextWarmCombatLookup=Time.realtimeSinceStartup+1;warmCombat=warmPlayer.GetComponentInChildren<Metin2CombatController>();}
            return warmCombat==null||(!warmCombat.AttackHeld&&!warmCombat.AutoAttackActive);
        }
#endif

        private void DrainPendingSpawns()
        {
            if (pendingSpawns.Count == 0) return;
            long start = System.Diagnostics.Stopwatch.GetTimestamp();
            long budget = (long)(SpawnBudgetMilliseconds * System.Diagnostics.Stopwatch.Frequency / 1000d);
            for (int index = 0; index < pendingSpawns.Count;)
            {
                if (System.Diagnostics.Stopwatch.GetTimestamp() - start >= budget) return;
                Metin2GamePacketCodec.EntityAdd data = pendingSpawns[index];
                pendingSpawns.RemoveAt(index);
                pendingSpawnPackets.TryGetValue(data.Vid,out var following);
                pendingSpawnPackets.Remove(data.Vid);
                if (cancelledVids.Remove(data.Vid)) continue;
                Spawn(data);
                // Spawn may register immediately or enter the Addressables queue. In both
                // cases replay through the normal dispatcher, preserving order and ownership.
                if(following!=null)foreach(var packet in following)HandlePacket(packet);
            }
        }

        private void SpawnBody(Metin2GamePacketCodec.EntityAdd data, bool visualDependenciesAttempted)
        {
            if (data.Race == Metin2WoodcuttingAssets.TreeRace) { SpawnWoodcutTree(data); return; }
            if (Metin2HerbalismAssets.IsPlant(data.Race)) { SpawnHerbPlant(data); return; }
            // The local player is driven by Metin2PlayerController, not by server broadcasts, but
            // its ADD packet still carries authoritative state/affect bits used by client-side skill
            // and PvP eligibility. Preserve those before suppressing the duplicate visual actor.
            if (IsLocalPlayer(data.Vid))
            {
                ApplyLocalStateFlags(data.State);
                ApplyLocalActorSpeeds(data.MoveSpeed, data.AttackSpeed);
                ApplyLocalAffects(data.Affect0, data.Affect1);

                // Native CNetworkActorManager::AppendActor recreates the MAIN actor from this exact
                // ADD position.  The Unity split between a network dictionary and a separately-driven
                // local root must preserve that authoritative correction instead of dropping it.
                if (LocalPlayerRoot != null)
                {
                    Vector3 authoritative = ToUnity(data.X, data.Y, data.Z);
                    Metin2PlayerController controller = LocalPlayerRoot.GetComponent<Metin2PlayerController>()
                                                     ?? LocalPlayerRoot.GetComponentInParent<Metin2PlayerController>();
                    float drift = Vector3.Distance(
                        new Vector3(LocalPlayerRoot.position.x, 0f, LocalPlayerRoot.position.z),
                        new Vector3(authoritative.x, 0f, authoritative.z));
                    Metin2NetworkDiagnostics.Event(0,
                        $"LOCAL_CHARACTER_ADD vid={data.Vid} x={data.X} y={data.Y} drift_m={drift:F2} " +
                        $"move_speed={data.MoveSpeed} attack_speed={data.AttackSpeed}");
                    controller?.ApplyAuthoritativeCharacterAdd(authoritative);
                }
                return;
            }
            bool hadEarlyAdditional = actorAdditional.TryGetValue(data.Vid, out var cachedAdditional);
            bool hadEarlyUpdate = actorUpdates.TryGetValue(data.Vid, out var cachedUpdate);
            Despawn(data.Vid);
            if (hadEarlyAdditional) actorAdditional[data.Vid] = cachedAdditional;
            if (hadEarlyUpdate) actorUpdates[data.Vid] = cachedUpdate;
            // Guild buildings (CHAR_TYPE_BUILDING, type 5) resolve through the Resources-backed
            // guild-building index, not the race system: their vnums (14xxx) were never staged as
            // race prefabs, which left every player-built house, wall and furnace invisible
            // (METIN2_ENTITY_SOURCE_MISSING). Rotations arrive as plain yaw in Angle (x/y limits
            // are 0 for every buildable), so the standard spawn below needs no special casing.
            GameObject buildingPrefab = null;
            bool fromBuildings = data.Type == 5 && Metin2GuildBuildingPrefabs.TryGet(data.Race, out buildingPrefab);
            bool playerActor = data.Type == 6 && data.Race < Metin2PlayerPrefabs.JobCount;
            bool prefabLoaded,raceDataLoaded,visualDependenciesLoaded,audioDependenciesLoaded;
            using(spawnDependencies.Auto())
            {
            prefabLoaded = fromBuildings || (playerActor
                ? Metin2PlayerPrefabs.IsLoaded(playerPrefabs, data.Race)
                : Metin2RacePrefabs.IsLoaded(raceCatalog, data.Race));
            raceDataLoaded = Metin2RaceDataBinder.IsLoaded(data.Race);
            visualDependenciesLoaded = raceDataLoaded && Metin2RaceDataBinder.AreVisualEffectsLoaded(data.Race);
            audioDependenciesLoaded = playerActor
                ? Metin2SoundManager.Instance.IsPlayerMotionAudioLoaded(data.Race)
                : raceDataLoaded && Metin2RaceDataBinder.AreMotionSoundsLoaded(data.Race);
            }
            if (!prefabLoaded || !raceDataLoaded ||
                (!visualDependenciesAttempted && (!audioDependenciesLoaded || !visualDependenciesLoaded)))
            {
                var pending = new DeferredSpawn { Data = data };
                deferredSpawns[data.Vid] = pending;
                AcquireAndSpawnAsync(pending, spawnLifetime.Token);
                return;
            }
            GameObject prefab = fromBuildings ? buildingPrefab : null;
            if (prefab == null)
                prefab = data.Type == 6 && data.Race < Metin2PlayerPrefabs.JobCount ? Metin2PlayerPrefabs.Get(playerPrefabs, data.Race) : null;
            if (prefab == null && !Metin2RacePrefabs.TryGet(raceCatalog, data.Race, out prefab))
            {
                // Race ids that resolve to no prefab are the usual reason an NPC or monster never
                // appears. Reporting the type and the catalog state separates a missing conversion
                // from an unbound catalog.
                missingSources++;
                Debug.LogWarning($"METIN2_ENTITY_SOURCE_MISSING vid={data.Vid} race={data.Race} type={data.Type} " +
                                 $"catalog={(raceCatalog == null ? "null" : "bound")} total={missingSources}");
                return;
            }
            // Entities that arrive before the map is staged would be placed with the raw fallback
            // conversion, which lands them far outside the loaded tiles.
            if (worldRuntime == null || !worldRuntime.HasMap)
                Debug.LogWarning($"METIN2_ENTITY_NO_MAP vid={data.Vid} race={data.Race} - placed with fallback coordinates");
            // The eight converted player meshes face local +X, exactly like the local-player
            // controller documents and corrects with -90 degrees. NPC/monster prefabs do not share
            // that model frame, so a single scene-wide entityYawOffset cannot be correct for both.
            float actorModelYawOffset = playerActor ? -90f : entityYawOffset;
            GameObject actor,instance;
#if UNITY_STANDALONE_WIN
            PreparedVisual prepared=TakePreparedVisual(data.Race,data.Type,prefab);
#endif
            using(spawnInstantiate.Auto())
            {
#if UNITY_STANDALONE_WIN
                actor=prepared!=null?prepared.actor:new GameObject($"Net_{data.Vid}_{prefab.name}");
#else
                actor=new GameObject($"Net_{data.Vid}_{prefab.name}");
#endif
                actor.name=$"Net_{data.Vid}_{prefab.name}";actor.transform.SetParent(entityRoot != null ? entityRoot : transform, false); actor.transform.SetPositionAndRotation(ToUnity(data.X, data.Y, data.Z), Metin2CoordinateSpace.ActorRotation(data.Angle, actorModelYawOffset));
#if UNITY_STANDALONE_WIN
                if(prepared!=null)
                {
                    using(preparedVisualHit.Auto())
                    {
                        instance=prepared.view;instance.transform.SetParent(actor.transform,false);
                        actor.SetActive(true);prepared.animator.keepAnimatorStateOnDisable=prepared.keepState;
                    }
                }
                else
#endif
                    instance = Instantiate(prefab, actor.transform);
                instance.name = "Visual_" + prefab.name;
#if UNITY_STANDALONE_WIN
                if(prepared!=null){instance.transform.localPosition=prepared.position;instance.transform.localRotation=prepared.rotation;instance.transform.localScale=prepared.scale;}
                else
#endif
                {instance.transform.localPosition = Vector3.zero; instance.transform.localRotation = Quaternion.identity;}
            }
            using(spawnPose.Auto())
            {
#if UNITY_STANDALONE_WIN
            if(prepared==null)
#endif
                InitializeSpawnPose(instance,data.Race);
            }
            Metin2NetworkEntity entity;
            using(spawnEntitySetup.Auto())
            {
#if UNITY_STANDALONE_WIN
            entity=prepared!=null?prepared.entity:actor.AddComponent<Metin2NetworkEntity>();
#else
            entity=actor.AddComponent<Metin2NetworkEntity>();
#endif
            entity.ModelYawOffset = actorModelYawOffset;
            using(spawnInitialize.Auto())
                entity.Initialize(data.Vid, data.Race, data.Type, data.MoveSpeed, data.AttackSpeed, equipmentCatalog, raceCatalog, instance.transform, itemCatalog);
            using(spawnTextTail.Auto())
            {
#if UNITY_STANDALONE_WIN
            bool reusableHeight=cacheSpawnTailHeight&&!playerActor&&(data.Type==0||data.Type==1||data.Type==2)&&Vector3.Dot(actor.transform.up,Vector3.up)>.99999f;
            var heightKey=(prefab,data.Race,data.Type,actor.transform.parent!=null?actor.transform.parent.lossyScale:Vector3.one);
            float? knownHeight=reusableHeight&&spawnTailHeights.TryGetValue(heightKey,out float savedHeight)?savedHeight:(float?)null;
            if(knownHeight.HasValue){using(spawnHeightHit.Auto())entity.BindTextTail(textTailLocale, LocalPlayerRoot, ResolveGuildName(entity.GuildId),knownHeight);}
            else entity.BindTextTail(textTailLocale, LocalPlayerRoot, ResolveGuildName(entity.GuildId));
            if(reusableHeight&&!knownHeight.HasValue&&entity.ScreenTextTail!=null)
                spawnTailHeights[heightKey]=entity.ScreenTextTail.BaseTopOffset;
#else
            entity.BindTextTail(textTailLocale, LocalPlayerRoot, ResolveGuildName(entity.GuildId));
#endif
            }
            using(spawnName.Auto())
            {
            entity.BindNameColorResolver(ResolveTextTailNameColor);
            bool tableName = data.TransName && data.Type == Metin2GamePacketCodec.EntityTypeNpc;
            if ((tableName || string.IsNullOrEmpty(entity.DisplayName)) && Metin2MobNames.TryGetName(data.Race, out string mobName))
                entity.SetDisplayName(mobName, true);
            }
            entity.BindMotionWorld(this, worldRuntime);
            }
            using(spawnRaceBinding.Auto())
                Metin2RaceDataBinder.Bind(entity, instance.transform, data.Race, data.Type, data.State);
            using(spawnFinalize.Auto())
            {
            entity.ApplySpawnStateFlags(data.State);
            if (actor.GetComponent<Metin2MotionSoundHandler>() == null)
                actor.AddComponent<Metin2MotionSoundHandler>();
            entities[data.Vid] = entity;
            if(data.Type==0)AttachFlight(actor,data.Vid,null);
            if (actorAdditional.TryGetValue(data.Vid, out var earlyAdditional)) entity.ApplyAdditional(earlyAdditional);
            if (actorUpdates.TryGetValue(data.Vid, out var earlyUpdate)) entity.ApplyUpdate(earlyUpdate);
            if (clientPlatforms.TryGetValue(data.Vid, out byte platform)) entity.SetClientPlatform(platform);
            EntityRegistered?.Invoke(entity);
            entity.ApplySpawnAffects(data.Affect0, data.Affect1);
            }
        }
        static void InitializeSpawnPose(GameObject instance,ushort race)
        {
            // Remote actors come from the same centimetre-scaled prefabs as the local player.
            Metin2.Runtime.Metin2Units.ApplyCharacterScale(instance);
            // The catalog may contain a standalone horse actor. Its legacy rig needs the same
            // model-derived axis restoration as the mounted presentation, before grounding.
            Metin2.Runtime.Metin2Units.AlignLegacyHorseAxes(instance.transform);
            Animator standaloneHorseAnimator = instance.GetComponentInChildren<Animator>(true);
            if (standaloneHorseAnimator != null)
            {
                int waitHash = Animator.StringToHash("00");
                if (standaloneHorseAnimator.HasState(0, waitHash)) standaloneHorseAnimator.Play(waitHash, 0, 0f);
                standaloneHorseAnimator.Update(.11f);
            }
            Metin2.Runtime.Metin2Units.LevelLegacyHorseStance(instance.transform);
            // The GR2 -> SMD -> FBX conversion did not give every rig the same model frame, and one
            // scene-wide entityYawOffset cannot express more than one of them: a large part of the
            // converted actors face local +X rather than +Z (they walk sideways), and a few are
            // turned about a horizontal axis instead (giant_scorpion stands on its claws,
            // blue_dragon/water_dragon lie on their side). The correction is visual only - it is put
            // on the mesh instance, never on the actor transform - so packet headings,
            // ModelYawOffset, attack direction and collision keep the untouched logical frame, and
            // the grounding pass below still measures the corrected pose. It runs AFTER
            // AlignLegacyHorseAxes/LevelLegacyHorseStance and turns around the actor origin, so a
            // legacy horse rig keeps the ground frame those two put on it.
            Metin2.Runtime.Metin2ActorOrientation.Apply(instance.transform, race);
            // Same pivot correction the local player uses: the converted meshes start about 0.95 m
            // above their origin, so without this the actors hover over the ground.
            // Native GR2 rigs already use the authored terrain origin (including deliberately
            // buried stone bases). The legacy smallest-submesh correction sinks multipart native
            // actors and shifts their bone-attached collision spheres away from the original.
            float footOffset;
            if (instance.GetComponent<Metin2.Runtime.Metin2NativeActor>() != null || instance.GetComponent<Metin2.Runtime.Metin2MetricActor>() != null) footOffset = 0f;
            else if (raceFootOffsets.TryGetValue(race, out footOffset)) { }
            else { footOffset = Metin2.Runtime.Metin2Units.MeasureFootOffset(instance); raceFootOffsets[race] = footOffset; }
            if (footOffset != 0f)
            {
                Vector3 groundedPosition = instance.transform.localPosition;
                groundedPosition.y = -footOffset;
                instance.transform.localPosition = groundedPosition;
            }
        }

        private async void AcquireAndSpawnAsync(DeferredSpawn pending, CancellationToken token)
        {
            Metin2GamePacketCodec.EntityAdd data = pending.Data;
            try
            {
                Task prefab = data.Type == 6 && data.Race < Metin2PlayerPrefabs.JobCount
                    ? Metin2PlayerPrefabs.PreloadAsync(data.Race, Metin2ContentScope.Session, token)
                    : Metin2RacePrefabs.PreloadAsync(data.Race, Metin2ContentScope.Map, token);
                await Task.WhenAll(prefab, Metin2RaceDataBinder.PreloadAsync(data.Race, Metin2ContentScope.Map, token));
                // Companion visibility must depend on its model/motions, never on an optional
                // audio/effect request still draining from the old map.
                Task cosmetics = WarmEntityCosmeticsAsync(data, token);
                bool companion = Metin2RaceDataBinder.TryGetEntry(data.Race, out var raceData) &&
                    raceData.descriptor != null && raceData.descriptor.StartsWith("npc_pet/", StringComparison.OrdinalIgnoreCase);
                if (!companion) await cosmetics;
                token.ThrowIfCancellationRequested();
                if (this != null && deferredSpawns.TryGetValue(data.Vid, out DeferredSpawn current) && ReferenceEquals(current, pending))
                {
                    byte[][] following = current.FollowingPackets.ToArray();
                    deferredSpawns.Remove(data.Vid);
                    UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.SpawnAfterLoad");
                    try { Spawn(current.Data, true); }
                    finally { UnityEngine.Profiling.Profiler.EndSample(); }
                    UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.ReplayDeferredPackets");
                    try { foreach (byte[] packet in following) HandlePacket(packet); }
                    finally { UnityEngine.Profiling.Profiler.EndSample(); }
                }
            }
            catch (OperationCanceledException) { if (deferredSpawns.TryGetValue(data.Vid, out DeferredSpawn current) && ReferenceEquals(current, pending)) deferredSpawns.Remove(data.Vid); }
            catch (Exception e)
            {
                if (deferredSpawns.TryGetValue(data.Vid, out DeferredSpawn current) && ReferenceEquals(current, pending))
                { deferredSpawns.Remove(data.Vid); Debug.LogWarning($"METIN2_ENTITY_CONTENT_LOAD_FAILED vid={data.Vid} race={data.Race} {e.Message}"); }
            }
        }

        private async Task WarmEntityCosmeticsAsync(Metin2GamePacketCodec.EntityAdd data, CancellationToken token)
        {
                try
                {
                    if (data.Type == 6 && data.Race < Metin2PlayerPrefabs.JobCount)
                        await Metin2SoundManager.Instance.PreloadPlayerMotionAudioAsync(data.Race, Metin2ContentScope.Map, token);
                    else
                        await Metin2RaceDataBinder.PreloadMotionSoundsAsync(data.Race, Metin2ContentScope.Map, token);
                }
                catch (OperationCanceledException) { return; }
                catch (Exception soundException)
                {
                    Debug.LogWarning($"METIN2_ENTITY_SOUND_PRELOAD_FAILED vid={data.Vid} race={data.Race} {soundException.Message}");
                }
                try
                {
                    if (data.Type == 6 && data.Race < Metin2PlayerPrefabs.JobCount)
                        await Metin2RaceDataBinder.PreloadPlayerVisualEffectsAsync(
                            (int)data.Race, Metin2ContentScope.Map, token);
                    else
                        await Metin2RaceDataBinder.PreloadVisualEffectsAsync(
                            data.Race, Metin2ContentScope.Map, token);
                }
                catch (OperationCanceledException) { return; }
                catch (Exception effectException)
                {
                    // A cosmetic definition must never make the actor itself disappear. Spawn once
                    // after the preload attempt; the existing per-effect diagnostics will identify
                    // any genuinely absent source.
                    Debug.LogWarning($"METIN2_ENTITY_EFFECT_PRELOAD_FAILED vid={data.Vid} race={data.Race} {effectException.Message}");
                }
                if (!token.IsCancellationRequested && this != null && entities.TryGetValue(data.Vid, out var entity) &&
                    entity != null && entity.Race == data.Race && Metin2RaceDataBinder.TryGetEntry(data.Race, out var readyData) &&
                    readyData.descriptor != null && readyData.descriptor.StartsWith("npc_pet/", StringComparison.OrdinalIgnoreCase))
                    Metin2RaceDataBinder.ApplyAttachedEffects(entity.gameObject, entity.transform, readyData, entity.Type, true);
        }
        private readonly HashSet<GameObject> fadingActors=new HashSet<GameObject>();
        private void Despawn(uint vid,bool fade=false)
        {
            woodcutSnapshots.Remove(vid);
            herbSnapshots.Remove(vid);
            actorAdditional.Remove(vid); actorUpdates.Remove(vid);
            // A queued ADD for a despawned vid must not resurrect the actor later.
            cancelledVids.Add(vid);
            pendingSpawnPackets.Remove(vid);
            long sample = Metin2.Runtime.Metin2FrameProfiler.Begin();
            try { DespawnBody(vid,fade); }
            finally { Metin2.Runtime.Metin2FrameProfiler.End(Metin2.Runtime.Metin2FrameProfiler.SlotEntityDespawn, sample); }
        }

        private void DespawnBody(uint vid,bool fade)
        {
            deferredSpawns.Remove(vid);clientPlatforms.Remove(vid);
            if(!entities.TryGetValue(vid,out Metin2NetworkEntity entity))return;
            entities.Remove(vid);EntityRemoved?.Invoke(vid);
            if(entity==null)return;
            if(fade&&Application.isPlaying&&(entity.Type==0||entity.Type==2))
            {
                entity.BeginDeletePresentation();fadingActors.Add(entity.gameObject);
                StartCoroutine(FinishDeletePresentation(entity.gameObject));
            }
            else Destroy(entity.gameObject);
        }
        private System.Collections.IEnumerator FinishDeletePresentation(GameObject actor)
        {
            // Original UpdateDeleting subtracts elapsed * 1.5 alpha per second.
            yield return new WaitForSeconds(1f/1.5f);
            fadingActors.Remove(actor);if(actor!=null)Destroy(actor);
        }

        private static byte DevicePlatform => Application.platform == RuntimePlatform.Android ? (byte)1 :
            Application.platform == RuntimePlatform.IPhonePlayer ? (byte)2 : (byte)0;
        public byte LocalPlatform { get; private set; } = DevicePlatform;

        public void SetClientPlatform(uint vid, byte platform)
        {
            if (platform > 2) return;
            clientPlatforms[vid] = platform;
            if (IsLocalPlayer(vid)) { LocalPlatform = platform; LocalTextTailChanged?.Invoke(); }
            else if (entities.TryGetValue(vid, out Metin2NetworkEntity actor) && actor != null)
                actor.SetClientPlatform(platform);
        }

        private void OnDestroy() { spawnLifetime.Cancel(); spawnLifetime.Dispose(); }
        private void Move(Metin2GamePacketCodec.EntityMove data)
        {
            if (Metin2.Runtime.CombatProof.Enabled &&
                (data.Function == 2 || data.Function == 3 || (data.Function & 0x80) != 0))
                Debug.Log($"METIN2_COMBAT_PROOF_REMOTE_MOVE vid={data.Vid} function={data.Function} argument={data.Argument} duration={data.Duration}");
            if (entities.TryGetValue(data.Vid, out Metin2NetworkEntity entity))
            {
                Vector3 position=ToUnity(data.X,data.Y,0);
                var flight=entity.GetComponent<Metin2MountFlight>();if(flight!=null&&flight.Flying)position.y=flight.Altitude;
                if(mobPositionTrace&&entity.Type==0&&(data.Function==2||data.Function==3||data.Function==4||(data.Function&0x80)!=0))
                {
                    Vector3 error=entity.transform.position-position;error.y=0;
                    Vector3 visualToPlayer=LocalPlayerRoot!=null?entity.transform.position-LocalPlayerRoot.position:Vector3.zero;visualToPlayer.y=0;
                    Vector3 packetToPlayer=LocalPlayerRoot!=null?position-LocalPlayerRoot.position:Vector3.zero;packetToPlayer.y=0;
                    Debug.Log($"METIN2_MOB_MOVE_ATTACK_POSITION vid={entity.Vid} race={entity.Race} errorM={error.magnitude:F3} visualDistanceToLocalM={visualToPlayer.magnitude:F3} packetDistanceToLocalM={packetToPlayer.magnitude:F3}");
                }
                entity.QueueMove(position, data.Rotation * 5f,
                    data.Duration * 0.001f, data.Function, data.Argument,
                    data.Time, ServerTimeMilliseconds);
            }
        }
        private void Attack(Metin2GamePacketCodec.EntityAttack data)
        {
            if(!entities.TryGetValue(data.AttackerVid,out Metin2NetworkEntity entity))return;
            entities.TryGetValue(data.VictimVid,out Metin2NetworkEntity victim);
            if((Metin2Diagnostics.Verbose||mobPositionTrace)&&entity.Type==0&&victim!=null)
            {
                Vector3 actual=entity.transform.position-victim.transform.position;actual.y=0;
                Vector3 estimated=entity.EstimatedServerPosition(ServerTimeMilliseconds)-victim.transform.position;estimated.y=0;
                Debug.Log($"METIN2_MOB_POSITION_AT_ATTACK vid={entity.Vid} race={entity.Race} visualDistanceM={actual.magnitude:F3} estimatedPacketDistanceM={estimated.magnitude:F3}");
            }
            entity.ApplyAttack(victim!=null?victim.transform:null,data.VictimVid,data.Type);
        }
        static readonly bool mobPositionTrace=Array.Exists(Environment.GetCommandLineArgs(),value=>value=="-metin2-mob-position-check");
        internal static bool MobPositionTraceEnabled=>mobPositionTrace;
        private void Stun(uint vid)
        {
            // RecvStunPacket calls Die() when the stunned VID is the main actor. The main player has
            // no Metin2NetworkEntity wrapper in Unity, so carry the same non-actionable/dead state
            // through LocalStateFlags for __CanUseSkill. A later authoritative character update can
            // clear it again on revive.
            if (IsLocalPlayer(vid)) { ApplyLocalStateFlags((byte)(LocalStateFlags | 1)); return; }
            if (entities.TryGetValue(vid, out Metin2NetworkEntity entity)) entity.ApplyStun();
        }
        private void ChangeSpeed(Metin2GamePacketCodec.EntitySpeed data) { if (entities.TryGetValue(data.Vid, out Metin2NetworkEntity entity)) entity.ApplySpeed(data.MovingSpeed); }
        private void Motion(Metin2GamePacketCodec.EntityMotion data) { if (entities.TryGetValue(data.Vid, out Metin2NetworkEntity entity)) entity.ApplyMotion(data.Motion, data.VictimVid); }
        public bool ApplyDigMotion(uint vid, uint targetVid, byte count)
        {
            if (!entities.TryGetValue(vid, out Metin2NetworkEntity entity) || entity == null) return false;
            entity.ApplyDig(entities.TryGetValue(targetVid, out Metin2NetworkEntity target) ? target.transform : null, count); return true;
        }
        public void ApplyOwnership(Metin2GamePacketCodec.Ownership ownership)
        {
            if (entities.TryGetValue(ownership.VictimVid, out Metin2NetworkEntity victim) && victim != null) victim.ApplyOwner(ownership.OwnerVid);
        }

        /// <summary>Raised for every GC_CHARACTER_POSITION (43) with the authoritative sit/stand
        /// state, so the actor presentation layer can react without re-parsing the packet.</summary>
        public event Action<uint, byte> PositionStateChanged;
        /// <summary>The local player's sit/stand state, mirroring the entity property.</summary>
        public byte LocalPositionState { get; private set; }
        public void ApplyPositionState(Metin2GamePacketCodec.CharacterPosition position)
        {
            if (IsLocalPlayer(position.Vid)) LocalPositionState = position.Position;
            else if (entities.TryGetValue(position.Vid, out Metin2NetworkEntity entity) && entity != null) entity.ApplyPosition(position.Position);
            PositionStateChanged?.Invoke(position.Vid, position.Position);
        }
        /// <summary>
        /// CInstanceBase::CheckAdvancing (InstanceBaseBattle.cpp:469-545) - the client's own movement
        /// blocking, run for every character every frame from CPythonCharacterManager (:277).
        ///
        /// This is the function that stops you walking through a monster, and it is NOT the one I
        /// reached for first. AdjustCollisionWithOtherObjects skips IsPC/IsNPC/IsEnemy, but that is a
        /// separate physics-blend path; CheckAdvancing skips nobody:
        ///
        ///   if (CanSkipCollision()) return FALSE;
        ///   if (bUsingSkill and not each.IsDoor()) continue;      during a skill only doors block
        ///   if (TestActorCollision(each))
        ///       if (++count == 2) { BlockMovement(); return TRUE; }   two at once: stop at once
        ///       AdjustDynamicCollisionMovement(each);                 otherwise try to slide past
        ///       if (TestActorCollision(each)) { BlockMovement(); return TRUE; }
        ///       else NEW_MoveToDestPixelPositionDirection(...);       slid free, carry on
        ///
        /// So the character slides along an obstacle rather than stopping dead against it, and only
        /// gives up when sliding still leaves it overlapping, or when two actors trap it at once.
        /// </summary>
        // Compatibility overload for parity/editor validators that only need the boolean
        // collision result. Runtime movement callers use the overload below so they can
        // consume the original-style slide-adjusted destination.
        public bool TryAdvance(Vector3 from, Vector3 to, uint movingVid,
                               Metin2.Runtime.Metin2CollisionProfile movingProfile,
                               int collisionType, bool usingSkill)
        {
            Vector3 adjusted;
            return TryAdvance(from, to, movingVid, movingProfile, collisionType, usingSkill, out adjusted);
        }

        public bool TryAdvance(Vector3 from, Vector3 to, uint movingVid,
                               Metin2.Runtime.Metin2CollisionProfile movingProfile,
                               int collisionType, bool usingSkill, out Vector3 adjusted)
        {
            adjusted = to;
            Metin2NetworkEntity first = null;
            int hits = 0;

            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                Metin2NetworkEntity entity = pair.Value;
                if (entity == null || pair.Key == movingVid) continue;
                // TestActorCollision's very first line: a dead actor never collides
                // (ActorInstanceCollisionDetection.cpp:588-589). A corpse still lying on the ground
                // must be walkable, and without this it kept blocking until it despawned.
                if (entity.IsDead) continue;
                if (!entity.BlocksMovementAsOriginalActor) continue;
                // ECharTypes 4 is the door. Everything else is ignored while a skill is playing.
                if (usingSkill && entity.Type != 4) continue;
                if (!Overlaps(from, to, movingProfile, collisionType, entity)) continue;

                hits++;
                // Two at once and the original stops immediately, without trying to slide.
                if (hits == 2) return false;
                first = entity;
            }

            if (first == null) return true;

            // CActorInstance::AdjustDynamicCollisionMovement hard-stops authored static actor
            // classes instead of applying the monster/player slide correction. entity.Type uses
            // the matching ECharTypes values: stone=2, door=4, building=5. An attack/wait path
            // tests DEFENDING spheres; native isAttacking returns without adjusting and the second
            // TestActorCollision consequently blocks the movement as well.
            if (first.Type == 2 || first.Type == 4 || first.Type == 5 ||
                collisionType == Metin2.Runtime.Metin2CollisionProfile.CollisionTypeDefending)
                return false;

            // AdjustDynamicCollisionMovement: keep the part of the movement that runs along the
            // obstacle and drop the part that pushes into it.
            Vector3 away = to - first.transform.position;
            away.y = 0f;
            if (away.sqrMagnitude < 0.000001f) return false;
            away.Normalize();
            Vector3 travel = to - from;
            travel.y = 0f;
            Vector3 slid = from + (travel - away * Mathf.Min(0f, Vector3.Dot(travel, away)));

            foreach (KeyValuePair<uint, Metin2NetworkEntity> pair in entities)
            {
                Metin2NetworkEntity entity = pair.Value;
                if (entity == null || pair.Key == movingVid || entity.IsDead) continue;
                if (!entity.BlocksMovementAsOriginalActor) continue;
                if (usingSkill && entity.Type != 4) continue;
                if (Overlaps(from, slid, movingProfile, collisionType, entity)) return false;
            }

            adjusted = new Vector3(slid.x, to.y, slid.z);
            return true;
        }

        /// <summary>
        /// TestActorCollision's body spheres, reduced to the horizontal circle that decides whether
        /// a step is refused. The 800-unit early out is kept: it is what stops this walking the whole
        /// character list at full cost.
        /// </summary>
        private static bool Overlaps(Vector3 from, Vector3 position,
                                     Metin2.Runtime.Metin2CollisionProfile movingProfile,
                                     int collisionType, Metin2NetworkEntity entity)
        {
            Vector3 centre = entity.transform.position;
            Vector3 delta = centre - position;
            delta.y = 0f;
            if (delta.sqrMagnitude > 64f) return false;   // 8 m, the port's scale of 800 units
            if (movingProfile == null || entity.CollisionProfile == null) return false;
            return movingProfile.WouldCollideAt(
                collisionType, from, position, entity.CollisionProfile, entity.transform.position);
        }

#if UNITY_EDITOR || DEVELOPMENT_BUILD
        public string DescribeMovementObstacles(Vector3 from, Vector3 to, uint movingVid,
            Metin2.Runtime.Metin2CollisionProfile movingProfile)
        {
            var obstacles = new System.Text.StringBuilder();
            foreach (var pair in entities)
            {
                var entity = pair.Value;
                if (entity == null || pair.Key == movingVid || entity.IsDead ||
                    !Overlaps(from, to, movingProfile, Metin2.Runtime.Metin2CollisionProfile.CollisionTypeBody, entity)) continue;
                if (obstacles.Length > 0) obstacles.Append('|');
                obstacles.Append($"vid={pair.Key},race={entity.Race},type={entity.Type},pos={entity.transform.position:F3}");
            }
            return obstacles.ToString();
        }
#endif

        private int syncSeen, syncMine, syncApplied;
        private float lastSyncReport;

        public void ApplySyncPositions(Metin2GamePacketCodec.SyncPosition[] positions)
        {
            if (positions == null) return;
            // Reported once a second, not per packet. Three things can silently break this path and
            // the counts separate them: no packet at all, a vid that never matches LocalVid, or a
            // missing LocalPlayerRoot.
            syncSeen += positions.Length;
            if (Time.time - lastSyncReport >= 1f)
            {
                lastSyncReport = Time.time;
                Metin2MotionDiagnostics.Log(
                    $"METIN2_SYNC seen={syncSeen} mine={syncMine} applied={syncApplied} " +
                    $"localVid={LocalVid} root={(LocalPlayerRoot != null)}");
                syncSeen = syncMine = syncApplied = 0;
            }
            foreach (Metin2GamePacketCodec.SyncPosition position in positions)
            {
                if (entities.TryGetValue(position.Vid, out Metin2NetworkEntity entity) && entity != null)
                {
                    entity.ApplySyncPosition(ToUnity(position.X, position.Y, 0));
                    continue;
                }
                // The local player is not in the entity dictionary, so its own correction fell on the
                // floor. SyncActor applies to whatever VID the packet names, the main character
                // included (NetworkActorManager.cpp:547-566).
                if (position.Vid == 0 || position.Vid != LocalVid || LocalPlayerRoot == null) continue;
                syncMine++;
                var controller = LocalPlayerRoot.GetComponent<Metin2PlayerController>()
                                 ?? LocalPlayerRoot.GetComponentInParent<Metin2PlayerController>();
                if (controller != null) { controller.SyncPosition(ToUnity(position.X, position.Y, 0)); syncApplied++; }
            }
        }
        // GC_CHARACTER_ADDITIONAL carries no affect flags - only ADD and UPDATE do - so this one is
        // left alone.
        /// <summary>
        /// The local player has no Metin2NetworkEntity, so its own ADDITIONAL packet used to fall
        /// through here and its parts - the hairstyle among them - were lost.
        /// </summary>
        public event System.Action<ushort[]> LocalPartsChanged;
        public event System.Action<uint> LocalMountChanged;
        public event System.Action<ushort, ushort> LocalActorSpeedsChanged;
        public event System.Action LocalTextTailChanged;
        public event System.Action LocalSkillColorsChanged;
        public uint LocalMountVnum { get; private set; }
        public ushort LocalMoveSpeed { get; private set; } = 100;
        public ushort LocalAttackSpeed { get; private set; } = 100;
        /// <summary>Authoritative relationship fields from the local ADDITIONAL/UPDATE packets.</summary>
        public bool LocalIdentityKnown { get; private set; }
        public byte LocalEmpire { get; private set; }
        public uint LocalGuildId { get; private set; }
        public byte LocalPkMode { get; private set; }
        public short LocalAlignment { get; private set; }
        /// <summary>
        /// The wire's uint32 alignment value before the short narrowing. CInstanceBase::m_sAlignment
        /// is a uint32_t and the rank tooltip shows the full point value (up to 5,000,000), so the
        /// short copy only serves the legacy text-tail colour callers.
        /// </summary>
        public uint LocalAlignmentRaw { get; private set; }
        public string LocalDisplayName { get; private set; } = string.Empty;
        public uint LocalLevel { get; private set; }
        public string LocalLanguage { get; private set; } = string.Empty;
        public ushort[] LocalParts { get; private set; } = Array.Empty<ushort>();
        public byte LocalStateFlags { get; private set; }
        public ulong LocalAffectFlags { get; private set; }
        public event System.Action<ulong> LocalAffectFlagsChanged;
        public bool LocalIsKiller => (LocalStateFlags & (1 << 3)) != 0;

        private void ApplyLocalStateFlags(byte state)
        {
            bool wasDead = (LocalStateFlags & 1) != 0;
            LocalStateFlags = state;
            localTextTailActor?.ApplyStateFlags(state);
            bool isDead = (state & 1) != 0;
            if (wasDead != isDead) LocalLifeStateChanged?.Invoke(isDead);
        }

        private void ApplyLocalMount(uint mountVnum)
        {
            // The mount field is repeated in ordinary character updates, including the affect-only
            // update sent after a buff.  Do not turn an unchanged field into a locomotion replay.
            if (LocalMountVnum == mountVnum) return;
            LocalMountVnum = mountVnum;
            LocalMountChanged?.Invoke(mountVnum);
        }

        private void ApplyLocalActorSpeeds(ushort moveSpeed, ushort attackSpeed)
        {
            if (LocalMoveSpeed == moveSpeed && LocalAttackSpeed == attackSpeed) return;
            LocalMoveSpeed = moveSpeed;
            LocalAttackSpeed = attackSpeed;
            LocalActorSpeedsChanged?.Invoke(moveSpeed, attackSpeed);
        }

        private void Additional(Metin2GamePacketCodec.EntityAdditional data)
        {
            actorAdditional[data.Vid] = data;
            if (entities.TryGetValue(data.Vid, out Metin2NetworkEntity entity)) { entity.ApplyAdditional(data); entity.SetGuildName(ResolveGuildName(data.GuildId)); return; }
            if (!IsLocalPlayer(data.Vid)) return;
            LocalIdentityKnown = true;
            LocalEmpire = data.Empire;
            LocalGuildId = data.GuildId;
            LocalPkMode = data.PkMode;
            LocalAlignment = data.Alignment;
            LocalAlignmentRaw = data.AlignmentRaw;
            LocalDisplayName = data.Name ?? string.Empty;
            LocalLevel = data.Level;
            LocalLanguage = data.Language ?? string.Empty;
            ApplyLocalMount(data.MountVnum);
            StoreLocalParts(data.Parts);
            ApplyLocalSkillColors(data.SkillColors);
            LocalTextTailChanged?.Invoke();
            RefreshTextTailColors();
        }

        private void UpdateEntity(Metin2GamePacketCodec.EntityUpdate data)
        {
            UnityEngine.Profiling.Profiler.BeginSample(IsLocalPlayer(data.Vid)
                ? "Metin2.Entity.LocalUpdate" : "Metin2.Entity.RemoteUpdate");
            try { UpdateEntityCore(data); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); }
        }

        private void UpdateEntityCore(Metin2GamePacketCodec.EntityUpdate data)
        {
            actorUpdates[data.Vid] = data;
            if (entities.TryGetValue(data.Vid, out Metin2NetworkEntity entity)) { entity.ApplyUpdate(data); entity.SetGuildName(ResolveGuildName(data.GuildId)); return; }
            if (!IsLocalPlayer(data.Vid)) return;
            LocalIdentityKnown = true;
            LocalGuildId = data.GuildId;
            LocalPkMode = data.PkMode;
            LocalAlignment = data.Alignment;
            LocalAlignmentRaw = data.AlignmentRaw;
            ApplyLocalStateFlags(data.State);
            ApplyLocalActorSpeeds(data.MoveSpeed, data.AttackSpeed);
            ApplyLocalAffects(data.Affect0, data.Affect1);
            ApplyLocalMount(data.MountVnum);
            StoreLocalParts(data.Parts);
            ApplyLocalSkillColors(data.SkillColors);
            LocalTextTailChanged?.Invoke();
            RefreshTextTailColors();
        }

        /// <summary>The local player's dwSkillColor block from GC 7/14 (CInstanceBase::ChangeSkillColor).</summary>
        public uint[][] LocalSkillColors { get; private set; } = BuildEmptySkillColorTable();

        public static uint[][] BuildEmptySkillColorTable()
        {
            var colors = new uint[11][];
            for (int slot = 0; slot < 11; slot++) colors[slot] = new uint[5];
            return colors;
        }

        private void ApplyLocalSkillColors(uint[][] colors)
        {
            if (colors == null) return;
            Metin2SkillEffectColors.Apply(localPlayerRoot,colors);
            bool changed = false;
            for (int slot = 0; slot < 11 && slot < colors.Length; slot++)
            {
                if (colors[slot] == null) continue;
                for (int layer = 0; layer < 5 && layer < colors[slot].Length; layer++)
                    if (LocalSkillColors[slot][layer] != colors[slot][layer]) { LocalSkillColors[slot][layer] = colors[slot][layer]; changed = true; }
            }
            if (changed) LocalSkillColorsChanged?.Invoke();
        }

        /// <summary>
        /// The local player has no Metin2NetworkEntity - it is driven by Metin2PlayerController - so
        /// its own character packets fell through both handlers and its affect flags were lost. This
        /// is the one path that matters for the player's own sword aura.
        /// </summary>
        public Transform LocalPlayerRoot
        {
            get => localPlayerRoot;
            set
            {
                localPlayerRoot = value;
                Metin2SkillEffectColors.Apply(value,LocalSkillColors);
                foreach (Metin2NetworkEntity entity in entities.Values)
                    entity?.BindTextTailDistanceOrigin(value);
            }
        }
        private Transform localPlayerRoot;

        private void StoreLocalParts(ushort[] parts)
        {
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.StoreLocalParts");
            try { StoreLocalPartsCore(parts); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); }
        }

        private void StoreLocalPartsCore(ushort[] parts)
        {
            int count = parts?.Length ?? 0;
            bool unchanged = LocalParts.Length == count;
            for (int i = 0; unchanged && i < count; i++) unchanged = LocalParts[i] == parts[i];
            if (unchanged) return;
            LocalParts = parts == null ? Array.Empty<ushort>() : (ushort[])parts.Clone();
            LocalPartsChanged?.Invoke(LocalParts);
        }

        private void ApplyLocalAffects(uint affect0, uint affect1)
        {
            ulong packed = affect0 | ((ulong)affect1 << 32);
            if (LocalAffectFlags != packed)
            {
                LocalAffectFlags = packed;
                LocalAffectFlagsChanged?.Invoke(packed);
            }
            if (LocalPlayerRoot == null) return;
            localTextTailActor?.SetGameMaster((affect0 & 1u) != 0);
            var effects = LocalPlayerRoot.GetComponent<Metin2AffectEffects>();
            if (effects == null) effects = LocalPlayerRoot.gameObject.AddComponent<Metin2AffectEffects>();
            effects.IsMainInstance = true;
            effects.ApplyAffectFlags(affect0, affect1);
        }
        /// <summary>
        /// Converts a server position and drops it onto the terrain.
        ///
        /// The server sends z = 0 for ground actors, so using it directly buried every monster and NPC
        /// under a terrain that sits around 177 units up. The local player already went through
        /// GroundHeight; remote actors have to do the same or they are simply invisible.
        /// </summary>
        public Vector3 ServerPositionToWorld(int x,int y,int z=0)=>ToUnity(x,y,z);
        public bool TrySampleGroundHeight(Vector3 position,out float height){height=position.y;if(worldRuntime==null||!worldRuntime.HasMap)return false;height=worldRuntime.GroundHeight(position);return true;}
        public event Action<byte,int,uint> FlightRequested;
        readonly Dictionary<uint,byte[]> flightSnapshots=new Dictionary<uint,byte[]>();
        public Metin2MountFlight AttachFlight(GameObject actor,uint vid,Metin2PlayerController controller)
        {
            var flight=actor.GetComponent<Metin2MountFlight>()??actor.AddComponent<Metin2MountFlight>();
            flight.Bind(controller,worldRuntime,(action,altitude,time)=>FlightRequested?.Invoke(action,altitude,time));
            if(flightSnapshots.TryGetValue(vid,out var snapshot))flight.ApplyPacket(snapshot);
            return flight;
        }
        void ApplyFlightPacket(byte[] packet)
        {
            if(packet.Length!=11)throw new ArgumentException("Invalid mount flight snapshot");
            uint vid=BitConverter.ToUInt32(packet,1);flightSnapshots[vid]=(byte[])packet.Clone();
            if(TryResolveActor(vid,out var root))root.GetComponent<Metin2MountFlight>()?.ApplyPacket(packet);
        }
        private Vector3 ToUnity(int x, int y, int z)
        {
            if (worldRuntime == null || !worldRuntime.HasMap) return new Vector3(x * 0.01f, z * 0.01f, -y * 0.01f);
            Vector3 local = worldRuntime.Current.ToLocal(x, y, z);
            local.y = worldRuntime.GroundHeight(local);
            return local;
        }
    }

    public sealed class Metin2NetworkEntity : MonoBehaviour
    {
        public uint Vid { get; private set; } public ushort Race { get; private set; } public byte Type { get; private set; }
        public Transform RaidVisualRoot => visualRoot;

        /// <summary>
        /// How far out this actor stops the player, derived from its authored MSM
        /// COLLISION_TYPE_BODY spheres.  The prior renderer-bounds approximation also enclosed
        /// weapons, armour and animation extremes and created the oversized invisible mob wall.
        /// An actor without BODY data has no invented client-side movement blocker, matching the
        /// original empty m_BodyPointInstanceList behaviour.
        /// </summary>
        public float BlockingRadius
        {
            get
            {
                if (blockingRadiusResolved) return blockingRadius;
                blockingRadiusResolved = true;
                Metin2.Runtime.Metin2CollisionProfile profile =
                    GetComponentInChildren<Metin2.Runtime.Metin2CollisionProfile>(true);
                if (profile != null && profile.TryGetPlanarRadius(
                        Metin2.Runtime.Metin2CollisionProfile.CollisionTypeBody, transform.position, out float authored))
                    blockingRadius = authored;
                return blockingRadius;
            }
        }
        private float blockingRadius;
        private bool blockingRadiusResolved;
        private bool raidScaleKnown, raidTintKnown;
        private Vector3 raidScaleBase;
        private Transform raidScaleTarget;
        private Color raidTint;
        private Renderer[] raidTintRenderers;
        private MaterialPropertyBlock[] raidTintOriginals;
        public void ApplyRaidScale(float factor)
        {
            if (Race < 62001 || Race > 62199 || (Type != 0 && Type != 1 && Type != 2)) return;
            if (!raidScaleKnown) { raidScaleKnown = true; raidScaleTarget = visualRoot != null ? visualRoot : transform; raidScaleBase = raidScaleTarget.localScale; }
            Vector3 desired = raidScaleBase * Mathf.Clamp(factor, .25f, 3f);
            if ((raidScaleTarget.localScale - desired).sqrMagnitude < .000001f) return;
            raidScaleTarget.localScale = desired; blockingRadiusResolved = false; InvalidatePickModel();
        }
        public void ApplyRaidTint(Color color)
        {
            if (Race < 62001 || Race > 62199 || (Type != 0 && Type != 1 && Type != 2) || (raidTintKnown && raidTint == color)) return;
            if (!raidTintKnown)
            {
                raidTintRenderers = GetComponentsInChildren<Renderer>(true);
                raidTintOriginals = new MaterialPropertyBlock[raidTintRenderers.Length];
                for (int i = 0; i < raidTintRenderers.Length; i++) { var original = new MaterialPropertyBlock(); raidTintRenderers[i].GetPropertyBlock(original); raidTintOriginals[i] = original; }
            }
            raidTintKnown = true; raidTint = color;
            for (int i = 0; i < raidTintRenderers.Length; i++)
            {
                if (raidTintRenderers[i] == null) continue;
                var block = new MaterialPropertyBlock(); raidTintRenderers[i].GetPropertyBlock(block);
                block.SetColor("_Color", color); block.SetColor("_BaseColor", color); block.SetColor("_TintColor", color);
                raidTintRenderers[i].SetPropertyBlock(block);
            }
        }
        private void ResetRaidPresentation()
        {
            if (raidScaleKnown && raidScaleTarget != null) raidScaleTarget.localScale = raidScaleBase;
            if (raidTintKnown) for (int i = 0; i < raidTintRenderers.Length; i++) if (raidTintRenderers[i] != null) raidTintRenderers[i].SetPropertyBlock(raidTintOriginals[i]);
            raidScaleKnown = raidTintKnown = false; raidScaleTarget = null; raidTintRenderers = null; raidTintOriginals = null;
            blockingRadiusResolved = false;
        }

        public Metin2.Runtime.Metin2CollisionProfile CollisionProfile
        {
            get
            {
                if (collisionProfile == null
#if UNITY_STANDALONE_WIN
                    && !collisionProfileResolved
#endif
                    )
                {
                    collisionProfile = GetComponentInChildren<Metin2.Runtime.Metin2CollisionProfile>(true);
#if UNITY_STANDALONE_WIN
                    collisionProfileResolved = true;
#endif
                }
                EnsureMissingDefendingSpheres();
                return collisionProfile;
            }
        }
        // Some imported custom bosses have a component but an empty sphere array.
        // Picking already falls back to model bounds; melee, splash and click reach must
        // share an actual defensive profile too. Never replace authored defensive data.
        private void EnsureMissingDefendingSpheres()
        {
            if (defendingProfileReady || (Type != 0 && Type != 2)) return;
            if (collisionProfile != null && collisionProfile.Spheres != null &&
                collisionProfile.Spheres.Any(s => s.collisionType == Metin2CollisionProfile.CollisionTypeDefending && s.radius > 0f))
            { defendingProfileReady = true; return; }
            if (!TryGetPickBounds(out Bounds bounds)) return; // model may still be loading
            float radius = Mathf.Min(bounds.extents.x, bounds.extents.z);
            if (radius <= .001f || float.IsNaN(radius) || float.IsInfinity(radius)) return;
            // Bounds picking has a small cursor allowance, not extra weapon reach.
            radius = Mathf.Max(.05f, radius - .075f);
            if (collisionProfile == null) collisionProfile = (visualRoot != null ? visualRoot.gameObject : gameObject).AddComponent<Metin2CollisionProfile>();
            float scale = Mathf.Max(Mathf.Abs(collisionProfile.transform.lossyScale.x),
                Mathf.Abs(collisionProfile.transform.lossyScale.y), Mathf.Abs(collisionProfile.transform.lossyScale.z));
            if (scale <= .000001f) return;
            var values = new List<Metin2CollisionSphere>(collisionProfile.Spheres ?? Array.Empty<Metin2CollisionSphere>());
            Vector3 centre = bounds.center;
            // Reach is tested from the character's feet. A tall empty-profile model must
            // not get an unreachable sphere halfway up its head or airborne ornaments.
            centre.y = Mathf.Clamp(centre.y, transform.position.y, transform.position.y + Mathf.Min(radius, 1f));
            bool alongX = bounds.extents.x >= bounds.extents.z;
            float halfLength = Mathf.Max(0f, (alongX ? bounds.extents.x : bounds.extents.z) - .075f - radius);
            int segments = Mathf.Clamp(Mathf.CeilToInt(halfLength / radius), 1, 8);
            for (int i = 0; i <= segments; i++)
            {
                Vector3 point = centre + (alongX ? Vector3.right : Vector3.forward) * Mathf.Lerp(-halfLength, halfLength, i / (float)segments);
                values.Add(new Metin2CollisionSphere { collisionType = Metin2CollisionProfile.CollisionTypeDefending,
                    radius = radius / scale, position = collisionProfile.transform.InverseTransformPoint(point), attachingBoneName = string.Empty });
            }
            collisionProfile.Configure(values.ToArray());
            recoveredDefendingProfile = true;
            defendingProfileReady = true;
            Debug.LogWarning($"METIN2_DEFENDING_PROFILE_RECOVERED vid={Vid} race={Race} spheres={segments + 1} source=model-body");
        }
        private bool defendingProfileReady;
        private bool recoveredDefendingProfile;
        private Metin2.Runtime.Metin2CollisionProfile collisionProfile;
        private bool collisionProfileResolved;
        private readonly List<Renderer> pickRenderers = new List<Renderer>();
        private bool pickRenderersResolved;
        private void InvalidatePickModel()
        {
            defendingProfileReady = false;
#if UNITY_STANDALONE_WIN
            pickRenderersResolved = false;
            Metin2.Runtime.Metin2ActorPresentationScheduler.InvalidateModel(this);
            collisionProfileResolved = false;
            collisionProfile = null;
            blockingRadiusResolved = false;
#endif
        }
        // Native actor defaults are 100%; validators and short-lived presentation helpers can
        // receive a motion before CHARACTER_ADD calls Initialize, so zero must not mean 1% speed.
        public ushort MoveSpeed { get; private set; } = 100;
        public ushort AttackSpeed { get; private set; } = 100;
        public uint OwnerVid { get; private set; }
        public float OwnerAge => OwnerVid == 0 ? float.PositiveInfinity : Time.time - ownerAssignedAt;
        private float ownerAssignedAt = -999f;
        public ushort CurrentMovingSpeed { get; private set; }
        public bool IsStunned { get; private set; }
        public bool IsDead { get; private set; }
        /// <summary>
        /// Auriga enables ENABLE_NO_COLLISION (Locale_inc.h). Native TestActorCollision
        /// ignores an unselected, nonattacking NPC/enemy/polymorph, except its five
        /// mandatory collision races. Stones, doors and ordinary players still block.
        /// </summary>
        public bool BlocksMovementAsOriginalActor
        {
            get
            {
                if (Race == 3970 || Race == 20387 || Race == 20388 || Race == 20385 || Race == 9424)
                    return true;
                if (clientNameTargeted) return true; // original EFFECT_TARGET presence
                if (motions != null && (motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Attack ||
                    motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.Combo ||
                    motions.LastPlayedDefinition?.events?.Any(e => e != null && e.type == 4) == true))
                    return true;
                return !(Type == 0 || Type == 1 || Type == 7 ||
                    (Type == 6 && Race >= Metin2PlayerPrefabs.JobCount));
            }
        }
        public uint LastVictimVid { get; private set; }
        public byte LastAttackType { get; private set; }
        public ushort LastMotion { get; private set; }
        public string DisplayName { get; private set; } public uint GuildId { get; private set; } public uint MountVnum { get; private set; }
        /// <summary>Sets the label text without touching the rest of the additional-packet state.</summary>
        private bool displayNameFromLocale;
        public void SetDisplayName(string value, bool localizedMobName = false) { displayNameFromLocale=localizedMobName; DisplayName = value; ApplyNameLabel(); }
        public void RefreshLocalizedText()
        {
            if(displayNameFromLocale&&Metin2MobNames.TryGetName(Race,out string name))DisplayName=name;
            ApplyNameLabel();
        }
        public byte ClientPlatform { get; private set; }
        public void SetClientPlatform(byte platform)
        {
            ClientPlatform = platform <= 2 ? platform : (byte)0;
            ScreenTextTail?.SetClientPlatform(ClientPlatform);
        }
        public void SetItemOnTitlePrefix(string value) => ScreenTextTail?.SetItemOnTitlePrefix(value);
        public void SetMountBeltCount(int mountCount, int beltCount)
            => ScreenTextTail?.SetMountBeltCount(mountCount, beltCount);
        public void SetGameMaster(bool value) => ScreenTextTail?.SetGameMaster(value);
        public byte PkMode { get; private set; }
        public byte StateFlags { get; private set; }
        public bool IsKiller => (StateFlags & (1 << 3)) != 0;
        public bool HasPartyStateFlag => (StateFlags & (1 << 4)) != 0;
        public bool HasAffectFlag(int index)
        {
            if (affectEffects == null) affectEffects = GetComponent<Metin2AffectEffects>();
            return affectEffects != null && affectEffects.IsAffectFlagSet(index);
        }
        TextMesh compatibilityNameLabel;
#if UNITY_STANDALONE_WIN
        static readonly bool lazyCompatibilityName=Metin2WindowsPerformance.Enabled("-benchmark-lazy-npc-compat-label","-benchmark-disable-lazy-npc-compat-label");
#else
        const bool lazyCompatibilityName=false;
#endif
        public TextMesh NameLabel
        {
            get
            {
                if(compatibilityNameLabel==null&&!string.IsNullOrEmpty(DisplayName)){UpdateCompatibilityNameLabel();ApplyClientNameColor();}
                return compatibilityNameLabel;
            }
        }
        public Metin2ActorTextTailView ScreenTextTail { get; private set; }
        private bool clientNameHovered;
        private bool clientNameTargeted;
        private Metin2LocaleText textTailLocale;
        private Func<Metin2NetworkEntity, Color32> nameColorResolver;
        private string guildName = string.Empty;
        public string LanguageCode { get; private set; } = string.Empty;
        public Text ShopSignLabel { get; private set; }
        public GameObject MountInstance { get; private set; }
        private Metin2EquipmentCatalog equipmentCatalog; private Metin2ItemCatalog itemCatalog; private Metin2.Runtime.Metin2CharacterEquipment equipment;
        private ushort[] appliedParts;
        /// <summary>Bumped by every parts change and by teardown, so a load that completes late
        /// for a superseded or despawned actor is dropped instead of dressing it again.</summary>
        private int partsGeneration;
        private uint appliedPartsMount = uint.MaxValue;
        private Metin2RaceCatalog raceCatalog;
        private Metin2CharacterMotionSet motions;
        private Transform visualRoot, visualOriginalParent; private Vector3 visualOriginalPosition, visualOriginalScale; private Quaternion visualOriginalRotation; private Animator mountAnimator;
        private Transform mountedSaddle, mountedRiderPivot;
        private Quaternion mountedRiderRotationFromActor;
        private int mountMotionPlaybackSerial;
        private Vector3 from, target; private Quaternion fromRotation, targetRotation; private float duration;
        private Vector3 mobMoveSource;
        private float mobMoveDistanceSquared;
        private float movementElapsed;
        private Vector3 networkSegmentVelocity;
        private bool continuousNetworkMove;
        private bool hasServerMoveSegment;
        private Vector3 serverMoveSource,serverMoveTarget;
        private uint serverMoveTime;
        private float serverMoveSeconds,lastObservedServerBaseSpeed;
        // OnMoving is sent every 300 ms. The native remote actor keeps consuming its RUN root
        // motion after reaching the last packet destination and only rebases after overshooting by
        // 100 native centimetres (InstanceBase.cpp:1633-1653). This small prediction window removes
        // the stop/start visible at every packet boundary without allowing an unbounded runaway.
        private const float MaximumMoveExtrapolationMetres = 1f;
        private float moveTurnSpeedDegreesPerSecond;
        private float packetFinalYaw;
        private bool applyPacketYawOnArrival;
        private struct RemoteMoveCommand
        {
            public Vector3 Position;
            public float Rotation;
            public float PacketDuration;
            public byte Function;
            public byte Argument;
            public uint CommandTime;
            public uint CheckTime;
        }
        private readonly Queue<RemoteMoveCommand> remoteMoveCommands = new Queue<RemoteMoveCommand>();
        private int averageNetworkGapMilliseconds;
        private uint lastProcessedMoveCommandTime;
        private float lastCombatPresentationAt = -999f;
        private Metin2NetworkEntityWorld motionWorld;
        private Metin2.Runtime.Metin2WorldRuntime terrainWorld;
        public bool IsWalkMode { get; private set; }
        public int QueuedMoveCommandCount => remoteMoveCommands.Count;
        public int AverageNetworkGapMilliseconds => averageNetworkGapMilliseconds;
        public uint LastProcessedMoveCommandTime => lastProcessedMoveCommandTime;
        public Vector3 EstimatedServerPosition(uint time)
        {
            if(!hasServerMoveSegment)return transform.position;
            float elapsed=Mathf.Max(0,unchecked((int)(time-serverMoveTime)))*.001f;
            return serverMoveSeconds>0?Vector3.Lerp(serverMoveSource,serverMoveTarget,Mathf.Clamp01(elapsed/serverMoveSeconds)):serverMoveTarget;
        }

        internal void BindMotionWorld(Metin2NetworkEntityWorld owner, Metin2.Runtime.Metin2WorldRuntime world)
        {
            motionWorld = owner;
            terrainWorld = world;
        }

        /// <summary>The logical actor direction without the converted player's mesh yaw offset.</summary>
        public Vector3 Heading
        {
            get
            {
                Vector3 heading = transform.rotation * Quaternion.Euler(0f, -modelYawOffset, 0f) * Vector3.forward;
                heading.y = 0f;
                return heading.sqrMagnitude > .0001f ? heading.normalized : Vector3.forward;
            }
        }

        /// <summary>
        /// Applies one __AccumulationMovement step to the remote actor root. Skills ignore ordinary
        /// actor bodies but still collide with doors and the background; packet interpolation is
        /// translated by the accepted delta so it cannot overwrite the Granny movement next frame.
        /// </summary>
        public void ApplySkillRootMotion(Vector3 delta)
        {
            Vector3 before = transform.position;
            Vector3 candidate = before + delta;
            if (terrainWorld != null && terrainWorld.IsBlocked(candidate)) return;
            if (motionWorld != null &&
                !motionWorld.TryAdvance(before, candidate, Vid, CollisionProfile,
                    Metin2.Runtime.Metin2CollisionProfile.CollisionTypeDefending, true, out candidate)) return;
            if (terrainWorld != null) candidate.y = terrainWorld.GroundHeight(candidate);
            Vector3 accepted = candidate - before;
            transform.position = candidate;
            from += accepted;
            target += accepted;
        }

        /// <summary>Remote-actor half of ProcessMotionEventWarp; keep interpolation on the new root.</summary>
        public bool ApplySkillWarp(Vector3 destination, Vector3 lookAt)
        {
            Vector3 before = transform.position;
            Vector3 candidate = new Vector3(destination.x, before.y, destination.z);
            if (terrainWorld != null && terrainWorld.IsBlocked(candidate))
            {
                FaceSkillTarget(lookAt);
                return false;
            }
            if (terrainWorld != null) candidate.y = terrainWorld.GroundHeight(candidate);
            Vector3 accepted = candidate - before;
            transform.position = candidate;
            from += accepted;
            target += accepted;
            FaceSkillTarget(lookAt);
            return true;
        }

        private void FaceSkillTarget(Vector3 worldPosition)
        {
            Vector3 direction = worldPosition - transform.position;
            direction.y = 0f;
            if (direction.sqrMagnitude <= .0001f) return;
            transform.rotation = Quaternion.LookRotation(direction, Vector3.up) *
                                 Quaternion.Euler(0f, modelYawOffset, 0f);
            fromRotation = targetRotation = transform.rotation;
        }

        public void FaceEmotionTarget(Vector3 worldPosition) => FaceSkillTarget(worldPosition);

        /// <summary>Immediate run/walk loop swap from HEADER_GC_WALK_MODE (111).</summary>
        public void ApplyWalkMode(bool enabled)
        {
            if (IsWalkMode == enabled) return;
            IsWalkMode = enabled;
            ApplyMountLocomotion(moving, enabled);
            if (motions == null || !motions.IsMoveMotion) return;
            if (enabled) motions.PlayWalk(); else motions.PlayMove(MountVnum != 0);
        }

        public void Initialize(uint vid, ushort race, byte type, ushort moveSpeed, ushort attackSpeed, Metin2EquipmentCatalog catalog = null, Metin2RaceCatalog races = null, Transform visual = null, Metin2ItemCatalog items = null) { ResetRaidPresentation(); Vid = vid; Race = race; Type = type; MoveSpeed = moveSpeed; AttackSpeed = attackSpeed; IsDead = false; equipmentCatalog = catalog; itemCatalog = items; raceCatalog = races; visualRoot = visual != null ? visual : transform; visualOriginalParent = visualRoot.parent; visualOriginalPosition = visualRoot.localPosition; visualOriginalRotation = visualRoot.localRotation; visualOriginalScale = visualRoot.localScale; if (equipment != null) equipment.ActiveMainChanged -= OnActiveMainChanged; equipment = GetComponentInChildren<Metin2.Runtime.Metin2CharacterEquipment>(true); if (equipment != null) { equipment.ActiveMainChanged -= OnActiveMainChanged; equipment.ActiveMainChanged += OnActiveMainChanged; } motions = GetComponentInChildren<Metin2CharacterMotionSet>(true);
            if(type==6 && race<8)equipment?.ConfigureGatheringHand(Metin2HerbalismAssets.UsesLeftHand(race));
            // Monsters and NPCs never get a motion set from the builder, which only configures the
            // eight player races. Without it PlayMove always failed and every actor stayed on the
            // Animator's default idle. Their clips are the numbered ones, resolved here at runtime.
            if (motions == null)
            {
                Animator entityAnimator = GetComponentInChildren<Animator>(true);
                if (entityAnimator != null)
                {
                    motions = entityAnimator.gameObject.AddComponent<Metin2CharacterMotionSet>();
                    motions.ConfigureNumbered(entityAnimator);
                }
            }
            else if (string.IsNullOrEmpty(motions.GeneralRun)) motions.ConfigureNumbered(GetComponentInChildren<Animator>(true));
            if (motions != null) { motions.SetActorSpeeds(moveSpeed, attackSpeed); motions.MotionChanged -= OnRiderMotionChanged; motions.MotionChanged += OnRiderMotionChanged; }
            InvalidatePickModel(); target = transform.position; }
        /// <summary>
        /// CInstanceBase::CanPickInstance - dead actors and the main instance are skipped by
        /// __NEW_Pick. Ground items have their own picking path, so only actors answer here.
        /// </summary>
        public bool CanPick => !IsDead && isActiveAndEnabled &&
                               (Race != Metin2WoodcuttingAssets.TreeRace || GetComponent<Metin2WoodcuttingTree>()?.Fallen != true) &&
                               (!Metin2HerbalismAssets.IsPlant(Race) || GetComponent<Metin2HerbalismPlant>()?.Depleted != true) &&
                               (Type != 6 || affectEffects == null ||
                                (!affectEffects.IsAffectFlagSet(1) &&
                                 !affectEffects.IsAffectFlagSet(18) &&
                                 !affectEffects.IsAffectFlagSet(27)));

        /// <summary>
        /// CGraphicThingInstance::Intersect runs against the actor's own model only. Attached
        /// effects are separate instances in the original (CActorInstance::m_AttachingEffectList)
        /// and never enlarge the picked volume, so a renderer that belongs to one is skipped here -
        /// otherwise selecting an actor grows its own click area, which is exactly the runaway the
        /// selection glow produced.
        /// </summary>
        internal static bool IsEffectRenderer(Renderer renderer)
            => renderer.GetComponentInParent<Metin2.Runtime.Metin2ActorAttachedEffect>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2EffectLifetime>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2EffectMeshPlayer>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2EffectTransformPlayer>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2NativeParticleSystem>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2ParticleEmitterController>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2TextureSequencePlayer>() != null
            || renderer.GetComponentInParent<Metin2.Runtime.Metin2AnimatedImage>() != null;

        /// <summary>
        /// World-space equivalent of CInstanceBase::IntersectBoundingBox's volume. Recomputed on
        /// demand, including continuous hover picking. Cache model membership, not bounds:
        /// animation, movement and renderer visibility must still affect every query.
        /// </summary>
        public bool TryGetPickBounds(out Bounds bounds)
        {
            bounds = default;
            bool any = false;
#if UNITY_STANDALONE_WIN
            if (!pickRenderersResolved)
            {
                pickRenderers.Clear();
                GetComponentsInChildren<Renderer>(true, pickRenderers);
                for (int i = pickRenderers.Count - 1; i >= 0; i--)
                    if (pickRenderers[i] is ParticleSystemRenderer || IsEffectRenderer(pickRenderers[i]))
                        pickRenderers.RemoveAt(i);
                pickRenderersResolved = true;
            }
            foreach (Renderer renderer in pickRenderers)
            {
                if (renderer == null || !renderer.enabled || !renderer.gameObject.activeInHierarchy) continue;
#else
            foreach (Renderer renderer in GetComponentsInChildren<Renderer>(false))
            {
                if (!renderer.enabled || renderer is ParticleSystemRenderer) continue;
                if (IsEffectRenderer(renderer)) continue;
#endif
                if (!any) { bounds = renderer.bounds; any = true; } else bounds.Encapsulate(renderer.bounds);
            }
            if (!any) return false;
            // A thin actor is hard to hit with a single ray; the original box is the instance's
            // bounding volume, which is generous in the same way.
            bounds.Expand(.15f);
            return true;
        }

        /// <summary>The model-box stand-in for CInstanceBase::IntersectBoundingBox.</summary>
        public bool IntersectModelBounds(Ray ray)
            => TryGetPickBounds(out Bounds bounds) && bounds.IntersectRay(ray);

        /// <summary>
        /// CActorInstance::IntersectDefendingSphere (ActorInstance.cpp:753-780), sphere for sphere:
        /// b = dot(dir, origin - centre), c = |origin - centre|^2 - r^2, hit when b*b - c >= 0.
        ///
        /// Returns false when this actor's MSM authors no COLLISION_TYPE_DEFENDING sphere at all,
        /// so the caller can fall back rather than treat "no data" as "no hit".
        /// </summary>
        public bool TryIntersectDefendingSpheres(Ray ray, out bool hit)
        {
            hit = false;
            Metin2.Runtime.Metin2CollisionProfile profile = CollisionProfile;
            // Keep the existing model-body selection fallback for imported actors that
            // had no authored defensive data; their generated combat spheres sit low.
            if (recoveredDefendingProfile) return false;
            if (profile == null) return false;
            Metin2.Runtime.Metin2CollisionSphere[] spheres = profile.Spheres;
            if (spheres == null || spheres.Length == 0) return false;
            Vector3 scale = profile.transform.lossyScale;
            float radiusScale = Mathf.Max(Mathf.Abs(scale.x), Mathf.Abs(scale.y), Mathf.Abs(scale.z));
            bool authored = false;
            foreach (Metin2.Runtime.Metin2CollisionSphere sphere in spheres)
            {
                if (sphere.collisionType != Metin2.Runtime.Metin2CollisionProfile.CollisionTypeDefending ||
                    sphere.radius <= 0f) continue;
                authored = true;
                Vector3 centre = profile.SphereWorldCentre(sphere);
                float radius = sphere.radius * radiusScale;
                Vector3 distance = ray.origin - centre;
                float b = Vector3.Dot(ray.direction, distance);
                float c = Vector3.Dot(distance, distance) - radius * radius;
                if (b * b - c >= 0f) { hit = true; return true; }
            }
            return authored;
        }

        /// <summary>GC_CHARACTER_ADDITIONAL carries these three and the port dropped them; the target
        /// board's empire, guild and level conditions all read them.</summary>
        public byte Empire { get; private set; }
        public uint CharacterLevel { get; private set; }
        public void SetCharacterLevel(uint value) { CharacterLevel = value; ApplyNameLabel(); }
        public long Alignment { get; private set; }
        public void ApplyAdditional(Metin2GamePacketCodec.EntityAdditional data)
        {
            Metin2SkillEffectColors.Apply(transform,data.SkillColors);
            displayNameFromLocale = Metin2MobNames.TryResolveAdditionalNpcName(Type, Race, out string localizedName);
            DisplayName = displayNameFromLocale ? localizedName : data.Name;
            GuildId = data.GuildId; Empire = data.Empire; PkMode = data.PkMode;
            CharacterLevel = data.Level; Alignment = data.AlignmentRaw != 0 ? (long)data.AlignmentRaw : data.Alignment; LanguageCode = data.Language ?? string.Empty;
            ApplyMount(data.MountVnum); ApplyParts(data.Parts); ApplyNameLabel();
        }
        public void SetTextTailMetadata(string name, uint level, long alignment, byte empire, uint guildId,
            string resolvedGuildName, string language)
        {
            displayNameFromLocale=false;
            DisplayName = name ?? string.Empty; CharacterLevel = level; Alignment = alignment;
            Empire = empire; GuildId = guildId; guildName = resolvedGuildName ?? string.Empty;
            LanguageCode = language ?? string.Empty; ApplyNameLabel();
        }

        public void BindTextTail(Metin2LocaleText locale, Transform player, string resolvedGuildName,float? knownBaseTopOffset=null)
        {
            textTailLocale = locale;
            guildName = resolvedGuildName ?? string.Empty;
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Spawn.TailComponent");
            try{if (ScreenTextTail == null) ScreenTextTail = gameObject.AddComponent<Metin2ActorTextTailView>();}
            finally{UnityEngine.Profiling.Profiler.EndSample();}
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Spawn.TailInitialize");
            try{ScreenTextTail.Initialize(Type, player, locale,knownBaseTopOffset);}
            finally{UnityEngine.Profiling.Profiler.EndSample();}
            ApplyNameLabel();
        }

        public void BindTextTailDistanceOrigin(Transform player) => ScreenTextTail?.BindDistanceOrigin(player);
        public void BindNameColorResolver(Func<Metin2NetworkEntity, Color32> resolver)
        { nameColorResolver = resolver; ApplyClientNameColor(); }
        public void SetGuildName(string value) { guildName = value ?? string.Empty; ApplyNameLabel(); }
        public void ApplySpawnStateFlags(byte state) { StateFlags = state; if ((state & 1) != 0) IsDead = true; ApplyClientNameColor(); }
        public void ApplyStateFlags(byte state) { StateFlags = state; ApplyClientNameColor(); }
        /// <summary>GC_CHARACTER_POSITION (43): the server broadcasts sit/stand transitions.
        /// The Auriga client parses the packet and its UpdatePosition call is commented out
        /// (PythonNetworkStreamPhaseGame.cpp:1977), so this keeps the authoritative state on the
        /// actor for the presentation layer instead of inventing a pose the original never showed.
        /// POSITION_GENERAL 0, POSITION_SITTING_GROUND 4, POSITION_SITTING_CHAIR 5.</summary>
        public byte PositionState { get; private set; }
        public void ApplyPosition(byte position) { PositionState = position; }
        public void ApplyOwner(uint ownerVid) { OwnerVid = ownerVid; ownerAssignedAt = Time.time; }
        public byte ShopBorderStyle { get; private set; }
        public void ApplyShopSign(string sign) => ApplyShopSign(sign, 0);
        public void ApplyShopSign(string sign, byte borderStyle)
        {
            ShopBorderStyle = (byte)Mathf.Clamp(borderStyle, 0, 5);
            if(string.IsNullOrEmpty(sign)){if(ShopSignBoard!=null)ShopSignBoard.SetActive(false);if(ShopSignLabel!=null)ShopSignLabel.gameObject.SetActive(false);return;}
            BuildShopSignBoard(sign);
        }

        /// <summary>
        /// ui.ShopDecoration (ui.py:3935-4086) as driven by OfflineShopAdvertisementBoard
        /// (uiofflineshopbuilder.py:88-177): four thinboard corners, four lines, a 0.51-alpha black
        /// interior bar and the shop name centred inside. Premium styles use the original
        /// fire/ice/paper/ribon/wing assets selected by the sign packet. The board keeps
        /// facing the camera in LateUpdate like the rest of the tail layer.
        /// </summary>
        private static readonly string[] ShopSignPieceNames =
        {
            "shopboard_corner_lefttop","shopboard_corner_leftbottom","shopboard_corner_righttop","shopboard_corner_rightbottom",
            "shopboard_line_left","shopboard_line_right","shopboard_line_top","shopboard_line_bottom",
        };
        private void BuildShopSignBoard(string sign)
        {
            Metin2UiSpriteCatalog sprites = Metin2UiSpriteCatalog.Load();
            const int fontSize = 22;
            Font font = Metin2.Runtime.Metin2UiFont.Get();
            if(ShopSignBoard==null)
            {
                GameObject board=new GameObject("PrivateShopSign");board.transform.SetParent(transform,false);
                Canvas canvas=board.AddComponent<Canvas>();canvas.renderMode=RenderMode.WorldSpace;canvas.sortingOrder=32;
                ShopSignBoardRect=board.transform as RectTransform;ShopSignBoardRect.sizeDelta=new Vector2(100,32);
                // 100 px = 1 m keeps the authored pixel sizes meaningful in world space.
                board.transform.localScale=Vector3.one*.01f;
                ShopSignBoard=board;ShopSignPieces=new Image[8];
                for(int i=0;i<8;i++)
                {
                    GameObject piece=new GameObject(ShopSignPieceNames[i],typeof(RectTransform),typeof(Image));piece.transform.SetParent(board.transform,false);
                    Image image=piece.GetComponent<Image>();image.raycastTarget=false;
                    if(i>=4)image.type=Image.Type.Tiled;
                    ShopSignPieces[i]=image;
                }
                GameObject barObject=new GameObject("ShopBoardBar",typeof(RectTransform),typeof(Image));barObject.transform.SetParent(board.transform,false);
                Sprite white=Sprite.Create(Texture2D.whiteTexture,new Rect(0,0,4,4),new Vector2(.5f,.5f),100f);white.name="ShopBoardBarSprite";
                ShopSignBar=barObject.GetComponent<Image>();ShopSignBar.sprite=white;ShopSignBar.color=new Color(0f,0f,0f,.51f);ShopSignBar.raycastTarget=false;
                GameObject textObject=new GameObject("ShopBoardText",typeof(RectTransform),typeof(Text));textObject.transform.SetParent(board.transform,false);
                ShopSignLabel=textObject.GetComponent<Text>();
            }
            Text label=ShopSignLabel;
            label.font=font;label.fontSize=fontSize;label.color=new Color(1f,.9f,.25f);
            label.alignment=TextAnchor.MiddleCenter;label.resizeTextForBestFit=false;label.horizontalOverflow=HorizontalWrapMode.Overflow;label.verticalOverflow=VerticalWrapMode.Overflow;label.raycastTarget=false;
            label.text=sign;
            for(int i=0;i<ShopSignPieces.Length;i++)
                ShopSignPieces[i].sprite=Metin2OfflineShopSignArt.Load(ShopBorderStyle,ShopSignPieceNames[i],sprites);
            int cornerWidth=ShopSignPieces[0].sprite!=null?Mathf.RoundToInt(ShopSignPieces[0].sprite.rect.width):(ShopBorderStyle==0?16:48);
            int cornerHeight=ShopSignPieces[0].sprite!=null?Mathf.RoundToInt(ShopSignPieces[0].sprite.rect.height):(ShopBorderStyle==0?16:32);
            int extraWidth=ShopBorderStyle==0?35:100;
            if(ShopSignBar!=null)ShopSignBar.enabled=ShopBorderStyle==0;
            // Piece layout is the original SetSize arithmetic (ui.py:4056-4086).
            int width=(int)Mathf.Max(cornerWidth*2f,label.preferredWidth+extraWidth);int height=Mathf.Max(cornerHeight*2,20);
            ShopSignBoardRect.sizeDelta=new Vector2(width,height);
            for(int i=0;i<8;i++)
            {
                Image piece=ShopSignPieces[i];
                Vector2 position,size;
                switch(i)
                {
                    case 0:position=new Vector2(0,height-cornerHeight);size=new Vector2(cornerWidth,cornerHeight);break;
                    case 1:position=new Vector2(0,0);size=new Vector2(cornerWidth,cornerHeight);break;
                    case 2:position=new Vector2(width-cornerWidth,height-cornerHeight);size=new Vector2(cornerWidth,cornerHeight);break;
                    case 3:position=new Vector2(width-cornerWidth,0);size=new Vector2(cornerWidth,cornerHeight);break;
                    case 4:position=new Vector2(0,cornerHeight);size=new Vector2(cornerWidth,height-cornerHeight*2f);break;
                    case 5:position=new Vector2(width-cornerWidth,cornerHeight);size=new Vector2(cornerWidth,height-cornerHeight*2f);break;
                    case 6:position=new Vector2(cornerWidth,height-cornerHeight);size=new Vector2(width-cornerWidth*2f,cornerHeight);break;
                    default:position=new Vector2(cornerWidth,0);size=new Vector2(width-cornerWidth*2f,cornerHeight);break;
                }
                RectTransform rect=piece.transform as RectTransform;rect.anchorMin=Vector2.zero;rect.anchorMax=Vector2.zero;rect.pivot=new Vector2(0f,0f);
                rect.anchoredPosition=position;rect.sizeDelta=size;
            }
            RectTransform barRect=ShopSignBar.transform as RectTransform;barRect.anchorMin=Vector2.zero;barRect.anchorMax=Vector2.zero;barRect.pivot=new Vector2(0f,0f);
            barRect.anchoredPosition=new Vector2(cornerWidth,cornerHeight);barRect.sizeDelta=new Vector2(Mathf.Max(1,width-cornerWidth*2f),Mathf.Max(1,height-cornerHeight*2f));
            RectTransform textRect=label.transform as RectTransform;textRect.anchorMin=Vector2.zero;textRect.anchorMax=Vector2.one;textRect.offsetMin=Vector2.zero;textRect.offsetMax=Vector2.zero;
            Renderer[] renderers=GetComponentsInChildren<Renderer>(true);float localTop=2.6f;if(renderers.Length>0){Bounds bounds=renderers[0].bounds;for(int i=1;i<renderers.Length;i++)bounds.Encapsulate(renderers[i].bounds);localTop=transform.InverseTransformPoint(bounds.max).y+.55f;}
            ShopSignBoard.transform.localPosition=new Vector3(0f,localTop,0f);ShopSignBoard.SetActive(true);
        }
        public GameObject ShopSignBoard{get;private set;}
        public RectTransform ShopSignBoardRect{get;private set;}
        public Image[] ShopSignPieces{get;private set;}
        public Image ShopSignBar{get;private set;}
        public void ApplyUpdate(Metin2GamePacketCodec.EntityUpdate data) { Metin2SkillEffectColors.Apply(transform,data.SkillColors); ApplyActorSpeeds(data.MoveSpeed, data.AttackSpeed); GuildId = data.GuildId; Alignment = data.AlignmentRaw != 0 ? (long)data.AlignmentRaw : data.Alignment; PkMode = data.PkMode; ApplyStateFlags(data.State); ApplyMount(data.MountVnum); ApplyParts(data.Parts); if (compatibilityNameLabel != null||ScreenTextTail!=null) ApplyNameLabel(); ApplyAffects(data.Affect0, data.Affect1); }

        /// <summary>
        /// The Affect0/Affect1 pair rides along on every character add and update packet and used to
        /// be discarded here, so no looping affect effect ever appeared on a remote actor. It is the
        /// same flag container CInstanceBase feeds to SetAffectFlagContainer (InstanceBase.cpp :792,
        /// :2837).
        /// </summary>
        public void ApplySpawnAffects(uint affect0, uint affect1)
        {
            ApplyAffects(affect0,affect1);
            if(Type!=0&&Type!=2)return;
            if(affectEffects==null)affectEffects=GetComponent<Metin2AffectEffects>()??gameObject.AddComponent<Metin2AffectEffects>();
            affectEffects.BeginPresentationFade(0,1,.5f);
            if(ShouldPlaySpawnEffect(StateFlags,affect0))PlayLifecycleEffect("effect/etc/appear_die/monster_appear.mse");
        }
        public static bool ShouldPlaySpawnEffect(byte state,uint affect0)=>(state&2)!=0&&(affect0&4)!=0;
        public bool IsDeletingPresentation { get; private set; }
        public void BeginDeletePresentation()
        {
            IsDeletingPresentation=true;moving=false;duration=0;remoteMoveCommands.Clear();push.Initialize();
            if(ScreenTextTail!=null)ScreenTextTail.HideForRemoval();
            if(compatibilityNameLabel!=null)compatibilityNameLabel.gameObject.SetActive(false);
            foreach(var collider in GetComponentsInChildren<Collider>(true))collider.enabled=false;
            if(affectEffects==null)affectEffects=GetComponent<Metin2AffectEffects>()??gameObject.AddComponent<Metin2AffectEffects>();
            affectEffects.BeginPresentationFade(affectEffects.PresentationAlpha,0,1f/1.5f);
        }
        private async void PlayLifecycleEffect(string path)
        {
            try
            {
                var catalog=Resources.Load<Metin2EffectCatalog>("Metin2EffectCatalog");if(catalog==null)return;
                var definition=await catalog.ResolveAsync(path,Metin2ContentScope.Map);
                if(this==null||IsDeletingPresentation||definition==null)return;
                var bone=GetComponentsInChildren<Transform>(true).FirstOrDefault(t=>t.name=="Bip01")??transform;
                var effect=Metin2EffectRuntime.Create(definition,bone);
                effect.transform.localRotation=Metin2Units.EffectFrameCorrection(transform,bone);
                // Runtime already compensates FBX scale for particles and mesh
                // elements. A second inverse scale would enlarge mesh effects 100x.
                if(Application.isPlaying)effect.AddComponent<Metin2EffectLifetime>();
            }
            catch(Exception error){if(this!=null)Debug.LogWarning("METIN2_MOB_LIFECYCLE_EFFECT_FAILED "+path+" "+error.Message);}
        }

        private void ApplyAffects(uint affect0, uint affect1)
        {
            if (affectEffects == null) affectEffects = GetComponent<Metin2AffectEffects>()??gameObject.AddComponent<Metin2AffectEffects>();
            affectEffects.IsMainInstance = false;
            affectEffects.ApplyAffectFlags(affect0, affect1);
            SetGameMaster((affect0 & 1u) != 0);
            // PythonTextTail only shows character labels for CanPickInstance actors.
            RefreshNameVisibility();
        }

        private Metin2AffectEffects affectEffects;
        /// <summary>Raised when a requested mount becomes genuinely drawable (or is removed).</summary>
        public event Action<bool> MountPresentationChanged;
        private void ApplyMount(uint mountVnum)
        {
            if (MountVnum == mountVnum && (mountVnum == 0 || MountInstance != null)) return;
            InvalidatePickModel();
            RestoreVisualRoot();
            if (MountInstance != null) { if (Application.isPlaying) Destroy(MountInstance); else DestroyImmediate(MountInstance); MountInstance = null; mountAnimator = null; mountState = null; mountMotionPlaybackSerial = 0; }
            mountedSaddle = null; mountedRiderPivot = null;
            MountVnum = mountVnum;
            ScreenTextTail?.SetMounted(mountVnum != 0);
            if (mountVnum == 0) { motions?.PlayIdle(false); MountPresentationChanged?.Invoke(false); return; }
            bool prefabReady = Metin2RacePrefabs.TryGet(raceCatalog, mountVnum, out GameObject prefab) && prefab != null;
            // A bound catalog is the synchronous editor/validator path. Runtime passes null and
            // must acquire the matching per-race data asset before choosing semantic motion names.
            bool raceDataReady = raceCatalog != null || Metin2RaceDataBinder.IsLoaded(mountVnum);
            if (!prefabReady || !raceDataReady)
            {
                // Not an error - the asset simply is not resident yet. Actor races are preloaded by
                // the spawn path, but a MOUNT race is never preloaded on map enter: the rider's own
                // ADDITIONAL packet is the first mention of it. At login the asset happened to be
                // there; after a teleport Metin2RacePrefabs.ReleaseMapScopedAssets has dropped it,
                // which is exactly when the horse disappeared. Acquire it the same way a deferred
                // actor spawn does and re-apply once it lands.
                // Keep both local and remote riders in a valid on-foot pose while content is in
                // flight. Otherwise the horse animation evaluates at the actor origin and looks
                // exactly like the character is sitting inside the terrain.
                motions?.PlayIdle(false);
                MountPresentationChanged?.Invoke(false);
                AcquireMountAsync(mountVnum);
                return;
            }
            mountAcquireVnum = 0;
            MountInstance = Instantiate(prefab, transform); MountInstance.name = $"Mount_{mountVnum}_{prefab.name}";
            Debug.Log($"METIN2_MOUNT_APPLY vid={Vid} mount={mountVnum} prefab={prefab.name}");
            // A mount is a full CRaceData actor too.  In particular Auriga's custom mounts use
            // semantic controller states such as "run" rather than the old pony convention "03",
            // and many carry MSM skins/permanent effects.  They do not pass through the ordinary
            // entity spawn binder, so apply the visual halves explicitly here.
            if (Metin2RaceDataBinder.TryGetEntry(mountVnum, out Metin2RaceDataCatalog.Entry mountData) &&
                mountData != null)
            {
                Metin2RaceDataBinder.ApplyShapeSkins(MountInstance, MountInstance.transform, mountData);
                Metin2RaceDataBinder.ApplyAttachedEffects(MountInstance, MountInstance.transform, mountData);
            }
            // Local player roots sit FootOffset below their logical terrain point. A remote actor's
            // wrapper already is the logical point, so only the local controller contributes here.
            Metin2PlayerController localController = GetComponent<Metin2PlayerController>();
            float logicalGroundOffset = localController != null ? localController.FootOffset : 0f;
            MountInstance.transform.localPosition = new Vector3(0f, logicalGroundOffset, 0f); MountInstance.transform.localRotation = Quaternion.identity;
            Metin2.Runtime.Metin2Units.ApplyCharacterScale(MountInstance);
            Metin2.Runtime.Metin2Units.AlignLegacyHorseAxes(MountInstance.transform);
            // Both rigs are imported in their FBX bind pose. In that pose the rider's Bip01 is
            // about one metre higher than it is in horse/wait.msa, so aligning before the actual
            // mounted clips evaluate places the visible rider one metre below the saddle. Evaluate
            // the same wait states the original installs first, then align the live bone frames.
            mountAnimator = MountInstance.GetComponentInChildren<Animator>(true); mountState = null;
            PlayMount("00"); if (mountAnimator != null) mountAnimator.Update(.11f);
            Metin2.Runtime.Metin2Units.LevelLegacyHorseStance(MountInstance.transform);
            // The wrapper/camera follows the native actor origin while the rider follows the
            // animated saddle. Several community mounts shipped with the whole rig translated in
            // X/Z, placing the rider and mount off to one side of the screen although their actor
            // position was correct. Centre the evaluated WAIT saddle on the logical actor origin;
            // this moves horse and rider together without altering their authored relationship.
            Transform centredSaddle = FindTransform(MountInstance.transform, "saddle");
            if (centredSaddle != null)
            {
                Vector3 centreDelta = transform.position - centredSaddle.position;
                centreDelta.y = 0f;
                MountInstance.transform.position += centreDelta;
            }
            // Deliberately NO Metin2ActorOrientation correction here, unlike the standalone actor
            // path. A mount hangs under a RIDER's actor transform, and a rider is always a player
            // actor, whose ModelYawOffset is the -90 the eight converted player meshes need. The
            // converted horse rigs face their own local +X too, so that same -90 already turns the
            // horse onto the rider's heading; applying the table on top of it turned the horse a
            // second quarter turn and the rider ended up sitting across the saddle. The standalone
            // horse NPC is the case that needs the table: it spawns with entityYawOffset 0, so
            // nothing compensates its +X frame there.
            // The imported horse origin is not its hoof plane. The original horse actor's logical
            // position is terrain height, so ground the evaluated WAIT pose to that point before
            // the rider is attached to the live saddle. This also covers non-player remote wrappers
            // without inventing a global vertical offset.
            //
            // The hoof plane is read from the POSED RIG, not from Renderer.bounds. A skinned
            // renderer's world AABB is its imported bind-pose box carried by the root bone, so it
            // only answers "where are the hooves" when the pose happens to sit where the bind pose
            // did - and the mount rigs disagree about that. Measured on the converted archive, in
            // the actor's frame after the legacy axis restoration, motion 00:
            //
            //     horse_master   mesh bottom   -2.6 cm   lowest bone   -3.6 cm
            //     horse2_master  mesh bottom   -2.6 cm   lowest bone   -3.6 cm
            //     unicorn        mesh bottom  -10.3 cm   lowest bone   -9.3 cm
            //     pony           mesh bottom -135.7 cm   lowest bone -136.7 cm
            //
            // The horses stand on their own origin, so a wrong or missing lift is invisible on
            // them; the PONY hangs 1.36 m below its origin and is the rig that exposed it - it was
            // buried to its back with the rider sunk to his chest. The lowest bone tracks the mesh
            // bottom within about a centimetre on every one of them, needs no render pass, and does
            // not care whether renderer.enabled is currently 0 (Metin2DistanceLod flips that per
            // level, which must not decide where a mount stands).
            float logicalGroundY = transform.position.y + logicalGroundOffset;
            float hoofY = Metin2.Runtime.Metin2Units.TryMeasurePosedWorldBounds(MountInstance, out Bounds visiblePose)
                ? visiblePose.min.y : LowestBoneY(MountInstance);
            if (!float.IsNaN(hoofY))
            {
                float lift = logicalGroundY - hoofY;
                MountInstance.transform.position += Vector3.up * lift;
                Metin2.Runtime.Metin2Units.TryMeasurePosedWorldBounds(MountInstance, out Bounds posed);
                string grounded =
                    $"METIN2_MOUNT_GROUND vid={Vid} mount={mountVnum} ground_y={logicalGroundY:F3} " +
                    $"hoof_bone_y={hoofY:F3} lift={lift:F3} final_y={MountInstance.transform.position.y:F3} " +
                    $"box_min_y={posed.min.y:F3} cached_min_y={CachedRendererMinY(MountInstance):F3}";
                Debug.Log(grounded);
                Metin2NetworkDiagnostics.Event(0, grounded);
            }
            else
            {
                Debug.LogWarning($"METIN2_MOUNT_NO_RIG vid={Vid} mount={mountVnum} - " +
                                 "hoof grounding skipped, the mount keeps its raw import origin");
            }
            motions?.PlayIdle(true); if (motions?.ActiveAnimator != null) motions.ActiveAnimator.Update(.11f);
            Transform saddle = centredSaddle != null ? centredSaddle : FindTransform(MountInstance.transform, "saddle");
            if (saddle == null || visualRoot == transform) { Debug.LogWarning($"METIN2_MOUNT_SADDLE_MISSING vid={Vid} mount={mountVnum}"); }
            else
            {
                Vector3 desiredScale = visualRoot.lossyScale; Quaternion desiredRotation = visualRoot.rotation;
                visualRoot.SetParent(saddle, true); visualRoot.position = saddle.position; visualRoot.rotation = desiredRotation;
                Vector3 parentScale = saddle.lossyScale;
                visualRoot.localScale = new Vector3(SafeDivide(desiredScale.x, parentScale.x), SafeDivide(desiredScale.y, parentScale.y), SafeDivide(desiredScale.z, parentScale.z));
                mountedSaddle = saddle;
                mountedRiderRotationFromActor = Quaternion.Inverse(transform.rotation) * desiredRotation;
                RefreshMountedRiderPivot();
            }
            MountPresentationChanged?.Invoke(true);
        }
        /// <summary>The mount race an acquire is running for, so one packet burst starts one load.</summary>
        private uint mountAcquireVnum;

        /// <summary>
        /// Brings a mount race's per-race prefab asset in and re-applies the mount. Mirrors the
        /// deferred actor spawn: Metin2RacePrefabs.PreloadAsync acquires the asset for the map
        /// scope, and the retry goes back through ApplyMount, whose "already applied" guard lets it
        /// through because MountInstance is still null.
        /// </summary>
        private async void AcquireMountAsync(uint mountVnum)
        {
            if (mountAcquireVnum == mountVnum) return;
            mountAcquireVnum = mountVnum;
            try
            {
                await Task.WhenAll(
                    Metin2RacePrefabs.PreloadAsync(mountVnum, Metin2.Runtime.Metin2ContentScope.Map),
                    Metin2RaceDataBinder.PreloadAsync(mountVnum, Metin2.Runtime.Metin2ContentScope.Map));
                if (this == null || MountVnum != mountVnum || MountInstance != null) return;
                if (!Metin2RacePrefabs.TryGet(raceCatalog, mountVnum, out GameObject loaded) || loaded == null)
                {
                    Debug.LogError($"METIN2_MOUNT_RACE_MISSING vid={Vid} mount={mountVnum} - " +
                                   "the server sent a mount race with no generated prefab");
                    MountPresentationChanged?.Invoke(false);
                    return;
                }
                ApplyMount(mountVnum);
                _ = WarmMountCosmeticsAsync(mountVnum);
            }
            catch (OperationCanceledException) { }
            catch (Exception exception)
            {
                Debug.LogWarning($"METIN2_MOUNT_PRELOAD_FAILED vid={Vid} mount={mountVnum} {exception.Message}");
            }
            finally { if (this != null && mountAcquireVnum == mountVnum) mountAcquireVnum = 0; }
        }

        private async Task WarmMountCosmeticsAsync(uint mountVnum)
        {
            try
            {
                await Task.WhenAll(
                    Metin2RaceDataBinder.PreloadVisualEffectsAsync(mountVnum, Metin2ContentScope.Map),
                    Metin2RaceDataBinder.PreloadMotionSoundsAsync(mountVnum, Metin2ContentScope.Map));
                if (this != null && MountVnum == mountVnum && MountInstance != null &&
                    Metin2RaceDataBinder.TryGetEntry(mountVnum, out var readyData))
                    Metin2RaceDataBinder.ApplyAttachedEffects(MountInstance, MountInstance.transform, readyData, byte.MaxValue, true);
            }
            catch (OperationCanceledException) { }
            catch (Exception e)
            {
                if (this != null) Debug.LogWarning($"METIN2_MOUNT_COSMETIC_PRELOAD_FAILED mount={mountVnum} {e.Message}");
            }
        }

        /// <summary>Lowest bone of a rig, the last-resort ground plane when no mesh can be measured.</summary>
        private static float LowestBoneY(GameObject root)
        {
            bool any = false; float result = 0f;
            foreach (Transform node in root.GetComponentsInChildren<Transform>(true))
            {
                if (!any || node.position.y < result) result = node.position.y;
                any = true;
            }
            return any ? result : float.NaN;
        }

        /// <summary>
        /// What the old grounding measure would have read, kept only so the MOUNT_GROUND trace shows
        /// whether Unity's cached AABB agreed with the posed geometry on this machine.
        /// </summary>
        private static float CachedRendererMinY(GameObject root)
        {
            bool any = false; float result = 0f;
            foreach (Renderer renderer in root.GetComponentsInChildren<Renderer>(true))
            {
                if (!renderer.enabled || renderer is ParticleSystemRenderer) continue;
                if (!any || renderer.bounds.min.y < result) result = renderer.bounds.min.y;
                any = true;
            }
            return any ? result : float.NaN;
        }

        public void SetMount(uint mountVnum)
        {
            ApplyMount(mountVnum);
            GetComponent<Metin2CostumeEffectAppearance>()?.SetMounted(MountVnum!=0);
        }
        /// <summary>Synchronises the horse actor with the rider's current local/remote locomotion.</summary>
        public void ApplyMountLocomotion(bool isMoving, bool isWalking)
        {
            if (MountVnum == 0) return;
            if(GetComponent<Metin2MountFlight>() is Metin2MountFlight flight&&flight.Flying){flight.ApplyAnimation(isMoving);return;}
            if(motions!=null&&motions.IsLocked&&
                (motions.CurrentMotionType==Metin2CharacterMotionSet.MotionType.Attack||motions.CurrentMotionType==Metin2CharacterMotionSet.MotionType.Combo))return;
            PlayMount(isMoving ? (isWalking ? "02" : "03") : "00");
        }
        /// <summary>
        /// NEW_UseSkill's MOVING_SKILL branch starts the horse's run before rider skill 121 is
        /// intercepted. The pony has no SKILL1 motion, so its existing RUN keeps evaluating while
        /// the rider executes wildattack. This explicit edge reproduces that two-actor state.
        /// </summary>
        public void ApplyMountMovingSkill(bool active)
        {
            if (MountVnum == 0) return;
            PlayMount(active ? "03" : "00", true);
        }
        public float MountedRiderRootTiltDegrees => visualRoot != null && mountedSaddle != null
            ? Vector3.Angle(visualRoot.up, transform.up) : 0f;
        /// <summary>True only when the cached saddle pivot belongs to the currently visible PART_MAIN.</summary>
        public bool MountedRiderUsesActiveSkeleton
        {
            get
            {
                Transform active = equipment != null ? equipment.ActiveSkeletonRoot : visualRoot;
                return active != null && mountedRiderPivot != null &&
                       (mountedRiderPivot == active || mountedRiderPivot.IsChildOf(active));
            }
        }
        /// <summary>
        /// Visible torso roll, measured from Bip01 to Bip01 Head in the actor's upright frame.
        /// Unlike MountedRiderRootTiltDegrees this follows replacement armour skeletons.
        /// </summary>
        public float MountedRiderBodyRollDegrees
        {
            get
            {
                Transform active = equipment != null ? equipment.ActiveSkeletonRoot : visualRoot;
                Transform head = active != null ? FindTransform(active, "Bip01 Head") : null;
                if (mountedRiderPivot == null || head == null) return 0f;
                Vector3 body = head.position - mountedRiderPivot.position;
                return Mathf.Abs(Mathf.Atan2(Vector3.Dot(body, transform.right),
                    Vector3.Dot(body, transform.up)) * Mathf.Rad2Deg);
            }
        }
        public void SyncMountedRiderFrameForValidation() => UpdateMountedRiderFrame();
        private static float SafeDivide(float value, float divisor) => Mathf.Abs(divisor) > 0.000001f ? value / divisor : value;
        private void RestoreVisualRoot() { mountedSaddle = null; mountedRiderPivot = null; if (visualRoot == null || visualRoot.parent == visualOriginalParent) return; visualRoot.SetParent(visualOriginalParent, false); visualRoot.localPosition = visualOriginalPosition; visualRoot.localRotation = visualOriginalRotation; visualRoot.localScale = visualOriginalScale; }

        /// <summary>
        /// The original rider receives the horse saddle matrix as model initial placement
        /// (ModelInstanceUpdate.cpp:124-153). Unity already evaluates the rider skeleton in its own
        /// converted upright basis, so inheriting the imported saddle bone's basis a second time
        /// injects a visible roll. Keep the actor-relative upright model rotation, then solve the
        /// translated Bip01 origin back onto the live saddle every LateUpdate. Saddle bob remains;
        /// the conversion-only roll does not.
        /// </summary>
        private void UpdateMountedRiderFrame()
        {
            if (visualRoot == null || mountedSaddle == null || mountedRiderPivot == null) return;
            visualRoot.rotation = transform.rotation * mountedRiderRotationFromActor;
            visualRoot.position += mountedSaddle.position - mountedRiderPivot.position;
        }
        private void OnActiveMainChanged()
        {
            InvalidatePickModel();
            if (MountVnum != 0 && mountedSaddle != null) RefreshMountedRiderPivot();
        }
        /// <summary>
        /// SetShape replaces model instance PART_MAIN in the original client.  Creation and shape
        /// changes subsequently call __AttachHorseSaddle, so the saddle always owns the current
        /// model instance (InstanceBase.cpp:758-839, 2824-2833).  Unity armour replacement creates
        /// a new skeleton; refresh the cached Bip01 at that same lifecycle edge.
        /// </summary>
        private void RefreshMountedRiderPivot()
        {
            Transform activeSkeleton = equipment != null && equipment.ActiveSkeletonRoot != null
                ? equipment.ActiveSkeletonRoot : visualRoot;
            Transform riderPivot = activeSkeleton != null ? FindTransform(activeSkeleton, "Bip01") : null;
            if (riderPivot == null)
            {
                mountedRiderPivot = null;
                Debug.LogWarning($"METIN2_RIDER_PIVOT_MISSING vid={Vid} mount={MountVnum} " +
                                 $"skeleton={(activeSkeleton != null ? activeSkeleton.name : "<null>")}");
                return;
            }
            mountedRiderPivot = riderPivot;
            UpdateMountedRiderFrame();
        }
        private static Transform FindTransform(Transform root, string name) { foreach (Transform value in root.GetComponentsInChildren<Transform>(true)) if (value.name.Equals(name, System.StringComparison.OrdinalIgnoreCase)) return value; return null; }
        /// <summary>
        /// The horse is a second actor whose motion the original only touches when the rider's own
        /// motion changes (__SetMotion forwards to m_pkHorse->SetMotion, ActorInstanceMotion.cpp
        /// :705-716). Re-issuing the same state restarts its clip, so the state it is already in is
        /// remembered and skipped - otherwise a mounted actor's gallop stuttered once per GC_MOVE.
        /// </summary>
        private string mountState;
        public string CurrentMountMotion => mountState;
        /// <summary>Monotonic test/diagnostic counter for accepted horse motion edges.</summary>
        public int MountMotionPlaybackSerial => mountMotionPlaybackSerial;
        private void PlayMount(string state, bool restart = false)
        {
            if (mountAnimator == null || (!restart && state == mountState)) return;
            string animatorState = ResolveMountAnimatorState(state);
            int hash = Animator.StringToHash(animatorState);
            if (!mountAnimator.HasState(0, hash))
            {
                // Some custom mounts provide WAIT/RUN but no attack clip. Do not retain RUN
                // beneath a stationary attacking rider when the requested attack is unavailable.
                if(state=="20"||state=="20_1")PlayMount("00",restart);
                return;
            }
            mountState = state;
            mountMotionPlaybackSerial++;
            mountAnimator.speed = MountPlaybackRatio(state);
            if (GetComponent<Metin2PlayerController>() != null)
                Debug.Log($"METIN2_MOUNT_MOTION mount={MountVnum} legacy={state} animator={animatorState} " +
                          $"move_speed={MoveSpeed} attack_speed={AttackSpeed} playback={mountAnimator.speed:0.000}");
            mountAnimator.CrossFadeInFixedTime(hash, .1f, 0, 0f);
        }

        private string ResolveMountAnimatorState(string legacyState)
        {
            if (!Metin2RaceDataBinder.TryGetEntry(MountVnum, out Metin2RaceDataCatalog.Entry data) || data == null)
                return legacyState;
            string type = null;
            switch (legacyState)
            {
                case "00": type = Metin2RaceDataCatalog.MotionWait; break;
                case "02": type = Metin2RaceDataCatalog.MotionWalk; break;
                case "03": type = Metin2RaceDataCatalog.MotionRun; break;
                case "20":
                case "20_1": type = Metin2RaceDataCatalog.MotionNormalAttack; break;
                case "25": type = Metin2RaceDataCatalog.MotionSpecial2; break;
                case "30": type = Metin2RaceDataCatalog.MotionDamage; break;
                case "31": type = Metin2RaceDataCatalog.MotionDead; break;
            }
            string resolved = type != null ? data.StateFor(type) : null;
            if (string.IsNullOrEmpty(resolved) && legacyState == "02")
                resolved = data.StateFor(Metin2RaceDataCatalog.MotionRun);
            if (string.IsNullOrEmpty(resolved) && legacyState == "03")
                resolved = data.StateFor(Metin2RaceDataCatalog.MotionWalk);
            return string.IsNullOrEmpty(resolved) ? legacyState : resolved;
        }

        private float MountPlaybackRatio(string state)
        {
            float serverRatio = 1f;
            if (state == "02" || state == "03") serverRatio = MoveSpeed / 100f;
            else if (state == "20" || state == "20_1") serverRatio = AttackSpeed / 100f;

            float locomotionCorrection = 1f;
            float authoredTimingCorrection = 0f;
            Metin2MotionDefinition activeDefinition = null;
            if (Metin2RaceDataBinder.TryGetEntry(MountVnum, out Metin2RaceDataCatalog.Entry raceData) &&
                raceData != null)
            {
                string motionType = null;
                switch (state)
                {
                    case "00": motionType = Metin2RaceDataCatalog.MotionWait; break;
                    case "02": motionType = Metin2RaceDataCatalog.MotionWalk; break;
                    case "03": motionType = Metin2RaceDataCatalog.MotionRun; break;
                    case "20":
                    case "20_1": motionType = Metin2RaceDataCatalog.MotionNormalAttack; break;
                    case "25": motionType = Metin2RaceDataCatalog.MotionSpecial2; break;
                    case "30": motionType = Metin2RaceDataCatalog.MotionDamage; break;
                    case "31": motionType = Metin2RaceDataCatalog.MotionDead; break;
                }
                activeDefinition = motionType != null ? raceData.DefinitionFor(motionType) : null;
                if (activeDefinition == null && state == "02")
                    activeDefinition = raceData.DefinitionFor(Metin2RaceDataCatalog.MotionRun);
                if (activeDefinition == null && state == "03")
                    activeDefinition = raceData.DefinitionFor(Metin2RaceDataCatalog.MotionWalk);
                if ((state == "02" || state == "03") &&
                    Metin2CharacterMotionSet.TryGetDefinitionTravelSpeed(activeDefinition, out float authoredSpeed))
                {
                    float minimumSpeed = state == "02" ? 3.75f : 7.5f;
                    locomotionCorrection = Mathf.Max(1f, minimumSpeed / authoredSpeed);
                }
            }

            // Mount animation is played on its own Animator instead of through
            // Metin2CharacterMotionSet.PlayMotion.  It therefore missed that path's Granny 30-fps
            // correction: Blender-exported FBXs declare the scene's 24 fps and ran at 24/30 speed.
            // Resolve the semantic state first because custom mounts do not necessarily use the
            // legacy 00/02/03 state names.
            string animatorState = ResolveMountAnimatorState(state);
            RuntimeAnimatorController controller = mountAnimator != null
                ? mountAnimator.runtimeAnimatorController : null;
            if (controller != null && !string.IsNullOrEmpty(animatorState))
            {
                foreach (AnimationClip clip in controller.animationClips)
                {
                    if (clip == null || clip.frameRate <= 0f) continue;
                    string label = clip.name;
                    int bar = label.LastIndexOf('|');
                    if (bar >= 0) label = label.Substring(bar + 1);
                    if (!label.Equals(animatorState, System.StringComparison.OrdinalIgnoreCase) &&
                        !clip.name.EndsWith(animatorState, System.StringComparison.OrdinalIgnoreCase))
                        continue;
                    // Native motion nodes run for the .msa MotionDuration. Converted FBXs can have
                    // both a misleading declared frame rate and padded take length, so 30/fps alone
                    // still produces slow motion. Make the complete Unity clip occupy the authored
                    // duration; fall back to the Granny 30-fps correction only when no duration was
                    // parsed for this state.
                    authoredTimingCorrection = activeDefinition != null && activeDefinition.sourceDuration > .0001f
                        ? clip.length / activeDefinition.sourceDuration
                        : 30f / clip.frameRate;
                    return authoredTimingCorrection * serverRatio * locomotionCorrection;
                }
            }
            return serverRatio * locomotionCorrection;
        }

        /// <summary>Applies character ADD/UPDATE actor clocks and forwards them to a mounted horse.</summary>
        public void ApplyActorSpeeds(ushort moveSpeed, ushort attackSpeed)
        {
            MoveSpeed = moveSpeed;
            AttackSpeed = attackSpeed;
            motions?.SetActorSpeeds(moveSpeed, attackSpeed);
            if (mountAnimator != null && (mountState == "02" || mountState == "03"))
                mountAnimator.speed = MountPlaybackRatio(mountState);
        }

        /// <summary>
        /// Mounted CActorInstance::__SetMotion sends the rider's resolved motion index to the horse
        /// in the same operation. Listening to the actual rider playback edge also covers ONCE
        /// fallback: the mounted WAIT that follows an attack immediately restores horse state 00.
        /// </summary>
        private void OnRiderMotionChanged(Metin2CharacterMotionSet.MotionType type, int motionIndex, string state)
        {
            if (MountVnum == 0) return;
            switch (type)
            {
                case Metin2CharacterMotionSet.MotionType.Wait: PlayMount("00", true); break;
                case Metin2CharacterMotionSet.MotionType.Move: PlayMount(motionIndex == 2 ? "02" : "03", true); break;
                case Metin2CharacterMotionSet.MotionType.Attack:
                case Metin2CharacterMotionSet.MotionType.Combo:
                    // RaceManager.cpp maps COMBO_ATTACK/1/2 to 14/15/16; pony/motlist.txt maps
                    // those exact indices to 20/20_1/20. Each rider __SetMotion is a horse motion
                    // edge even when the resolved horse state repeats (16 -> 14 across cycles).
                    PlayMount(motionIndex == 15 ? "20_1" : "20", true); break;
                case Metin2CharacterMotionSet.MotionType.Damage: PlayMount("30", true); break;
                case Metin2CharacterMotionSet.MotionType.Die: PlayMount("31", true); break;
                case Metin2CharacterMotionSet.MotionType.Skill:
                    // Pony's exact SKILL2 state is 25. It accompanies the base horse charge;
                    // weapon-specific wild/splash rider motions stop the horse instead.
                    PlayMount(!string.IsNullOrEmpty(state) &&
                              state.IndexOf("charge", StringComparison.OrdinalIgnoreCase) >= 0 ? "25" : "00", true);
                    break;
            }
        }

        private void OnDestroy()
        {
#if UNITY_STANDALONE_WIN
            Metin2.Runtime.Metin2ActorPresentationScheduler.Remove(this);
#endif
            stonePresentation?.Dispose();
            if (motions != null) motions.MotionChanged -= OnRiderMotionChanged;
            if (equipment != null) equipment.ActiveMainChanged -= OnActiveMainChanged;
            MountPresentationChanged = null;
            // An equipment load still in flight must not dress a destroyed actor.
            partsGeneration++;
        }
        /// <summary>
        /// Equipment definitions became Addressable content, so the four parts an actor arrives
        /// with may not be resident yet. The parts are applied from whatever is already loaded
        /// only after one asynchronous attempt to fetch them; <paramref name="afterLoad"/> is what
        /// bounds that to a single retry, so an equipment asset that genuinely fails to load
        /// leaves the actor partially dressed instead of looping forever.
        /// </summary>
        private void ApplyParts(ushort[] parts) => ApplyParts(parts, false);

        private void ApplyParts(ushort[] parts, bool afterLoad)
        {
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.ApplyParts");
            try { ApplyPartsCore(parts, afterLoad); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); }
        }

        private void ApplyPartsCore(ushort[] parts, bool afterLoad)
        {
            if(parts!=null&&parts.Length>6&&Race<8)
            {
                var costumeEffects=GetComponent<Metin2CostumeEffectAppearance>()??gameObject.AddComponent<Metin2CostumeEffectAppearance>();
                costumeEffects.Apply(parts[5],parts[6],(byte)Race,MountVnum!=0);
            }
            if (parts != null && parts.Length > 4 && Race < 8)
            {
                var sash = GetComponent<Metin2SashAppearance>() ?? gameObject.AddComponent<Metin2SashAppearance>();
                sash.Apply(parts[4], (byte)Race, MountVnum != 0);
            }
            // Auriga extends CHR_EQUIPPART_NUM to eight. Index 7 is PART_RUNE and drives the
            // registered EFFECT_RUNE independently of armour/weapon/hair loading.
            if (parts != null && parts.Length > 7)
            {
                Metin2RuneCharacterEffect rune = GetComponent<Metin2RuneCharacterEffect>() ??
                                                  gameObject.AddComponent<Metin2RuneCharacterEffect>();
                rune.Apply(parts[7]);
            }
            if (equipment == null || equipmentCatalog == null || parts == null || parts.Length < 4 || Race >= 8) return;
            if (!afterLoad && !equipmentCatalog.ArePartsLoaded((byte)Race, parts))
            {
                // Deliberately before appliedParts is written: the retry must not be swallowed by
                // the unchanged check below.
                EnsurePartsLoadedAsync(parts, ++partsGeneration);
                return;
            }
            // ChangeArmor/ChangeWeapon/ChangeHair in the original return before RefreshState when
            // the received part is already equipped. GC_CHARACTER_UPDATE repeats all four parts for
            // affect-only changes, so rebuilding identical equipment here cancelled remote buff
            // motions by calling PlayIdle below. Mount participates in the identity because it can
            // change weapon hand/socket rules even when the item VNUMs stay the same.
            bool unchanged = appliedParts != null && appliedParts.Length >= 4 && appliedPartsMount == MountVnum;
            for (int index = 0; unchanged && index < 4; index++) unchanged = appliedParts[index] == parts[index];
            if (unchanged) return;
            if (appliedParts == null || appliedParts.Length != 4) appliedParts = new ushort[4];
            System.Array.Copy(parts, appliedParts, 4);
            appliedPartsMount = MountVnum;
            // TPacketGCCharacterAdditionalInfo/Update awPart[CHR_EQUIPPART_ARMOR] is fed directly
            // to CInstanceBase::SetArmor, which first treats it as an item VNUM and only falls back
            // to SetShape(value) when no item exists (PythonNetworkStreamPhaseGameActor.cpp:162,
            // InstanceBase.cpp:2590-2610). Preserve that fallback for base shape 0.
            Metin2EquipmentCatalog.ItemEntry? armorItem = null;
            Metin2.Runtime.ArmorDefinition armor = null;
            float armorSpecular = 0f;
            if (equipmentCatalog.TryGetItem(parts[0], out Metin2EquipmentCatalog.ItemEntry remoteArmor) &&
                remoteArmor.kind == Metin2EquipmentCatalog.Kind.Armor &&
                equipmentCatalog.TryGetArmor((byte)Race, remoteArmor.appearance, out armor))
            {
                armorItem = remoteArmor;
                armorSpecular = remoteArmor.specularPower;
            }
            else equipmentCatalog.TryGetArmor((byte)Race, parts[0], out armor);
            equipment.EquipArmor(armor, armorSpecular, armorItem);
            equipment.SetArmorAppearance(armorItem.HasValue ? armorItem.Value.appearance : parts[0]);
            equipment.EquipHair(equipmentCatalog.TryGetHair((byte)Race, parts[3], out Metin2.Runtime.HairDefinition hair) ? hair : null);
            Metin2EquipmentCatalog.ItemEntry? weaponItem = null;
            Metin2.Runtime.WeaponDefinition weaponDefinition = null;
            if (equipmentCatalog.TryGetItem(parts[1], out Metin2EquipmentCatalog.ItemEntry weapon) &&
                weapon.kind == Metin2EquipmentCatalog.Kind.Weapon)
            {
                weaponItem = weapon;
                equipmentCatalog.TryGetWeapon(parts[1], out weaponDefinition);
            }
            if (weaponDefinition == null) itemCatalog?.TryGetToolDefinition(parts[1], out weaponDefinition);
            bool mounted = MountVnum != 0;
            equipment.EquipWeapon(weaponDefinition, mounted, weaponItem);
            InvalidatePickModel();
            // Remote actors switch motion mode from the same rule as the local player
            // (CInstanceBase::SetWeapon, UserInterface/InstanceBase.cpp:2908-2975).
            bool rod = !mounted && itemCatalog != null && itemCatalog.TryGet(parts[1], out Metin2ItemCatalog.Entry tool) &&
                       tool.itemType == Metin2ItemConstants.TypeRod;
            if (motions != null && (rod ? motions.SetMode(Metin2MotionMode.Fishing) :
                motions.SetModeForWeapon(weaponDefinition == null ? Metin2.Runtime.Metin2WeaponType.None : weaponDefinition.weaponType, mounted))) motions.PlayIdle();
        }
        private async void EnsurePartsLoadedAsync(ushort[] parts, int generation)
        {
            ushort[] requested = (ushort[])parts.Clone();
            try
            {
                // Map scope: a remote actor's equipment belongs to the world it is standing in and
                // is released with it, exactly like its race prefab.
                await equipmentCatalog.PreloadPartsAsync((byte)Race, requested, Metin2.Runtime.Metin2ContentScope.Map);
            }
            catch (System.OperationCanceledException) { return; }
            catch (System.Exception exception)
            {
                Debug.LogWarning($"METIN2_ENTITY_EQUIPMENT_LOAD_FAILED vid={Vid} race={Race} {exception.Message}");
            }
            // The actor was despawned, the map changed, or newer parts already arrived.
            if (this == null || generation != partsGeneration) return;
            UnityEngine.Profiling.Profiler.BeginSample("Metin2.Entity.ApplyPartsAfterLoad");
            try { ApplyParts(requested, true); }
            finally { UnityEngine.Profiling.Profiler.EndSample(); }
        }

        private void ApplyNameLabel()
        {
            if (string.IsNullOrEmpty(DisplayName))
            {
                if (compatibilityNameLabel != null) compatibilityNameLabel.gameObject.SetActive(false);
                ScreenTextTail?.SetVisibility(false, false);
                return;
            }
            if(!lazyCompatibilityName||Type==6||compatibilityNameLabel!=null)UpdateCompatibilityNameLabel();
            if (ScreenTextTail == null)
            {
                ScreenTextTail = gameObject.AddComponent<Metin2ActorTextTailView>();
                ScreenTextTail.Initialize(Type, null, textTailLocale);
            }
            ScreenTextTail.SetData(DisplayName, CharacterLevel, Alignment, Empire, guildName, LanguageCode, GuildId);
            ScreenTextTail.SetClientPlatform(ClientPlatform);
            ApplyClientNameColor();
            RefreshNameVisibility();
        }
        void UpdateCompatibilityNameLabel()
        {
            if (compatibilityNameLabel == null)
            {
                var label = new GameObject("NameLabel");
                // This is compatibility state, never the rendered name. Do not
                // activate its native 3D font mesh just to immediately hide it.
                if(Metin2ActorTextTailView.SpawnUiPooling)label.SetActive(false);
                label.transform.SetParent(transform, false); compatibilityNameLabel = label.AddComponent<TextMesh>(); compatibilityNameLabel.anchor = TextAnchor.LowerCenter; compatibilityNameLabel.alignment = TextAlignment.Center; compatibilityNameLabel.fontSize = 48; compatibilityNameLabel.characterSize = 0.025f; compatibilityNameLabel.color = Color.white;
            }
            Renderer[] renderers = GetComponentsInChildren<Renderer>(true); float localTop = 2f;
            if (renderers.Length > 0) { Bounds bounds = renderers[0].bounds; for (int i = 1; i < renderers.Length; i++) bounds.Encapsulate(renderers[i].bounds); localTop = transform.InverseTransformPoint(bounds.max).y + 0.15f; }
            compatibilityNameLabel.transform.localPosition = new Vector3(0f, localTop, 0f); compatibilityNameLabel.text = DisplayName;
            // Kept as a state-compatible object for existing integrations; rendering belongs to
            // CPythonTextTail's screen-space equivalent below.
            Renderer compatibilityRenderer = compatibilityNameLabel.GetComponent<Renderer>();
            if (compatibilityRenderer != null) compatibilityRenderer.enabled = false;
            compatibilityNameLabel.gameObject.SetActive(false);
        }
        /// <summary>
        /// Yaw correction between the mesh's forward axis and the travel direction.
        ///
        /// Model-only yaw correction. Packet/source angle conversion is handled separately; moving
        /// actors use their measured Unity travel vector, stationary actors use ActorYaw.
        /// </summary>
        [SerializeField] private float modelYawOffset;
        public float ModelYawOffset { get => modelYawOffset; set => modelYawOffset = value; }

        /// <summary>
        /// TPacketGCMove.bFunc: 0 wait, 1 move, 2 attack, 3 combo, 4 mob skill, 5 skill, 6 dead.
        /// The previous version played the run animation for every function, so an attacking or dying
        /// monster still ran on the spot, and it took the server rotation verbatim, which left the
        /// actors spinning instead of facing the way they travel.
        /// </summary>
        public void QueueMove(Vector3 position, float rotation, float seconds, byte function, byte argument,
            uint commandTime, uint receiveTime)
        {
            float observedBaseSpeed=lastObservedServerBaseSpeed;
            if(Type==0||Type==1)
            {
                // NetworkActorManager::SNetworkActorData maintains an independent
                // server source/destination path. Never derive server speed from
                // the lagging visual transform after each retarget/correction.
                Vector3 source=transform.position;
                if(hasServerMoveSegment)
                    source=EstimatedServerPosition(commandTime);
                Vector3 travel=position-source;travel.y=0;
                // Server AI Goto sends FUNC_WAIT with destination + full travel
                // duration (char_state.cpp), not only FUNC_MOVE. WAIT therefore
                // can START a chase; it is a stop only when its path has ended.
                bool serverTravelling=function==1||(function==0&&seconds>=.05f&&travel.sqrMagnitude>.0004f);
                if(serverTravelling&&seconds>=.05f&&travel.sqrMagnitude>.0004f)
                    observedBaseSpeed=travel.magnitude/seconds/(Mathf.Max(1f,MoveSpeed)/100f);
                serverMoveSource=serverTravelling?source:position;serverMoveTarget=position;
                serverMoveSeconds=serverTravelling?Mathf.Max(0,seconds):0;
                serverMoveTime=commandTime;hasServerMoveSegment=true;
                if(Metin2NetworkEntityWorld.MobPositionTraceEnabled&&Type==0)
                    Debug.Log($"METIN2_MOB_SEGMENT vid={Vid} race={Race} func={function} cmd={commandTime} recv={receiveTime} seconds={seconds:F4} serverDistanceM={travel.magnitude:F3} visualDistanceM={Vector3.Distance(transform.position,position):F3} moveRatio={MoveSpeed} diagnosticEstimatedMps={observedBaseSpeed*MoveSpeed/100f:F3} actualLocomotionMps={ResolveNetworkLocomotionSpeed(0):F3}");
                lastObservedServerBaseSpeed=observedBaseSpeed;
            }
            // CInstanceBase::PushTCPState (InstanceBase.cpp:1196-1228): smooth the observed
            // command-to-receive gap per actor, then schedule this command against that stable gap.
            // This is the native jitter buffer.  It intentionally uses signed wrap-safe DWORD
            // subtraction because both clocks wrap together after roughly 49.7 days.
            int networkGap = unchecked((int)(receiveTime - commandTime));
            averageNetworkGapMilliseconds =
                (averageNetworkGapMilliseconds * 70 + networkGap * 30) / 100;
            uint checkTime = unchecked(commandTime + (uint)averageNetworkGapMilliseconds);
            remoteMoveCommands.Enqueue(new RemoteMoveCommand
            {
                Position = position,
                Rotation = rotation,
                PacketDuration = seconds,
                Function = function,
                Argument = argument,
                CommandTime = commandTime,
                CheckTime = checkTime
            });

            if (Metin2.Runtime.CombatProof.Enabled && Type == 6)
                Debug.Log($"METIN2_REMOTE_MOVE_QUEUE vid={Vid} cmd={commandTime} recv={receiveTime} " +
                          $"gap={networkGap} average={averageNetworkGapMilliseconds} check={checkTime} " +
                          $"queued={remoteMoveCommands.Count}");
        }

        private static bool HasServerTimeReached(uint current, uint targetTime) =>
            unchecked((int)(current - targetTime)) >= 0;

        private void ProcessRemoteMoveCommands(uint serverTime)
        {
            // CInstanceBase::StateProcess drains every command whose check time has elapsed before
            // MovementProcess advances the actor for this frame.
            while (remoteMoveCommands.Count > 0 &&
                   HasServerTimeReached(serverTime, remoteMoveCommands.Peek().CheckTime))
            {
                RemoteMoveCommand command = remoteMoveCommands.Dequeue();
                lastProcessedMoveCommandTime = command.CommandTime;
                // InstanceBase.cpp:1298-1330: StateProcess pops the command FIRST and only then
                // asks __CanProcessNetworkStatePacket. A refusal therefore discards the popped
                // command and RETURNS - the rest of the queue waits for a later frame instead of
                // being drained into an actor that must not be repositioned.
                if (!CanProcessNetworkStatePacket())
                {
                    if (Metin2Diagnostics.Verbose && push.IsBlending)
                        Debug.Log($"METIN2_PUSH_HELD vid={Vid} dropped_func={command.Function} " +
                                  $"motion={(motions != null ? motions.CurrentMotionType.ToString() : "<null>")} " +
                                  $"queued={remoteMoveCommands.Count}");
                    return;
                }
                ApplyMove(command.Position, command.Rotation, command.PacketDuration,
                    command.Function, command.Argument);
            }
        }

        /// <summary>
        /// CInstanceBase::__CanProcessNetworkStatePacket (InstanceBase.cpp:1254-1265). A dead or
        /// KNOCKED DOWN actor refuses network state packets outright.
        ///
        /// This is what lets a knocked-back monster stay where it landed. The server does NOT move
        /// a victim that was thrown back - the shove is client-side presentation in the original
        /// too - so the next GC_MOVE names the unmoved server position, and processing it retargets
        /// the actor there and yanks it straight back the moment it has landed. TYPE_KNOCKDOWN is
        /// DAMAGE_FLYING / DAMAGE_FLYING_BACK only (RaceMotionData.cpp:52-55); STAND_UP is
        /// TYPE_STANDUP and does accept packets again, which is how the victim rejoins the server's
        /// idea of where it is once it is back on its feet.
        ///
        /// Omitted arm, stated: the original also refuses while IsUsingSkill() and the skill cannot
        /// be cancelled. This port has no CanCancelSkill equivalent, and adding a blanket skill
        /// refusal would change remote skill movement that behaves correctly today.
        /// </summary>
        private bool CanProcessNetworkStatePacket()
            => !IsDead &&
               (motions == null || motions.CurrentMotionType != Metin2CharacterMotionSet.MotionType.KnockDown);

        public void ApplyMove(Vector3 position, float rotation, float seconds, byte function, byte argument)
        {
            ReportPushInterference($"move:func{function}", position, seconds);
            // Repeated WAIT/attack packets can name the same endpoint while the
            // actor is still turning toward it. Keep the original arrival boundary
            // for that approach; only a new destination starts a new one.
            bool continuingMobApproach = (Type == 0 || Type == 1) && moving && duration > 0f &&
                target.x == position.x && target.z == position.z;
            from = transform.position;
            target = new Vector3(position.x, position.y, position.z);
            fromRotation = transform.rotation;

            Vector3 travel = target - from; travel.y = 0f;
            if ((Type == 0 || Type == 1) && !continuingMobApproach) ResetMobArrivalProgress(from);
            float packetYaw = Metin2CoordinateSpace.ActorYaw(rotation);
            float heading = travel.sqrMagnitude > 0.0004f
                ? Quaternion.LookRotation(travel, Vector3.up).eulerAngles.y
                : packetYaw;
            targetRotation = Quaternion.Euler(0f, heading + modelYawOffset, 0f);

            // CInstanceBase::SetAdvancingRotation + MovementProcess: while a remote actor walks,
            // advancing rotation comes from SRC->DST, not from GC_MOVE.bRot. Turns larger than 45
            // degrees use the native maximum (1200 deg/s on foot, 300 deg/s mounted); smaller
            // turns use 5/12 of that maximum (500 / 125 deg/s respectively).
            // The packet rotation is retained as m_fDstRot and applied after arrival for WAIT/attack
            // style commands. FUNC_MOVE keeps the advancing heading.
            float logicalCurrent = Mathf.Repeat(transform.eulerAngles.y - modelYawOffset, 360f);
            float turn = Mathf.Abs(Mathf.DeltaAngle(logicalCurrent, heading));
            float maxTurnSpeed = MountVnum != 0 ? 300f : 1200f;
            moveTurnSpeedDegreesPerSecond = turn > 45f ? maxTurnSpeed : maxTurnSpeed * 5f / 12f;
            packetFinalYaw = packetYaw;
            applyPacketYawOnArrival = function != 1;

            if (Type == 6)
            {
                Metin2NetworkDiagnostics.Event(0,
                    $"REMOTE_PC_MOVE vid={Vid} func={function} distance_m={travel.magnitude:F3} duration_s={seconds:F3} " +
                    $"heading={heading:F1} packet_yaw={packetYaw:F1} model_offset={modelYawOffset:F1} " +
                    $"turn_deg={turn:F1} turn_speed={moveTurnSpeedDegreesPerSecond:F0}");
            }

            Vector3 previousNetworkVelocity = networkSegmentVelocity;
            // StateProcess handles FUNC_MOVE identically for PCs and mobs. The
            // native movement vector comes from RUN/WALK accumulation, never
            // distance / GC_MOVE.dwDuration (a late short duration is not a speed).
            continuousNetworkMove = function == 1 && Type == 6;
            float travelSeconds = seconds;
            if (function==1)
            {
                // GC_MOVE.dwDuration is server bookkeeping, not the remote render clock. Native
                // MovementProcess advances through the authored RUN/WALK accumulation at the
                // actor's move-speed ratio. Reconstruct that same duration from source motion.
                float locomotionSpeed = ResolveNetworkLocomotionSpeed(previousNetworkVelocity.magnitude);
                if (travel.sqrMagnitude > .0004f)
                    networkSegmentVelocity = travel.normalized * locomotionSpeed;
                else if (previousNetworkVelocity.sqrMagnitude > .0001f)
                    networkSegmentVelocity = previousNetworkVelocity.normalized * locomotionSpeed;
                else
                    networkSegmentVelocity = Heading * locomotionSpeed;
                travelSeconds = travel.magnitude / Mathf.Max(.01f, locomotionSpeed);
            }
            // FUNC_WAIT / ATTACK / COMBO packets carry zero duration, and the original does NOT
            // teleport across the gap. InstanceBase.cpp:1339-1373 (WAIT, threshold 1 native cm),
            // :1402-1430 (COMBO, 50 cm) and :1433-1470 (ATTACK, 50 cm) all take
            // NEW_SetSrcPixelPosition(current) + NEW_SetDstPixelPosition(dst) + StartWalking() and
            // let the actor cover the WHOLE distance at its own locomotion speed. Only inside the
            // threshold do they SCRIPT_SetPixelPosition straight onto the destination.
            //
            // The fixed 0.2 s catch-up cap that used to stand here is what undid every shove: a
            // 6.3 m knock-back was reeled back in under a fifth of a second, some 30 m/s. Measured,
            // not guessed - METIN2_PUSH_INTERRUPTED source=move:func2 back_m=6.26 seconds=0.00.
            else if ((travelSeconds <= .001f || Type==0 || Type==1) && travel.sqrMagnitude > .0004f)
            {
                float threshold = function == 0 ? .01f : ActionArrivalThresholdMetres;
                if (travel.magnitude <= threshold) travelSeconds = .02f;
                else
                {
                    float speed = ResolveNetworkLocomotionSpeed(previousNetworkVelocity.magnitude);
                    travelSeconds = Mathf.Max(.02f, travel.magnitude / Mathf.Max(.01f, speed));
                }
            }
            movementElapsed = 0f;
            duration = Mathf.Max(0.001f, travelSeconds);
            if (!continuousNetworkMove) networkSegmentVelocity = travel / duration;
            segmentMoveSpeedRatio=Mathf.Max(1f,MoveSpeed)/100f;

            // EMoveFuncType: WAIT 0, MOVE 1, ATTACK 2, COMBO 3, MOB_SKILL 4, EMOTION 5;
            // player skills are FUNC_SKILL (0x80) OR-ed with their motion index.
            bool skill = (function & 0x80) != 0;
            bool movingSkill = skill && (argument & 0x10) != 0;
            bool attacking = skill || function == 2 || function == 3 || function == 4;
            // m_kMovAfterFunc (InstanceBase.cpp:1402-1470 and the arrival switch at :1650-1730):
            // FUNC_ATTACK and FUNC_COMBO run their motion only when the actor is ALREADY within
            // 50 native centimetres of the packet position. Further out the original stores the
            // action, walks the whole way, and runs it on arrival. Playing the attack immediately
            // while sliding the actor across the gap is not the original behaviour.
            bool walkToAction = attacking && !movingSkill &&
                                travel.magnitude >= ActionArrivalThresholdMetres;
            pendingActionFunction = walkToAction ? function : NoPendingAction;
            pendingActionArgument = argument;
            // Locomotion is decided by whether the actor really travels, not by the function byte.
            // Monsters approach with function values that are not FUNC_MOVE, and keying the animation
            // off the packet alone left them sliding in the idle pose.
            moving = walkToAction ||
                     (!attacking && (continuousNetworkMove || travel.sqrMagnitude > 0.0004f));

            if (attacking && !walkToAction)
            {
                lastCombatPresentationAt = Time.time;
                bool played = false;
                if (motions != null)
                {
                    if (skill) played = motions.PlaySkillMotion(function & 0x7f);
                    else if (function == 3) played = motions.PlayComboMotionArgument(argument);
                    else if (function == 4) played = motions.PlaySpecial(argument);
                    else played = motions.PlayAttack();
                    if (!played) motions.PlayIdle(MountVnum != 0);
                }
                if (movingSkill)
                {
                    remoteMovingSkillEndsAt = Time.time + Mathf.Max(.05f,
                        motions != null && motions.LastPlaybackSeconds > 0f ? motions.LastPlaybackSeconds : 2f);
                    ApplyMountMovingSkill(true);
                }
            }
            else if (moving)
            {
                // FUNC_MOVE, CInstanceBase::NEW_Move (InstanceBase.cpp:1376-1399):
                //
                //     if (!IsWalking())
                //         StartWalking();
                //     else
                //         // "already walking"
                //
                // The guard is the point, not decoration. A running actor gets a GC_MOVE about
                // every 300 ms, and playing the run again restarts the clip from zero each time -
                // the same jolt in the legs the local player's own run had.
                if (motions != null && !motions.IsMoveMotion)
                {
                    bool played = IsWalkMode ? motions.PlayWalk() : motions.PlayMove(MountVnum != 0);
                    if (!played) motions.PlayIdle(MountVnum != 0);
                }
                ApplyMountLocomotion(true, IsWalkMode);
            }
            else
            {
                // FUNC_WAIT (:1367-1369): if (!IsWaiting()) EndWalking() - and nothing when the
                // actor is already standing.
                motions?.StopIntoWait(MountVnum != 0);
                ApplyMountLocomotion(false, IsWalkMode);
            }
        }

        private float ResolveNetworkLocomotionSpeed(float observedFallback)
        {
            // A GC_MOVE contains an endpoint, not an authoritative source. AI
            // retargets, sync and missing earlier packets invalidate distance /
            // duration inference. Never use that diagnostic estimate as speed.
            if (motions != null && motions.TryGetLocomotionSpeed(IsWalkMode, out float authoredSpeed) &&
                authoredSpeed > .01f)
                return authoredSpeed * Mathf.Max(1f, MoveSpeed) / 100f;
            // A mob's previous packet-derived velocity can already be the bad
            // catch-up speed. Never carry that error into the next correction.
            if (Type==6 && observedFallback > .1f) return observedFallback;
            float sourceFallback = IsWalkMode
                ? 3f
                : Metin2CharacterMotionSet.OriginalPlayerGeneralRunMetresPerSecond;
            return sourceFallback * Mathf.Max(1f, MoveSpeed) / 100f;
        }

        /// <summary>GC_CHARACTER_DEAD (header 14) is what actually kills an actor.</summary>
        public void ApplyDead()
        {
            if(IsDead)return;
            if(affectEffects!=null&&affectEffects.IsAffectFlagSet(2))PlayLifecycleEffect("effect/etc/appear_die/monster_die.mse");
            moving = false; duration = 0f; IsDead = true; pendingActionFunction = NoPendingAction;
            // CInstanceBase::Die clears a stone's smoke before CActorInstance::Die starts the dead
            // motion. Doing it here avoids leaving the damage smoke alive for one extra frame.
            GetComponent<Metin2StoneSmokeEffects>()?.ClearForDeath();
            if (motions != null && !motions.PlayDead()) motions.PlayIdle(false);
        }

        /// <summary>Re-applies the systemSetting presentation flags to existing text tails.</summary>
        public void RefreshClientOptionVisibility()
        {
            Metin2ClientSettings settings = Metin2ClientSettings.Current;
            // InstanceBase::Update (InstanceBase.cpp:2199-2239): the 1/2/4 hide boxes make mounts,
            // pets and the offline-shop races invisible by re-asserting AFFECT_INVISIBILITY. The
            // actor keeps updating; only its rendering and text tail disappear.
            bool optionHidden = (settings.IsHidden(0) && Type == 1 && IsMountRace(Race)) ||
                                (settings.IsHidden(1) && Type == 1 && IsPetRace(Race)) ||
                                (settings.IsHidden(3) && Race >= 30000 && Race <= 30007);
            if (optionHidden && affectEffects == null)
            {
                affectEffects = gameObject.AddComponent<Metin2AffectEffects>();
                affectEffects.IsMainInstance = false;
            }
            if (affectEffects != null) affectEffects.OptionHidden = optionHidden;
            if (ShopSignBoard != null)
                ShopSignBoard.SetActive(settings.ShowSalesText && !settings.IsHidden(4) &&
                    ShopSignLabel != null && !string.IsNullOrEmpty(ShopSignLabel.text));
            RefreshNameVisibility();
            ApplyClientNameColor();
        }

        // CActorInstance::IsMount (ActorInstance.cpp:319-368): standalone mount NPC races only -
        // a ridden horse is part of the PC actor and is unaffected by the hide box.
        private static readonly HashSet<ushort> MountRaces = new HashSet<ushort>
        {
            20101, 20102, 20103, 20104, 20105, 20106, 20107, 20108, 20109, 20111,
            20113, 20114, 20115, 20117, 20122, 20222, 20227, 20231, 20232, 20244,
            20245, 20246, 20247, 20248, 20249, 20250, 20251, 20252, 20261, 20262,
            20265, 20266, 20269, 20270, 60005, 60006, 60007, 60008,
        };

        // CActorInstance::IsPet (ActorInstance.cpp:370-442): the new pet system's 34041-34111 range
        // plus the explicit old pet races.
        private static readonly HashSet<ushort> PetRaces = new HashSet<ushort>
        {
            34005, 34006, 34007, 34008, 34009, 34010, 34011, 34012, 34013, 34014,
            34015, 34016, 34017, 34018, 34019, 34020, 34021, 34022, 34023, 34024,
            34026, 34027, 34028, 34029, 34030, 34031, 34032, 34033, 34034, 34035,
            34039, 34055, 34056, 34057, 34058, 34059, 34060, 34061, 34062, 34063,
            34066, 34084, 34085, 34093, 34094, 34114, 34115, 34116,
            34201, 34202, 34203, 34204, 34205, 34206, 34207,
        };

        private static bool IsMountRace(ushort race) => MountRaces.Contains(race);
        private static bool IsPetRace(ushort race) =>
            (race >= 34041 && race <= 34111) || PetRaces.Contains(race);

        public void SetClientNameHovered(bool value) { clientNameHovered = value; RefreshNameVisibility(); }
        public void SetClientNameTargeted(bool value) { clientNameTargeted = value; RefreshNameVisibility(); }
        /// <summary>Called by the affect renderer when /in, Eunhyeong or revive invisibility changes.</summary>
        public void RefreshAffectVisibility()
        {
            RefreshNameVisibility();
            if (ShopSignBoard != null && affectEffects != null && affectEffects.IsHardInvisible)
                ShopSignBoard.SetActive(false);
        }
        private void RefreshNameVisibility()
        {
            if (compatibilityNameLabel != null)
                compatibilityNameLabel.gameObject.SetActive(false);
            ScreenTextTail?.SetVisibility(
                Metin2ClientSettings.Current.AlwaysShowName || clientNameHovered || clientNameTargeted, CanPick);
        }

        private void ApplyClientNameColor()
        {
            Color32 color = nameColorResolver != null ? nameColorResolver(this) : IsKiller && Type == 6
                ? new Color32(180, 100, 0, 255)
                : Metin2ActorTextTailView.ResolveNameColor(Type, Empire, Metin2ClientSettings.Current.NameColorMode);
            if (compatibilityNameLabel != null) compatibilityNameLabel.color = color;
            ScreenTextTail?.SetNameColor(color);
        }

        public void ApplyDamage()
        {
            ApplyDamage(false);
        }

        public bool ApplyDamage(bool fromBehind)
        {
            // CActorInstance::__HitGood returns immediately for a knocked-down victim. Preserve the
            // reaction here as well: cancelling movement or falling back to idle would still break
            // PlayKnockDown's flying phase even if PlayDamage refused.
            if (motions != null &&
                motions.CurrentMotionType == Metin2CharacterMotionSet.MotionType.KnockDown)
                return false;
            // An actor locked into its own attack, combo or skill motion keeps playing it
            // (CActorInstance::isLock); falling back to the idle here would cancel the swing just
            // as surely as the flinch itself.
            if (motions != null && motions.IsLocked) return false;
            // CActorInstance::__SetMotion (ActorInstanceMotion.cpp:665-674) refuses all four damage
            // motions to an actor that is NOT the main character while a TYPE_MOVE motion is playing.
            // A running monster does not flinch in the original, and nothing about its movement
            // changes either - so the interpolation must not be cut short here.
            if (motions != null && motions.IsMoveMotion) return false;
            moving = false; duration = 0f;
            if (motions == null) return false;
            if (!motions.PlayDamage(fromBehind)) motions.PlayIdle(MountVnum != 0);
            return true;
        }

        public bool ApplyKnockDown(bool fromBehind)
        {
            pendingActionFunction = NoPendingAction;
            if (motions == null) return false;
            Metin2CharacterMotionSet.MotionType before = motions.CurrentMotionType;
            bool played = motions.PlayKnockDown(fromBehind, IsStunned);
            if (played) { moving = false; duration = 0f; }
            if (Metin2Diagnostics.Verbose)
                Debug.Log($"METIN2_PUSH_KNOCKDOWN vid={Vid} played={played} before={before} " +
                          $"after={motions.CurrentMotionType} locked={motions.IsLocked} move={motions.IsMoveMotion}");
            return played;
        }

        public void ApplyAttack(Transform victim, uint victimVid, byte attackType)
        {
            moving = false; duration = 0f; IsStunned = false; LastVictimVid = victimVid; LastAttackType = attackType;
            if (victim != null)
            {
                Vector3 direction = victim.position - transform.position; direction.y = 0f;
                if (direction.sqrMagnitude > .0004f) transform.rotation = Quaternion.LookRotation(direction, Vector3.up) * Quaternion.Euler(0f, modelYawOffset, 0f);
            }
            // GC_ATTACK is the authoritative hit, but GC_MOVE/FUNC_COMBO or FUNC_SKILL already
            // started the visible motion. Replaying here restarted the animation at impact time.
            if (Time.time - lastCombatPresentationAt > .75f && motions != null && !motions.PlayAttack())
                motions.PlayIdle(MountVnum != 0);
        }

        public void ApplyStun()
        {
            // CInstanceBase::Stun calls NEW_Stop before setting the actor's stun flag
            // (InstanceBaseBattle.cpp:683-688). NEW_Stop ends walking unless a locked attack or a
            // moving skill owns the motion. Merely cancelling our interpolation left the Animator's
            // RUN loop alive, which is the visible "mob running in place" failure.
            if (motions != null && !motions.IsLocked && !motions.IsSkillMotion)
                motions.StopIntoWait(MountVnum != 0);
            ApplyMountLocomotion(false, IsWalkMode);
            moving = false; duration = 0f; IsStunned = true;
        }

        public void ApplySpeed(ushort movingSpeed) { CurrentMovingSpeed = movingSpeed; ApplyActorSpeeds(movingSpeed,AttackSpeed); }

        public void ApplyMotion(ushort motion, uint victimVid)
        {
            // The target client RecvMotionPacket reads and resolves both actors but intentionally
            // performs no animation call. Keep the packet observable without inventing semantics.
            LastMotion = motion; LastVictimVid = victimVid;
        }
        public void ApplyDig(Transform targetActor, byte count)
        {
            moving = false; duration = 0f;
            if (targetActor != null)
            {
                Vector3 direction = targetActor.position - transform.position; direction.y = 0f;
                if (direction.sqrMagnitude > .0004f) transform.rotation = Quaternion.LookRotation(direction, Vector3.up) * Quaternion.Euler(0f, modelYawOffset, 0f);
            }
            // The server sends one GC_DIG_MOTION with count swings and reports the result after
            // the whole 2 * count second event. Play every requested swing, as the source client
            // does with count PushOnceMotion(NAME_DIG) calls.
            if (motions == null || !motions.PlayNamedRepeated("dig", count)) motions?.PlayIdle(MountVnum != 0);
        }
        public void ApplyFishing(byte subHeader, byte direction)
        {
            if (subHeader == 0)
                transform.rotation = Quaternion.Euler(0f,
                    Metin2CoordinateSpace.ActorYaw(direction * 5f) + modelYawOffset, 0f);
            motions?.ApplyFishingEvent(subHeader);
        }
        public bool FishingCastActive => motions != null && motions.FishingCastActive;
        /// <summary>
        /// GC_SYNC_POSITION is CNetworkActorManager::SyncActor/NEW_SyncPixelPosition. The original
        /// TEMP_Push corrects the actor's rendered position without clearing m_isGoing, the queued
        /// TCP states, or the active RUN motion. Preserve that lifecycle here: rebasing an active
        /// segment keeps it advancing, while an idle actor simply adopts the correction.
        /// </summary>
        public void ApplySyncPosition(Vector3 position)
        {
            if(Type==0||Type==1)
            {
                serverMoveSource=serverMoveTarget=position;serverMoveSeconds=0;
                serverMoveTime=motionWorld!=null?motionWorld.ServerTimeMilliseconds:unchecked((uint)Environment.TickCount);
                hasServerMoveSegment=true;
                // ActorInstanceSync::__Push: correction is a one-second ease-out
                // displacement, not a teleport followed by compressed remaining
                // movement time. Keep the queued TCP states and active walk alive.
                push.BlendPosition(transform.position,position,terrainWorld!=null?terrainWorld.IsBlocked:(Func<Vector3,bool>)null);
                return;
            }
            // CActorInstance::__Push (ActorInstanceSync.cpp:7-51) writes the corrected destination
            // into the SAME interpolator pair the shove uses, so an authoritative sync always
            // replaces an in-flight push rather than fighting it.
            //
            // Deviation, stated: the original blends to the sync position over one second
            // (SetBlendingPosition's default) and drops the correction entirely when the swept path
            // meets world collision. This port adopts the position directly, as it did before.
            ReportPushInterference("sync", position, 0f);
            push.Initialize();
            bool activeSegment = duration > 0f;
            float remaining = Mathf.Max(0.001f, duration - movementElapsed);
            transform.position = position;
            from = position;
            movementElapsed = 0f;
            if (activeSegment)
            {
                // FUNC_MOVE is open-ended between packets; non-continuous correction segments retain
                // only their unelapsed portion and still finish through their normal WAIT/action edge.
                if (!continuousNetworkMove) duration = remaining;
            }
            else
            {
                target = position;
                duration = 0f;
            }
        }

        private bool moving;
        private float segmentMoveSpeedRatio=1f;
        public bool IsNetworkMoving => moving;
        public bool HasActiveNetworkMovement => duration > 0f;
        private float remoteMovingSkillEndsAt;
        // StartWalking -> Move keeps the actor's current movement-speed ratio in the native client.
        // 100 is this port's 6 m/s baseline; remote GC speed updates therefore scale wildattack too.
        private float MovingSkillSpeedMetresPerSecond => 6f * Mathf.Max(1f, MoveSpeed) / 100f;
        public bool IsRemoteMovingSkillActive => Time.time < remoteMovingSkillEndsAt;
        public void AdvanceRemoteMovingSkillForValidation(float seconds)
        {
            if (seconds > 0f) ApplySkillRootMotion(Heading * (MovingSkillSpeedMetresPerSecond * seconds));
        }
        private void Update()
        {
            long sample = Metin2.Runtime.Metin2FrameProfiler.Begin();
            UpdateEntity();
            Metin2.Runtime.Metin2FrameProfiler.End(Metin2.Runtime.Metin2FrameProfiler.SlotEntities, sample);
        }

        private void UpdateEntity()
        {
            if(IsDeletingPresentation)return;
            ProcessRemoteMoveCommands(motionWorld != null
                ? motionWorld.ServerTimeMilliseconds
                : unchecked((uint)Environment.TickCount));
            if (remoteMovingSkillEndsAt > 0f)
            {
                if (Time.time < remoteMovingSkillEndsAt)
                    AdvanceRemoteMovingSkillForValidation(Time.deltaTime);
                else
                {
                    remoteMovingSkillEndsAt = 0f;
                    ApplyMountMovingSkill(false);
                }
            }
            // No interpolation is active until ApplyMove/ApplySyncPosition gives this entity a
            // segment.  This guard is especially important for the local-player helper entity used
            // only for mount presentation: it is added while the LocalPlayer root is still at the
            // GameObject default (0,0,0), then Metin2PlayerController places that same root on the
            // terrain.  Running the interpolation with duration == 0 used the stale zero target and
            // overwrote the correct spawn on the very next frame.
            AdvanceNetworkMovement(Time.deltaTime);
            AdvancePush(Time.deltaTime);
        }

        private readonly Metin2.Runtime.Metin2PushPhysics push = new Metin2.Runtime.Metin2PushPhysics();

        /// <summary>
        /// CActorInstance::__SetFallingDirection + CPhysicsObject::IncreaseExternalForce. The
        /// direction is __PushCircle's: attacker position to victim position, flattened and
        /// normalised.
        /// </summary>
        /// <summary>CActorInstance::IsPushing - m_PhysicsObject.isBlending.</summary>
        public bool IsPushing => push.IsBlending;
        public Vector3 PushSyncDestination { get; private set; }

        public bool BeginPush(Vector3 direction, float force)
        {
            Vector2 planar = new Vector2(direction.x, direction.z);
            if (planar.sqrMagnitude < .000001f) return false;
            planar.Normalize();
            Func<Vector3, bool> blocked = terrainWorld != null ? terrainWorld.IsBlocked : (Func<Vector3, bool>)null;
            // planned_m is a LOG value, and computing it re-ran the whole hundred-step swept
            // collision integration that IncreaseExternalForce runs immediately afterwards - every
            // push paid for the sweep twice. When a swing lands on a pack every mob is pushed in the
            // same frame, and the entity update measured 23-31 ms for 27 actors because of it.
            bool started = push.IncreaseExternalForce(planar, force, transform.position, blocked);
            if(started)
            {
                Vector2 planned=push.PlannedMovementMetres;
                PushSyncDestination=transform.position+new Vector3(planned.x,0f,planned.y);
            }
            pushDiagnosticVid = started ? Vid : 0u;
            pushStartedAt = Time.time;
            pushOrigin = transform.position;
            if (Metin2Diagnostics.Verbose)
            {
                Vector2 planned = Metin2.Runtime.Metin2PushPhysics.IntegrateMovement(
                                      planar, force, transform.position, blocked)
                                  * Metin2.Runtime.Metin2Units.CentimetreToMetre;
                Debug.Log($"METIN2_PUSH_BEGIN vid={Vid} started={started} force={force:F1} " +
                          $"planned_m={planned.magnitude:F2} motion={(motions != null ? motions.CurrentMotionType.ToString() : "<null>")} " +
                          $"duration={duration:F2} queued={remoteMoveCommands.Count}");
            }
            return started;
        }

        // Push diagnostics: one line per interference while a shove is being paid out, so the thing
        // that undoes it names itself instead of being guessed at.
        private uint pushDiagnosticVid;
        private float pushStartedAt;
        private Vector3 pushOrigin;

        private void ReportPushInterference(string source, Vector3 destination, float seconds)
        {
            if (!Metin2Diagnostics.Verbose || !push.IsBlending) return;
            Debug.LogWarning($"METIN2_PUSH_INTERRUPTED vid={Vid} source={source} " +
                             $"age={Time.time - pushStartedAt:F2}s moved_m={(transform.position - pushOrigin).magnitude:F2} " +
                             $"back_m={(destination - transform.position).magnitude:F2} seconds={seconds:F2} " +
                             $"motion={(motions != null ? motions.CurrentMotionType.ToString() : "<null>")}");
        }

        /// <summary>
        /// CActorInstance::PhysicsProcess (ActorInstance.cpp:454-458): every frame the actor adds
        /// the interpolator's delta to its own position with AddMovement.
        ///
        /// The network interpolation endpoints are shifted by the same delta, or the very next
        /// AdvanceNetworkMovement would recompute the position from the untouched segment and undo
        /// the shove frame after frame. In the native client AddMovement moves m_x/m_y themselves
        /// and the next TCP state re-syncs the actor, which is the same arrangement.
        /// </summary>
        private void AdvancePush(float deltaTime)
        {
            if (!push.IsBlending) return;
            Vector2 delta = push.Advance(deltaTime);
            if (Mathf.Abs(delta.x) < 1e-6f && Mathf.Abs(delta.y) < 1e-6f) return;
            Vector3 movement = new Vector3(delta.x, 0f, delta.y);
            Vector3 next = transform.position + movement;
            // isPhysicalCollision is already consulted while integrating; this is the per-frame
            // guard for collision that appeared after the shove began.
            if (terrainWorld != null && terrainWorld.IsBlocked(next)) { push.Initialize(); return; }
            if (terrainWorld != null) next.y = GetComponent<Metin2MountFlight>() is Metin2MountFlight f&&f.Flying?f.Altitude:terrainWorld.GroundHeight(next);
            transform.position = next;
            if(Type==0||Type==1)
            {
                from=next;
                if(duration<=0f)target=next;
                ResetMobArrivalProgress(next);
            }
            else { from += movement; target += movement; }
        }

        private void ResetMobArrivalProgress(Vector3 source)
        {
            mobMoveSource = source;
            Vector3 travel = target - source;
            travel.y = 0f;
            mobMoveDistanceSquared = travel.sqrMagnitude;
        }

        private void AdvanceNetworkMovement(float deltaTime)
        {
            if (duration <= 0f) return;

            if((Type==0||Type==1)&&moving)
            {
                // Re-solve from the actual corrected position. A sync/push must
                // not preserve a stale heading or turn remaining time into speed.
                Vector3 remaining=target-transform.position;remaining.y=0f;
                // Motion accumulation was resolved once by ApplyMove. Do not
                // traverse the motion catalog or allocate its source label per
                // mob per frame; apply live speed-ratio changes to that value.
                float speed=networkSegmentVelocity.magnitude*(Mathf.Max(1f,MoveSpeed)/100f)/segmentMoveSpeedRatio;
                if(remaining.sqrMagnitude>.0004f)
                    targetRotation=Quaternion.LookRotation(remaining,Vector3.up)*Quaternion.Euler(0f,modelYawOffset,0f);
                from=transform.position;movementElapsed=0f;
                duration=Mathf.Max(.001f,remaining.magnitude/Mathf.Max(.01f,speed));
            }

            movementElapsed += Mathf.Max(0f, deltaTime);
            float t = Mathf.Clamp01(movementElapsed / duration);
            Vector3 nextPosition;
            if (continuousNetworkMove)
            {
                float speed = networkSegmentVelocity.magnitude;
                float segmentDistance = Vector3.Distance(
                    new Vector3(from.x, 0f, from.z), new Vector3(target.x, 0f, target.z));
                float travelled = Mathf.Min(speed * movementElapsed,
                    segmentDistance + MaximumMoveExtrapolationMetres);
                Vector3 direction = speed > .001f
                    ? networkSegmentVelocity / speed
                    : Vector3.zero;
                nextPosition = from + direction * travelled;
                if (terrainWorld != null) nextPosition.y = GetComponent<Metin2MountFlight>() is Metin2MountFlight f&&f.Flying?f.Altitude:terrainWorld.GroundHeight(nextPosition);

                // Our one-metre prediction cap is a Unity-side safety boundary; the native actor
                // itself has no frozen transform paired with a live RUN loop. If no newer TCP state
                // arrives by the time the cap is exhausted, keeping the locomotion motion alive can
                // only produce the visible "mob runs in one place" state. Settle into WAIT here.
                // A normal 300 ms MOVE stream replaces this segment long before the extra metre is
                // consumed, so continuously updated actors never see this edge.
                bool predictionExhausted = travelled >= segmentDistance + MaximumMoveExtrapolationMetres - .001f;
                if (predictionExhausted && remoteMoveCommands.Count == 0)
                {
                    transform.position = nextPosition;
                    motions?.StopIntoWait(MountVnum != 0);
                    ApplyMountLocomotion(false, IsWalkMode);
                    moving = false;
                    continuousNetworkMove = false;
                    duration = 0f;
                    networkSegmentVelocity = Vector3.zero;
                    return;
                }
            }
            else
            {
                nextPosition = Vector3.Lerp(from, target, t);
            }
            transform.position = nextPosition;

            // FUNC_MOVE does not call EndWalking when its destination is reached. Between the
            // sender's 300 ms notifications the native actor continues at the current motion speed.
            // The position above therefore keeps advancing through the authored locomotion speed,
            // capped at the same one-metre correction boundary as native MovementProcess.

            // Original remote locomotion does not slerp over a fraction of packet duration.
            // MovementProcess rotates toward m_fAdvancingRotation at a fixed angular speed every
            // frame (1200/500 deg/s on foot, 300/125 deg/s mounted). Keep modelYawOffset visual-only.
            float targetYaw = targetRotation.eulerAngles.y;
            float currentYaw = transform.eulerAngles.y;
            float nextYaw = Mathf.MoveTowardsAngle(currentYaw, targetYaw,
                Mathf.Max(1f, moveTurnSpeedDegreesPerSecond) * Mathf.Max(0f, deltaTime));
            transform.rotation = Quaternion.Euler(0f, nextYaw, 0f);
            if((Type==0||Type==1)&&moving&&t<1f)
            {
                // Native RUN accumulation is rotated by the CURRENT actor
                // heading. Translating along the destination while a 180-degree
                // turn is still in progress makes a mob visibly run backwards.
                Vector3 step=nextPosition-from;step.y=0f;
                Vector3 alongFacing=from+Heading*step.magnitude;
                alongFacing.y=nextPosition.y;
                // Native MovementProcess detects arrival against the command's
                // fixed SRC->DST distance. The per-frame interpolation origin
                // above is rebased while turning; using it for arrival too let
                // short corrections orbit forever and kept arrival attacks queued.
                Vector3 before = from - mobMoveSource;
                Vector3 after = alongFacing - mobMoveSource;
                before.y = after.y = 0f;
                if (before.sqrMagnitude <= mobMoveDistanceSquared &&
                    after.sqrMagnitude >= mobMoveDistanceSquared)
                {
                    transform.position = target;
                    t = 1f;
                }
                else transform.position = alongFacing;
            }

            if (t < 1f || continuousNetworkMove) return;
            duration = 0f;
            if (applyPacketYawOnArrival)
            {
                // FUNC_WAIT and action packets preserve GC_MOVE.bRot as the destination-facing
                // rotation. The original applies/blends it after the positional segment completes.
                transform.rotation = Quaternion.Euler(0f, packetFinalYaw + modelYawOffset, 0f);
                fromRotation = targetRotation = transform.rotation;
            }
            if (moving) { motions?.StopIntoWait(MountVnum != 0); if (MountVnum != 0) PlayMount("00"); }
            moving = false;
            if (pendingActionFunction != NoPendingAction)
            {
                byte pending = pendingActionFunction, pendingArgument = pendingActionArgument;
                pendingActionFunction = NoPendingAction;
                RunArrivalAction(pending, pendingArgument);
            }
        }

        /// <summary>50 native centimetres - the FUNC_ATTACK / FUNC_COMBO arrival threshold.</summary>
        private const float ActionArrivalThresholdMetres = .5f;
        private const byte NoPendingAction = 0xFF;
        private byte pendingActionFunction = NoPendingAction, pendingActionArgument;

        /// <summary>
        /// m_kMovAfterFunc executed on arrival: the same motion selection ApplyMove would have run
        /// immediately for a nearby actor.
        /// </summary>
        private void RunArrivalAction(byte function, byte argument)
        {
            lastCombatPresentationAt = Time.time;
            if (motions == null) return;
            bool played;
            if ((function & 0x80) != 0) played = motions.PlaySkillMotion(function & 0x7f);
            else if (function == 3) played = motions.PlayComboMotionArgument(argument);
            else if (function == 4) played = motions.PlaySpecial(argument);
            else played = motions.PlayAttack();
            if (!played) motions.PlayIdle(MountVnum != 0);
        }

        /// <summary>Deterministic editor proof hook; production uses Update with the same path.</summary>
        public void AdvanceRemoteNetworkStateForValidation(uint serverTime, float deltaTime)
        {
            ProcessRemoteMoveCommands(serverTime);
            AdvanceNetworkMovement(deltaTime);
            AdvancePush(deltaTime);
        }
        private Metin2.Runtime.Metin2StaticStonePresentation stonePresentation;
#if UNITY_STANDALONE_WIN
        internal void UpdateStaticStonePresentation()
        {
            if(Type!=Metin2GamePacketCodec.EntityTypeStone)return;
            stonePresentation??=new Metin2.Runtime.Metin2StaticStonePresentation(transform);
            stonePresentation.Tick(!IsDead);
        }
        public static AnimatorCullingMode RemoteAnimationCulling(byte actorType,bool dead,float distanceSquared,bool hasGameplayTarget)
            =>actorType==0&&!dead&&hasGameplayTarget&&distanceSquared>30f*30f
                ?AnimatorCullingMode.CullUpdateTransforms:AnimatorCullingMode.AlwaysAnimate;
        private static int presentationCameraFrame=-1;
        private static Camera presentationCamera;
        private static Metin2FollowCamera presentationFollowCamera;
#endif
        private void LateUpdate()
        {
#if UNITY_STANDALONE_WIN
            if(Metin2.Runtime.Metin2MobCostProbeFlags.FreezeActorPresentation&&Metin2.Runtime.Metin2MobCostProbeFlags.Actors.Contains(GetInstanceID()))return;
            Metin2.Runtime.Metin2ActorPresentationScheduler.Register(this,motions?.ActiveAnimator);
            if(presentationCameraFrame!=Time.frameCount||presentationCamera==null)
            {
                presentationCameraFrame=Time.frameCount;presentationCamera=Camera.main;
                presentationFollowCamera=presentationCamera!=null?presentationCamera.GetComponent<Metin2FollowCamera>():null;
            }
            var gameplayTarget=presentationFollowCamera!=null?presentationFollowCamera.Target:null;
            if(Type==0&&motions?.ActiveAnimator!=null&&!Metin2.Runtime.Metin2ActorPresentationScheduler.Owns(motions.ActiveAnimator)&&!Metin2.Runtime.Metin2ActorPresentationScheduler.ManagesNative(this))
            {
                // Keep the animation clock/state machine alive. Only native
                // retarget/IK/bone writes are culled when this distant model is
                // genuinely not visible; nearby collision/attack bones retain
                // the full original path, as do PCs, mounts, stones and death.
                var mode=RemoteAnimationCulling(Type,IsDead,gameplayTarget!=null?(transform.position-gameplayTarget.position).sqrMagnitude:0f,gameplayTarget!=null);
                if(motions.ActiveAnimator.cullingMode!=mode)motions.ActiveAnimator.cullingMode=mode;
            }
            if(Type==Metin2GamePacketCodec.EntityTypeStone&&!Metin2.Runtime.Metin2ActorPresentationScheduler.Owns(motions?.ActiveAnimator))
            {
                UpdateStaticStonePresentation();
            }
            Metin2.Runtime.Metin2ActorPresentationScheduler.ConfigureNative(this,motions?.ActiveAnimator,presentationCamera,gameplayTarget,stonePresentation?.StaticPartCount>0);
#endif
            UpdateMountedRiderFrame();
#if UNITY_STANDALONE_WIN
            if(presentationCamera==null)return;
            if(compatibilityNameLabel!=null)compatibilityNameLabel.transform.rotation=presentationCamera.transform.rotation;
            if(ShopSignBoard!=null&&ShopSignBoard.activeSelf)ShopSignBoard.transform.rotation=presentationCamera.transform.rotation;
#else
            if (Camera.main == null) return;
            if (compatibilityNameLabel != null) compatibilityNameLabel.transform.rotation = Camera.main.transform.rotation;
            if (ShopSignBoard != null && ShopSignBoard.activeSelf) ShopSignBoard.transform.rotation = Camera.main.transform.rotation;
#endif
        }
    }
}
