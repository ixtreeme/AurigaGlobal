# Run in an administrator PowerShell once clients are ready for a LAN test.
param([string]$Package = 'D:\AurigaGlobal\Server2-Local')
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Administrator PowerShell is required to add Windows Firewall rules.'
}
$packagePath = (Resolve-Path -LiteralPath $Package).Path.TrimEnd('\')
$config = Get-Content -LiteralPath (Join-Path $packagePath 'server.json') -Raw | ConvertFrom-Json
$bindAddress = [string]$config.game_defaults.network.bind_ip
if (-not (Get-NetIPAddress -AddressFamily IPv4 | Where-Object IPAddress -EQ $bindAddress)) {
    throw 'The configured server address does not belong to this computer.'
}
foreach ($entry in @(
    @{ Name='NexoriaLocalGame'; Binary='GameServer.exe'; Ports=@(30054,30056,30078) },
    @{ Name='NexoriaLocalAuth'; Binary='LoginServer.exe'; Ports=@(30052) }
)) {
    $programPath = (Resolve-Path -LiteralPath (Join-Path $packagePath ('bin\' + $entry.Binary))).Path
    if (-not $programPath.StartsWith($packagePath + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected executable path.' }
    if (Get-NetFirewallRule -Name $entry.Name -ErrorAction SilentlyContinue) {
        Set-NetFirewallRule -Name $entry.Name -Enabled True -Action Allow -Direction Inbound -Profile Any -Program $programPath -Protocol TCP -LocalPort $entry.Ports -LocalAddress $bindAddress -RemoteAddress LocalSubnet | Out-Null
    } else {
        New-NetFirewallRule -Name $entry.Name -DisplayName $entry.Name -Enabled True -Action Allow -Direction Inbound -Profile Any -Program $programPath -Protocol TCP -LocalPort $entry.Ports -LocalAddress $bindAddress -RemoteAddress LocalSubnet | Out-Null
    }
}
if (Get-NetFirewallRule -Name NexoriaLocalContent -ErrorAction SilentlyContinue) {
    Set-NetFirewallRule -Name NexoriaLocalContent -Enabled True -Action Allow -Direction Inbound -Profile Any -Protocol TCP -LocalPort 30090 -LocalAddress $bindAddress -RemoteAddress LocalSubnet | Out-Null
} else {
    New-NetFirewallRule -Name NexoriaLocalContent -DisplayName NexoriaLocalContent -Enabled True -Action Allow -Direction Inbound -Profile Any -Protocol TCP -LocalPort 30090 -LocalAddress $bindAddress -RemoteAddress LocalSubnet | Out-Null
}
Write-Output ('Nexoria LAN rules enabled for ' + $bindAddress + '; remote scope: LocalSubnet. Database and P2P ports are unchanged.')
