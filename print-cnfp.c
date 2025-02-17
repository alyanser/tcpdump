/*	$OpenBSD: print-cnfp.c,v 1.2 1998/06/25 20:26:59 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: Cisco NetFlow protocol printer */

/*
 * Cisco NetFlow protocol
 *
 * See
 *
 *    https://www.cisco.com/c/en/us/td/docs/net_mgmt/netflow_collection_engine/3-6/user/guide/format.html#wp1005892
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "tcp.h"
#include "ipproto.h"

struct nfhdr_v1 {
	nd_uint16_t	version;	/* version number */
	nd_uint16_t	count;		/* # of records */
	nd_uint32_t	msys_uptime;
	nd_uint32_t	utc_sec;
	nd_uint32_t	utc_nsec;
};

struct nfrec_v1 {
	nd_ipv4		src_ina;
	nd_ipv4		dst_ina;
	nd_ipv4		nhop_ina;
	nd_uint16_t	input;		/* SNMP index of input interface */
	nd_uint16_t	output;		/* SNMP index of output interface */
	nd_uint32_t	packets;	/* packets in the flow */
	nd_uint32_t	octets;		/* layer 3 octets in the packets of the flow */
	nd_uint32_t	start_time;	/* sys_uptime value at start of flow */
	nd_uint32_t	last_time;	/* sys_uptime value when last packet of flow was received */
	nd_uint16_t	srcport;	/* TCP/UDP source port or equivalent */
	nd_uint16_t	dstport;	/* TCP/UDP source port or equivalent */
	nd_byte		pad1[2];	/* pad */
	nd_uint8_t	proto;		/* IP protocol type */
	nd_uint8_t	tos;		/* IP type of service */
	nd_uint8_t	tcp_flags;	/* cumulative OR of TCP flags */
	nd_byte		pad[3];		/* padding */
	nd_uint32_t	reserved;	/* unused */
};

struct nfhdr_v5 {
	nd_uint16_t	version;	/* version number */
	nd_uint16_t	count;		/* # of records */
	nd_uint32_t	msys_uptime;
	nd_uint32_t	utc_sec;
	nd_uint32_t	utc_nsec;
	nd_uint32_t	sequence;	/* flow sequence number */
	nd_uint8_t	engine_type;	/* type of flow-switching engine */
	nd_uint8_t	engine_id;	/* slot number of the flow-switching engine */
	nd_uint16_t	sampling_interval; /* sampling mode and interval */
};

struct nfrec_v5 {
	nd_ipv4		src_ina;
	nd_ipv4		dst_ina;
	nd_ipv4		nhop_ina;
	nd_uint16_t	input;		/* SNMP index of input interface */
	nd_uint16_t	output;		/* SNMP index of output interface */
	nd_uint32_t	packets;	/* packets in the flow */
	nd_uint32_t	octets;		/* layer 3 octets in the packets of the flow */
	nd_uint32_t	start_time;	/* sys_uptime value at start of flow */
	nd_uint32_t	last_time;	/* sys_uptime value when last packet of flow was received */
	nd_uint16_t	srcport;	/* TCP/UDP source port or equivalent */
	nd_uint16_t	dstport;	/* TCP/UDP source port or equivalent */
	nd_byte		pad1;		/* pad */
	nd_uint8_t	tcp_flags;	/* cumulative OR of TCP flags */
	nd_uint8_t	proto;		/* IP protocol type */
	nd_uint8_t	tos;		/* IP type of service */
	nd_uint16_t	src_as;		/* AS number of the source */
	nd_uint16_t	dst_as;		/* AS number of the destination */
	nd_uint8_t	src_mask;	/* source address mask bits */
	nd_uint8_t	dst_mask;	/* destination address prefix mask bits */
	nd_byte		pad2[2];
	nd_ipv4		peer_nexthop;	/* v6: IP address of the nexthop within the peer (FIB)*/
};

