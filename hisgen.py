#!/usr/bin/env python3
"""
updatehistory.txt:

    @2026-05-11
    some text

    @2026-05-11 00:30
    some text

usage:
    python3 hisgen.py updatehistory.txt
    python3 hisgen.py updatehistory.txt -o PARAM.HIS
    python3 hisgen.py updatehistory.txt --tz Etc/GMT+5
"""
import argparse
import datetime as dt
import re
import struct
import sys
from zoneinfo import ZoneInfo

MARKER = re.compile(r"^@(\d{4}-\d{1,2}-\d{1,2}(?: \d{1,2}:\d{2})?|\d+)\s*$")


def parse_timestamp(value, tz="Etc/GMT+5"):
    if value.isdigit():
        return int(value)
    for fmt in ("%Y-%m-%d %H:%M", "%Y-%m-%d"):
        try:
            naive = dt.datetime.strptime(value, fmt)
            return int(naive.replace(tzinfo=ZoneInfo(tz)).timestamp())
        except ValueError:
            pass
    raise ValueError(f"unrecognised timestamp: {value!r}")


def split_sections(text, tz="Etc/GMT+5"):
    sections = []
    current = None
    for lineno, line in enumerate(text.split("\n"), 1):
        m = MARKER.match(line)
        if m:
            current = (parse_timestamp(m.group(1), tz), [])
            sections.append(current)
        elif line.startswith("@"):
            raise ValueError(f"line {lineno}: bad timestamp {line.strip()!r} "
                             "use @YYYY-MM-DD, @YYYY-MM-DD HH:MM or epoch)")
        elif current is None:
            if line.strip():
                raise ValueError(f"line {lineno}: text before the first @timestamp line")
        else:
            current[1].append(line)
    if not sections:
        raise ValueError("no @timestamp lines found")
    result = []
    for ts, lines in sections:
        while lines and not lines[-1].strip():   # drop trailing blank lines
            lines.pop()
        result.append((ts, "\n".join(lines) + "\n"))
    return result


def main():
    p = argparse.ArgumentParser()
    p.add_argument("input", help="text file containing @timestamp sections")
    p.add_argument("-o", "--output", default="PARAM.HIS")
    p.add_argument("--tz", default="Etc/GMT+5",
                   help="timezone for timestamps (default: Etc/GMT+5)")
    args = p.parse_args()

    try:
        with open(args.input, "rb") as f:
            text = f.read().decode("utf-8-sig")   # also strips bom
        text = text.replace("\r\n", "\n").replace("\r", "\n")
        sections = split_sections(text, args.tz)

        data = b""
        for ts, body in sections:
            # 0x01 + timestamp (8 bytes, big-endian) + 0x02 + text
            data += b"\x01" + struct.pack(">Q", ts) + b"\x02" + body.encode("utf-8")
    except (ValueError, UnicodeDecodeError, OSError) as e:
        sys.exit(f"error: {e}")

    with open(args.output, "wb") as f:
        f.write(data)
    print(f"Written {args.output} ({len(sections)} entries found, {len(data)} bytes)")


if __name__ == "__main__":
    main()