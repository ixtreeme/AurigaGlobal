param(
    [Parameter(Mandatory=$true)][string]$ConfigPath,
    [Parameter(Mandatory=$true)][string]$LocalRoot,
    [Parameter(Mandatory=$true)][string]$PlanPath,
    [Parameter(Mandatory=$true)][ValidateSet('windows','android','ios')][string]$Platform,
    [switch]$BundlesOnly,
    [switch]$SkipRelease
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'PublisherProgress.ps1')
Add-Type -Path 'C:\Program Files (x86)\WinSCP\WinSCPnet.dll'
$config = Get-Content -Raw -LiteralPath $ConfigPath | ConvertFrom-Json
if ($config.protocol -ne 'ftps' -or $config.host -notmatch '^[A-Za-z0-9.-]+$' -or
    $config.username -notmatch '^[A-Za-z0-9._-]+$' -or
    $config.remoteRoot -notmatch '^/[A-Za-z0-9/_.-]+$' -or
    [int]$config.port -lt 1 -or [int]$config.port -gt 65535) {
    throw 'Invalid FTPS publisher configuration.'
}
$password = [Environment]::GetEnvironmentVariable($config.passwordEnvironmentVariable)
if ([string]::IsNullOrEmpty($password) -and -not [string]::IsNullOrEmpty($config.credentialFilePath)) {
    $lines = [IO.File]::ReadAllLines($config.credentialFilePath)
    if ($lines.Length -lt 4) { throw 'Local credential file is incomplete.' }
    $password = ($lines[3] -split "`t")[-1].Trim()
}
if ([string]::IsNullOrEmpty($password)) { throw 'FTPS password is not configured.' }

