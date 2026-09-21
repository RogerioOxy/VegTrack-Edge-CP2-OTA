$ErrorActionPreference = 'Stop'
$cli = 'C:/Users/roger/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
Push-Location $PSScriptRoot
try {
    foreach ($number in 1, 2) {
        # A cópia de raiz é a fonte completa usada também no PDF.
        Copy-Item -LiteralPath "firmware_v$number.ino" -Destination "fw$number/fw$number.ino"
        & $cli compile --fqbn esp32:esp32:esp32 --build-property 'build.partitions=default' --output-dir "build$number" "fw$number"
        if ($LASTEXITCODE -ne 0) { throw "Compilação FW$number falhou." }
        Copy-Item -LiteralPath "build$number/fw$number.ino.bin" -Destination "firmware_v$number.bin"
        if ((Get-Item "firmware_v$number.bin").Length -gt 0x140000) {
            throw "Firmware excede o slot OTA."
        }
        Get-Item "firmware_v$number.bin" | Select-Object Name, Length
        Get-FileHash "firmware_v$number.bin" -Algorithm SHA256
    }
    python ./verify_artifacts.py
    if ($LASTEXITCODE -ne 0) { throw "Verificação de artefatos falhou." }
} finally { Pop-Location }
