/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-rsvp.c,v 1.3 2002-10-10 19:34:51 hannes Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"

/*
 * RFC 2205 common header
 *
 *               0             1              2             3
 *        +-------------+-------------+-------------+-------------+
 *        | Vers | Flags|  Msg Type   |       RSVP Checksum       |
 *        +-------------+-------------+-------------+-------------+
 *        |  Send_TTL   | (Reserved)  |        RSVP Length        |
 *        +-------------+-------------+-------------+-------------+
 *
 */

struct rsvp_common_header {
    u_char version_flags;
    u_char msg_type;
    u_char checksum[2];
    u_char ttl;
    u_char reserved;
    u_char length[2];
};

/* 
 * RFC2205 object header
 *
 * 
 *               0             1              2             3
 *        +-------------+-------------+-------------+-------------+
 *        |       Length (bytes)      |  Class-Num  |   C-Type    |
 *        +-------------+-------------+-------------+-------------+
 *        |                                                       |
 *        //                  (Object contents)                   //
 *        |                                                       |
 *        +-------------+-------------+-------------+-------------+
 */

struct rsvp_object_header {
    u_char length[2];
    u_char class_num;
    u_char ctype;
};

#define RSVP_VERSION            1
#define	RSVP_EXTRACT_VERSION(x) (((x)&0xf0)>>4) 

#define	RSVP_MSGTYPE_PATH       1
#define	RSVP_MSGTYPE_RESV       2
#define	RSVP_MSGTYPE_PATHERR    3
#define	RSVP_MSGTYPE_RESVERR    4
#define	RSVP_MSGTYPE_PATHTEAR   5
#define	RSVP_MSGTYPE_RESVTEAR   6
#define	RSVP_MSGTYPE_RESVCONF   7
#define RSVP_MSGTYPE_AGGREGATE  12
#define RSVP_MSGTYPE_ACK        13
#define RSVP_MSGTYPE_HELLO_OLD  14      /* ancient Hellos */
#define RSVP_MSGTYPE_SREFRESH   15
#define	RSVP_MSGTYPE_HELLO      20

static const struct tok rsvp_msg_type_values[] = {
    { RSVP_MSGTYPE_PATH,	"Path" },
    { RSVP_MSGTYPE_RESV,	"Resv" },
    { RSVP_MSGTYPE_PATHERR,	"PathErr" },
    { RSVP_MSGTYPE_RESVERR,	"ResvErr" },
    { RSVP_MSGTYPE_PATHTEAR,	"PathTear" },
    { RSVP_MSGTYPE_RESVTEAR,	"ResvTear" },
    { RSVP_MSGTYPE_RESVCONF,	"ResvConf" },
    { RSVP_MSGTYPE_AGGREGATE,	"Aggregate" },
    { RSVP_MSGTYPE_ACK,	        "Acknowledgement" },
    { RSVP_MSGTYPE_HELLO_OLD,	"Hello (Old)" },
    { RSVP_MSGTYPE_SREFRESH,	"Refresh" },
    { RSVP_MSGTYPE_HELLO,	"Hello" },
    { 0, NULL}
};

#define	RSVP_OBJ_SESSION            1
#define	RSVP_OBJ_RSVP_HOP           3
#define	RSVP_OBJ_INTEGRITY          4
#define	RSVP_OBJ_TIME_VALUES        5
#define	RSVP_OBJ_ERROR_SPEC         6
#define	RSVP_OBJ_SCOPE              7
#define	RSVP_OBJ_STYLE              8
#define	RSVP_OBJ_FLOWSPEC           9
#define	RSVP_OBJ_FILTERSPEC         10
#define	RSVP_OBJ_SENDER_TEMPLATE    11
#define	RSVP_OBJ_SENDER_TSPEC       12
#define	RSVP_OBJ_ADSPEC             13
#define	RSVP_OBJ_POLICY_DATA        14
#define	RSVP_OBJ_CONFIRM            15
#define	RSVP_OBJ_LABEL              16
#define	RSVP_OBJ_LABEL_REQ          19
#define	RSVP_OBJ_ERO                20
#define	RSVP_OBJ_RRO                21
#define	RSVP_OBJ_HELLO              22
#define	RSVP_OBJ_MESSAGE_ID         23
#define	RSVP_OBJ_MESSAGE_ID_ACK     24
#define	RSVP_OBJ_MESSAGE_ID_LIST    25
#define	RSVP_OBJ_RECOVERY_LABEL     34
#define	RSVP_OBJ_UPSTREAM_LABEL     35
#define	RSVP_OBJ_DETOUR             63
#define	RSVP_OBJ_SUGGESTED_LABEL    129
#define	RSVP_OBJ_PROPERTIES         204
#define	RSVP_OBJ_FASTREROUTE        205
#define	RSVP_OBJ_SESSION_ATTTRIBUTE 207
#define	RSVP_OBJ_RESTART_CAPABILITY 131 /* draft-pan-rsvp-te-restart */