struct nfhdr_v6 {
	nd_uint16_t	version;	/* version number */
	nd_uint16_t	count;		/* # of records */
	nd_uint32_t	msys_uptime;
	nd_uint32_t	utc_sec;
	nd_uint32_t	utc_nsec;
	nd_uint32_t	sequence;	/* v5 flow sequence number */
	nd_uint32_t	reserved;	/* v5 only */
};

struct nfrec_v6 {
	nd_ipv4		src_ina;
	nd_ipv4		dst_ina;
	nd_ipv4		nhop_ina;
	nd_uint16_t	input;		/* SNMP index of input interface */
	nd_uint16_t	output;		/* SNMP index of output interface */
	nd_uint32_t	packets;	/* packets in the flow */
	nd_uint32_t	octets;		/* layer 3 octets in the packets of the flow */
	nd_uint32_t	start_time;	/* sys_uptime value at start of flow */
	nd_uint32_t	last_time;	/* sys_uptime value when last packet of flow was received */
	nd_uint16_t	srcport;	/* TCP/UDP source port or equivalent */
	nd_uint16_t	dstport;	/* TCP/UDP source port or equivalent */
	nd_byte		pad1;		/* pad */
	nd_uint8_t	tcp_flags;	/* cumulative OR of TCP flags */
	nd_uint8_t	proto;		/* IP protocol type */
	nd_uint8_t	tos;		/* IP type of service */
	nd_uint16_t	src_as;		/* AS number of the source */
	nd_uint16_t	dst_as;		/* AS number of the destination */
	nd_uint8_t	src_mask;	/* source address mask bits */
	nd_uint8_t	dst_mask;	/* destination address prefix mask bits */
	nd_uint16_t	flags;
	nd_ipv4		peer_nexthop;	/* v6: IP address of the nexthop within the peer (FIB)*/
};

