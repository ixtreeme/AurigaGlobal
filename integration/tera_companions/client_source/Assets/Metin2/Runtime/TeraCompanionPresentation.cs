using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using UnityEngine;

namespace Metin2.Runtime
{
    /// <summary>Original TERA motion-linked cosmetics. Assets are owned by the race prefab.</summary>
    public sealed class TeraCompanionPresentation : MonoBehaviour
    {
        [Serializable] public sealed class Motion { public string state, source; public float duration; }
        [Serializable] public sealed class Effect
        {
            public string context, condition, clip, resource, bone;
            public GameObject prefab;
            public Vector3 position, rotation, scale = Vector3.one;
            public float delay, time, rate = 1;
            public string[] velocityEmitters = Array.Empty<string>();
        }
        [Serializable] public sealed class Sound
        {
            public string state, cue;
            public float time, volume = 1;
            public AudioClip[] clips = Array.Empty<AudioClip>();
            public TeraSoundCue program;
        }
        public string modelKey;
        public Motion[] motions = Array.Empty<Motion>();
        public Effect[] effects = Array.Empty<Effect>();
        public Sound[] sounds = Array.Empty<Sound>();
        public int PlayedSounds { get; private set; }
        public int SpawnedEffects { get; private set; }
        public string CurrentMotion => current;
        Animator animator;
        AudioSource audioSource;
        string current = "";
        float previous;
        readonly Dictionary<Effect,GameObject> attached = new Dictionary<Effect,GameObject>();
        readonly Dictionary<Effect,Coroutine> pending = new Dictionary<Effect,Coroutine>();
        readonly Dictionary<string, Transform> bones = new Dictionary<string, Transform>(StringComparer.OrdinalIgnoreCase);

