"""Check the built AXF and the hand-written scatter configuration before flash."""
import hashlib
import json
import re
import struct
from pathlib import Path

from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/Target 1"
mic_header = (ROOT / "APP/mic/mic.h").read_text(encoding="utf-8")
rate = int(re.search(r"#define\s+MIC_RATE\s+(\d+)u", mic_header)[1])
max_ms = int(re.search(r"#define\s+MIC_MAX_MS\s+(\d+)u", mic_header)[1])
expected_pcm_bytes = rate * 2 * max_ms // 1000
assert 0 < expected_pcm_bytes <= 0x100000
result = {"sample_rate": rate, "max_record_ms": max_ms, "pcm_bytes": expected_pcm_bytes}
with (BUILD / "Template.axf").open("rb") as file:
    elf = ELFFile(file)
    symbols = {s.name: s for s in elf.get_section_by_name(".symtab").iter_symbols()}
    begin = symbols["Region$$Table$$Base"]["st_value"]
    end = symbols["Region$$Table$$Limit"]["st_value"]
    for segment in elf.iter_segments():
        base = segment["p_vaddr"]
        if base <= begin and end <= base + segment["p_filesz"]:
            entries = list(struct.iter_unpack("<IIII", segment.data()[begin-base:end-base]))
            assert len(entries) == 2, "Expected two startup initialization entries"
            for source, dest, size, function in entries:
                assert 0x20000000 <= dest and dest + size <= 0x20020000
            result["startup_entries"] = [
                {"source": hex(a), "destination": hex(b), "size": c, "function": hex(d)}
                for a, b, c, d in entries
            ]
            break
    else:
        raise AssertionError("Region table missing")
    for name in ("hadc", "htim", "hdma", "raw", "filled", "pcm", "USART1_DIAG_BUF", "USART1_DIAG_WR"):
        address = symbols[name]["st_value"]
        if name == "pcm":
            assert address == 0x68000000 and symbols[name]["st_size"] == expected_pcm_bytes
        else:
            assert 0x20000000 <= address < 0x20020000
        result[name] = hex(address)

map_text = (BUILD / "Template.map").read_text(errors="replace")
region = re.search(r"Execution Region RW_IRAM1 \(Base: 0x20000000, Size: (0x[0-9a-fA-F]+)", map_text)
assert region and 0 < int(region[1], 16) <= 0x20000
external_region = re.search(r"Execution Region RW_RAM1 .*Size: (0x[0-9a-fA-F]+).*UNINIT", map_text)
assert external_region and int(external_region[1], 16) == expected_pcm_bytes
for filename in ("Template.sct", "fixed.sct"):
    scatter = (BUILD / filename).read_text(encoding="utf-8")
    external = scatter.split("LR_ERAM", 1)[1]
    assert "UNINIT" in external and "mic.o (.bss)" in external and ".ANY" not in external

result["RW_IRAM1"] = region[1]
result["hex_sha256"] = hashlib.sha256((BUILD / "Template.hex").read_bytes()).hexdigest()
(ROOT / "build/voice_validation.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps(result, indent=2))
