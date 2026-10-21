# A disposable Windows MariaDB process; never registers or uses a service.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$MariaDBBin,
    [Parameter(Mandatory = $true)][string]$ClientLibrary,
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$MariaDBBin = (Resolve-Path -LiteralPath $MariaDBBin).Path
$ClientLibrary = (Resolve-Path -LiteralPath $ClientLibrary).Path
foreach ($schemaExecutable in @('mariadb-install-db.exe', 'mariadbd.exe', 'mariadb.exe', 'mariadb-admin.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $MariaDBBin $schemaExecutable))) {
        throw "Missing MariaDB executable: $schemaExecutable"
    }
}
& $Python --version
if ($LASTEXITCODE -ne 0) { throw 'Python 3.9 or newer is required.' }

$schemaListener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$schemaListener.Start()
$schemaPort = $schemaListener.LocalEndpoint.Port
$schemaListener.Stop()
$schemaDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('auriga-schema-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $schemaDirectory)
$schemaDataDirectory = Join-Path $schemaDirectory 'data'
$schemaReport = Join-Path $schemaDirectory 'report.json'
$schemaProcess = $null

function Test-SchemaConnection {
    $schemaDataPath = & (Join-Path $MariaDBBin 'mariadb.exe') --protocol=tcp --host=127.0.0.1 "--port=$schemaPort" --user=root --connect-timeout=1 --batch --skip-column-names '--execute=SELECT @@datadir' 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $schemaDataPath) { return $false }
    $schemaActualPath = [System.IO.Path]::GetFullPath(($schemaDataPath | Select-Object -Last 1).Trim()).TrimEnd('\', '/')
    return $schemaActualPath -eq $schemaDataDirectory
}

try {
    & (Join-Path $MariaDBBin 'mariadb-install-db.exe') "--datadir=$schemaDataDirectory" "--port=$schemaPort" 2>&1 |
        Out-File -LiteralPath (Join-Path $schemaDirectory 'initialize.log') -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw "MariaDB initialization failed: $schemaDirectory" }

    $schemaArguments = @(('"--defaults-file=' + (Join-Path $schemaDataDirectory 'my.ini') + '"'), '--bind-address=127.0.0.1', '--console')
    $schemaProcess = Start-Process -FilePath (Join-Path $MariaDBBin 'mariadbd.exe') -ArgumentList $schemaArguments -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $schemaDirectory 'server.out.log') -RedirectStandardError (Join-Path $schemaDirectory 'server.err.log')
    $schemaDeadline = [DateTime]::UtcNow.AddSeconds(30)
    $schemaReady = $false
    do {
        $schemaProcess.Refresh()
        if ($schemaProcess.HasExited) { throw "Isolated MariaDB exited: $schemaDirectory" }
        # Checking @@datadir also protects against an unrelated process taking the port.
        $schemaReady = Test-SchemaConnection
        if (-not $schemaReady) { Start-Sleep -Milliseconds 200 }
    } while (-not $schemaReady -and [DateTime]::UtcNow -lt $schemaDeadline)
    if (-not $schemaReady) { throw "Isolated MariaDB did not become ready: $schemaDirectory" }

    & $Python (Join-Path $PSScriptRoot 'validate_schema.py') --port $schemaPort --library $ClientLibrary --report $schemaReport
    if ($LASTEXITCODE -ne 0) { throw "Schema validation failed: $schemaReport" }
}
finally {
    if ($null -ne $schemaProcess) {
        $schemaProcess.Refresh()
        if (-not $schemaProcess.HasExited) {
            $schemaOwner = Get-CimInstance Win32_Process -Filter "ProcessId=$($schemaProcess.Id)"
            if ($schemaOwner.Name -ne 'mariadbd.exe' -or -not $schemaOwner.CommandLine.Contains($schemaDirectory)) {
                throw 'Refusing cleanup: test process identity does not match.'
            }
            if (Test-SchemaConnection) {
                & (Join-Path $MariaDBBin 'mariadb-admin.exe') --protocol=tcp --host=127.0.0.1 "--port=$schemaPort" --user=root shutdown
                [void]$schemaProcess.WaitForExit(10000)
            }
            $schemaProcess.Refresh()
            if (-not $schemaProcess.HasExited) {
                # Only the process created above can reach this fallback.
                Stop-Process -Id $schemaProcess.Id -Force
            }
        }
        $schemaProcess.Dispose()
    }
    Write-Host "Validation files retained: $schemaDirectory"
}
