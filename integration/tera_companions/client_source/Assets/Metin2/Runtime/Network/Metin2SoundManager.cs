using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Metin2.Runtime;
using UnityEngine;

namespace Metin2.Runtime.Network
{
    /// <summary>
    /// Unity port of MilesLib CSoundManager + PythonSoundManagerModule (snd) API.
    /// Covers BGM (field music), 2D UI sounds, 3D positional sounds, ambience loop and volume.
    /// Original: MilesLib/SoundManager.h, GameLib/ActorInstanceMotionEvent.cpp (MOTION_EVENT_TYPE_SOUND),
    ///           GameLib/Area.cpp (Ambience), root/game.py BGM logic, item use/drop sounds.
    /// FileName convention mirrors original: "BGM/...mp3" or "sound/...wav" (pack virtual path).
    /// Unity Resources path: "BGM/..." -> Resources/BGM, "sound/..." -> Resources/Sound/...
    /// </summary>
    public sealed class Metin2SoundManager : MonoBehaviour
    {
        private static Metin2SoundManager instance;
        public static Metin2SoundManager Instance
        {
            get
            {
                if (instance != null) return instance;
                var go = new GameObject("Metin2SoundManager");
                // DontDestroyOnLoad throws outside play mode, and the editor validation gates drive
                // the very panels that play UI sounds, so the singleton has to be constructible there.
                if (Application.isPlaying) DontDestroyOnLoad(go);
                instance = go.AddComponent<Metin2SoundManager>();
                instance.Initialize();
                return instance;
            }
        }

        private AudioSource bgmSource;
        private AudioSource bgmFadeSource;
        private readonly List<AudioSource> sfxPool = new List<AudioSource>();
        private readonly Dictionary<int, AudioSource> positionalSoundHandles = new Dictionary<int, AudioSource>();
        private readonly Dictionary<string, AudioClip> clipCache = new Dictionary<string, AudioClip>(StringComparer.OrdinalIgnoreCase);
        private float fieldMusicVolume = 1f;
        private float fieldSoundVolume = 1f;
        public static float EffectiveSoundVolume => Metin2ClientSettings.Current.SoundVolume * (instance != null ? instance.fieldSoundVolume : 1f);
        private Coroutine fadeCoroutine;
        private Transform listener;
        private int nextPositionalSoundHandle = 1;

        private const int SfxPoolInitial = 16;
        private const float Max3DDistance = 50f;

        private void Awake()
        {
            if (instance != null && instance != this) { Destroy(gameObject); return; }
            instance = this;
            DontDestroyOnLoad(gameObject);
            Initialize();
        }

        private void Initialize()
        {
            if (bgmSource != null) return;
            // Előző rendszer duplikált BGM-jének leállítása (Metin2BGM GameObject M2BG-vel)
            var oldBgmObj = GameObject.Find("Metin2BGM");
            if (oldBgmObj != null)
            {
                var oldSrc = oldBgmObj.GetComponent<AudioSource>();
                if (oldSrc != null && oldSrc.isPlaying) oldSrc.Stop();
                oldSrc.volume = 0f;
                // Ne töröljük azonnal, ApplyRuntime majd némítja, de most is elhallgattatjuk
                Destroy(oldBgmObj, 0.5f);
            }
            bgmSource = gameObject.AddComponent<AudioSource>();
            bgmSource.loop = true;
            bgmSource.playOnAwake = false;
            bgmSource.spatialBlend = 0f;
            bgmSource.volume = Metin2ClientSettings.Current.MusicVolume * fieldMusicVolume;

            bgmFadeSource = gameObject.AddComponent<AudioSource>();
            bgmFadeSource.loop = true;
            bgmFadeSource.playOnAwake = false;
            bgmFadeSource.spatialBlend = 0f;
            bgmFadeSource.volume = 0f;

            for (int i = 0; i < SfxPoolInitial; i++) CreateSfxSource();

            listener = Camera.main != null ? Camera.main.transform : null;
            Metin2ClientSettings.Current.Changed += OnSettingsChanged;
        }

        private AudioSource CreateSfxSource()
        {
            var go = new GameObject("SFX_" + sfxPool.Count);
            go.transform.SetParent(transform);
            var src = go.AddComponent<AudioSource>();
            src.playOnAwake = false;
            src.spatialBlend = 0f;
            src.rolloffMode = AudioRolloffMode.Linear;
            src.maxDistance = Max3DDistance;
            src.volume = Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume;
            sfxPool.Add(src);
            return src;
        }

        private AudioSource GetFreeSfxSource()
        {
            foreach (var src in sfxPool)
                if (!src.isPlaying)
                {
                    ForgetPositionalHandle(src);
                    return src;
                }
            return CreateSfxSource();
        }