$root = [IO.Path]::GetFullPath($LocalRoot).TrimEnd('\','/')
$plan = Get-Content -Raw -LiteralPath $PlanPath | ConvertFrom-Json
if (-not $plan.files) { throw 'Publish plan is empty.' }
$remoteRoot = $config.remoteRoot.TrimEnd('/') + '/' + $Platform
$options = New-Object WinSCP.SessionOptions
$options.Protocol = [WinSCP.Protocol]::Ftp
$options.FtpSecure = [WinSCP.FtpSecure]::Explicit
$options.HostName = $config.host
$options.PortNumber = [int]$config.port
$options.UserName = $config.username
$options.Password = $password
$session = New-Object WinSCP.Session
$session.ExecutablePath = 'C:\Program Files (x86)\WinSCP\WinSCP.exe'
$session.add_FileTransferProgress({ Write-AurigaTransferProgress $_ })

function Open-Session {
    if ($session.Opened) { $session.Close() }
    $session.Open($options)
}

function Get-Local([string]$relative) {
    if ($relative -notmatch '^[A-Za-z0-9/_.-]+$' -or $relative.Contains('..') -or $relative.StartsWith('/')) {
        throw 'Unsafe publish filename.'
    }
    $path = [IO.Path]::GetFullPath((Join-Path $root ($relative -replace '/', '\')))
    if (-not $path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw 'Publish file is missing or outside the output directory.'
    }
    return $path
}

function Ensure-RemoteDirectory([string]$directory) {
    try { $null = $session.ListDirectory($directory) }
    catch { $session.CreateDirectory($directory) }
}

function Verify-Https([string]$relative, [long]$length) {
    $url = ($config.publicBaseUrl.TrimEnd('/') + '/') + $Platform + '/' + $relative
    $request = [Net.HttpWebRequest][Net.WebRequest]::Create($url)
    $request.Method = 'HEAD'
    $request.Timeout = 30000
    try {
        $response = [Net.HttpWebResponse]$request.GetResponse()
        try {
            if ([int]$response.StatusCode -ne 200 -or
                ($response.ContentLength -ge 0 -and $response.ContentLength -ne $length) -or
                ($response.ContentType -and $response.ContentType.StartsWith('text/html'))) {
                throw 'HTTPS size or content type mismatch.'
            }
        } finally { $response.Dispose() }
    } catch [Net.WebException] {
        $status = $_.Exception.Response.StatusCode
        if ([int]$status -notin @(405,501)) { throw }
        $_.Exception.Response.Dispose()
        $range = [Net.HttpWebRequest][Net.WebRequest]::Create($url)
        $range.Method = 'GET'
        $range.AddRange(0,0)
        $range.Timeout = 30000
        $response = [Net.HttpWebResponse]$range.GetResponse()
        try {
            if ([int]$response.StatusCode -ne 206 -or
                -not $response.Headers['Content-Range'].EndsWith('/' + $length)) {
                throw 'HTTPS range verification failed.'
            }
        } finally { $response.Dispose() }
    }
}

function Upload([string]$relative, [bool]$activate) {
    $local = Get-Local $relative
    $target = $remoteRoot + '/' + $relative
    $directory = $target.Substring(0, $target.LastIndexOf('/'))
    $temporary = $target + '.uploading'
    $length = (Get-Item -LiteralPath $local).Length
    for ($attempt = 1; $attempt -le 4; $attempt++) {
        try {
            if (-not $session.Opened) { Open-Session }
            if (-not $activate -and $session.FileExists($target) -and $session.GetFileInfo($target).Length -eq $length) {
                Verify-Https $relative $length
                Write-Output ('METIN2_PATCH_VERIFY existing ' + $Platform + '/' + $relative)
                return
            }
            if ($directory -ne $remoteRoot) { Ensure-RemoteDirectory $directory }
            if ($session.FileExists($temporary)) { $session.RemoveFiles($temporary).Check() }
            $session.PutFiles($local, $temporary, $false).Check()
            if ($session.GetFileInfo($temporary).Length -ne $length) { throw 'FTPS uploaded size mismatch.' }
            $session.MoveFile($temporary, $target)
            if ($session.GetFileInfo($target).Length -ne $length) { throw 'FTPS activated size mismatch.' }
            Verify-Https $relative $length
            Write-Output ('METIN2_PATCH_VERIFY ' + $Platform + '/' + $relative)
            return
        } catch {
            if ($attempt -eq 4) { throw }
            try { if ($session.Opened) { $session.Close() } } catch {}
            Start-Sleep -Seconds ([Math]::Min(8, [Math]::Pow(2, $attempt)))
            Open-Session
        }
    }
}

try {
    Open-Session
    Ensure-RemoteDirectory ($config.remoteRoot.TrimEnd('/'))
    Ensure-RemoteDirectory $remoteRoot
    $files = @($plan.files)
    foreach ($file in $files | Where-Object { $_.path -like '*.bundle' }) { Upload $file.path $false }
    if ($BundlesOnly) {
        Write-Output ('METIN2_PATCH_PUBLISH bundles-only-success platform=' + $Platform)
        exit 0
    }
    foreach ($file in $files | Where-Object { $_.path -notlike '*.bundle' -and $_.path -notlike 'catalog*.json' -and $_.path -notlike 'catalog*.hash' -and $_.path -ne 'release.json' }) { Upload $file.path $false }
    foreach ($file in $files | Where-Object { $_.path -like 'catalog*.json' }) { Upload $file.path $true }
    foreach ($file in $files | Where-Object { $_.path -like 'catalog*.hash' }) { Upload $file.path $true }
    if ($SkipRelease) { Write-Output ('METIN2_PATCH_PUBLISH data-staged-success platform=' + $Platform); exit 0 }
    Upload 'release.json' $true
    Write-Output ('METIN2_PATCH_PUBLISH success platform=' + $Platform)
} catch {
    $message = $_.Exception.Message.Replace($password, '[redacted]')
    Write-Error ('METIN2_PATCH_PUBLISH failed: ' + $message)
    exit 1
} finally {
    $session.Dispose()
    $password = $null
}
