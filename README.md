# rtl8xxxu-dkms

The mainline Linux `rtl8xxxu` Wi-Fi driver with working monitor-mode
packet injection and real TX status, packaged for DKMS so you can use it
today, before the changes reach your kernel.

The code is the upstream driver plus a small set of patches that are being
submitted to the Linux kernel (`linux-wireless`, `rtw-next` tree). There is
no vendor code here. Everything is GPL-2.0, like the kernel.

## Supported adapters

`rtl8xxxu` is one driver for many Realtek USB Wi-Fi chips (the "xxx" is a
wildcard), so this package works with adapters based on:

**RTL8188CU, RTL8192CU, RTL8188RU, RTL8191CU, RTL8723AU, RTL8188EU,
RTL8188EUS, RTL8192EU, RTL8723BU, RTL8188FU, RTL8710BU (RTL8188GU),
RTL8192FU**

To check yours, run `lsusb` and look for the `ID xxxx:xxxx` of your
adapter in [SUPPORTED_DEVICES.md](SUPPORTED_DEVICES.md) (122 USB IDs,
generated from the driver). For example, an RTL8192CU often shows up as
`ID 0bda:8178 Realtek Semiconductor Corp. RTL8192CU 802.11n WLAN Adapter`.

Some of these chips also have other drivers (the vendor driver, or
`rtl8192cu` in the kernel). This package replaces the kernel's `rtl8xxxu`
module; see [Install](#install) for making sure it is the one that binds
to your adapter. Not every chip has every fix yet, see
[Hardware status](#hardware-status).

## What it fixes

| Problem in the stock driver | With rtl8xxxu-dkms |
|---|---|
| The rate in the radiotap header of injected frames is ignored. The firmware picks the rate instead (on an RTL8192CU every injected frame went out at MCS 7). | Injected frames go out at exactly the requested legacy rate or HT MCS, with the requested guard interval and no rate fallback. |
| The radiotap retry count of injected frames is ignored, and `NOACK` frames are still retried up to 48 times. Unicast frames to an absent station stalled injection for more than 100 seconds in testing. | The retry count is honoured (0–63), and `NOACK` frames are sent once. |
| Injected frames are always sent at 20 MHz, even on a 40 MHz channel. | Frames that ask for 40 MHz are sent at 40 MHz (HT40+ and HT40-). |
| Every frame is reported as ACKed once its USB transfer finishes. | `NOACK` frames are reported as such. On RTL8188CU/RTL8192CU, the real ACK status and retry count come from the firmware's TX report. |

## Hardware status

| Chip | Injected rate | Retry count | 40 MHz | Real TX status |
|---|---|---|---|---|
| RTL8188CU, RTL8192CU | ✅ tested on RTL8192CU | ✅ tested on RTL8192CU | ✅ tested on RTL8192CU | ✅ tested on RTL8192CU |
| RTL8723AU | should work, untested | should work, untested | should work, untested | no |
| RTL8192EU, RTL8723BU, RTL8188FU, RTL8710BU, RTL8192FU | compile tested only | compile tested only | no | no |
| RTL8188EU | compile tested only | compile tested only | compile tested only | no |

If you own one of the untested chips, test reports are very welcome. See
[Testing](#testing).

## Install

Requires kernel headers and DKMS. Tested on Linux 7.3. Older kernels may
not build, because the sources follow the current kernel API.

```sh
git clone https://github.com/niko3x/rtl8xxxu-dkms.git
sudo dkms install ./rtl8xxxu-dkms
```

To build and load without DKMS:

```sh
make
sudo rmmod rtl8xxxu; sudo insmod src/rtl8xxxu.ko
```

The RTL8188CU/RTL8192CU are also claimed by the separate `rtl8192cu`
driver. If that one binds to your device (`readlink
/sys/class/net/<iface>/device/driver`), stop it from loading:

```sh
echo "blacklist rtl8192cu" | sudo tee /etc/modprobe.d/rtl8xxxu-dkms.conf
```

To uninstall: `sudo dkms remove rtl8xxxu-dkms/0.1.0 --all` (and delete
that modprobe file if you created it).

## Usage

Set your country first, so the right channels and power limits apply:

```sh
sudo iw reg set XX            # your ISO country code
sudo ip link set wlan1 down
sudo iw dev wlan1 set type monitor
sudo ip link set wlan1 up
sudo iw dev wlan1 set channel 6
```

Inject with any tool that writes radiotap frames to the monitor interface
(for example wfb-ng, aircrack-ng, scapy). These radiotap fields are
honoured:

- `RATE` (legacy rates) and `MCS` (HT MCS 0–15, short GI, 40 MHz)
- `DATA_RETRIES`
- `TX_FLAGS` `NOACK`

For 40 MHz, set the monitor interface to a 40 MHz channel first, e.g.
`iw dev wlan1 set channel 6 HT40+`.

## TX status

On RTL8188CU/RTL8192CU, the firmware can report whether a frame was ACKed
and how many retries it took. The report does not say which frame it
belongs to, so only one frame at a time gets one. A frame that asks for
status while another is still waiting is reported the old way (ACKed if
the USB transfer succeeded). Broadcast/multicast and `NOACK` frames never
use a report.

You can see the reported status with a capture on the monitor interface:
mac80211 hands every transmitted frame back with its status in the
radiotap header (`TX_FLAGS` fail bit, `DATA_RETRIES`).

## Testing

`tools/` has the two small programs used to test these patches:

- `inject IFACE COUNT [[w][s]MCS|L<rate in 500 kbps units>|-1] [DST BSSID]`
  sends test frames (`w` = 40 MHz, `s` = short GI, e.g. `ws7`). `NOACK=0`, `RETRIES=n`, `DELAY_US=n` and `NOSLEEP=1`
  in the environment change how.
- `capture IFACE SECONDS` counts the test frames seen on a monitor
  interface and prints their rate, MCS, GI and retry flag, plus the TX
  status echoes when run on the transmitting interface.

Build them with `make tools`. A second radio in monitor mode on the same
channel is needed to check what actually goes over the air.

## Upstream

The patches in this package have been submitted to `linux-wireless` for
inclusion in the kernel (`rtw-next` tree):

| Series | Status |
|---|---|
| Injected rate, retry count and 40 MHz (RTL8188CU/RTL8192CU/RTL8723AU) | under review |
| Real TX status (RTL8188CU/RTL8192CU) | under review |
| The same injection fixes for gen2 chips and RTL8188EU | not submitted yet (needs testers) |

When they are merged, the stock kernel driver will behave the same and
this package will no longer be needed. `src/BASE` records which upstream
commit the sources are based on. `scripts/sync-from-kernel.sh` refreshes
`src/` from a kernel tree.

## Development note

The patches were developed with help from an AI coding assistant (Claude
Code), and reviewed and hardware-tested by the maintainer of this
repository.

## License

GPL-2.0-only. See [LICENSE](LICENSE).
