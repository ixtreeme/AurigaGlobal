# Stream files directly through .NET: Unity can inherit a PowerShell module
# search path that does not expose the Windows PowerShell Get-FileHash cmdlet.
function Get-AurigaFileSha256([string]$LiteralPath) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $inputStream = $null
    try {
        $inputStream = [IO.File]::OpenRead([IO.Path]::GetFullPath($LiteralPath))
        return [BitConverter]::ToString($sha.ComputeHash($inputStream)).Replace('-','').ToLowerInvariant()
    } finally {
        if ($null -ne $inputStream) { $inputStream.Dispose() }
        $sha.Dispose()
    }
}
