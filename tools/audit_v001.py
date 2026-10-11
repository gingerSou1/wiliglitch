"""Offline audit of the frozen v001 UF2; never contacts a device."""
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "81a4039c1f310f06d180c768483fb7473d7d82c4fbc21daef52d7ab97cee71ad"
path = ROOT / "dist/WiliGlitch-v001.uf2"
data = path.read_bytes()
assert hashlib.sha256(data).hexdigest() == EXPECTED, "Frozen artifact changed"
assert data and len(data) % 512 == 0
blocks = []
numbers = set()
families = set()
for offset in range(0, len(data), 512):
    block = data[offset:offset + 512]
    m0, m1, flags, address, size, number, count, family = struct.unpack_from("<8I", block)
    assert (m0, m1, struct.unpack_from("<I", block, 508)[0]) == (0x0A324655, 0x9E5D5157, 0x0AB16F30)
    assert count == len(data) // 512 and number < count and number not in numbers
    numbers.add(number)
    assert not flags & 1 and 0 < size <= 476
    assert 0x20000000 <= address < address + size <= 0x20070000
    assert flags & 0x2000, "Missing UF2 family ID"
    families.add(family)
    blocks.append((address, block[32:32 + size]))
blocks.sort()
assert all(a + len(payload) <= b for (a, payload), (b, _) in zip(blocks, blocks[1:])), "Overlapping payload"
assert families == {0xE48BFF59}, "Expected RP2350 ARM secure family"
assert blocks[0][0] == 0x20000000
stack, reset = struct.unpack_from("<2I", blocks[0][1])
assert 0x20000000 < stack <= 0x20082000 and stack % 8 == 0
assert reset & 1 and blocks[0][0] <= (reset & ~1) < blocks[-1][0] + len(blocks[-1][1])
print(json.dumps({
    "artifact": str(path.relative_to(ROOT)), "sha256": EXPECTED,
    "file_bytes": len(data), "blocks": len(blocks),
    "payload_bytes": sum(len(p) for _, p in blocks),
    "payload_start": hex(blocks[0][0]),
    "payload_end_exclusive": hex(blocks[-1][0] + len(blocks[-1][1])),
    "supported_payload_window_bytes": 0x70000,
    "initial_stack_pointer": hex(stack), "reset_vector": hex(reset),
    "family_id": hex(next(iter(families))), "status": "PASS"
}, indent=2))
