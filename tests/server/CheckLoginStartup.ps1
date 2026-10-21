param([Parameter(Mandatory = $true)][string]$Binary)

$ErrorActionPreference = 'Stop'
$taskBinary = (Resolve-Path -LiteralPath $Binary).Path
$taskOutput = Join-Path ([IO.Path]::GetTempPath()) ('auriga-login-startup-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOutput | Out-Null

foreach ($taskMode in @('refused', 'silent')) {
    $taskProbe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $taskProbe.Start()
    $taskSqlPort = $taskProbe.LocalEndpoint.Port
    if ($taskMode -eq 'refused') { $taskProbe.Stop() }
    $taskDir = Join-Path $taskOutput $taskMode
    New-Item -ItemType Directory -Path $taskDir | Out-Null
    $taskConfig = Join-Path $taskDir 'config.json'
    @{
        schema_version = 1
        common = @{
            sql = @{
                account = @{ host = '127.0.0.1'; port = $taskSqlPort; user = 'root'; password = ''; database = 'account' }
                player = @{ host = '127.0.0.1'; database = 'player' }
                common = @{ host = '127.0.0.1'; database = 'common' }
            }
            tls = @{ cert_file = 'unused-cert.pem'; key_file = 'unused-key.pem' }
        }
        database = @{ item_id_range = @(1, 2) }
        login = @{ port = 55991; working_directory = '.' }
        game_servers = @{}
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskConfig
    $taskProcess = $null
    $taskTimer = [Diagnostics.Stopwatch]::StartNew()
    try {
        # The silent listener accepts TCP in the kernel, but never sends an
        # SQL greeting. Only this script's child process may be terminated.
        $taskProcess = Start-Process -FilePath $taskBinary -ArgumentList @('--config', ('"' + $taskConfig + '"')) `
            -WorkingDirectory $taskDir -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $taskDir 'stdout.log') `
            -RedirectStandardError (Join-Path $taskDir 'stderr.log')
        if (-not $taskProcess.WaitForExit(12000)) {
            throw "$taskMode SQL exceeded the startup deadline"
        }
        $taskProcess.Refresh()
        if ($taskProcess.ExitCode -ne 1) {
            throw "$taskMode SQL exited with $($taskProcess.ExitCode), expected 1"
        }
        $taskLogs = Get-ChildItem -LiteralPath $taskDir -Recurse -File -Filter '*.txt' |
            Where-Object Name -in @('syslog.txt', 'syserr.txt') |
            ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }
        $taskText = $taskLogs -join "`n"
        if ($taskText -notmatch 'account database is not available' -or
            $taskText -match 'TLS certificate could not be loaded|listening on port') {
            throw "$taskMode SQL did not abort before TLS/listener initialization"
        }
        Write-Output "$taskMode SQL: exit 1 in $([Math]::Round($taskTimer.Elapsed.TotalSeconds, 2)) seconds"
    } finally {
        $taskProbe.Stop()
        if ($taskProcess) {
            if (-not $taskProcess.HasExited) { Stop-Process -Id $taskProcess.Id }
            $taskProcess.Dispose()
        }
    }
}
Write-Output "Startup logs: $taskOutput"
