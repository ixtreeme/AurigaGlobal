param([Parameter(Mandatory=$true)][string]$ScriptPath)
$ErrorActionPreference='Stop'
$resolved=(Resolve-Path -LiteralPath $ScriptPath).Path
$code=[IO.File]::ReadAllText($resolved)
for($attempt=0;$attempt -lt 20;$attempt++) {
    try {
        try { $photoshop=[Runtime.InteropServices.Marshal]::GetActiveObject('Photoshop.Application') }
        catch { $photoshop=New-Object -ComObject Photoshop.Application }
        [void]$photoshop.DoJavaScript($code)
        break
    } catch {
        $hresult=$_.Exception.HResult
        if($attempt -eq 19 -or $hresult -notin @(-2147417846,-2147418111)) { throw }
        Start-Sleep -Seconds 2
    }
}
Write-Output 'NEXORIA_PHOTOSHOP_EXPORT=SUCCESS'
