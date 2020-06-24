/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: charles <charles.cabergs@gmail.com>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2020/06/24 08:09:54 by charles           #+#    #+#             */
/*   Updated: 2020/06/24 14:59:29 by charles          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_ping.h"

#define DEFAULT_TTL 64

/*
** https://www.youtube.com/watch?v=ppU41c15Xho
**
** Add 16-bit chunks:
**  10110010
**  00100100
**  10110101
**  --------
** 110001011 -> (11 0001011)
**
** Add carried (after 16th bit) to first 16 bit:
**
**      11
** 0001011
** -------
** 0001110
**
** NOT the result
** ~0001110
**  1110001
*/

uint16_t	checksum(uint16_t *data, size_t size)
{
	uint32_t	sum;

	size /= 2;
	sum = 0;
	while (size-- > 0)
		sum += data[size];
	return ~((sum << 16 >> 16) + (sum >> 16));
}

#include <string.h>
int main()
{
	int				sock;

	/* sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); */
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock == -1)
	{
		perror("socket creation");
		return 1;
	}

	/* int a[1] = {1}; */
	/* if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, a, sizeof(int)) == -1) */
	/* { */
	/* 	perror("setsockopt"); */
	/* 	return 1; */
	/* } */


	struct in_addr	src_addr;
	struct in_addr	dst_addr;
	inet_pton(AF_INET, "127.0.0.1", &src_addr);
	inet_pton(AF_INET, "127.0.0.1", &dst_addr);

	struct iphdr	ip_header;
	ip_header.ihl = 5; // 20 byte = 160 bit; 160 / 32 = 5
	ip_header.version = 4;
	ip_header.tos = 0;
	ip_header.tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
	ip_header.id = 0;
	ip_header.frag_off = 0;
	ip_header.ttl = DEFAULT_TTL;
	ip_header.protocol = IPPROTO_ICMP;
	ip_header.check = 0;
	ip_header.saddr = src_addr.s_addr;
	ip_header.daddr = dst_addr.s_addr;
	ip_header.check = checksum((uint16_t*)&ip_header, sizeof(ip_header));


	struct icmphdr	icmp_header;
	icmp_header.type = ICMP_ECHO;
	icmp_header.code = 0;
	icmp_header.checksum = 0;
	icmp_header.un.echo.id = 0;
	icmp_header.un.echo.sequence = 1;
	icmp_header.checksum = checksum((uint16_t*)&icmp_header, sizeof(icmp_header));

	uint8_t	msg_buf[sizeof(struct iphdr) + sizeof(struct icmphdr)];
	memcpy(msg_buf, &ip_header, sizeof(struct iphdr));
	memcpy(msg_buf + sizeof(struct iphdr), &icmp_header, sizeof(struct icmphdr));

	struct addrinfo *res;

	if (getaddrinfo("127.0.0.1", "echo", NULL, &res) != 0)
	{
		perror("getaddrinfo");
		return 1;
	}

	/* for (; res != NULL; res = res->ai_next) { */
		/* char hostname[NI_MAXHOST]; */
		/* if (getnameinfo(res2->ai_addr, res2->ai_addrlen, hostname, NI_MAXHOST, NULL, 0, 0) != 0) */
		/* { */
		/* 	perror("getnameaddr"); */
			/* fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(error)); */
		/* 	continue; */
		/* } */
		/* printf("hostname: %s\n", hostname); */
		/* printf("protocol: %d\n", curr->ai_protocol); */
		if (sendto(sock, msg_buf, sizeof(msg_buf), 0, res->ai_addr, sizeof(struct sockaddr)) == -1)
		{
			perror("send");
			close(sock);
			return 1;
		}
	/* } */

	struct msghdr msg;
	if (recvmsg(sock, &msg, 0) == -1)
	{
		perror("recv");
		close(sock);
		return 1;
	}

	close(sock);

	return 0;
}
