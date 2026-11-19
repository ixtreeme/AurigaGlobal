$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'PublisherProgress.ps1')
. (Join-Path $PSScriptRoot 'WindowsClientUploadPlan.ps1')
$source=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../../Assets/Metin2/Editor/Patching/Metin2PublisherActivity.cs')
$test=@'
public static class PublisherActivityChecks {
 public static void Run(){
  var monitor=new Metin2.Editor.Patching.Metin2PublisherActivity(1000);
  for(long now=0;now<10000000;now+=500){monitor.Observe(now);if(monitor.IsStalled(now+499))throw new System.Exception("Progressing transfer timed out");}
  if(!monitor.IsStalled(10001000))throw new System.Exception("Silent transfer did not time out");
  monitor.Observe(11000000);monitor.Observe(1);
  if(monitor.IsStalled(11000001))throw new System.Exception("Late callback rewound activity");
  bool rejected=false;try{new Metin2.Editor.Patching.Metin2PublisherActivity(0);}catch(System.ArgumentOutOfRangeException){rejected=true;}
  if(!rejected)throw new System.Exception("Invalid watchdog deadline accepted");
 }
}
'@
Add-Type -TypeDefinition ($source+[Environment]::NewLine+$test)
[PublisherActivityChecks]::Run()
$bytes=New-Object byte[] 3145728
for($i=0;$i -lt $bytes.Length;$i++){ $bytes[$i]=[byte]($i%251) }
$stream=New-Object IO.MemoryStream(,$bytes)
$sha=[Security.Cryptography.SHA256]::Create()
try{
 $expected=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','').ToLowerInvariant()
 $actual=Get-AurigaStreamSha256 $stream $bytes.Length 'fixture'
 if($actual -ne $expected){throw 'Streaming verification changed SHA256'}
}finally{$stream.Dispose();$sha.Dispose()}
$console=[Console]::Out;$capture=New-Object IO.StringWriter
try{
 [Console]::SetOut($capture)
 Write-AurigaTransferProgress ([pscustomobject]@{FileName='fixture';FileProgress=0.0})
 for($i=0;$i -lt 100;$i++){Write-AurigaTransferProgress ([pscustomobject]@{FileName='fixture';FileProgress=0.0})}
 Write-AurigaTransferProgress ([pscustomobject]@{FileName='fixture';FileProgress=1.0})
}finally{[Console]::SetOut($console)}
if(@($capture.ToString().Split([Environment]::NewLine.ToCharArray(),[StringSplitOptions]::RemoveEmptyEntries)).Count -ne 2){throw 'Stationary transfer generated fake progress'}
$hashA='a'*64;$hashB='b'*64
$files=@([pscustomobject]@{sha256=$hashA;size=1},[pscustomobject]@{sha256=$hashB;size=2},[pscustomobject]@{sha256=$hashA;size=1})
$remaining=@(Get-AurigaClientUploadDelta $files @($hashA))
if($remaining.Count -ne 1 -or $remaining[0].sha256 -ne $hashB){throw 'Checkpoint resume did not skip only verified blobs'}
Write-Output 'PUBLISHER_PROGRESS_TESTS=PASS watchdog>2h, inactivity, late callback, SHA256, real-progress events and partial resume'
