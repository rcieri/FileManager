function fm {
    FileManager
    $raw = Get-Content -Path "$env:APPDATA\FileManager\history.txt"  -Tail 1
    $clean = $raw -replace '^"|"$', '' -replace '\\', '\'

    if (Test-Path $clean) {
        Set-Location $clean
        Write-Host "Changed directory to: $clean"
    } else {
        Write-Host "Path not found: $clean"
    }
}
