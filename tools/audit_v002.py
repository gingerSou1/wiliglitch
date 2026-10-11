"""Offline v002 UF2/ELF audit through official validators; no device access."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--bsp", required=True)
parser.add_argument("--elf", required=True)
parser.add_argument("--uf2", required=True)
args = parser.parse_args()
sys.path.insert(0, str(Path(args.bsp) / "tools"))
from fw import check_app_uf2
from check_app_uf2 import load_image
from check_uf2_info import check

target = check_app_uf2(args.uf2)
name, version, description = check(load_image(args.uf2))
assert target == "SRAM" and name == "WiliGlitch" and version == 2
data = Path(args.uf2).read_bytes()
memory = {}
for offset in range(0, len(data), 512):
    header = struct.unpack_from("<8I", data, offset)
    flags, address, length, family = header[2], header[3], header[4], header[7]
    assert flags & 0x2000 and family == 0xE48BFF59
    if flags & 1 or not length:
        continue
    for i, byte in enumerate(data[offset + 32:offset + 32 + length]):
        assert address + i not in memory, "Overlapping payload"
        memory[address + i] = byte
elf = Path(args.elf).read_bytes()
assert elf[:6] == b"\x7fELF\x01\x01"
phoff = struct.unpack_from("<I", elf, 28)[0]
phsize, phcount = struct.unpack_from("<HH", elf, 42)
segments = []
for i in range(phcount):
    kind, offset, vaddr, paddr, filesz, memsz, flags, align = struct.unpack_from("<8I", elf, phoff + i * phsize)
    if kind != 1:
        continue
    assert 0x20000000 <= vaddr <= vaddr + memsz <= 0x20082000
    if filesz:
        assert all(memory.get(paddr + j) == byte for j, byte in enumerate(elf[offset:offset + filesz])), "UF2 differs from ELF"
    segments.append({"start": hex(vaddr), "file_bytes": filesz, "ram_bytes": memsz})
symbols = subprocess.check_output(["arm-none-eabi-nm.exe", args.elf], text=True)
names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
assert {"ow_open_fwgui", "ow_raw_send", "ow_raw_next_response", "fw2_app_recovery_task", "watchdog_reboot"} <= names
assert not {"ow_io_analog_out_set_v_prog_vout", "ow_io_analog_out_set_glitch"} & names
assert any(s["start"] == "0x20081000" and s["ram_bytes"] == 4096 for s in segments), "Expected 4K app stack"
root = Path(__file__).resolve().parents[1]
for path in [root / "dist/WiliGlitch-v001.uf2", root / "dist/rollback/v001/WiliGlitch-v001.uf2"]:
    assert hashlib.sha256(path.read_bytes()).hexdigest() == "81a4039c1f310f06d180c768483fb7473d7d82c4fbc21daef52d7ab97cee71ad"
report = {"status": "PASS", "name": name, "version": version, "description": description,
          "target": target, "uf2_bytes": len(data), "payload_bytes": len(memory),
          "payload_start": hex(min(memory)), "payload_end_exclusive": hex(max(memory) + 1),
          "sha256": hashlib.sha256(data).hexdigest(), "elf_load_segments": segments,
          "live_output_command_symbols": "ABSENT", "v001_and_rollback": "UNCHANGED"}
(root / "docs/M2A-ARTIFACT-AUDIT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
