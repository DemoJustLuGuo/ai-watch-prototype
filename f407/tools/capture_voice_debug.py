"""Read printf's RAM mirror over the selected F407 probe without halting/resetting.

The AXF must match the flashed firmware. No serial or C3 access is performed.
"""
import argparse
import codecs
import time
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from pyocd.core.helpers import ConnectHelper

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--uid", default="0602A0002016")
parser.add_argument("--elf", type=Path, default=ROOT / "build/Target 1/Template.axf")
parser.add_argument("--duration", type=float, default=50)
parser.add_argument("--output", type=Path, default=ROOT / "build/voice_swd.log")
args = parser.parse_args()
with args.elf.open("rb") as file:
    elf = ELFFile(file)
    symbols = {s.name: s for s in elf.get_section_by_name(".symtab").iter_symbols()}
    buf = symbols["USART1_DIAG_BUF"]["st_value"]
    capacity = symbols["USART1_DIAG_BUF"]["st_size"]
    wr = symbols["USART1_DIAG_WR"]["st_value"]

session = ConnectHelper.session_with_chosen_probe(
    unique_id=args.uid, target_override="stm32f407zgtx", frequency=1000000,
    connect_mode="attach", auto_unlock=False, resume_on_disconnect=False,
    options={"cmsis_dap.limit_packets": True, "cmsis_dap.deferred_transfers": False},
)
if session is None:
    raise SystemExit("Selected probe unavailable")
args.output.parent.mkdir(parents=True, exist_ok=True)
with session, args.output.open("wb") as log:
    target = session.target
    count = target.read32(wr)
    if count == 0xFFFFFFFF:
        raise SystemExit("Application log is uninitialized; check the Flash boot entry first")
    cursor = max(0, count - capacity)
    deadline = time.monotonic() + args.duration
    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
    while time.monotonic() < deadline:
        count = target.read32(wr)
        if count < cursor:  # target reset
            cursor = max(0, count - capacity)
        if count - cursor > capacity:
            cursor = count - capacity
            print("\n[reader] older log bytes overwritten", flush=True)
        if count > cursor:
            # Snapshot the ring, then reject a chunk overwritten while reading.
            data = bytes(target.read_memory_block8(buf, capacity))
            after = target.read32(wr)
            if after < count or after - cursor > capacity:
                continue
            payload = bytes(data[i % capacity] for i in range(cursor, count))
            cursor = count
            log.write(payload)
            log.flush()
            print(decoder.decode(payload), end="", flush=True)
        time.sleep(0.1)
print(f"\nSaved: {args.output}", flush=True)
