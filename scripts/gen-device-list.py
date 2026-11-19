#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Generate SUPPORTED_DEVICES.md from the USB device table in src/core.c.
import collections, re, sys

CHIPS = {
    "rtl8192cu_fops": "RTL8188CU / RTL8192CU (and RTL8188RU, RTL8191CU)",
    "rtl8723au_fops": "RTL8723AU",
    "rtl8188eu_fops": "RTL8188EU / RTL8188EUS / RTL8188ETV",
    "rtl8192eu_fops": "RTL8192EU",
    "rtl8723bu_fops": "RTL8723BU",
    "rtl8188fu_fops": "RTL8188FU / RTL8188FTV",
    "rtl8710bu_fops": "RTL8710BU (RTL8188GU)",
    "rtl8192fu_fops": "RTL8192FU",
}

src = open(sys.argv[1] if len(sys.argv) > 1 else "src/core.c").read()
start = src.index("static const struct usb_device_id dev_table[]")
table = src[start:src.index("};", start)]
ids = collections.OrderedDict((k, []) for k in CHIPS)
cur = None
for line in table.split("\n"):
    m = re.search(r"USB_DEVICE_AND_INTERFACE_INFO\((0x[0-9a-fA-F]+|USB_VENDOR_ID_REALTEK),\s*(0x[0-9a-fA-F]+)", line)
    if m:
        vid = 0x0BDA if m.group(1) == "USB_VENDOR_ID_REALTEK" else int(m.group(1), 16)
        cur = f"{vid:04x}:{int(m.group(2), 16):04x}"
    n = re.search(r"driver_info = \(unsigned long\)&(\w+)", line)
    if n and cur:
        ids.setdefault(n.group(1), []).append(cur)
        cur = None

out = ["# Supported USB devices", "",
       "Generated from the USB device table in `src/core.c` by",
       "`scripts/gen-device-list.py`. Find your adapter's ID with `lsusb`",
       "(the `ID xxxx:xxxx` part) and search this page for it.", ""]
total = 0
for fops, lst in ids.items():
    if not lst:
        continue
    total += len(lst)
    out += [f"## {CHIPS.get(fops, fops)}", "", f"{len(lst)} IDs:", ""]
    out += ["`" + "` `".join(sorted(set(lst))) + "`", ""]
out.insert(5, f"{total} USB IDs in total.")
out.insert(6, "")
print("\n".join(out))
