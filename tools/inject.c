// SPDX-License-Identifier: GPL-2.0-only
// Minimal radiotap injector: sends N beacon-like data frames on a monitor iface.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: %s IFACE COUNT [[w][s]MCS (w=40MHz, s=short GI)|L<500kbps units>|-1] [DST BSSID]\n", argv[0]); return 1; }
	int count = atoi(argv[2]), mcs = -1, legacy = -1, sgi = 0, bw40 = 0;
	if (argc > 3) {
		const char *r = argv[3];
		if (*r == 'w') { bw40 = 1; r++; }	/* w = 40 MHz */
		if (*r == 's') { sgi = 1; r++; }	/* s = short GI */
		if (*r == 'L') legacy = atoi(r + 1);
		else mcs = atoi(r);
	}
	int s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s < 0) { perror("socket"); return 1; }
	struct sockaddr_ll sll = { .sll_family = AF_PACKET, .sll_ifindex = if_nametoindex(argv[1]) };
	if (!sll.sll_ifindex) { perror("if_nametoindex"); return 1; }

	unsigned char pkt[256]; int n = 0;
	/* radiotap: TX_FLAGS (bit15) + optional MCS (bit19) */
	/* radiotap: RATE (bit2, 1 byte + 1 pad) + TX_FLAGS (bit15) + optional MCS (bit19) */
	/* env: NOACK=0 to expect an ACK, RETRIES=n for radiotap DATA_RETRIES (bit17) */
	const char *ev = getenv("NOACK"), *rv = getenv("RETRIES");
	int noack = !ev || atoi(ev), retries = rv ? atoi(rv) : -1;
	unsigned int present = (legacy >= 0 ? (1u << 2) : 0) | (1u << 15) |
			       (retries >= 0 ? (1u << 17) : 0) | (mcs >= 0 ? (1u << 19) : 0);
	unsigned char rt[16] = {0};
	int o = 8;
	if (legacy >= 0) { rt[o] = legacy; o += 2; }
	rt[o] = noack ? 0x08 : 0x00; rt[o + 1] = 0x00; o += 2;   /* IEEE80211_RADIOTAP_F_TX_NOACK */
	if (retries >= 0) rt[o++] = retries;
	if (mcs >= 0) {   /* known: MCS idx + GI; flags: GI */
		/* known: BW, MCS idx, GI; flags: BW 1 = 40 MHz, GI */
		rt[o] = 0x01 | 0x02 | 0x04; rt[o + 1] = (bw40 ? 0x01 : 0) | (sgi ? 0x04 : 0);
		rt[o + 2] = mcs; o += 3;
	}
	int rtlen = o;
	rt[2] = rtlen; memcpy(rt + 4, &present, 4);
	memcpy(pkt, rt, rtlen); n = rtlen;
	unsigned char hdr[24] = { 0x08, 0x00, 0, 0,          /* data frame */
		0xff,0xff,0xff,0xff,0xff,0xff,                     /* addr1 bcast */
		0x02,0x57,0x42,0x75,0x05,0xd6,                     /* addr2 test */
		0x02,0x57,0x42,0x75,0x05,0xd6, 0, 0 };
	if (argc > 5) {	/* addressed FromDS frame: addr1=DST, addr2=addr3=BSSID */
		unsigned int d[6], b[6];
		sscanf(argv[4], "%x:%x:%x:%x:%x:%x", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5]);
		sscanf(argv[5], "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
		hdr[1] = 0x02;
		for (int i = 0; i < 6; i++) { hdr[4 + i] = d[i]; hdr[10 + i] = b[i]; hdr[16 + i] = b[i]; }
	}
	memcpy(pkt + n, hdr, 24); n += 24;
	const char *payload = "LIBRE-RTL8XXXU-INJECT-TEST";
	memcpy(pkt + n, payload, strlen(payload)); n += strlen(payload);

	int ok = 0;
	for (int i = 0; i < count; i++) {
		if (sendto(s, pkt, n, 0, (struct sockaddr *)&sll, sizeof(sll)) == n) ok++;
		if (!getenv("NOSLEEP")) usleep(getenv("DELAY_US") ? atoi(getenv("DELAY_US")) : 1000);
	}
	printf("sent %d/%d frames (%d bytes each)\n", ok, count, n);
	return 0;
}
