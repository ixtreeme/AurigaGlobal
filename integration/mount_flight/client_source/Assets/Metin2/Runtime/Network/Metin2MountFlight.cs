using System;
using System.Collections.Generic;
using UnityEngine;
namespace Metin2.Runtime.Network
{
    // Runs before actor motion, so effect emitters also see the correct airborne position.
    [DefaultExecutionOrder(-100)]
    public sealed class Metin2MountFlight:MonoBehaviour
    {
        public bool Capable{get;private set;}
        public bool Flying{get;private set;}
        public float Altitude{get;private set;}
        public int Axis{get;private set;}
        public const float VerticalSpeed=6f;
        Metin2PlayerController controller;
        Metin2WorldRuntime terrain;
        Action<byte,int,uint> send;
        float targetAltitude,nextSend;Vector3 previous;
        bool local;bool initialized;
        GameObject animationMount;Animator flightAnimator;
        readonly Dictionary<string,int> animationStates=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase);
        static readonly string[] UpStates={"FlyUp","FlyForward","FlyWait"},DownStates={"FlyDescent","FlyForward","FlyWait"},MoveStates={"FlyForward","FlyFast"},WaitStates={"FlyWait","FlyForward"};
        public void Bind(Metin2PlayerController player,Metin2WorldRuntime world,Action<byte,int,uint> sender)
        {controller=player;local=player!=null;terrain=world;send=sender;previous=transform.position;}
        public void ApplyPacket(byte[] p)
        {
            if(p.Length!=11||p[0]!=225)throw new ArgumentException("Invalid flight packet");
            bool was=Flying,wasCapable=Capable;Capable=(p[5]&1)!=0;Flying=Capable&&(p[5]&2)!=0;
#if UNITY_EDITOR || DEVELOPMENT_BUILD
            if(local&&(!initialized||wasCapable!=Capable||was!=Flying))Debug.Log($"MOUNT_FLIGHT_LOCAL_STATE capable={Capable} flying={Flying} altitudeCm={BitConverter.ToInt32(p,6)}");
#endif
            targetAltitude=BitConverter.ToInt32(p,6)*.01f;Axis=(sbyte)p[10];
            if(!initialized||!local||!was||!Flying||Mathf.Abs(Altitude-targetAltitude)>1f)
            {if(!initialized||!was||local)Altitude=targetAltitude;}
            initialized=true;
            if(was&&!Flying)Ground();
            if(Flying)ApplyHeight();
        }
        public void Tick(int axis,float dt,uint time)
        {
            if(!local)return;
            if(!controller.IsMounted){if(Flying)Land();return;}
            if(!Capable)return;
            if(!Flying&&axis>0){Flying=true;Altitude=controller.LogicalGroundPosition.y;Axis=1;nextSend=0;}
            if(!Flying)return;
            Altitude=Mathf.Clamp(Altitude+Mathf.Clamp(axis,-1,1)*VerticalSpeed*Mathf.Min(dt,.1f),-10000,10000);Axis=axis;
            if(axis<0&&terrain!=null&&terrain.HasMap&&Altitude<=terrain.GroundHeight(transform.position)+.02f)
            {Altitude=terrain.GroundHeight(transform.position);Land();return;}
            ApplyHeight();
            if(Time.unscaledTime>=nextSend){nextSend=Time.unscaledTime+.1f;send?.Invoke(1,Mathf.RoundToInt(Altitude*100),time);}
        }
        public bool CanAdvance(Vector3 position)
        {
            if(terrain==null||!terrain.HasMap)return true;
            terrain.Current.ToGlobal(position,out int x,out int y);
            return terrain.Current.Contains(x,y)&&terrain.GroundHeight(position)<Altitude-.02f;
        }
        public void Land()
        {
            if(!Flying)return;Flying=false;Axis=0;Ground();
            if(local)send?.Invoke(2,Mathf.RoundToInt(Altitude*100),unchecked((uint)Environment.TickCount));
            GetComponent<Metin2NetworkEntity>()?.ApplyMountLocomotion(false,false);
        }
        void Ground(){if(terrain!=null&&terrain.HasMap){Altitude=terrain.GroundHeight(transform.position);var p=transform.position;p.y=Altitude-(local?controller.FootOffset:0);transform.position=p;}}
        void ApplyHeight(){var p=transform.position;p.y=Altitude-(local?controller.FootOffset:0);transform.position=p;}
        void Update(){if(!local&&Flying){Altitude=Mathf.MoveTowards(Altitude,targetAltitude,VerticalSpeed*Time.deltaTime);ApplyHeight();}}
        void LateUpdate()
        {
            if(Flying){ApplyHeight();var delta=transform.position-previous;delta.y=0;ApplyAnimation(delta.sqrMagnitude>.000001f);}
            previous=transform.position;
        }
        public void ApplyAnimation(bool moving)
        {
            var mount=local?controller.ActiveMountInstance:GetComponent<Metin2NetworkEntity>()?.MountInstance;
            if(!mount)return;
            if(animationMount!=mount||!flightAnimator)
            {
                animationMount=mount;flightAnimator=mount.GetComponentInChildren<Animator>();animationStates.Clear();
                if(flightAnimator&&flightAnimator.runtimeAnimatorController)
                    foreach(var clip in flightAnimator.runtimeAnimatorController.animationClips)animationStates[clip.name]=Animator.StringToHash(clip.name);
            }
            if(!flightAnimator)return;
            var choices=Axis>0?UpStates:Axis<0?DownStates:moving?MoveStates:WaitStates;
            foreach(var choice in choices)if(animationStates.TryGetValue(choice,out int hash))
            {if(flightAnimator.GetCurrentAnimatorStateInfo(0).shortNameHash!=hash)flightAnimator.Play(hash,0);return;}
        }
    }
}
