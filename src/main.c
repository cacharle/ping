#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

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

uint16_t
checksum(uint16_t *data, size_t size)
{
    uint32_t sum;

    size /= 2;
    sum = 0;
    while (size-- > 0)
        sum += data[size];
    return ~((sum << 16 >> 16) + (sum >> 16));
}

int
main()
{
    int sock;

    // sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    sock = socket(AF_INET, SOCK_RAW, 1);
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

    struct in_addr src_addr;
    struct in_addr dst_addr;
    inet_pton(AF_INET, "127.0.0.1", &src_addr);
    inet_pton(AF_INET, "127.0.0.1", &dst_addr);

    struct iphdr ip_header = {
        .ihl = 5,  // 20 byte = 160 bit; 160 / 32 = 5
        .version = 4,
        .tos = 0,
        .tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr),
        .id = 0,
        .frag_off = 0,
        .ttl = DEFAULT_TTL,
        .protocol = IPPROTO_ICMP,
        .check = 0,
        .saddr = src_addr.s_addr,
        .daddr = dst_addr.s_addr,
    };
    ip_header.check = checksum((uint16_t *)&ip_header, sizeof(ip_header));

    struct icmphdr icmp_header = {
        .type = ICMP_ECHO,
        .code = 0,
        .checksum = 0,
        .un.echo.id = 0,
        .un.echo.sequence = 1,
    };
    icmp_header.checksum = checksum((uint16_t *)&icmp_header, sizeof(icmp_header));

    uint8_t msg_buf[sizeof(struct iphdr) + sizeof(struct icmphdr)];
    memcpy(msg_buf, &ip_header, sizeof(struct iphdr));
    memcpy(msg_buf + sizeof(struct iphdr), &icmp_header, sizeof(struct icmphdr));

    struct addrinfo *result;
    if (getaddrinfo("127.0.0.1", NULL, NULL, &result) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }
    for (struct addrinfo *r = result; r != NULL; r = r->ai_next)
    {
        char hostname[1084] = "";
        getnameinfo(r->ai_addr, r->ai_addrlen, hostname, 1084, NULL, 0, 0);
        printf("hostname: %s\n", hostname);
    }

    printf("sending\n");
    if (sendto(sock, msg_buf, sizeof(msg_buf), 0, result->ai_addr, result->ai_addrlen) == -1)
    {
        perror("send");
        close(sock);
        return 1;
    }


    memset(msg_buf, 0, sizeof(msg_buf));
    printf("receiving\n");
    if (recvfrom(sock, &msg_buf, sizeof(msg_buf), 0, result->ai_addr, &result->ai_addrlen) == -1)
    {
        perror("recv");
        close(sock);
        return 1;
    }
    for (int i = 0; i < sizeof(msg_buf); i++)
        printf("%x ", msg_buf[i]);
    printf("\n");


    close(sock);

    return 0;
}
