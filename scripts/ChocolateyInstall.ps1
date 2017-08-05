$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"
Copy-Item "$toolsDir\fwup" "$($env:ChocolateyInstall)\bin\fwup" 