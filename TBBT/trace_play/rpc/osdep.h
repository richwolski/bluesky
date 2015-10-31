/*
 * @(#)osdep.h     2.1     97/10/23
 */
/*      @(#)types.h 1.18 87/07/24 SMI      */
/*
 *   Copyright (c) 1992-1997,2001 by Standard Performance Evaluation Corporation
 *	All rights reserved.
 *		Standard Performance Evaluation Corporation (SPEC)
 *		6585 Merchant Place, Suite 100
 *		Warrenton, VA 20187
 *
 *	This product contains benchmarks acquired from several sources who
 *	understand and agree with SPEC's goal of creating fair and objective
 *	benchmarks to measure computer performance.
 *
 *	This copyright notice is placed here only to protect SPEC in the
 *	event the source is misused in any manner that is contrary to the
 *	spirit, the goals and the intent of SPEC.
 *
 *	The source code is provided to the user or company under the license
 *	agreement for the SPEC Benchmark Suite for this product.
 */

#ifndef __RPC_OSDEP_H__
#define __RPC_OSDEP_H__

/*
 * OS dependancies
 *
 * These are non-XPG4.2 standard include files, if not compiling in a
 * strict environment simply #include them, otherwise we must define
 * our own missing pieces.  The definitions below are specific to
 * Solaris 2.X and may be different on other systems.
 */

#if !defined(_XOPEN_SOURCE) || defined (OSF1) || defined(AIX)
#if defined(SVR4)
#define BSD_COMP
#endif
#if defined(AIX)
#include <sys/param.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#else
#if defined(_BIG_ENDIAN) && !defined(ntohl) && !defined(lint)
/* big-endian */
#define ntohl(x)        (x)
#define ntohs(x)        (x)
#define htonl(x)        (x)
#define htons(x)        (x)
 
#elif !defined(ntohl) /* little-endian */
 
extern unsigned short ntohs(unsigned short ns);
extern unsigned short htons(unsigned short hs);
extern unsigned long  ntohl(unsigned long nl);
extern unsigned long  htonl(unsigned long hl);
 
#endif
/*
 * Internet address
 *	This definition contains obsolete fields for compatibility
 *	with SunOS 3.x and 4.2bsd.  The presence of subnets renders
 *	divisions into fixed fields misleading at best.  New code
 *	should use only the s_addr field.
 */
struct in_addr {
	union {
		struct { unsigned char s_b1, s_b2, s_b3, s_b4; } S_un_b;
		struct { unsigned short s_w1, s_w2; } S_un_w;
		unsigned long S_addr;
	} S_un;
#define s_addr  S_un.S_addr             /* should be used for all code */
#define s_host  S_un.S_un_b.s_b2        /* OBSOLETE: host on imp */
#define s_net   S_un.S_un_b.s_b1        /* OBSOLETE: network */
#define s_imp   S_un.S_un_w.s_w2        /* OBSOLETE: imp */
#define s_impno S_un.S_un_b.s_b4        /* OBSOLETE: imp # */
#define s_lh    S_un.S_un_b.s_b3        /* OBSOLETE: logical host */
};

struct sockaddr_in {
        short   sin_family;
        unsigned short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
};

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	uint16_t sa_family;		/* address family */
	char	sa_data[14];		/* up to 14 bytes of direct address */
};

/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
#define	IFNAMSIZ	16
	char	ifr_name[IFNAMSIZ];             /* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		char	ifru_oname[IFNAMSIZ];	/* other if name */
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		char	ifru_data[1];		/* interface dependent data */
		char	ifru_enaddr[6];
		int	if_muxid[2];		/* mux id's for arp and ip */

		/* Struct for FDDI ioctl's */
		struct ifr_dnld_reqs {
			void	*v_addr;
			void	*m_addr;
			void	*ex_addr;
			uint_t	size;
		} ifru_dnld_req;

		/* Struct for FDDI stats */
		struct ifr_fddi_stats {
			uint_t	stat_size;
			void	*fddi_stats;
		} ifru_fddi_stat;

		struct ifr_netmapents {
			uint_t	map_ent_size,	/* size of netmap structure */
				entry_number;	/* index into netmap list */
			void	*fddi_map_ent;	/* pointer to user structure */
		} ifru_netmapent;

		/* Field for generic ioctl for fddi */

		struct ifr_fddi_gen_struct {
			int	ifru_fddi_gioctl; /* field for gen ioctl */
			void	*ifru_fddi_gaddr; /* Generic ptr to a field */
		} ifru_fddi_gstruct;

	} ifr_ifru;

#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_oname	ifr_ifru.ifru_oname	/* other if name */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr /* broadcast address */
#define	ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define	ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
#define	ifr_enaddr	ifr_ifru.ifru_enaddr	/* ethernet address */

/* FDDI specific */
#define	ifr_dnld_req	ifr_ifru.ifru_dnld_req
#define	ifr_fddi_stat	ifr_ifru.ifru_fddi_stat
#define	ifr_fddi_netmap	ifr_ifru.ifru_netmapent /* FDDI network map entries */
#define	ifr_fddi_gstruct ifr_ifru.ifru_fddi_gstruct

#define	ifr_ip_muxid	ifr_ifru.if_muxid[0]
#define	ifr_arp_muxid	ifr_ifru.if_muxid[1]
};

struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		void	*ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req ifc_ifcu.ifcu_req	/* array of structures returned */
};

#define	AF_INET			2
#define	IPPROTO_TCP		6
#define	IPPROTO_UDP		17
#define	IPPORT_RESERVED		1024
#define	SOCK_DGRAM		1
#define	SOCK_STREAM		2
#define	SO_SNDBUF		0x1001
#define	SO_RCVBUF		0x1002
#define	SOL_SOCKET		0xffff
#define	INADDR_ANY		(uint32_t)0x00000000
#define	IFF_BROADCAST		0x2
#define	IFF_UP			0x1

#define	IOCPARM_MASK	0xff
#define	IOC_OUT		0x40000000
#define	IOC_IN		0x80000000
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
#define	_IOWR(x, y, t) \
	(IOC_INOUT|((((int)sizeof (t))&IOCPARM_MASK)<<16)|(x<<8)|y)
#define	SIOCGIFCONF		_IOWR('i', 20, struct ifconf)
#define	SIOCGIFFLAGS	_IOWR('i', 17, struct ifreq)
#define	SIOCGIFBRDADDR	_IOWR('i', 23, struct ifreq)

extern int accept(int, struct sockaddr *, int *);
extern int bind(int, struct sockaddr *, int);
extern int connect(int, struct sockaddr *, int);
extern int getsockname(int, struct sockaddr *, int *);
extern int getsockopt(int, int, int, char *, int *);
extern int listen(int, int);
extern int recvfrom(int, char *, int, int, struct sockaddr *, int *);
extern int sendto(int, const char *, int, int, const struct sockaddr *, int);
extern int setsockopt(int, int, int, const char *, int);
extern int socket(int, int, int);
extern unsigned long inet_netof(struct in_addr);
extern struct in_addr inet_makeaddr(int, int);

#endif /* _XOPEN_SOURCE */

#endif /* __RPC_OSDEP_H__ */
