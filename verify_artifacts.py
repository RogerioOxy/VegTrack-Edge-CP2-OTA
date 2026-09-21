"""Verifica cópias, tamanho dos slots e registra hashes dos arquivos locais."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parent
for number in (1, 2):
    assert (root / f'firmware_v{number}.ino').read_bytes() == (root / f'fw{number}/fw{number}.ino').read_bytes()
    binary = (root / f'firmware_v{number}.bin').read_bytes()
    assert binary == (root / f'build{number}/fw{number}.ino.bin').read_bytes()
    assert len(binary) <= 0x140000
assert (root / 'build1/fw1.ino.partitions.bin').read_bytes() == (root / 'build2/fw2.ino.partitions.bin').read_bytes()
files = ['firmware_v1.ino', 'firmware_v2.ino', 'firmware_v1.bin', 'firmware_v2.bin',
         'version.json', 'partitions.csv', 'build1/fw1.ino.merged.bin',
         'build1/fw1.ino.partitions.bin', 'build2/fw2.ino.partitions.bin']
artifacts = []
for name in files:
    data = (root / name).read_bytes()
    artifacts.append({'file': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
metadata = {'core': 'esp32:esp32@3.3.11', 'fqbn': 'esp32:esp32:esp32',
            'partition_scheme': 'default', 'ota_slot_bytes': 0x140000,
            'artifacts': artifacts,
            'runtime_note': 'Local build metadata. Text hashes include local line endings. Wokwi online used core 3.3.7; runtime acceptance is separate.'}
(root / 'ARTIFACTS.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('PASS: standalone/sketch copies, binary copies, two matching partition tables and both OTA slot sizes.')
for item in artifacts:
    print(item['file'], item['bytes'], item['sha256'])
