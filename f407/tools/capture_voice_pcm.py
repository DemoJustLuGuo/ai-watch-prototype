"""Export the last completed F407 recording through SWD without resetting it.

The map must match the flashed firmware; use --map for an archived build.
"""
import argparse
import json
import math
import re
import struct
import wave
from pathlib import Path

from pyocd.core.helpers import ConnectHelper

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--map", type=Path, default=root / "build/Target 1/Template.map")
args = parser.parse_args()
map_text = args.map.read_text(errors="replace")
mic_symbols = {
    name: (int(address, 16), int(size))
    for name, address, size in re.findall(
        r"^\s*(\w+)\s+(0x[0-9a-fA-F]+)\s+Data\s+(\d+)\s+mic\.o", map_text, re.M
    )
}

def mic_symbol(name):
    if name not in mic_symbols:
        raise ValueError(f"mic.o symbol missing: {name}")
    return mic_symbols[name][0]

pcm_capacity = mic_symbols["pcm"][1]
assert 0 < pcm_capacity <= 0x100000 and pcm_capacity % 2 == 0

session = ConnectHelper.session_with_chosen_probe(
    unique_id="0602A0002016", target_override="stm32f407zgtx",
    frequency=1000000, connect_mode="attach", auto_unlock=False,
    resume_on_disconnect=False,
    options={"cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False},
)
if session is None:
    raise SystemExit("Selected probe unavailable")
with session:
    target = session.target
    assert target.read8(mic_symbol("running")) == 0, "Recording still running"
    count = target.read32(mic_symbol("filled"))
    error = target.read32(mic_symbol("error"))
    assert error == 0 and 0 < count * 2 <= pcm_capacity, f"Invalid recording: samples={count}, error={error}"
    data = bytes(target.read_memory_block8(mic_symbol("pcm"), count * 2))
    raw = bytes(target.read_memory_block8(mic_symbol("raw"), 4096))
    assert target.read8(mic_symbol("running")) == 0
    assert target.read32(mic_symbol("filled")) == count
    assert target.read32(mic_symbol("error")) == 0
values = struct.unpack("<" + "h" * count, data)
adc = struct.unpack("<2048H", raw)
mean = sum(values) / count
result = {
    "samples": count, "bytes": len(data), "duration_ms": count / 16,
    "pcm_min": min(values), "pcm_max": max(values), "pcm_mean": mean,
    "pcm_ac_rms": math.sqrt(sum((v-mean)**2 for v in values) / count),
    "invalid_low_bits": sum(bool(v & 15) for v in values),
    "raw_last_ring_min": min(adc), "raw_last_ring_max": max(adc),
    "raw_last_ring_mean": sum(adc) / len(adc),
}
assert result["invalid_low_bits"] == 0, "PCM does not match the 12-bit to 16-bit conversion"
out = root / "build/voice_last_record"
out.with_suffix(".pcm").write_bytes(data)
out.with_suffix(".raw").write_bytes(raw)
with wave.open(str(out.with_suffix(".wav")), "wb") as file:
    file.setnchannels(1)
    file.setsampwidth(2)
    file.setframerate(16000)
    file.writeframes(data)
out.with_suffix(".json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
print("Saved:", out.with_suffix(".wav"))
