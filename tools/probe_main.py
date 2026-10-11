"""Read-only MAIN probe using pinned official Python API; does not launch DISPLAY."""
import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

import pyfwfinder

parser = argparse.ArgumentParser()
parser.add_argument("--onewili", required=True)
parser.add_argument("--serial", required=True)
args = parser.parse_args()
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "build/python-deps"))
sys.path.insert(0, str(Path(args.onewili) / "python"))
from onewili.device import OneWili
from onewili.transport import Transport

devices = pyfwfinder.find_all()
assert len(devices) == 1 and devices[0].device_type == pyfwfinder.DeviceType.FreeWili2
device = devices[0]
assert str(device.serial) == args.serial
main = [u for u in device.usb_devices if u.kind == pyfwfinder.USBDeviceType.SerialMain]
assert len(main) == 1
report = {"utc": datetime.now(timezone.utc).isoformat(), "device": str(device),
          "main_usb_identity": str(main[0].name), "port": str(main[0].port),
          "transport": "PC USB MAIN serial (not DISPLAY FwGUI)", "commands": []}

class RecordingTransport(Transport):
    def send(self, command):
        assert command == "h\\a\\b 0 16", "Only read-only OTP query permitted"
        report["commands"].append({"request": command})
        super().send(command)

    def wait_frame(self, timeout=5.0):
        frame = super().wait_frame(timeout)
        report["commands"][-1]["response"] = asdict(frame) if frame else None
        return frame

transport = RecordingTransport(str(main[0].port))
try:
    with OneWili(transport=transport) as dev:
        for _ in range(3):
            begin = time.monotonic()
            result = dev.hardware.system.read_otp_info(0, 16)
            report["commands"][-1]["elapsed_ms"] = round((time.monotonic() - begin) * 1000, 2)
            report["commands"][-1]["result"] = str(result)
            print(json.dumps(report["commands"][-1]), flush=True)
    report["status"] = "PASS" if all(c["response"] and c["response"]["success"]
        and c["result"].startswith("Ok(") for c in report["commands"]) else "FAILED"
finally:
    path = Path(__file__).resolve().parents[1] / "docs/M2A-MAIN-PROBE.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print("Recorded read-only MAIN responses:", report.get("status"))
