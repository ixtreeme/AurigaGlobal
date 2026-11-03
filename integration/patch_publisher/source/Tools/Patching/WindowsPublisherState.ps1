$ErrorActionPreference = 'Stop'
if (-not ('AurigaPublisherAtomicState' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AurigaPublisherAtomicState {
    [DllImport("kernel32.dll", EntryPoint="MoveFileExW", CharSet=CharSet.Unicode, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool Move(string source,string destination,uint flags);
}
'@
}
function Save-AurigaPublisherState([string]$Path,[string]$Json) {
    $Path=[IO.Path]::GetFullPath($Path)
    $temporary=$Path+'.tmp'
    [IO.File]::WriteAllText($temporary,$Json,(New-Object Text.UTF8Encoding $false))
    if([IO.File]::Exists($Path)){[IO.File]::Copy($Path,$Path+'.bak',$true)}
    if(-not [AurigaPublisherAtomicState]::Move($temporary,$Path,9)) {
        throw (New-Object ComponentModel.Win32Exception ([Runtime.InteropServices.Marshal]::GetLastWin32Error()))
    }
}
