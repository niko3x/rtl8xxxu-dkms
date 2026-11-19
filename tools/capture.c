// SPDX-License-Identifier: GPL-2.0-only
// Count captured test frames and report the rate/MCS seen in radiotap.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

static const int fsz[20]   = {8,1,1,4,2,1,1,2,2,2,1,1,1,1,2,2,1,1,8,3};
static const int falign[20] = {8,1,1,2,2,1,1,2,2,2,1,1,1,1,2,2,1,1,4,1};

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: %s IFACE SECONDS\n", argv[0]); return 1; }
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	struct sockaddr_ll sll = { .sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_ALL),
				   .sll_ifindex = if_nametoindex(argv[1]) };
	if (s < 0 || bind(s, (struct sockaddr *)&sll, sizeof(sll))) { perror("socket/bind"); return 1; }
	struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	int hist_legacy[128] = {0}, hist_mcs[32] = {0}, total = 0, sgi = 0, bw40 = 0, retry = 0;
	int txst = 0, txfail = 0, txretries[64] = {0};
	long rssi_sum = 0;
	time_t end = time(NULL) + atoi(argv[2]);
	unsigned char b[4096];
	while (time(NULL) < end) {
		struct sockaddr_ll from; socklen_t fl = sizeof(from);
		int n = recvfrom(s, b, sizeof(b), 0, (struct sockaddr *)&from, &fl);
		if (n > 0 && from.sll_pkttype == PACKET_OUTGOING) continue;
		if (n < 8 || !memmem(b, n, "RTL8XXXU-INJECT", 15)) continue;
		int rtlen = b[2] | b[3] << 8; uint32_t present; memcpy(&present, b + 4, 4);
		int off = 8; uint32_t p = present;
		while (p & (1u << 31)) { memcpy(&p, b + off, 4); off += 4; }
		int rate = -1, mcs = -1, sig = 0, txflags = -1, dretries = -1; unsigned char mflags = 0;
		for (int f = 0; f < 20; f++) {
			if (!(present & (1u << f))) continue;
			off = (off + falign[f] - 1) & ~(falign[f] - 1);
			if (off + fsz[f] > rtlen) break;
			if (f == 2) rate = b[off];
			if (f == 5) sig = (signed char)b[off];
			if (f == 19) { mflags = b[off + 1]; mcs = b[off + 2]; }
			if (f == 15) txflags = b[off] | b[off + 1] << 8;
			if (f == 17) dretries = b[off];
			off += fsz[f];
		}
		if (txflags >= 0) {	/* TX status echo from mac80211 */
			txst++; if (txflags & 1) txfail++;
			if (dretries >= 0) txretries[dretries & 63]++;
			continue;
		}
		total++; rssi_sum += sig;
		if (rtlen + 1 < n && (b[rtlen + 1] & 0x08)) retry++;
		if (mcs >= 0) { hist_mcs[mcs & 31]++; if (mflags & 4) sgi++; if ((mflags & 3) == 1) bw40++; }
		else if (rate >= 0) hist_legacy[rate & 127]++;
	}
	printf("captured %d test frames, avg signal %ld dBm\n", total, total ? rssi_sum / total : 0);
	for (int i = 0; i < 128; i++) if (hist_legacy[i]) printf("  legacy %.1f Mbps: %d\n", i / 2.0, hist_legacy[i]);
	for (int i = 0; i < 32; i++) if (hist_mcs[i]) printf("  MCS %d: %d\n", i, hist_mcs[i]);
	printf("  with retry bit: %d\n", retry);
	if (txst) {
		printf("  tx status: %d frames, %d acked, %d failed; reported retries:", txst, txst - txfail, txfail);
		for (int i = 0; i < 64; i++) if (txretries[i]) printf(" %dx%d", i, txretries[i]);
		printf("\n");
	}
	if (sgi || bw40) printf("  short GI: %d, 40 MHz: %d\n", sgi, bw40);
	return 0;
}