        void Awake()
        {
            animator = GetComponent<Animator>();
            audioSource = gameObject.AddComponent<AudioSource>();
            audioSource.playOnAwake = false; audioSource.spatialBlend = 1;
            audioSource.minDistance = 3; audioSource.maxDistance = 35;
            audioSource.rolloffMode = AudioRolloffMode.Logarithmic;
            foreach (Transform t in GetComponentsInChildren<Transform>(true))
                if (!bones.ContainsKey(t.name)) bones.Add(t.name, t);
        }
        void Update()
        {
            if (!animator || !animator.isInitialized) return;
            var info = animator.GetCurrentAnimatorStateInfo(0);
            var clips = animator.GetCurrentAnimatorClipInfo(0);
            if (animator.IsInTransition(0))
            {
                var next = animator.GetNextAnimatorClipInfo(0);
                if (next.Length > 0) { clips = next; info = animator.GetNextAnimatorStateInfo(0); }
            }
            if (clips.Length == 0) return;
            Motion motion = Array.Find(motions, m => m.state == clips[0].clip.name);
            if (motion == null) return;
            Advance(motion.source, info.normalizedTime, motion.duration);
        }
        public void Advance(string source, float normalized, float duration)
        {
            if(duration<=.0011f||float.IsNaN(normalized)||float.IsInfinity(normalized))normalized=0;
            if (source != current || normalized < previous)
            {
                bool hadMotion=current.Length>0,wasMoving=IsMoving(current),moving=IsMoving(source);current = source; previous = -.0001f;
                foreach(var voice in GetComponentsInChildren<TeraCuePlayback>())
                    if(voice.looping&&voice.GetComponentInParent<TeraCompanionPresentation>()==this&&!Array.Exists(sounds,s=>s.program==voice.cue&&(string.Equals(s.state,source,StringComparison.OrdinalIgnoreCase)||(moving&&s.state.Equals("onmove",StringComparison.OrdinalIgnoreCase))))){voice.stopping=true;Destroy(voice.gameObject);}
                if(wasMoving!=moving)
                    foreach(var sound in sounds)
                        if(sound.state.Equals(moving?"onstartmove":"onendmove",StringComparison.OrdinalIgnoreCase)||(moving&&sound.state.Equals("onmove",StringComparison.OrdinalIgnoreCase)))PlaySound(sound);
                foreach (var effect in effects)
                {
                    if(effect.context!="attached")continue;
                    bool want=Matches(effect.condition,source)&& (hadMotion||!(effect.condition??"").EndsWith("WALKING_OFF",StringComparison.OrdinalIgnoreCase));
                    if(!want)
                    {
                        if(pending.TryGetValue(effect,out var coroutine)){StopCoroutine(coroutine);pending.Remove(effect);}
                        if(attached.TryGetValue(effect,out var go)){if(go)Destroy(go);attached.Remove(effect);}
                    }
                    else if(!attached.ContainsKey(effect)&&!pending.ContainsKey(effect))
                    {
                        if(effect.delay>0)pending[effect]=StartCoroutine(Delayed(effect));else Spawn(effect);
                    }
                }
            }
            float from = previous, to = normalized;
            // Limit catch-up after a paused/background client; never emit an unbounded backlog.
            from = Mathf.Max(from, to - 2f);
            foreach (var sound in sounds)
                if (string.Equals(sound.state, source, StringComparison.OrdinalIgnoreCase) && Crossed(from, to, sound.time, duration))
                {
                    PlaySound(sound);
                }
            foreach (var effect in effects)
                if (effect.context == "animation" && string.Equals(effect.clip, source, StringComparison.OrdinalIgnoreCase) && Crossed(from, to, effect.time, duration)) Spawn(effect);
            previous = to;
        }
        static bool IsMoving(string name)=>Regex.IsMatch(name??"","run|walk|flyforward|flyfast|flyback",RegexOptions.IgnoreCase);
        void PlaySound(Sound sound)
        {
            if(sound.program){if(sound.program.Play(transform,sound.volume))PlayedSounds++;return;}
            if(sound.clips.Length==0)return;
            var clip=sound.clips[UnityEngine.Random.Range(0,sound.clips.Length)];
            if(clip){audioSource.PlayOneShot(clip,Mathf.Max(0,sound.volume));PlayedSounds++;}
        }
        static bool Crossed(float from, float to, float seconds, float duration)
        {
            if(duration<=.0011f)return from<0&&to>=0&&seconds<=.001f;
            if (seconds > duration + .001f) return false;
            float at = Mathf.Clamp01(seconds / duration);
            int loop = Mathf.FloorToInt(to);
            return (loop + at > from && loop + at <= to) || (loop - 1 + at > from && loop - 1 + at <= to);
        }
        IEnumerator Delayed(Effect effect)
        {
            if (effect.delay > 0) yield return new WaitForSeconds(effect.delay);
            pending.Remove(effect);
            Spawn(effect);
        }
        void Spawn(Effect effect)
        {
            if (effect.context == "spawn" || !effect.prefab) return;
            Transform bone = transform;
            if (!string.IsNullOrEmpty(effect.bone) && bones.TryGetValue(effect.bone, out var found)) bone = found;
            var go = Instantiate(effect.prefab, bone, false); go.name = "TERA_FX_" + effect.resource;
            go.transform.localPosition = effect.position;
            go.transform.localRotation = Quaternion.Euler(effect.rotation);
            go.transform.localScale = effect.scale;
            float lifetime = 1;
            foreach (var ps in go.GetComponentsInChildren<ParticleSystem>())
            {
                var main = ps.main; main.simulationSpeed = Mathf.Max(.01f, effect.rate);
                if (Array.IndexOf(effect.velocityEmitters, ps.name) >= 0)
                    ps.GetComponent<ParticleSystemRenderer>().alignment = ParticleSystemRenderSpace.Velocity;
                if (effect.context == "animation") main.loop = false;
                lifetime = Mathf.Max(lifetime, main.duration + main.startLifetime.constantMax + main.startDelay.constantMax);
                ps.Play(false);
            }
            if (effect.context == "attached") attached[effect]=go; else Destroy(go, lifetime / Mathf.Max(.01f, effect.rate) + 1);
            SpawnedEffects++;
        }
        public static bool Matches(string condition, string name)
        {
            string c = (condition ?? "").ToUpperInvariant(), n = (name ?? "").ToLowerInvariant();
            if (c == "" || c == "NPC_ALIVE_ON") return !Regex.IsMatch(n, "death|die");
            if (c == "NPC_ALIVE_START") return !Regex.IsMatch(n,"death|die");
            if (c == "NPC_DEATH_START") return Regex.IsMatch(n, "death|die");
            if (c == "NPC_ANGRY_START") return Regex.IsMatch(n, "angry|rage|anger");
            if (c.EndsWith("WALKING_OFF")) return !Regex.IsMatch(n, "run|walk");
            if (Regex.IsMatch(c, "DEPART|TAKEOFF")) return Regex.IsMatch(n, "jumpstart|flyup");
            if (c.Contains("LANDING")) return Regex.IsMatch(n, "jumpendland|jumpend");
            if (c.Contains("BOOST")) return Regex.IsMatch(n, "flyfast|runfast");
            if (c.Contains("DESCENT")) return n.Contains("flydescent");
            if (Regex.IsMatch(c, "NORAML|NORMAL")) return n.StartsWith("fly") && !Regex.IsMatch(n, "fast|descent");
            if (Regex.IsMatch(c, "MOVE|RUN|WALK")) return Regex.IsMatch(n, "run|walk");
            return false;
        }
        void OnDisable()
        {
            StopAllCoroutines(); foreach (var go in attached.Values) if (go) Destroy(go);
            attached.Clear();pending.Clear(); current = ""; previous = 0;
        }
    }
}