static const struct tok rsvp_obj_values[] = {
    { RSVP_OBJ_SESSION,            "Session" },
    { RSVP_OBJ_RSVP_HOP,           "RSVP Hop" },
    { RSVP_OBJ_INTEGRITY,          "Integrity" },
    { RSVP_OBJ_TIME_VALUES,        "Time Values" },
    { RSVP_OBJ_ERROR_SPEC,         "Error Spec" },
    { RSVP_OBJ_SCOPE,              "Scope" },
    { RSVP_OBJ_STYLE,              "Style" },
    { RSVP_OBJ_FLOWSPEC,           "Flowspec" },
    { RSVP_OBJ_FILTERSPEC,         "FilterSpec" },
    { RSVP_OBJ_SENDER_TEMPLATE,    "Sender Template" },
    { RSVP_OBJ_SENDER_TSPEC,       "Sender TSpec" },
    { RSVP_OBJ_ADSPEC,             "Adspec" },
    { RSVP_OBJ_POLICY_DATA,        "Policy Data" },
    { RSVP_OBJ_CONFIRM,            "Confirm" },
    { RSVP_OBJ_LABEL,              "Label" },
    { RSVP_OBJ_LABEL_REQ,          "Label Request" },
    { RSVP_OBJ_ERO,                "ERO" },
    { RSVP_OBJ_RRO,                "RRO" },
    { RSVP_OBJ_HELLO,              "Hello" },
    { RSVP_OBJ_MESSAGE_ID,         "Message ID" },
    { RSVP_OBJ_MESSAGE_ID_ACK,     "Message ID Ack" },
    { RSVP_OBJ_MESSAGE_ID_LIST,    "Message ID List" },
    { RSVP_OBJ_RECOVERY_LABEL,     "Recovery Label" },
    { RSVP_OBJ_UPSTREAM_LABEL,     "Upstream Label" },
    { RSVP_OBJ_DETOUR,             "Detour" },
    { RSVP_OBJ_SUGGESTED_LABEL,    "Suggested Label" },
    { RSVP_OBJ_PROPERTIES,         "Properties" },
    { RSVP_OBJ_FASTREROUTE,        "Fast Re-Route" },
    { RSVP_OBJ_SESSION_ATTTRIBUTE, "Session Attribute" },
    { RSVP_OBJ_RESTART_CAPABILITY, "Restart Capability" },
    { 0, NULL}
};

#define	RSVP_CTYPE_IPV4        1
#define	RSVP_CTYPE_IPV6        2
#define	RSVP_CTYPE_TUNNEL_IPV4 7
#define	RSVP_CTYPE_TUNNEL_IPV6 8
#define RSVP_CTYPE_1           1
#define RSVP_CTYPE_2           2
#define RSVP_CTYPE_3           3

/*
 * the ctypes are not globally unique so for
 * translating it to strings we build a table based
 * on objects offsetted by the ctype
 */

static const struct tok rsvp_ctype_values[] = {
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_IPV4,	           "IPv4" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_IPV6,	           "IPv6" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_TUNNEL_IPV4,         "Tunnel IPv4" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_TUNNEL_IPV4, "Tunnel IPv4" },
    { 256*RSVP_OBJ_STYLE+RSVP_CTYPE_1,                     "1" },
    { 256*RSVP_OBJ_HELLO+RSVP_CTYPE_1,                     "Hello Request" },
    { 256*RSVP_OBJ_HELLO+RSVP_CTYPE_2,                     "Hello Ack" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_1,	           "without label range" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_2,	           "with ATM label range" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_3,                 "with FR label range" },
    { 256*RSVP_OBJ_LABEL+RSVP_CTYPE_1,                     "1" },
    { 256*RSVP_OBJ_ERO+RSVP_CTYPE_IPV4,                    "IPv4" },
    { 256*RSVP_OBJ_RRO+RSVP_CTYPE_IPV4,                    "IPv4" },
    { 256*RSVP_OBJ_RESTART_CAPABILITY+RSVP_CTYPE_1,        "IPv4" },
    { 0, NULL}
};

