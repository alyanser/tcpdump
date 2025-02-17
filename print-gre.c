/*	$OpenBSD: print-gre.c,v 1.6 2002/10/30 03:04:04 fgsch Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: Generic Routing Encapsulation (GRE) printer */

/*
 * netdissect printer for GRE - Generic Routing Encapsulation
 * RFC1701 (GRE), RFC1702 (GRE IPv4), and RFC2637 (Enhanced GRE)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtostr.h"
#include "extract.h"
#include "ethertype.h"


#define	GRE_CP		0x8000		/* checksum present */
#define	GRE_RP		0x4000		/* routing present */
#define	GRE_KP		0x2000		/* key present */
#define	GRE_SP		0x1000		/* sequence# present */
#define	GRE_sP		0x0800		/* source routing */
#define	GRE_AP		0x0080		/* acknowledgment# present */

static const struct tok gre_flag_values[] = {
    { GRE_CP, "checksum present"},
    { GRE_RP, "routing present"},
    { GRE_KP, "key present"},
    { GRE_SP, "sequence# present"},
    { GRE_sP, "source routing present"},
    { GRE_AP, "ack present"},
    { 0, NULL }
};

#define	GRE_RECRS_MASK	0x0700		/* recursion count */
#define	GRE_VERS_MASK	0x0007		/* protocol version */

/* source route entry types */
#define	GRESRE_IP	0x0800		/* IP */
#define	GRESRE_ASN	0xfffe		/* ASN */

static void gre_print_0(netdissect_options *, const u_char *, u_int);
static void gre_print_1(netdissect_options *, const u_char *, u_int);
static int gre_sre_print(netdissect_options *, uint16_t, uint8_t, uint8_t, const u_char *, u_int);
static int gre_sre_ip_print(netdissect_options *, uint8_t, uint8_t, const u_char *, u_int);
static int gre_sre_asn_print(netdissect_options *, uint8_t, uint8_t, const u_char *, u_int);

