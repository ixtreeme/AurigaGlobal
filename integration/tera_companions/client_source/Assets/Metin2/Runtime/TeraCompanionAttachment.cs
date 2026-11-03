using System;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEngine;
namespace Metin2.Runtime
{
    /// <summary>Preserves the original separate wing/body rig and follows its parent's motion edges.</summary>
    public sealed class TeraCompanionAttachment : MonoBehaviour
    {
        public TeraCompanionPresentation parent;
        string previous="";
        Animator animator;
        TeraCompanionPresentation presentation;
        void Awake(){animator=GetComponent<Animator>();presentation=GetComponent<TeraCompanionPresentation>();}
        void LateUpdate()
        {
            if(!parent||!animator||!presentation)return;
            var parentAnimator=parent.GetComponent<Animator>();if(parentAnimator)animator.speed=parentAnimator.speed;
            if(parent.CurrentMotion==previous)return;
            previous=parent.CurrentMotion;
            var motion=presentation.motions.FirstOrDefault(m=>m.source.Equals(previous,StringComparison.OrdinalIgnoreCase))
                ??presentation.motions.FirstOrDefault(m=>Regex.IsMatch(m.source,"^(unarmedwait|idle01|idle|wait)$",RegexOptions.IgnoreCase))
                ??presentation.motions.FirstOrDefault();
            if(motion!=null)animator.Play(motion.state,0,0);
        }
    }
}
