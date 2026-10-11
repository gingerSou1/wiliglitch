"""Install frozen v001 through the official BSP SD workflow with identity/hash guards.

Requires explicit physical-deployment authorization. Does not launch an app.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

import pyfwfinder

ROOT = Path(__file__).resolve().parents[1]
HASH = "81a4039c1f310f06d180c768483fb7473d7d82c4fbc21daef52d7ab97cee71ad"
parser = argparse.ArgumentParser()
parser.add_argument("--bsp", required=True)
parser.add_argument("--serial", required=True)
parser.add_argument("--port", required=True)
parser.add_argument("--verify-only", action="store_true")
args = parser.parse_args()
source = ROOT / "dist/WiliGlitch-v001.uf2"
assert hashlib.sha256(source.read_bytes()).hexdigest() == HASH, "Source changed"

def device():
    devices = pyfwfinder.find_all()
    assert len(devices) == 1, "Expected exactly one connected FreeWili"
    dev = devices[0]
    assert str(dev.serial) == args.serial and dev.device_type == pyfwfinder.DeviceType.FreeWili2
    main = [u for u in dev.usb_devices if u.kind == pyfwfinder.USBDeviceType.SerialMain]
    assert len(main) == 1 and str(main[0].port) == args.port, "MAIN identity changed"
    return dev

dev = device()
spec = importlib.util.spec_from_file_location("official_fw", Path(args.bsp) / "tools/fw.py")
fw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fw)
official_wait = fw._wait_for_sd
official_replace = fw.os.replace
report = {"device": str(dev), "serial": args.serial, "main_port": args.port,
          "source_sha256": HASH, "sd_destination": "/apps/WiliGlitch-v001.uf2",
          "launched": False, "status": "STARTED"}
before = None
destination = None

def snapshot(volume):
    return {p.relative_to(volume).as_posix(): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in volume.rglob("*") if p.is_file()}

def guarded_wait(baseline, timeout=25, poll=0.25):
    global before, destination
    volume = official_wait(baseline, timeout, poll)
    readers = [u for u in device().usb_devices if u.kind == pyfwfinder.USBDeviceType.MassStorage]
    paths = [Path(p).resolve() for u in readers for p in (u.paths or [])]
    assert volume.resolve() in paths, "Mounted volume is not the identified FREE-WILi SD reader"
    destination = volume / "apps" / source.name
    if not args.verify_only:
        assert not destination.exists(), "Existing app destination will not be overwritten"
    assert not Path(str(destination) + ".tmp").exists(), "Existing temporary file will not be overwritten"
    before = snapshot(volume)
    report["pc_volume"] = str(volume)
    report["existing_files"] = len(before)
    print("Confirmed device SD volume:", volume, "existing files:", len(before), flush=True)
    return volume

def verified_replace(src, dst):
    assert destination is not None and Path(dst) == destination
    assert not destination.exists(), "Destination appeared during copy; refusing overwrite"
    assert hashlib.sha256(Path(src).read_bytes()).hexdigest() == HASH, "Temporary copy mismatch"
    official_replace(src, dst)
    deployed_hash = hashlib.sha256(destination.read_bytes()).hexdigest()
    assert deployed_hash == HASH, "Deployed checksum mismatch"
    after = snapshot(destination.parent.parent)
    assert all(after.get(name) == info for name, info in before.items()), "Existing SD file metadata changed"
    assert set(after) - set(before) == {"apps/" + source.name}, "Unexpected new SD files"
    report["deployed_sha256"] = deployed_hash
    report["existing_file_metadata_preserved"] = True
    print("Deployed file checksum MATCH:", deployed_hash, flush=True)

fw._wait_for_sd = guarded_wait
fw.os.replace = verified_replace
try:
    if args.verify_only:
        baseline = fw._mounted_volumes()
        selected = False
        try:
            fw._set_sd_host(args.port, True)
            selected = True
            fw.time.sleep(fw.SD_HANDOFF_SETTLE_SECONDS)
            volume = guarded_wait(baseline)
            report["deployed_sha256"] = hashlib.sha256(destination.read_bytes()).hexdigest()
            assert report["deployed_sha256"] == HASH, "Installed file differs from v001"
            report["deployed_file_bytes"] = destination.stat().st_size
            report["verification_only"] = True
            report["sd_files_present"] = len(before)
            print("Read-back checksum MATCH:", report["deployed_sha256"], flush=True)
            fw.time.sleep(fw.SD_HANDOFF_SETTLE_SECONDS)
        finally:
            if selected:
                fw._set_sd_host(args.port, False)
    else:
        fw.install_app(str(source), serial_number=args.serial, port=args.port)
    assert report.get("deployed_sha256") == HASH
    assert hashlib.sha256(source.read_bytes()).hexdigest() == HASH
    report["status"] = "INSTALLED_AND_VERIFIED"
    report["sd_returned_to_main"] = True
except Exception as exc:
    report["status"] = "ERROR"
    report["error"] = str(exc)
    raise
finally:
    fw.os.replace = official_replace
    (ROOT / "docs/V001-DEPLOYMENT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)
