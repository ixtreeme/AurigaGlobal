using System;
using System.Collections.Generic;
using UnityEngine;

namespace Metin2.Runtime
{
    public sealed class TeraSoundCue : ScriptableObject
    {
        [Serializable] public sealed class Node
        {
            public string kind,algorithm;
            public int[] children=Array.Empty<int>();
            public float[] weights=Array.Empty<float>(),volumes=Array.Empty<float>();
            public float volumeMin=.95f,volumeMax=1.05f,pitchMin=.95f,pitchMax=1.05f;
            public float radiusMin=4,radiusMax=40,lpfMin=30,lpfMax=60;
            public bool lowpass;
            public AudioClip wave;
        }
        public string source;
        public int first,maxConcurrent=16;
        public float volume=1,pitch=1;
        public Node[] nodes=Array.Empty<Node>();
        struct Parameters
        {
            public float volume,pitch,min,max,lpfMin,lpfMax;
            public bool loop,lowpass,logarithmic;
        }
        public bool Play(Transform actor,float eventVolume)
        {
            var native=actor.GetComponentInParent<Metin2NativeActor>();var scope=native?native.transform:actor;
            int count=0;foreach(var playback in scope.GetComponentsInChildren<TeraCuePlayback>())if(playback.cue==this&&!playback.stopping){if(playback.looping)return false;count++;}
            if(maxConcurrent>0&&count>=maxConcurrent)return false;
            var group=new GameObject("TERA_Audio_"+name);group.transform.SetParent(actor,false);
            var playbackOwner=group.AddComponent<TeraCuePlayback>();playbackOwner.cue=this;
            var parameters=new Parameters{volume=Mathf.Max(0,eventVolume*volume),pitch=Mathf.Max(.01f,pitch),min=4,max=40,lpfMin=30,lpfMax=60};
            Evaluate(first,parameters,playbackOwner,0);
            if(playbackOwner.voices.Count==0){Destroy(group);return false;}
            playbackOwner.Begin();return true;
        }
        void Evaluate(int index,Parameters p,TeraCuePlayback owner,int depth)
        {
            if(index<0||index>=nodes.Length||depth>32)return;
            var node=nodes[index];
            switch(node.kind)
            {
                case "wave":
                    if(!node.wave)return;
                    var voice=new GameObject(node.wave.name);voice.transform.SetParent(owner.transform,false);
                    var audio=voice.AddComponent<AudioSource>();audio.playOnAwake=false;audio.clip=node.wave;audio.spatialBlend=1;
                    audio.volume=Mathf.Clamp01(p.volume);audio.pitch=Mathf.Clamp(p.pitch,.01f,3);audio.loop=p.loop;
                    audio.minDistance=Mathf.Max(.01f,p.min);audio.maxDistance=Mathf.Max(audio.minDistance+.01f,p.max);
                    audio.rolloffMode=p.logarithmic?AudioRolloffMode.Logarithmic:AudioRolloffMode.Linear;
                    var filter=p.lowpass?voice.AddComponent<AudioLowPassFilter>():null;
                    if(p.loop)voice.AddComponent<TeraCueGain>().gain=p.volume;
                    owner.voices.Add(new TeraCuePlayback.Voice{audio=audio,filter=filter,lpfMin=p.lpfMin,lpfMax=p.lpfMax,gain=p.volume});
                    owner.looping|=p.loop;return;
                case "attenuation":
                    p.min=node.radiusMin;p.max=node.radiusMax;p.lowpass=node.lowpass;p.lpfMin=node.lpfMin;p.lpfMax=node.lpfMax;
                    p.logarithmic=(node.algorithm??"").IndexOf("log",StringComparison.OrdinalIgnoreCase)>=0;break;
                case "modulator":
                    p.volume*=UnityEngine.Random.Range(node.volumeMin,node.volumeMax);p.pitch*=UnityEngine.Random.Range(node.pitchMin,node.pitchMax);break;
                case "looping": p.loop=true;break;
                case "random":
                    float sum=0;for(int i=0;i<node.children.Length;i++)sum+=i<node.weights.Length?Mathf.Max(0,node.weights[i]):1;
                    if(sum<=0)return;
                    float selected=UnityEngine.Random.value*sum;
                    for(int i=0;i<node.children.Length;i++)
                    {
                        selected-=i<node.weights.Length?Mathf.Max(0,node.weights[i]):1;
                        if(selected<=0){Evaluate(node.children[i],p,owner,depth+1);return;}
                    }
                    return;
                case "mixer":
                    for(int i=0;i<node.children.Length;i++)
                    {
                        var child=p;child.volume*=i<node.volumes.Length?node.volumes[i]:1;Evaluate(node.children[i],child,owner,depth+1);
                    }
                    return;
            }
            foreach(int child in node.children)Evaluate(child,p,owner,depth+1);
        }
    }
    public sealed class TeraCuePlayback : MonoBehaviour
    {
        public sealed class Voice {public AudioSource audio;public AudioLowPassFilter filter;public float lpfMin,lpfMax,gain;}
        public TeraSoundCue cue;
        public bool looping;
        public bool stopping;
        public readonly List<Voice> voices=new List<Voice>();
        static AudioListener listener;
        public void Begin()
        {
            foreach(var voice in voices)
                if(voice.audio.loop){voice.audio.volume=Network.Metin2SoundManager.EffectiveSoundVolume;voice.audio.Play();}
                else{voice.audio.volume=Network.Metin2SoundManager.EffectiveSoundVolume;voice.audio.PlayOneShot(voice.audio.clip,Mathf.Max(0,voice.gain));}
        }
        void Update()
        {
            bool alive=false;if(!listener)listener=FindObjectOfType<AudioListener>();
            float distance=listener?Vector3.Distance(listener.transform.position,transform.position):0;
            foreach(var voice in voices)
            {
                if(!voice.audio)continue;alive|=voice.audio.isPlaying;
                voice.audio.volume=Network.Metin2SoundManager.EffectiveSoundVolume;
                if(voice.filter)voice.filter.cutoffFrequency=Mathf.Lerp(22000,1000,Mathf.InverseLerp(voice.lpfMin,voice.lpfMax,distance));
            }
            if(!alive)Destroy(gameObject);
        }
    }
    public sealed class TeraCueGain : MonoBehaviour
    {
        public volatile float gain=1;
        void OnAudioFilterRead(float[] data,int channels)
        {
            float value=gain;for(int i=0;i<data.Length;i++)data[i]*=value;
        }
    }
}
