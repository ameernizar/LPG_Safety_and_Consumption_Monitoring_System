#!/usr/bin/env python3
"""Create RAM payloads only; never flash, trigger, or obtain a timestamp."""
import argparse
import datetime
import pathlib
import struct
import subprocess
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('timestamp', help='explicit local IST YYYY-MM-DD HH:MM')
p.add_argument('--elf', default='Debug/STM32_HW_Test.elf')
p.add_argument('--out', default='/tmp/rtc-request')
a = p.parse_args()
try:
    t = datetime.datetime.strptime(a.timestamp, '%Y-%m-%d %H:%M')
except ValueError as e:
    p.error(str(e))
if not 2000 <= t.year <= 2199:
    p.error('supported year range: 2000..2199')
nm = shutil.which('arm-none-eabi-nm') or shutil.which('nm')
if not nm:
    p.error('nm is required to find the mailbox in this exact ELF')
lines = subprocess.check_output([nm, a.elf], text=True).splitlines()
address = next((int(line.split()[0], 16) for line in lines
                if len(line.split()) == 3 and line.split()[2] == 'rtc_set_request'), None)
if address is None:
    p.error('ELF does not contain rtc_set_request')
out = pathlib.Path(a.out)
out.mkdir(parents=True, exist_ok=True)
# Target first, command separately LAST: avoid MCU observing a partial RAM upload.
(out/'target.bin').write_bytes(struct.pack('<5I', t.year, t.month, t.day, t.hour, t.minute))
(out/'arm.bin').write_bytes(struct.pack('<I', 0x41524D31))
(out/'trigger.bin').write_bytes(struct.pack('<I', 0x53455431))
print(f'Target: {t:%d-%m-%Y %H:%M}:00 IST (UTC+05:30)')
print(f'Write {out}/target.bin to RAM 0x{address:08X}')
print(f'Then write {out}/arm.bin to RAM 0x{address+20:08X}; observe SET MODE: ARMED')
print(f'At target minute write {out}/trigger.bin to RAM 0x{address+20:08X}')
print('No reset between writes. ARM expires after 60 seconds. No hardware action performed.')