static void
cnfp_v1_print(netdissect_options *ndo, const u_char *cp)
{
	const struct nfhdr_v1 *nh;
	const struct nfrec_v1 *nr;
	const char *p_name;
	uint8_t proto;
	u_int nrecs, ver;
#if 0
	time_t t;
#endif

	nh = (const struct nfhdr_v1 *)cp;

	ver = GET_BE_U_2(nh->version);
	nrecs = GET_BE_U_4(nh->count);
#if 0
	/*
	 * This is seconds since the UN*X epoch, and is followed by
	 * nanoseconds.  XXX - format it, rather than just dumping the
	 * raw seconds-since-the-Epoch.
	 */
	t = GET_BE_U_4(nh->utc_sec);
#endif

	ND_PRINT(C_RESET, "NetFlow v%x, %u.%03u uptime, %u.%09u, ", ver,
	       GET_BE_U_4(nh->msys_uptime)/1000,
	       GET_BE_U_4(nh->msys_uptime)%1000,
	       GET_BE_U_4(nh->utc_sec), GET_BE_U_4(nh->utc_nsec));

	nr = (const struct nfrec_v1 *)&nh[1];

	ND_PRINT(C_RESET, "%2u recs", nrecs);

	for (; nrecs != 0; nr++, nrecs--) {
		ND_PRINT(C_RESET, "\n  started %u.%03u, last %u.%03u",
		       GET_BE_U_4(nr->start_time)/1000,
		       GET_BE_U_4(nr->start_time)%1000,
		       GET_BE_U_4(nr->last_time)/1000,
		       GET_BE_U_4(nr->last_time)%1000);

		ND_PRINT(C_RESET, "\n    %s:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->src_ina)),
			GET_BE_U_2(nr->srcport));

		ND_PRINT(C_RESET, "> %s:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->dst_ina)),
			GET_BE_U_2(nr->dstport));

		ND_PRINT(C_RESET, ">> %s\n    ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->nhop_ina)));

		proto = GET_U_1(nr->proto);
		if (!ndo->ndo_nflag && (p_name = netdb_protoname(proto)) != NULL)
			ND_PRINT(C_RESET, "%s ", p_name);
		else
			ND_PRINT(C_RESET, "%u ", proto);

		/* tcp flags for tcp only */
		if (proto == IPPROTO_TCP) {
			u_int flags;
			flags = GET_U_1(nr->tcp_flags);
			if (flags)
				ND_PRINT(C_RESET, "%s ", bittok2str_nosep(tcp_flag_values, "", flags));
		}

		ND_PRINT(C_RESET, "tos %u, %u (%u octets)",
		       GET_U_1(nr->tos),
		       GET_BE_U_4(nr->packets),
		       GET_BE_U_4(nr->octets));
		/* This was not all of struct nfrec_v1. */
		ND_TCHECK_SIZE(nr);
	}
}

static void
cnfp_v5_print(netdissect_options *ndo, const u_char *cp)
{
	const struct nfhdr_v5 *nh;
	const struct nfrec_v5 *nr;
	const char *p_name;
	uint8_t proto;
	u_int nrecs, ver;
#if 0
	time_t t;
#endif

	nh = (const struct nfhdr_v5 *)cp;

	ver = GET_BE_U_2(nh->version);
	nrecs = GET_BE_U_4(nh->count);
#if 0
	/*
	 * This is seconds since the UN*X epoch, and is followed by
	 * nanoseconds.  XXX - format it, rather than just dumping the
	 * raw seconds-since-the-Epoch.
	 */
	t = GET_BE_U_4(nh->utc_sec);
#endif

	ND_PRINT(C_RESET, "NetFlow v%x, %u.%03u uptime, %u.%09u, ", ver,
	       GET_BE_U_4(nh->msys_uptime)/1000,
	       GET_BE_U_4(nh->msys_uptime)%1000,
	       GET_BE_U_4(nh->utc_sec), GET_BE_U_4(nh->utc_nsec));

	ND_PRINT(C_RESET, "#%u, ", GET_BE_U_4(nh->sequence));
	/* This was not all of struct nfhdr_v5. */
	ND_TCHECK_SIZE(nh);
	nr = (const struct nfrec_v5 *)&nh[1];

	ND_PRINT(C_RESET, "%2u recs", nrecs);

	for (; nrecs != 0; nr++, nrecs--) {
		ND_PRINT(C_RESET, "\n  started %u.%03u, last %u.%03u",
		       GET_BE_U_4(nr->start_time)/1000,
		       GET_BE_U_4(nr->start_time)%1000,
		       GET_BE_U_4(nr->last_time)/1000,
		       GET_BE_U_4(nr->last_time)%1000);

		ND_PRINT(C_RESET, "\n    %s/%u:%u:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->src_ina)),
			GET_U_1(nr->src_mask), GET_BE_U_2(nr->src_as),
			GET_BE_U_2(nr->srcport));

		ND_PRINT(C_RESET, "> %s/%u:%u:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->dst_ina)),
			GET_U_1(nr->dst_mask), GET_BE_U_2(nr->dst_as),
			GET_BE_U_2(nr->dstport));

		ND_PRINT(C_RESET, ">> %s\n    ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->nhop_ina)));

		proto = GET_U_1(nr->proto);
		if (!ndo->ndo_nflag && (p_name = netdb_protoname(proto)) != NULL)
			ND_PRINT(C_RESET, "%s ", p_name);
		else
			ND_PRINT(C_RESET, "%u ", proto);

		/* tcp flags for tcp only */
		if (proto == IPPROTO_TCP) {
			u_int flags;
			flags = GET_U_1(nr->tcp_flags);
			if (flags)
				ND_PRINT(C_RESET, "%s ", bittok2str_nosep(tcp_flag_values, "", flags));
		}

		ND_PRINT(C_RESET, "tos %u, %u (%u octets)",
		       GET_U_1(nr->tos),
		       GET_BE_U_4(nr->packets),
		       GET_BE_U_4(nr->octets));
		/* This was not all of struct nfrec_v5. */
		ND_TCHECK_SIZE(nr);
	}
}

static void
cnfp_v6_print(netdissect_options *ndo, const u_char *cp)
{
	const struct nfhdr_v6 *nh;
	const struct nfrec_v6 *nr;
	const char *p_name;
	uint8_t proto;
	u_int nrecs, ver;
#if 0
	time_t t;
#endif

	nh = (const struct nfhdr_v6 *)cp;

	ver = GET_BE_U_2(nh->version);
	nrecs = GET_BE_U_4(nh->count);
#if 0
	/*
	 * This is seconds since the UN*X epoch, and is followed by
	 * nanoseconds.  XXX - format it, rather than just dumping the
	 * raw seconds-since-the-Epoch.
	 */
	t = GET_BE_U_4(nh->utc_sec);
#endif

	ND_PRINT(C_RESET, "NetFlow v%x, %u.%03u uptime, %u.%09u, ", ver,
	       GET_BE_U_4(nh->msys_uptime)/1000,
	       GET_BE_U_4(nh->msys_uptime)%1000,
	       GET_BE_U_4(nh->utc_sec), GET_BE_U_4(nh->utc_nsec));

	ND_PRINT(C_RESET, "#%u, ", GET_BE_U_4(nh->sequence));
	/* This was not all of struct nfhdr_v6. */
	ND_TCHECK_SIZE(nh);
	nr = (const struct nfrec_v6 *)&nh[1];

	ND_PRINT(C_RESET, "%2u recs", nrecs);

	for (; nrecs != 0; nr++, nrecs--) {
		ND_PRINT(C_RESET, "\n  started %u.%03u, last %u.%03u",
		       GET_BE_U_4(nr->start_time)/1000,
		       GET_BE_U_4(nr->start_time)%1000,
		       GET_BE_U_4(nr->last_time)/1000,
		       GET_BE_U_4(nr->last_time)%1000);

		ND_PRINT(C_RESET, "\n    %s/%u:%u:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->src_ina)),
			GET_U_1(nr->src_mask), GET_BE_U_2(nr->src_as),
			GET_BE_U_2(nr->srcport));

		ND_PRINT(C_RESET, "> %s/%u:%u:%u ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->dst_ina)),
			GET_U_1(nr->dst_mask), GET_BE_U_2(nr->dst_as),
			GET_BE_U_2(nr->dstport));

		ND_PRINT(C_RESET, ">> %s\n    ",
			intoa(GET_IPV4_TO_NETWORK_ORDER(nr->nhop_ina)));

		proto = GET_U_1(nr->proto);
		if (!ndo->ndo_nflag && (p_name = netdb_protoname(proto)) != NULL)
			ND_PRINT(C_RESET, "%s ", p_name);
		else
			ND_PRINT(C_RESET, "%u ", proto);

		/* tcp flags for tcp only */
		if (proto == IPPROTO_TCP) {
			u_int flags;
			flags = GET_U_1(nr->tcp_flags);
			if (flags)
				ND_PRINT(C_RESET, "%s ", bittok2str_nosep(tcp_flag_values, "", flags));
		}

		ND_PRINT(C_RESET, "tos %u, %u (%u octets) (%u<>%u encaps)",
		       GET_U_1(nr->tos),
		       GET_BE_U_4(nr->packets),
		       GET_BE_U_4(nr->octets),
		       (GET_BE_U_2(nr->flags) >> 8) & 0xff,
		       (GET_BE_U_2(nr->flags)) & 0xff);
		/* This was not all of struct nfrec_v6. */
		ND_TCHECK_SIZE(nr);
	}
}

void
cnfp_print(netdissect_options *ndo, const u_char *cp)
{
	int ver;

	/*
	 * First 2 bytes are the version number.
	 */
	ndo->ndo_protocol = "cnfp";
	ver = GET_BE_U_2(cp);
	switch (ver) {

	case 1:
		cnfp_v1_print(ndo, cp);
		break;

	case 5:
		cnfp_v5_print(ndo, cp);
		break;

	case 6:
		cnfp_v6_print(ndo, cp);
		break;

	default:
		ND_PRINT(C_RESET, "NetFlow v%x", ver);
		break;
	}
}