void
gre_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	u_int vers;

	ndo->ndo_protocol = "gre";
	nd_print_protocol_caps(ndo);
	ND_ICHECK_U(length, <, 2);
	vers = GET_BE_U_2(bp) & GRE_VERS_MASK;
	ND_PRINT(C_RESET, "v%u",vers);

	switch(vers) {
	case 0:
		gre_print_0(ndo, bp, length);
		break;
	case 1:
		gre_print_1(ndo, bp, length);
		break;
	default:
		ND_PRINT(C_RESET, " ERROR: unknown-version");
		break;
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
gre_print_0(netdissect_options *ndo, const u_char *bp, u_int length)
{
	u_int len = length;
	uint16_t flags, prot;

	ND_ICHECK_U(len, <, 2);
	flags = GET_BE_U_2(bp);
	if (ndo->ndo_vflag)
		ND_PRINT(C_RESET, ", Flags [%s]",
			 bittok2str(gre_flag_values,"none",flags));

	len -= 2;
	bp += 2;

	ND_ICHECK_U(len, <, 2);
	prot = GET_BE_U_2(bp);
	len -= 2;
	bp += 2;

	if ((flags & GRE_CP) | (flags & GRE_RP)) {
		uint16_t sum;

		ND_ICHECK_U(len, <, 2);
		sum =  GET_BE_U_2(bp);
		if (ndo->ndo_vflag)
			ND_PRINT(C_RESET, ", sum 0x%x", sum);
		bp += 2;
		len -= 2;

		ND_ICHECK_U(len, <, 2);
		ND_PRINT(C_RESET, ", off 0x%x", GET_BE_U_2(bp));
		bp += 2;
		len -= 2;
	}

	if (flags & GRE_KP) {
		ND_ICHECK_U(len, <, 4);
		ND_PRINT(C_RESET, ", key=0x%x", GET_BE_U_4(bp));
		bp += 4;
		len -= 4;
	}

	if (flags & GRE_SP) {
		ND_ICHECK_U(len, <, 4);
		ND_PRINT(C_RESET, ", seq %u", GET_BE_U_4(bp));
		bp += 4;
		len -= 4;
	}

	if (flags & GRE_RP) {
		for (;;) {
			uint16_t af;
			uint8_t sreoff;
			uint8_t srelen;

			ND_ICHECK_U(len, <, 4);
			af = GET_BE_U_2(bp);
			sreoff = GET_U_1(bp + 2);
			srelen = GET_U_1(bp + 3);
			bp += 4;
			len -= 4;

			if (af == 0 && srelen == 0)
				break;

			if (!gre_sre_print(ndo, af, sreoff, srelen, bp, len))
				goto invalid;

			ND_ICHECK_U(len, <, srelen);
			bp += srelen;
			len -= srelen;
		}
	}

	if (ndo->ndo_eflag)
		ND_PRINT(C_RESET, ", proto %s (0x%04x)",
			 tok2str(ethertype_values,"unknown",prot), prot);

	ND_PRINT(C_RESET, ", length %u",length);

	if (ndo->ndo_vflag < 1)
		ND_PRINT(C_RESET, ": "); /* put in a colon as protocol demarc */
	else
		ND_PRINT(C_RESET, "\n\t"); /* if verbose go multiline */

	switch (prot) {
	case ETHERTYPE_IP:
		ip_print(ndo, bp, len);
		break;
	case ETHERTYPE_IPV6:
		ip6_print(ndo, bp, len);
		break;
	case ETHERTYPE_MPLS:
		mpls_print(ndo, bp, len);
		break;
	case ETHERTYPE_IPX:
		ipx_print(ndo, bp, len);
		break;
	case ETHERTYPE_ATALK:
		atalk_print(ndo, bp, len);
		break;
	case ETHERTYPE_GRE_ISO:
		isoclns_print(ndo, bp, len);
		break;
	case ETHERTYPE_TEB:
		ether_print(ndo, bp, len, ND_BYTES_AVAILABLE_AFTER(bp), NULL, NULL);
		break;
	default:
		ND_PRINT(C_RESET, "gre-proto-0x%x", prot);
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static void
gre_print_1(netdissect_options *ndo, const u_char *bp, u_int length)
{
	u_int len = length;
	uint16_t flags, prot;

	ND_ICHECK_U(len, <, 2);
	flags = GET_BE_U_2(bp);
	len -= 2;
	bp += 2;

	if (ndo->ndo_vflag)
		ND_PRINT(C_RESET, ", Flags [%s]",
			 bittok2str(gre_flag_values,"none",flags));

	ND_ICHECK_U(len, <, 2);
	prot = GET_BE_U_2(bp);
	len -= 2;
	bp += 2;


	if (flags & GRE_KP) {
		uint32_t k;

		ND_ICHECK_U(len, <, 4);
		k = GET_BE_U_4(bp);
		ND_PRINT(C_RESET, ", call %u", k & 0xffff);
		len -= 4;
		bp += 4;
	}

	if (flags & GRE_SP) {
		ND_ICHECK_U(len, <, 4);
		ND_PRINT(C_RESET, ", seq %u", GET_BE_U_4(bp));
		bp += 4;
		len -= 4;
	}

	if (flags & GRE_AP) {
		ND_ICHECK_U(len, <, 4);
		ND_PRINT(C_RESET, ", ack %u", GET_BE_U_4(bp));
		bp += 4;
		len -= 4;
	}

	if ((flags & GRE_SP) == 0)
		ND_PRINT(C_RESET, ", no-payload");

	if (ndo->ndo_eflag)
		ND_PRINT(C_RESET, ", proto %s (0x%04x)",
			 tok2str(ethertype_values,"unknown",prot), prot);

	ND_PRINT(C_RESET, ", length %u",length);

	if ((flags & GRE_SP) == 0)
		return;

	if (ndo->ndo_vflag < 1)
		ND_PRINT(C_RESET, ": "); /* put in a colon as protocol demarc */
	else
		ND_PRINT(C_RESET, "\n\t"); /* if verbose go multiline */

	switch (prot) {
	case ETHERTYPE_PPP:
		ppp_print(ndo, bp, len);
		break;
	default:
		ND_PRINT(C_RESET, "gre-proto-0x%x", prot);
		break;
	}
	return;

invalid:
	nd_print_invalid(ndo);
}

static int
gre_sre_print(netdissect_options *ndo, uint16_t af, uint8_t sreoff,
	      uint8_t srelen, const u_char *bp, u_int len)
{
	int ret;

	switch (af) {
	case GRESRE_IP:
		ND_PRINT(C_RESET, ", (rtaf=ip");
		ret = gre_sre_ip_print(ndo, sreoff, srelen, bp, len);
		ND_PRINT(C_RESET, ")");
		break;
	case GRESRE_ASN:
		ND_PRINT(C_RESET, ", (rtaf=asn");
		ret = gre_sre_asn_print(ndo, sreoff, srelen, bp, len);
		ND_PRINT(C_RESET, ")");
		break;
	default:
		ND_PRINT(C_RESET, ", (rtaf=0x%x)", af);
		ret = 1;
	}
	return (ret);
}

static int
gre_sre_ip_print(netdissect_options *ndo, uint8_t sreoff, uint8_t srelen,
		 const u_char *bp, u_int len)
{
	const u_char *up = bp;
	char buf[INET_ADDRSTRLEN];

	if (sreoff & 3) {
		ND_PRINT(C_RESET, ", badoffset=%u", sreoff);
		goto invalid;
	}
	if (srelen & 3) {
		ND_PRINT(C_RESET, ", badlength=%u", srelen);
		goto invalid;
	}
	if (sreoff >= srelen) {
		ND_PRINT(C_RESET, ", badoff/len=%u/%u", sreoff, srelen);
		goto invalid;
	}

	while (srelen != 0) {
		ND_ICHECK_U(len, <, 4);

		ND_TCHECK_LEN(bp, sizeof(nd_ipv4));
		addrtostr(bp, buf, sizeof(buf));
		ND_PRINT(C_RESET, " %s%s",
			 ((bp - up) == sreoff) ? "*" : "", buf);

		bp += 4;
		len -= 4;
		srelen -= 4;
	}
	return 1;

invalid:
	return 0;
}

static int
gre_sre_asn_print(netdissect_options *ndo, uint8_t sreoff, uint8_t srelen,
		  const u_char *bp, u_int len)
{
	const u_char *up = bp;

	if (sreoff & 1) {
		ND_PRINT(C_RESET, ", badoffset=%u", sreoff);
		goto invalid;
	}
	if (srelen & 1) {
		ND_PRINT(C_RESET, ", badlength=%u", srelen);
		goto invalid;
	}
	if (sreoff >= srelen) {
		ND_PRINT(C_RESET, ", badoff/len=%u/%u", sreoff, srelen);
		goto invalid;
	}

	while (srelen != 0) {
		ND_ICHECK_U(len, <, 2);

		ND_PRINT(C_RESET, " %s%x",
			 ((bp - up) == sreoff) ? "*" : "", GET_BE_U_2(bp));

		bp += 2;
		len -= 2;
		srelen -= 2;
	}
	return 1;

invalid:
	return 0;
}
