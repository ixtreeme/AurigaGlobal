using System;
using System.Threading;
namespace Metin2.Editor.Patching
{
    internal sealed class Metin2PublisherActivity
    {
        readonly long idleLimit;
        long lastActivity;
        internal Metin2PublisherActivity(long idleMilliseconds)
        {if(idleMilliseconds<=0)throw new ArgumentOutOfRangeException("idleMilliseconds");idleLimit=idleMilliseconds;}
        internal void Observe(long milliseconds)
        {
            long previous=Interlocked.Read(ref lastActivity);
            while(milliseconds>previous)
            {
                long actual=Interlocked.CompareExchange(ref lastActivity,milliseconds,previous);
                if(actual==previous)return;previous=actual;
            }
        }
        internal bool IsStalled(long milliseconds){return milliseconds-Interlocked.Read(ref lastActivity)>=idleLimit;}
    }
}