        private void ForgetPositionalHandle(AudioSource source)
        {
            var stale = new List<int>();
            foreach (var pair in positionalSoundHandles)
                if (pair.Value == null || pair.Value == source) stale.Add(pair.Key);
            foreach (var handle in stale) positionalSoundHandles.Remove(handle);
        }

        private void OnSettingsChanged()
        {
            if (bgmSource != null) bgmSource.volume = Metin2ClientSettings.Current.MusicVolume * fieldMusicVolume;
            float sfxVol = Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume;
            foreach (var src in sfxPool) if (!src.isPlaying || src.spatialBlend == 0f) src.volume = sfxVol;
        }

        private void Update()
        {
            if (listener == null && Camera.main != null) listener = Camera.main.transform;
            // Keep listener position synced for 3D audio (Unity does this automatically via AudioListener on camera,
            // but we also update our pooled 3D sources' positions if needed)
        }

        // ----- Path conversion -----
        private static string ToResourcePath(string fileName)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return null;
            string n = fileName.Trim().Replace('\\', '/');
            // Strip leading d:/ymir work/ or BGM/ etc handling
            // Original passes like "BGM/m2bg.mp3" or "sound/ui/click.wav" or "sound/common/swing/hand_1.wav"
            // Resources paths are "BGM/m2bg" and "Sound/ui/click"
            if (n.StartsWith("BGM/", StringComparison.OrdinalIgnoreCase))
            {
                n = n.Substring(4);
                n = System.IO.Path.Combine("BGM", System.IO.Path.GetFileNameWithoutExtension(n)).Replace('\\','/');
                return n;
            }
            if (n.StartsWith("sound/", StringComparison.OrdinalIgnoreCase))
            {
                n = n.Substring(6);
                n = System.IO.Path.Combine("Sound", System.IO.Path.GetFileNameWithoutExtension(n) != "" ? System.IO.Path.ChangeExtension(n, null).TrimEnd('.') : n).Replace('\\','/');
                // For nested like Sound/common/swing/hand_1 -> need preserve subfolders
                // The above GetFileNameWithoutExtension loses directory; instead do:
                // Let's do proper: take original after "sound/" keep directory
            }
            // Recompute correctly for sound/
            if (fileName.Trim().StartsWith("sound/", StringComparison.OrdinalIgnoreCase) || fileName.Trim().StartsWith("sound\\", StringComparison.OrdinalIgnoreCase))
            {
                string after = fileName.Trim().Substring(6).Replace('\\','/');
                string withoutExt = System.IO.Path.ChangeExtension(after, null);
                if (withoutExt == null) withoutExt = after;
                return "Sound/" + withoutExt.TrimEnd('.');
            }
            // Fallback: try as is without extension under Sound/
            if (n.EndsWith(".mp3", StringComparison.OrdinalIgnoreCase) || n.EndsWith(".wav", StringComparison.OrdinalIgnoreCase))
                n = n.Substring(0, n.LastIndexOf('.'));
            // If already contains Sound/ or BGM/, keep
            if (n.StartsWith("Sound/", StringComparison.OrdinalIgnoreCase) || n.StartsWith("BGM/", StringComparison.OrdinalIgnoreCase))
                return n;
            // Default to Sound/
            return "Sound/" + n;
        }

