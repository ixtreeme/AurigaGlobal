function Get-AurigaClientUploadDelta($Files, $KnownHashes) {
    $known = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($hash in @($KnownHashes)) { if ($hash -match '^[a-f0-9]{64}$') { $null = $known.Add([string]$hash) } }
    $unique = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($file in @($Files)) {
        if ($file.sha256 -notmatch '^[a-f0-9]{64}$') { throw 'Invalid client blob hash.' }
        if ($unique.Add([string]$file.sha256) -and -not $known.Contains([string]$file.sha256)) { $file }
    }
}