#define RSVP_OBJ_XRO_MASK_SUBOBJ(x)   ((x)&0x7f)
#define RSVP_OBJ_XRO_MASK_LOOSE(x)    ((x)&0x80)

#define	RSVP_OBJ_XRO_RES       0
#define	RSVP_OBJ_XRO_IPV4      1
#define	RSVP_OBJ_XRO_IPV6      2
#define	RSVP_OBJ_XRO_ASN       32
#define	RSVP_OBJ_XRO_MPLS      64

static const struct tok rsvp_obj_xro_values[] = {
    { RSVP_OBJ_XRO_RES,	      "Reserved" },
    { RSVP_OBJ_XRO_IPV4,      "IPv4 prefix" },
    { RSVP_OBJ_XRO_IPV6,      "IPv6 prefix" },
    { RSVP_OBJ_XRO_ASN,       "Autonomous system number" },
    { RSVP_OBJ_XRO_MPLS,      "MPLS label switched path termination" },
    { 0, NULL}
};

static const struct tok rsvp_resstyle_values[] = {
    { 17,	              "Wildcard Filter" },
    { 10,                     "Fixed Filter" },
    { 18,                     "Shared Explicit" },
    { 0, NULL}
};

#define FALSE 0
#define TRUE  1

void
rsvp_print(register const u_char *pptr, register u_int len) {

    const struct rsvp_common_header *rsvp_com_header;
    const struct rsvp_object_header *rsvp_obj_header;
    const u_char *tptr,*obj_tptr;
    u_short tlen,rsvp_obj_len,rsvp_obj_ctype,obj_tlen;
    int hexdump;

    tptr=pptr;
    rsvp_com_header = (const struct rsvp_common_header *)pptr;
    TCHECK(*rsvp_com_header);

    /*
     * Sanity checking of the header.
     */
    if (RSVP_EXTRACT_VERSION(rsvp_com_header->version_flags) != RSVP_VERSION) {
	printf("RSVP version %u packet not supported",
               RSVP_EXTRACT_VERSION(rsvp_com_header->version_flags));
	return;
    }

    /* in non-verbose mode just lets print the basic Message Type*/
    if (vflag < 1) {
        printf("RSVP %s Message, length: %u",
               tok2str(rsvp_msg_type_values, "unknown (%u)",rsvp_com_header->msg_type),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */

    printf("RSVP, length: %u",len);

    tlen=EXTRACT_16BITS(rsvp_com_header->length);

    printf("\n\tv: %u, msg-type: %s, length: %u, ttl: %u, checksum: 0x%04x",
           RSVP_EXTRACT_VERSION(rsvp_com_header->version_flags),
           tok2str(rsvp_msg_type_values, "unknown, type: %u",rsvp_com_header->msg_type),
           tlen,
           rsvp_com_header->ttl,
           EXTRACT_16BITS(rsvp_com_header->checksum));

    tptr+=sizeof(const struct rsvp_common_header);
    tlen-=sizeof(const struct rsvp_common_header);

    while(tlen>0) {
        /* did we capture enough for fully decoding the object header ? */
        if (!TTEST2(*tptr, sizeof(struct rsvp_object_header)))
            goto trunc;

        rsvp_obj_header = (const struct rsvp_object_header *)tptr;
        rsvp_obj_len=EXTRACT_16BITS(rsvp_obj_header->length);
        rsvp_obj_ctype=rsvp_obj_header->ctype;

        if(rsvp_obj_len % 4 || rsvp_obj_len < 4)
            return;

        printf("\n\t  %s Object (%u) flags: [%s",
               tok2str(rsvp_obj_values,
                       "Unknown",
                       rsvp_obj_header->class_num),
               rsvp_obj_header->class_num,
               ((rsvp_obj_header->class_num)&0x80) ? "ignore" : "reject");

        if (rsvp_obj_header->class_num > 128)
            printf(" %s",
                   ((rsvp_obj_header->class_num)&0x40) ? "and forward" : "silently");

        printf(" if unknown], Class-Type: %s (%u), length: %u",
               tok2str(rsvp_ctype_values,
                       "Unknown",
                       ((rsvp_obj_header->class_num)<<8)+rsvp_obj_ctype),
               rsvp_obj_ctype,
               rsvp_obj_len);

        obj_tptr=tptr+sizeof(struct rsvp_object_header);
        obj_tlen=rsvp_obj_len-sizeof(struct rsvp_object_header);

        /* did we capture enough for fully decoding the object ? */
        if (!TTEST2(*tptr, rsvp_obj_len))
            goto trunc;
        hexdump=FALSE;

        switch(rsvp_obj_header->class_num) {
        case RSVP_OBJ_SESSION:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                printf("\n\t    IPv4 DestAddress: %s, Protocol ID: 0x%02x",
                       ipaddr_string(obj_tptr),
                       *(obj_tptr+4));
                printf("\n\t    Flags: 0x%02x, DestPort %u",
                       *(obj_tptr+5),
                       EXTRACT_16BITS(obj_tptr+6));
                obj_tlen-=8;
                obj_tptr+=8;                
                break;
#ifdef INET6
            case RSVP_CTYPE_IPV6:
                printf("\n\t    IPv6 DestAddress: %s, Protocol ID: 0x%02x",
                       ip6addr_string(obj_tptr),
                       *(obj_tptr+16));
                printf("\n\t    Flags: 0x%02x, DestPort %u",
                       *(obj_tptr+17),
                       EXTRACT_16BITS(obj_tptr+18));
                obj_tlen-=20;
                obj_tptr+=20;                
                break;
            case RSVP_CTYPE_TUNNEL_IPV6:
                printf("\n\t    IPv6 Tunnel EndPoint: %s, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       ip6addr_string(obj_tptr),
                       EXTRACT_16BITS(obj_tptr+18),
                       ip6addr_string(obj_tptr+20));
                obj_tlen-=36;
                obj_tptr+=36;                
                break;
#endif
            case RSVP_CTYPE_TUNNEL_IPV4:
                printf("\n\t    IPv4 Tunnel EndPoint: %s, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       ipaddr_string(obj_tptr),
                       EXTRACT_16BITS(obj_tptr+6),
                       ipaddr_string(obj_tptr+8));
                obj_tlen-=12;
                obj_tptr+=12;                
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_LABEL:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                while(obj_tlen >= 4 ) {
                    printf("\n\t    Label: %u", EXTRACT_32BITS(obj_tptr));
                    obj_tlen-=4;
                    obj_tptr+=4;
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_STYLE:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                printf("\n\t    Reservation Style: %s, Flags: 0x%02x",
                       tok2str(rsvp_resstyle_values,
                               "Unknown",
                               EXTRACT_24BITS(obj_tptr+1)),
                       *(obj_tptr));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_SENDER_TEMPLATE:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_TUNNEL_IPV4:
                printf("\n\t    IPv4 Tunnel Sender Address: %s, LSP-ID: 0x%04x",
                       ipaddr_string(obj_tptr),
                       EXTRACT_16BITS(obj_tptr+6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_LABEL_REQ:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                while(obj_tlen >= 4 ) {
                    printf("\n\t    L3 Protocol ID: %s",
                           tok2str(ethertype_values,
                                   "Unknown Protocol 0x%04x",
                                   EXTRACT_16BITS(obj_tptr+2)));
                    obj_tlen-=4;
                    obj_tptr+=4;
                }
                break;
            case RSVP_CTYPE_2:
                printf("\n\t    L3 Protocol ID: %s",
                       tok2str(ethertype_values,
                               "Unknown Protocol 0x%04x",
                               EXTRACT_16BITS(obj_tptr+2)));
                printf(",%s merge capability",((*(obj_tptr+4))&0x80) ? "no" : "" );
                printf("\n\t    Minimum VPI/VCI %u/%u",
                       (EXTRACT_16BITS(obj_tptr+4))&0xfff,
                       (EXTRACT_16BITS(obj_tptr+6))&0xfff);
                printf("\n\t    Maximum VPI/VCI %u/%u",
                       (EXTRACT_16BITS(obj_tptr+8))&0xfff,
                       (EXTRACT_16BITS(obj_tptr+10))&0xfff);
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            case RSVP_CTYPE_3:
                printf("\n\t    L3 Protocol ID: %s",
                       tok2str(ethertype_values,
                               "Unknown Protocol 0x%04x",
                               EXTRACT_16BITS(obj_tptr+2)));
                printf("\n\t    Minimum/Maximum DLCI %u/%u, %s%s bit DLCI",
                       (EXTRACT_32BITS(obj_tptr+4))&0x7fffff,
                       (EXTRACT_32BITS(obj_tptr+8))&0x7fffff,
                       (((EXTRACT_16BITS(obj_tptr+4)>>7)&3) == 0 ) ? "10" : "",
                       (((EXTRACT_16BITS(obj_tptr+4)>>7)&3) == 2 ) ? "23" : "");
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_RRO:
        case RSVP_OBJ_ERO:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                while(obj_tlen >= 4 ) {
                    printf("\n\t    Subobject Type: %s",
                           tok2str(rsvp_obj_xro_values,
                                   "Unknown %u",
                                   RSVP_OBJ_XRO_MASK_SUBOBJ(*obj_tptr)));                
                    switch(RSVP_OBJ_XRO_MASK_SUBOBJ(*obj_tptr)) {
                    case RSVP_OBJ_XRO_IPV4:
                        printf(", %s, %s/%u",
                               RSVP_OBJ_XRO_MASK_LOOSE(*obj_tptr) ? "Loose" : "Strict",
                               ipaddr_string(obj_tptr+2),
                               *(obj_tptr+6));
                    }
                    obj_tlen-=*(obj_tptr+1);
                    obj_tptr+=*(obj_tptr+1);
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_HELLO:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
            case RSVP_CTYPE_2:
                printf("\n\t    Source Instance 0x%08x, Destination Instance 0x%08x",
                       EXTRACT_32BITS(obj_tptr),
                       EXTRACT_32BITS(obj_tptr+4));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            default:
                hexdump=TRUE;
            }
            break;
        case RSVP_OBJ_RESTART_CAPABILITY:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                printf("\n\t    Restart  Time: %ums\n\t    Recovery Time: %ums",
                       EXTRACT_16BITS(obj_tptr),
                       EXTRACT_16BITS(obj_tptr+4));
                break;
            default:
                hexdump=TRUE;
            }
            break;
        /*
         *  FIXME those are the defined objects that lack a decoder
         *  you are welcome to contribute code ;-)
         */
        case RSVP_OBJ_RSVP_HOP:
        case RSVP_OBJ_INTEGRITY:
        case RSVP_OBJ_TIME_VALUES:
        case RSVP_OBJ_ERROR_SPEC:
        case RSVP_OBJ_SCOPE:
        case RSVP_OBJ_FILTERSPEC:
        case RSVP_OBJ_SENDER_TSPEC:
        case RSVP_OBJ_ADSPEC:
        case RSVP_OBJ_POLICY_DATA:
        case RSVP_OBJ_CONFIRM:
        case RSVP_OBJ_MESSAGE_ID:
        case RSVP_OBJ_MESSAGE_ID_ACK:
        case RSVP_OBJ_MESSAGE_ID_LIST:
        case RSVP_OBJ_RECOVERY_LABEL:
        case RSVP_OBJ_UPSTREAM_LABEL:
        case RSVP_OBJ_DETOUR:
        case RSVP_OBJ_SUGGESTED_LABEL:
        case RSVP_OBJ_PROPERTIES:
        case RSVP_OBJ_FASTREROUTE:
        case RSVP_OBJ_SESSION_ATTTRIBUTE:
        default:
            if (vflag <= 1)
                print_unknown_data(obj_tptr,"\n\t    ",obj_tlen);
            break;
        }
        /* do we want to see an additionally hexdump ? */
        if (vflag > 1 || hexdump==TRUE)
            print_unknown_data(tptr+sizeof(sizeof(struct rsvp_object_header)),"\n\t    ",
                               rsvp_obj_len-sizeof(struct rsvp_object_header));

        tptr+=rsvp_obj_len;
        tlen-=rsvp_obj_len;
    }
    return;
trunc:
    printf("\n\t\t packet exceeded snapshot");
}

