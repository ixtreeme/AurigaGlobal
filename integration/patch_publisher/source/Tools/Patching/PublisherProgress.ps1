$script:aurigaProgressFile = ''
$script:aurigaProgressValue = -1.0
$script:aurigaProgressTime = [DateTime]::MinValue
function Write-AurigaTransferProgress($event) {
    if ($null -eq $event) { return }
    if ($script:aurigaProgressFile -ne $event.FileName) {
        $script:aurigaProgressFile = $event.FileName
        $script:aurigaProgressValue = -1.0
        $script:aurigaProgressTime = [DateTime]::MinValue
    }
    $now = [DateTime]::UtcNow
    if ($event.FileProgress -le $script:aurigaProgressValue) { return }
    if (($now - $script:aurigaProgressTime).TotalSeconds -lt 5 -and $event.FileProgress -lt 1) { return }
    $script:aurigaProgressValue = $event.FileProgress
    $script:aurigaProgressTime = $now
    [Console]::WriteLine(('METIN2_PUBLISH_PROGRESS upload percent={0:F1} file={1}' -f ($event.FileProgress*100),[IO.Path]::GetFileName($event.FileName)))
}
function Get-AurigaStreamSha256($stream,[long]$length,[string]$label) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $buffer = New-Object byte[] 1048576
    $received = [long]0
    $reported = [DateTime]::UtcNow
    try {
        while (($read = $stream.Read($buffer,0,$buffer.Length)) -gt 0) {
            $null = $sha.TransformBlock($buffer,0,$read,$buffer,0)
            $received += $read
            if (([DateTime]::UtcNow-$reported).TotalSeconds -ge 5) {
                [Console]::WriteLine(('METIN2_PUBLISH_PROGRESS verify bytes={0}/{1} file={2}' -f $received,$length,$label))
                $reported = [DateTime]::UtcNow
            }
        }
        $null = $sha.TransformFinalBlock((New-Object byte[] 0),0,0)
        return [BitConverter]::ToString($sha.Hash).Replace('-','').ToLowerInvariant()
    } finally { $sha.Dispose() }
}
