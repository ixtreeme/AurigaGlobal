param([Parameter(Mandatory=$true)][string]$PlayerRoot,[Parameter(Mandatory=$true)][string]$ConfigPath,[switch]$VerifyUnchanged)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PublisherProgress.ps1')
. (Join-Path $PSScriptRoot 'WindowsClientHash.ps1')
. (Join-Path $PSScriptRoot 'WindowsClientUploadPlan.ps1')
. (Join-Path $PSScriptRoot 'WindowsPublisherState.ps1')
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root = [IO.Path]::GetFullPath($PlayerRoot).TrimEnd('\','/')
$releaseId = [IO.File]::ReadAllText((Join-Path $projectRoot 'PatchClientReleases/prepared.txt')).Trim()
if ($releaseId -notmatch '^[0-9]{8}-[0-9]{6}-[a-f0-9]{8}$') { throw 'Unsafe client release identifier.' }
$releasePath = Join-Path $projectRoot ('PatchClientReleases/' + $releaseId + '/release.json')
& (Join-Path $projectRoot 'Build/WindowsLauncher/LauncherTests.exe') --verify-player $releasePath $root
if ($LASTEXITCODE -ne 0) { throw 'Client changed after signing; refusing publish.' }
$envelope = Get-Content -Raw -LiteralPath $releasePath | ConvertFrom-Json
$manifest = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($envelope.payload)) | ConvertFrom-Json
$contentRoot = Join-Path $projectRoot 'PatchOutput/windows'
$content = Get-Content -Raw -LiteralPath (Join-Path $contentRoot 'release.json') | ConvertFrom-Json
if ($content.releaseId -ne $manifest.contentReleaseId -or $content.clientVersion -ne $manifest.clientVersion) { throw 'Prepared client content release changed.' }
# Catalogs/bundles are available before a client referencing them can activate.
$dataPlan = Join-Path $contentRoot 'client-publish-plan.json'
if (-not (Test-Path -LiteralPath $dataPlan)) { throw 'Verified client content delta plan missing.' }
if (@((Get-Content -Raw -LiteralPath $dataPlan | ConvertFrom-Json).files).Count -gt 0) {
    & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Publish-Ftps.ps1') -ConfigPath $ConfigPath -LocalRoot $contentRoot -PlanPath $dataPlan -Platform windows -SkipRelease
    if ($LASTEXITCODE -ne 0) { throw 'Windows content staging failed; client not activated.' }
}
Add-Type -Path 'C:\Program Files (x86)\WinSCP\WinSCPnet.dll'
$config = Get-Content -Raw -LiteralPath $ConfigPath | ConvertFrom-Json
if ($config.protocol -ne 'ftps' -or $config.host -notmatch '^[A-Za-z0-9.-]+$' -or $config.username -notmatch '^[A-Za-z0-9._-]+$' -or $config.remoteRoot -notmatch '^/[A-Za-z0-9/_.-]+$' -or $config.publicBaseUrl -ne 'https://www.metin2pegasus.eu/aurigapatch') { throw 'Unsafe or unexpected client publisher configuration.' }
$password = [Environment]::GetEnvironmentVariable($config.passwordEnvironmentVariable)
if (-not $password -and $config.credentialFilePath) { $password = ([IO.File]::ReadAllLines($config.credentialFilePath)[3] -split "`t")[-1].Trim() }
if (-not $password) { throw 'FTPS credential missing.' }
$options = New-Object WinSCP.SessionOptions
$options.Protocol=[WinSCP.Protocol]::Ftp; $options.FtpSecure=[WinSCP.FtpSecure]::Explicit
$options.HostName=$config.host; $options.PortNumber=[int]$config.port; $options.UserName=$config.username; $options.Password=$password
$session = New-Object WinSCP.Session
$session.ExecutablePath='C:\Program Files (x86)\WinSCP\WinSCP.exe'
$session.add_FileTransferProgress({ Write-AurigaTransferProgress $_ })
$remote = $config.remoteRoot.TrimEnd('/') + '/windows'
$public = $config.publicBaseUrl.TrimEnd('/') + '/windows'
$receiptPath = Join-Path $projectRoot 'PatchClientReleases/verified-blobs.json'
$destination = $config.protocol + '://' + $config.host + ':' + $config.port + $remote
$knownHashes = @()
if (-not $VerifyUnchanged -and (Test-Path -LiteralPath $receiptPath)) {
    try { $receipt = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json; if ($receipt.destination -eq $destination) { $knownHashes = @($receipt.sha256s) } } catch { $knownHashes = @() }
}
if (-not $VerifyUnchanged -and $knownHashes.Count -eq 0) {
    # Bootstrap the fast path from an already successful local publication.
    # A changed destination/configuration never inherits the previous receipt.
    $previousReceipt = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'PatchClientReleases') -Filter published.txt -File -Recurse | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($previousReceipt -and (Get-Item -LiteralPath $ConfigPath).LastWriteTimeUtc -le $previousReceipt.LastWriteTimeUtc) {
        try {
            $previousEnvelope = Get-Content -Raw -LiteralPath (Join-Path $previousReceipt.DirectoryName 'release.json') | ConvertFrom-Json
            $previousManifest = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($previousEnvelope.payload)) | ConvertFrom-Json
            $knownHashes = @($previousManifest.files | ForEach-Object { $_.sha256 })
        } catch { $knownHashes = @() }
    }
}
$verifiedHashes = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
foreach ($hash in @($knownHashes)) { if ($hash -match '^[a-f0-9]{64}$') { $null=$verifiedHashes.Add([string]$hash) } }
$uploadFiles = @(Get-AurigaClientUploadDelta $manifest.files $knownHashes)
Write-Output ('METIN2_WINDOWS_CLIENT_DELTA changedBlobs=' + $uploadFiles.Count + ' changedBytes=' + (($uploadFiles | Measure-Object size -Sum).Sum))
function Open-Session { if($session.Opened){$session.Close()}; $session.Open($options) }
function Ensure-Directory([string]$path) { try{$null=$session.ListDirectory($path)}catch{$session.CreateDirectory($path)} }
function Verify-Public([string]$relative,[long]$size,[string]$hash) {
    $request=[Net.HttpWebRequest][Net.WebRequest]::Create($public+'/'+$relative+'?verify='+[DateTime]::UtcNow.Ticks)
    $request.Method=if($hash){'GET'}else{'HEAD'}; $request.Timeout=30000; $request.ReadWriteTimeout=60000
    $response=$request.GetResponse()
    try {
        if([int]$response.StatusCode -ne 200 -or ($response.ContentLength -ge 0 -and $response.ContentLength -ne $size)){throw 'Public file size mismatch.'}
        if($hash){$actual=Get-AurigaStreamSha256 $response.GetResponseStream() $size $relative; if($actual -ne $hash){throw 'Public metadata hash mismatch.'}}
    } finally {$response.Dispose()}
}
function Upload-File([string]$source,[string]$relative,[bool]$activate) {
    $target=$remote+'/'+$relative; $temp=$target+'.uploading'; $size=(Get-Item -LiteralPath $source).Length
    for($attempt=1;$attempt -le 4;$attempt++){
        try {
            if(-not $session.Opened){Open-Session}
            if(-not $activate -and $session.FileExists($target) -and $session.GetFileInfo($target).Length -eq $size){
                $blobHash=if($relative -match '^client/blobs/([a-f0-9]{64})$'){$Matches[1]}else{''}
                try{Verify-Public $relative $size $blobHash;return}catch{Write-Output ('METIN2_WINDOWS_CLIENT_REPAIR '+$relative)}
            }
            $session.PutFiles($source,$temp,$false).Check()
            if($session.GetFileInfo($temp).Length -ne $size){throw 'Client upload incomplete.'}
            $session.MoveFile($temp,$target)
            $hash=if($activate){Get-AurigaFileSha256 $source}else{''}
            Verify-Public $relative $size $hash
            Write-Output ('METIN2_WINDOWS_CLIENT_UPLOAD '+$relative)
            return
        } catch {if($attempt -eq 4){throw};try{$session.Close()}catch{};Start-Sleep -Seconds (2*$attempt);Open-Session}
    }
}
try {
    Open-Session
    foreach($directory in @('client','client/blobs','client/releases',('client/releases/'+$releaseId))){Ensure-Directory ($remote+'/'+$directory)}
    foreach($file in $uploadFiles){
        $local=[IO.Path]::GetFullPath((Join-Path $root $file.path.Replace('/','\')))
        if(-not $local.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $file.sha256 -notmatch '^[a-f0-9]{64}$'){throw 'Unsafe client filename.'}
        Upload-File $local ('client/blobs/'+$file.sha256) $false
        $null=$verifiedHashes.Add([string]$file.sha256)
        $checkpoint=[ordered]@{schema=1;destination=$destination;sha256s=@($verifiedHashes | Sort-Object);complete=$false} | ConvertTo-Json -Compress
        Save-AurigaPublisherState $receiptPath $checkpoint
    }
    Upload-File $releasePath ('client/releases/'+$releaseId+'/release.json') $true
    # One signed envelope avoids a manifest/signature activation race.
    Upload-File $releasePath 'client/latest.json' $true
    # Raise the minimum client version only after the matching automatic update is live.
    Upload-File (Join-Path $contentRoot 'release.json') 'release.json' $true
    [IO.File]::WriteAllText((Join-Path $projectRoot ('PatchClientReleases/'+$releaseId+'/published.txt')),[DateTime]::UtcNow.ToString('o'))
    $receipt = [ordered]@{schema=1;destination=$destination;sha256s=@($manifest.files | ForEach-Object { $_.sha256 } | Sort-Object -Unique)} | ConvertTo-Json -Compress
    Save-AurigaPublisherState $receiptPath $receipt
    Write-Output ('METIN2_WINDOWS_CLIENT_PUBLISH=SUCCESS release='+$releaseId)
} catch {Write-Error $_.Exception.Message.Replace($password,'[redacted]');exit 1}
finally {$session.Dispose();$password=$null}
