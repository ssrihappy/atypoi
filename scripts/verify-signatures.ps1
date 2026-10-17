param([string]$ArtifactDirectory = '', [switch]$RequireTrustedSignatures)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $ArtifactDirectory) { $ArtifactDirectory = Join-Path $projectRoot 'dist' }
$source = [IO.File]::ReadAllText((Join-Path $projectRoot 'src\win\platform.h'))
$version = [regex]::Match($source,'version\[\]=L"([^"]+)"').Groups[1].Value
$results = @(foreach ($name in @('Atypoi.exe','Atypoi.IME.dll','Atypoi.Setup.exe')) {
    $path = Join-Path $ArtifactDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing signing artifact: $name" }
    $metadata = [Diagnostics.FileVersionInfo]::GetVersionInfo($path)
    if ($metadata.ProductName -cne 'Atypoi' -or $metadata.ProductVersion -cne $version) { throw "Product metadata does not match source: $name" }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    if ($signature.Status -ne 'NotSigned' -and $signature.Status -ne 'Valid') { throw "Invalid or untrusted signature: $name ($($signature.Status))" }
    if ($signature.Status -eq 'Valid' -and -not $signature.TimeStamperCertificate) { throw "Trusted timestamp is missing: $name" }
    [pscustomobject]@{Name=$name;Signed=($signature.Status -eq 'Valid');Certificate=$signature.SignerCertificate}
})
$signed = @($results | Where-Object Signed)
if ($signed.Count -gt 0 -and $signed.Count -ne $results.Count) { throw 'All three product binaries must be signed together.' }
if ($signed.Count -gt 0 -and @($signed.Certificate.Thumbprint | Select-Object -Unique).Count -ne 1) { throw 'Product binaries have different publisher certificates.' }
if ($RequireTrustedSignatures -and $signed.Count -ne $results.Count) { throw 'Signed release required; unsigned artifacts cannot be published.' }
[pscustomobject]@{
    Signed=($signed.Count -eq $results.Count)
    Publisher=$(if($signed.Count){$signed[0].Certificate.Subject}else{''})
    CertificateThumbprint=$(if($signed.Count){$signed[0].Certificate.Thumbprint}else{''})
}
