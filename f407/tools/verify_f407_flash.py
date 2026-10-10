"""Compare every byte represented by Template.hex with F407 Flash over SWD."""
import hashlib
import json
from pathlib import Path

from intelhex import IntelHex
from pyocd.core.helpers import ConnectHelper

root = Path(__file__).resolve().parents[1]
hex_path = root / "build/Target 1/Template.hex"
firmware = IntelHex(str(hex_path))
session = ConnectHelper.session_with_chosen_probe(
    unique_id="0602A0002016", target_override="stm32f407zgtx",
    frequency=1000000, connect_mode="attach", auto_unlock=False,
    resume_on_disconnect=False,
    options={"cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False},
)
if session is None:
    raise SystemExit("Selected probe unavailable")
count = 0
digest = hashlib.sha256()
with session:
    for start, end in firmware.segments():
        assert 0x08000000 <= start < end <= 0x08100000, "Unexpected HEX destination"
        for address in range(start, end, 4096):
            size = min(4096, end - address)
            expected = bytes(firmware.tobinarray(start=address, size=size))
            actual = bytes(session.target.read_memory_block8(address, size))
            if actual != expected:
                mismatch = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
                raise SystemExit(f"Flash mismatch at 0x{address+mismatch:08X}")
            digest.update(actual)
            count += size
result = {
    "verified_bytes": count, "flash_data_sha256": digest.hexdigest(),
    "hex_file_sha256": hashlib.sha256(hex_path.read_bytes()).hexdigest(),
    "probe": "0602A0002016", "target": "stm32f407zgtx",
}
(root / "build/voice_flash_verify.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(f"PASS: all {count} HEX bytes match F407 Flash", flush=True)