        // More robust conversion
        private static string ConvertFileNameToResourcePath(string fileName)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return null;
            string trimmed = fileName.Trim().Replace('\\','/');
            string lower = trimmed.ToLowerInvariant();
            if (lower.StartsWith("bgm/"))
            {
                string name = System.IO.Path.GetFileNameWithoutExtension(trimmed.Substring(4));
                return "BGM/" + name;
            }
            if (lower.StartsWith("sound/"))
            {
                string after = trimmed.Substring(6);
                string withoutExt = after;
                int dot = after.LastIndexOf('.');
                if (dot >= 0) withoutExt = after.Substring(0, dot);
                return "Sound/" + withoutExt;
            }
            if (lower.StartsWith("d:/ymir work/"))
            {
                // e.g., d:/ymir work/ui/game/taskbar/... but sound files are not there
                return null;
            }
            // For cases like "sound/ui/click.wav" already handled, fallback
            string noExt = trimmed;
            int lastDot = trimmed.LastIndexOf('.');
            if (lastDot >= 0) noExt = trimmed.Substring(0, lastDot);
            if (noExt.StartsWith("Sound/", StringComparison.OrdinalIgnoreCase)) return noExt;
            return "Sound/" + noExt;
        }

        /// <summary>
        /// Resolves a clip WITHOUT blocking. Since the audio Addressables migration the clips live
        /// in family bundles at the same virtual addresses they had as Resources keys, so this reads
        /// the local cache and then whatever Metin2ContentProvider already holds.
        ///
        /// Playback is reactive - an attack impact, a footstep, a UI click - and must keep the
        /// original timing, so nothing here may await. That is what the preloads exist for: UI and
        /// common sounds are resident from bootstrap, and an actor's motion-sound family is loaded
        /// with its race, before the actor can play anything.
        ///
        /// A miss is still cached as a miss and still logs METIN2_SOUND_MISSING exactly as before,
        /// which preserves the documented behaviour for the 25 audio files the source pack lacks.
        /// </summary>
        private AudioClip LoadClip(string fileName)
        {
            string resPath = ConvertFileNameToResourcePath(fileName);
            if (string.IsNullOrEmpty(resPath)) return null;
            if (clipCache.TryGetValue(resPath, out AudioClip cached)) return cached;
            Metin2ContentProvider.TryGetLoaded(resPath, out AudioClip clip);
            if (clip == null)
            {
                // Not resident. Fetch it for next time - a repeated sound (footsteps, swings) then
                // plays from the second occurrence on - but do not stall this frame waiting for it.
                RequestClipAsync(resPath, Metin2ContentScope.Map);
                // Once per address. A sound that is not resident is asked for again on every swing
                // and every footstep of every mob, and the warning was being written each time.
                Metin2Diagnostics.WarnOnce("SOUND_MISSING:" + resPath,
                    $"METIN2_SOUND_MISSING file={fileName} resPath={resPath}");
                return null;
            }
            clipCache[resPath] = clip;
            return clip;
        }

        /// <summary>
        /// Plays an authored MSS at an effect/impact position.  Motion handlers own actor-motion
        /// timing; this entry point covers standalone MSE effects (ordinary hit and critical) that
        /// are spawned directly by the combat/effect pipeline.  It reads only the already-built
        /// catalog and the resident clip cache -- no synchronous or reactive Addressables load.
        /// </summary>
        public void PlayMotionSoundScript3D(Vector3 position, string mssVirtualPath)
        {
            if (string.IsNullOrWhiteSpace(mssVirtualPath)) return;
            if (motionSoundCatalog == null)
                motionSoundCatalog = Resources.Load<Metin2MotionSoundCatalog>("Metin2MotionSoundCatalog");
            if (motionSoundCatalog == null || !motionSoundCatalog.TryGet(mssVirtualPath,
                out Metin2MotionSoundCatalog.SoundData[] sounds)) return;
            foreach (Metin2MotionSoundCatalog.SoundData sound in sounds)
            {
                if (string.IsNullOrWhiteSpace(sound.fileName)) continue;
                if (sound.time <= 0f) PlaySound3D(position, sound.fileName);
                else StartCoroutine(PlayMotionSoundAfterDelay(position, sound.fileName, sound.time));
            }
        }

        private IEnumerator PlayMotionSoundAfterDelay(Vector3 position, string fileName, float delay)
        {
            yield return new WaitForSeconds(delay);
            PlaySound3D(position, fileName);
        }

        // ----------------------------------------------------------- async preloading

        private Metin2SoundIndex soundIndex;
        private Metin2MotionSoundCatalog motionSoundCatalog;
        private bool soundIndexResolved;
        private readonly Dictionary<string, Task> soundLoads = new Dictionary<string, Task>(StringComparer.Ordinal);
        private readonly HashSet<string> loadedFamilies = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private CancellationTokenSource audioLifetime = new CancellationTokenSource();

        public Metin2SoundIndex Index
        {
            get
            {
                if (soundIndexResolved) return soundIndex;
                soundIndexResolved = true;
                soundIndex = Resources.Load<Metin2SoundIndex>(Metin2SoundIndex.ResourceName);
                if (soundIndex == null)
                    Debug.LogWarning("METIN2_SOUND_INDEX_MISSING - run Metin2/Build/Sound Index; " +
                                     "no audio family can be preloaded without it.");
                return soundIndex;
            }
        }

        /// <summary>Is every clip of this family resident? An unknown family answers true.</summary>
        public bool IsFamilyLoaded(string family)
        {
            if (string.IsNullOrEmpty(family)) return true;
            lock (soundLoads) { if (loadedFamilies.Contains(family)) return true; }
            Metin2SoundIndex index = Index;
            if (index == null || !index.Contains(family)) return true;
            foreach (string address in index.AddressesOf(family))
                if (!Metin2ContentProvider.TryGetLoaded(address, out AudioClip _)) return false;
            return true;
        }

        /// <summary>
        /// Loads one audio family - a monster, a playable class, the UI set, one BGM track - as a
        /// unit. Families are the bundle boundary, so this is one bundle load however many clips it
        /// contains, and concurrent requests for the same family share one operation.
        /// </summary>
        public Task PreloadFamilyAsync(string family, Metin2ContentScope scope, CancellationToken token = default)
        {
            if (string.IsNullOrEmpty(family)) return Task.CompletedTask;
            Metin2SoundIndex index = Index;
            if (index == null || !index.Contains(family)) return Task.CompletedTask;
            lock (soundLoads)
            {
                if (loadedFamilies.Contains(family)) return Task.CompletedTask;
                if (soundLoads.TryGetValue(family, out Task existing)) return existing;
                Task created = LoadFamilyAsync(index, family, scope, token);
                soundLoads[family] = created;
                return created;
            }
        }

        private async Task LoadFamilyAsync(Metin2SoundIndex index, string family, Metin2ContentScope scope, CancellationToken token)
        {
            try
            {
                List<Task> loads = new List<Task>();
                foreach (string address in index.AddressesOf(family))
                    loads.Add(AcquireClipAsync(address, scope, token));
                await Task.WhenAll(loads);
                lock (soundLoads) loadedFamilies.Add(family);
            }
            finally { lock (soundLoads) soundLoads.Remove(family); }
        }

        /// <summary>The family that owns one virtual file name, e.g. "sound/monster/wolf/attack.wav".</summary>
        public Task PreloadForFileAsync(string fileName, Metin2ContentScope scope, CancellationToken token = default)
            => PreloadFamilyAsync(Metin2SoundIndex.FamilyOf(ConvertFileNameToResourcePath(fileName)), scope, token);

        /// <summary>
        /// Preloads the distinct Addressables families referenced by an already-resolved motion set.
        /// This is deliberately a lifecycle operation (race/world entry), never an MSS callback:
        /// timed playback remains cache-only and therefore keeps its authored event time.
        /// </summary>
        public Task PreloadForFilesAsync(IEnumerable<string> fileNames, Metin2ContentScope scope,
                                         CancellationToken token = default)
        {
            if (fileNames == null) return Task.CompletedTask;
            var families = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string fileName in fileNames)
            {
                string family = Metin2SoundIndex.FamilyOf(ConvertFileNameToResourcePath(fileName));
                if (!string.IsNullOrEmpty(family)) families.Add(family);
            }
            if (families.Count == 0) return Task.CompletedTask;

            var loads = new List<Task>(families.Count);
            foreach (string family in families) loads.Add(PreloadFamilyAsync(family, scope, token));
            return Task.WhenAll(loads);
        }

        /// <summary>
        /// Preloads every clip mentioned by authored MSS files under a virtual-path prefix.  An
        /// MSS may legitimately point outside its owner tree (mounted charge uses horse and class
        /// clips), so loading only the apparent character family is not sufficient.
        /// </summary>
        public Task PreloadMotionSoundPrefixAsync(string virtualPathPrefix, Metin2ContentScope scope,
                                                   CancellationToken token = default)
        {
            if (string.IsNullOrWhiteSpace(virtualPathPrefix)) return Task.CompletedTask;
            Metin2MotionSoundCatalog catalog = Resources.Load<Metin2MotionSoundCatalog>("Metin2MotionSoundCatalog");
            return catalog == null ? Task.CompletedTask :
                PreloadForFilesAsync(catalog.FilesUnder(virtualPathPrefix), scope, token);
        }

        /// <summary>Whether the indexed family which owns a file is resident for synchronous playback.</summary>
        public bool IsFileFamilyLoaded(string fileName)
        {
            string family = Metin2SoundIndex.FamilyOf(ConvertFileNameToResourcePath(fileName));
            return string.IsNullOrEmpty(family) || IsFamilyLoaded(family);
        }

        /// <summary>
        /// Player jobs are not represented by the NPC/monster race-data catalog.  Their complete
        /// usable motion-audio family must therefore be acquired explicitly before the local actor
        /// is spawned, rather than relying on a reactive MSS callback after the first attack.
        /// </summary>
        public async Task PreloadPlayerMotionAudioAsync(int job, Metin2ContentScope scope,
                                                        CancellationToken token = default)
        {
            string family = PlayerMotionFamilyForJob(job);
            string root = PlayerMotionRootForJob(job);
            if (string.IsNullOrEmpty(family) || string.IsNullOrEmpty(root)) return;
            await Task.WhenAll(
                PreloadFamilyAsync(family, scope, token),
                PreloadMotionSoundPrefixAsync("sound/" + root + "/", scope, token));
            Debug.Log($"METIN2_AUDIO_PLAYER_PRELOAD job={job} family={family} root={root} scope={scope}");
        }

        public bool IsPlayerMotionAudioLoaded(int job)
        {
            string family = PlayerMotionFamilyForJob(job);
            return string.IsNullOrEmpty(family) || IsFamilyLoaded(family);
        }

        private static string PlayerMotionFamilyForJob(int job)
        {
            switch (job)
            {
                case 0: return "audio-pc-warrior";
                case 1: return "audio-pc-assassin";
                case 2: return "audio-pc-sura";
                case 3: return "audio-pc-shaman";
                case 4: return "audio-pc2-warrior";
                case 5: return "audio-pc2-assassin";
                case 6: return "audio-pc2-sura";
                case 7: return "audio-pc2-shaman";
                default: return null;
            }
        }

        private static string PlayerMotionRootForJob(int job)
        {
            switch (job)
            {
                case 0: return "pc/warrior";
                case 1: return "pc/assassin";
                case 2: return "pc/sura";
                case 3: return "pc/shaman";
                case 4: return "pc2/warrior";
                case 5: return "pc2/assassin";
                case 6: return "pc2/sura";
                case 7: return "pc2/shaman";
                default: return null;
            }
        }

        /// <summary>
        /// The always-resident set: UI and the shared hit/swing/spell/footstep clips every actor
        /// draws from. About 12.8 MiB, played reactively from any frame, so it is loaded once at
        /// Core scope and never released - which is precisely why it is a small, explicit set
        /// rather than "whatever happened to be in Resources".
        /// </summary>
        public async Task PreloadCoreAudioAsync(CancellationToken token = default)
        {
            Metin2SoundIndex index = Index;
            if (index == null) return;
            List<Task> loads = new List<Task>();
            foreach (Metin2SoundIndex.Family family in index.families)
                if (family.name != null &&
                    (family.name.Equals("audio-ui", StringComparison.OrdinalIgnoreCase) ||
                     family.name.StartsWith("audio-common", StringComparison.OrdinalIgnoreCase)))
                    loads.Add(PreloadFamilyAsync(family.name, Metin2ContentScope.Core, token));
            // These visual effects can be emitted for any actor, independent of its race. Their
            // MSS files include both the normal weapon impact and the critical-confirmation clip.
            loads.Add(PreloadMotionSoundPrefixAsync("sound/effect/hit/", Metin2ContentScope.Core, token));
            if (loads.Count == 0) return;
            try { await Task.WhenAll(loads); }
            catch (OperationCanceledException) { return; }
            catch (Exception exception) { Debug.LogWarning("METIN2_CORE_AUDIO_LOAD_FAILED " + exception.Message); }
            Debug.Log($"METIN2_CORE_AUDIO_READY families={loads.Count}");
        }

        private async Task AcquireClipAsync(string address, Metin2ContentScope scope, CancellationToken token)
        {
            try
            {
                AudioClip clip = await Metin2ContentProvider.AcquireAsync<AudioClip>(address, scope, token);
                if (clip != null) clipCache[address] = clip;
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception)
            {
                // The source pack is known to lack 25 referenced clips. A missing address is a
                // warning at play time, not a failed family load that would strand the rest.
                clipCache[address] = null;
            }
        }

        private async void RequestClipAsync(string address, Metin2ContentScope scope)
        {
            lock (soundLoads) { if (soundLoads.ContainsKey(address)) return; }
            Task load = AcquireClipAsync(address, scope, audioLifetime.Token);
            lock (soundLoads) soundLoads[address] = load;
            try { await load; }
            catch (OperationCanceledException) { }
            catch (Exception) { }
            finally { lock (soundLoads) soundLoads.Remove(address); }
        }

        // ----- Public API mirroring snd module -----
        public void PlaySound(string fileName, float volumeScale = 1f)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return;
            AudioClip clip = LoadClip(fileName);
            if (clip == null)
            {
                Debug.LogWarning($"METIN2_SOUND_MISSING {fileName}");
                return;
            }
            AudioSource src = GetFreeSfxSource();
            src.spatialBlend = 0f;
            src.volume = Mathf.Clamp01(Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume * volumeScale);
            src.clip = clip;
            src.loop = false;
            src.pitch = 1f;
            src.Play();
            if (Metin2Diagnostics.Verbose)
                Debug.Log($"METIN2_SOUND_PLAY {fileName} vol={src.volume:F2}");
        }

        public void PlaySound3D(float x, float y, float z, string fileName, float volumeScale = 1f)
        {
            PlaySound3D(new Vector3(x, z, -y), fileName, volumeScale);
        }

        /// <summary>Monotonic play total, differenced per frame by the debug HUD.</summary>
        public static int TotalSoundsPlayed { get; private set; }

        public void PlaySound3D(Vector3 position, string fileName, float volumeScale = 1f)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return;
            TotalSoundsPlayed++;
            AudioClip clip = LoadClip(fileName);
            if (clip == null)
            {
                Debug.LogWarning($"METIN2_SOUND_3D_MISSING {fileName}");
                return;
            }
            AudioSource src = GetFreeSfxSource();
            src.transform.position = position;
            // Miles PlaySound3D is positional throughout its attenuation range.
            // Do not turn distant effects into a partly 2D sound.
            src.spatialBlend = 1f;
            src.minDistance = 2f;
            src.maxDistance = Max3DDistance;
            src.rolloffMode = AudioRolloffMode.Linear;
            src.volume = Mathf.Clamp01(Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume * volumeScale);
            src.clip = clip;
            src.loop = false;
            src.pitch = 1f;
            src.Play();
            if (Metin2Diagnostics.Verbose)
                Debug.Log($"METIN2_SOUND_3D_PLAY {fileName} pos={position} vol={src.volume:F2}");
        }

        /// <summary>
        /// MilesLib PlayAmbienceSound3D equivalent. <paramref name="playCount"/>
        /// is 1 for once/step and 0 for an infinite loop, matching Area.cpp.
        /// The returned handle is valid until its source stops or is reused.
        /// </summary>
        public int PlayAmbienceSound3D(float x, float y, float z, string fileName, int playCount = 1, float volumeScale = 1f)
            => PlayAmbienceSound3D(new Vector3(x, z, -y), fileName, playCount, volumeScale);

        public int PlayAmbienceSound3D(Vector3 position, string fileName, int playCount = 1, float volumeScale = 1f)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return -1;
            AudioClip clip = LoadClip(fileName);
            if (clip == null) return -1;
            AudioSource src = GetFreeSfxSource();
            src.transform.position = position;
            src.spatialBlend = 1f;
            src.minDistance = 5f;
            src.maxDistance = 80f;
            src.rolloffMode = AudioRolloffMode.Logarithmic;
            src.volume = Mathf.Clamp01(Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume * volumeScale);
            src.clip = clip;
            src.loop = playCount == 0;
            src.Play();
            int handle = nextPositionalSoundHandle++;
            if (nextPositionalSoundHandle == int.MaxValue) nextPositionalSoundHandle = 1;
            positionalSoundHandles[handle] = src;
            return handle;
        }

        public void SetSoundVolume3D(int handle, float volume)
        {
            if (!positionalSoundHandles.TryGetValue(handle, out AudioSource source) || source == null || !source.isPlaying)
            {
                positionalSoundHandles.Remove(handle);
                return;
            }
            source.volume = Mathf.Clamp01(Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume * volume);
        }

        public void StopSound3D(int handle)
        {
            if (!positionalSoundHandles.TryGetValue(handle, out AudioSource source)) return;
            if (source != null) source.Stop();
            positionalSoundHandles.Remove(handle);
        }

        public void StopAllSound()
        {
            foreach (var src in sfxPool) if (src.isPlaying) src.Stop();
            positionalSoundHandles.Clear();
        }

        /// <summary>
        /// Clears map/monster SFX clips accumulated by the persistent sound manager while preserving
        /// the currently referenced BGM clips. Called only during a loading transition, never when a
        /// single mob dies, so repeated spawns do not thrash Resources and hitch gameplay.
        /// </summary>
        public void ReleaseMapScopedAssets()
        {
            StopAllSound();
            foreach (AudioSource source in sfxPool)
                if (source != null) source.clip = null;

            AudioClip keepA = bgmSource != null ? bgmSource.clip : null;
            AudioClip keepB = bgmFadeSource != null ? bgmFadeSource.clip : null;
            var remove = new List<string>();
            foreach (var pair in clipCache)
                if (pair.Value != keepA && pair.Value != keepB) remove.Add(pair.Key);
            foreach (string key in remove) clipCache.Remove(key);
            // Map-scoped families have to be re-requested for the next map; the Addressables handles
            // themselves are released by Metin2ContentProvider.ReleaseMapScope, which runs after this.
            // Core families (ui, common) are untouched: they were acquired at Core scope and stay.
            lock (soundLoads)
            {
                loadedFamilies.RemoveWhere(family =>
                    !family.StartsWith("audio-ui", StringComparison.OrdinalIgnoreCase) &&
                    !family.StartsWith("audio-common", StringComparison.OrdinalIgnoreCase));
            }
        }

        private void OnDestroy()
        {
            audioLifetime.Cancel();
            audioLifetime.Dispose();
        }

        private static string NormalizeMusicFileName(string fileName)
        {
            if (string.IsNullOrWhiteSpace(fileName)) return "BGM/m2bg.mp3";
            if (fileName.StartsWith("BGM/", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("bgm/", StringComparison.OrdinalIgnoreCase))
                return fileName;
            return "BGM/" + System.IO.Path.GetFileName(fileName);
        }

        /// <summary>Returns a resident clip without starting a Map-scoped reactive SFX load.</summary>
        private AudioClip TryGetResidentClip(string fileName)
        {
            string resPath = ConvertFileNameToResourcePath(fileName);
            if (string.IsNullOrEmpty(resPath)) return null;
            if (clipCache.TryGetValue(resPath, out AudioClip cached) && cached != null) return cached;
            if (!Metin2ContentProvider.TryGetLoaded(resPath, out AudioClip clip) || clip == null) return null;
            clipCache[resPath] = clip;
            return clip;
        }

        public void PlayMusic(string fileName, float volume = 1f)
        {
            fileName = NormalizeMusicFileName(fileName);
            int generation = ++musicGeneration;
            AudioClip clip = TryGetResidentClip(fileName);
            if (clip == null)
            {
                // BGM is Session-scoped and loaded one track at a time. Do not route the first
                // request through LoadClip(), because that helper intentionally schedules misses
                // as reactive Map-scoped SFX and does not retry playback after the load completes.
                EnsureMusicAsync(fileName, volume, generation);
                return;
            }
            PlayMusicClip(clip, volume);
        }

        private void PlayMusicClip(AudioClip clip, float volume)
        {
            if (clip == null) return;
            if (bgmSource.clip == clip && bgmSource.isPlaying) return;
            bgmSource.clip = clip;
            bgmSource.volume = Mathf.Clamp01(Metin2ClientSettings.Current.MusicVolume * fieldMusicVolume * volume);
            bgmSource.loop = true;
            bgmSource.Play();
        }

        private int musicGeneration;

        private async void EnsureMusicAsync(string fileName, float volume, int generation)
        {
            try { await PreloadForFileAsync(fileName, Metin2ContentScope.Session, audioLifetime.Token); }
            catch (OperationCanceledException) { return; }
            catch (Exception exception)
            {
                Debug.LogWarning($"METIN2_BGM_MISSING {fileName} {exception.Message}");
                return;
            }
            if (this == null || generation != musicGeneration) return;
            AudioClip clip = TryGetResidentClip(fileName);
            if (clip != null)
            {
                PlayMusicClip(clip, volume);
                Debug.Log($"METIN2_BGM_PLAY {fileName}");
            }
            else Debug.LogWarning($"METIN2_BGM_MISSING {fileName}");
        }

        public void FadeInMusic(string fileName, float fadeSpeed = 0.02f)
        {
            fileName = NormalizeMusicFileName(fileName);
            int generation = ++musicGeneration;
            if (fadeCoroutine != null)
            {
                StopCoroutine(fadeCoroutine);
                fadeCoroutine = null;
            }

            AudioClip clip = TryGetResidentClip(fileName);
            if (clip != null)
            {
                fadeCoroutine = StartCoroutine(FadeInCoroutine(clip, fadeSpeed));
                return;
            }

            EnsureFadeInMusicAsync(fileName, fadeSpeed, generation);
        }

        private async void EnsureFadeInMusicAsync(string fileName, float fadeSpeed, int generation)
        {
            try { await PreloadForFileAsync(fileName, Metin2ContentScope.Session, audioLifetime.Token); }
            catch (OperationCanceledException) { return; }
            catch (Exception exception)
            {
                Debug.LogWarning($"METIN2_BGM_MISSING {fileName} {exception.Message}");
                return;
            }
            if (this == null || generation != musicGeneration) return;

            AudioClip clip = TryGetResidentClip(fileName);
            if (clip == null)
            {
                Debug.LogWarning($"METIN2_BGM_MISSING {fileName}");
                return;
            }

            if (fadeCoroutine != null) StopCoroutine(fadeCoroutine);
            fadeCoroutine = StartCoroutine(FadeInCoroutine(clip, fadeSpeed));
            Debug.Log($"METIN2_BGM_FADEIN {fileName}");
        }

        public void FadeOutMusic(string fileName = null, float fadeSpeed = 0.02f)
        {
            if (fadeCoroutine != null) StopCoroutine(fadeCoroutine);
            fadeCoroutine = StartCoroutine(FadeOutCoroutine(fadeSpeed, false));
        }

        public void FadeOutAllMusic(float fadeSpeed = 0.02f)
        {
            if (fadeCoroutine != null) StopCoroutine(fadeCoroutine);
            fadeCoroutine = StartCoroutine(FadeOutCoroutine(fadeSpeed, true));
        }

        public void FadeLimitOutMusic(string fileName, float limitVolume, float fadeSpeed = 0.02f)
        {
            if (fadeCoroutine != null) StopCoroutine(fadeCoroutine);
            fadeCoroutine = StartCoroutine(FadeLimitOutCoroutine(Mathf.Clamp01(limitVolume), fadeSpeed));
        }

        // Compatibility for older Unity call sites that did not expose the
        // original fLimitVolume parameter. The old behaviour was a full fade;
        // preserve that semantics explicitly rather than silently pretending it
        // was a limit fade.
        public void FadeLimitOutMusic(string fileName, float fadeSpeed = 0.02f) => FadeOutMusic(fileName, fadeSpeed);

        public void StopMusic()
        {
            if (bgmSource.isPlaying) bgmSource.Stop();
        }

        public void SetMusicVolume(float volume)
        {
            fieldMusicVolume = Mathf.Clamp01(volume);
            if (bgmSource != null) bgmSource.volume = Metin2ClientSettings.Current.MusicVolume * fieldMusicVolume;
        }

        public void SetSoundVolume(float volume)
        {
            fieldSoundVolume = Mathf.Clamp01(volume);
            foreach (var src in sfxPool)
                if (!src.isPlaying) src.volume = Metin2ClientSettings.Current.SoundVolume * fieldSoundVolume;
        }

        private IEnumerator FadeInCoroutine(AudioClip clip, float speed)
        {
            if (clip == null) yield break;
            // Prepare fade source
            bgmFadeSource.clip = clip;
            bgmFadeSource.volume = 0f;
            bgmFadeSource.loop = true;
            bgmFadeSource.Play();
            // Fade out current
            float targetVol = Metin2ClientSettings.Current.MusicVolume * fieldMusicVolume;
            while (bgmSource.volume > 0.01f)
            {
                bgmSource.volume = Mathf.MoveTowards(bgmSource.volume, 0f, speed);
                bgmFadeSource.volume = Mathf.MoveTowards(bgmFadeSource.volume, targetVol, speed);
                yield return null;
            }
            bgmSource.Stop();
            // Swap
            var tmp = bgmSource;
            bgmSource = bgmFadeSource;
            bgmFadeSource = tmp;
            bgmSource.volume = targetVol;
            bgmFadeSource.Stop();
            bgmFadeSource.volume = 0f;
        }

        private IEnumerator FadeLimitOutCoroutine(float limitVolume, float speed)
        {
            // SoundManager.cpp stores the converted limit directly on the music instance.
            // The caller already supplies its desired ratio (game.py uses setting * 0.05),
            // so multiplying the current master volume a second time made the result too quiet.
            float target = RatioToApplyVolume(limitVolume);
            while (bgmSource != null && bgmSource.volume > target + .0001f)
            {
                bgmSource.volume = Mathf.MoveTowards(bgmSource.volume, target, speed);
                yield return null;
            }
        }

        private static float RatioToApplyVolume(float ratio) => ratio < .1f ? ratio : Mathf.Pow(10f, -1f + ratio);

        private IEnumerator FadeOutCoroutine(float speed, bool stopAll)
        {
            while (bgmSource.volume > 0.01f)
            {
                bgmSource.volume = Mathf.MoveTowards(bgmSource.volume, 0f, speed);
                yield return null;
            }
            bgmSource.Stop();
            bgmSource.volume = 0f;
            if (stopAll && bgmFadeSource.isPlaying) bgmFadeSource.Stop();
        }

        // ----- Field music helper -----
        public void ApplyFieldMusic(string fileName, float volumeScale = 1f)
        {
            fieldMusicVolume = Mathf.Clamp01(volumeScale);
            if (string.IsNullOrWhiteSpace(fileName)) fileName = Metin2ClientSettings.Current.MusicFile;
            else Metin2ClientSettings.Current.MusicFile = System.IO.Path.GetFileName(fileName);
            // Persist like musicInfo.SaveLastPlayFieldMusic
            Metin2ClientSettings.Current.SaveAndApply();
            PlayMusic(fileName, fieldMusicVolume);
        }

        public void UpdateListener(Vector3 position, Vector3 forward)
        {
            if (listener == null) return;
            listener.position = position;
            listener.forward = forward;
        }
    }
}
