#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
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

void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

uint16_t
checksum(uint16_t *data, size_t size)
{
    uint32_t sum = 0;
    size_t   data_len = size / 2;
    for (size_t i = 0; i < data_len; i++)
        sum += data[i];
    return ~((sum << 16 >> 16) + (sum >> 16));
}

struct in_addr
addrinfo_to_ip(const struct addrinfo *addrinfo)
{
    // from: https://stackoverflow.com/questions/20115295
    struct sockaddr_in *addr = (struct sockaddr_in *)addrinfo->ai_addr;
    return (struct in_addr)addr->sin_addr;
}

struct ping_packet
{
    struct iphdr   ip;
    struct icmphdr icmp;
};

static int                sock = -1;
static struct ping_packet send_packet;
static struct ping_packet recv_packet;
static struct addrinfo   *destination_addrinfo;
struct in_addr            dst_addr;
struct in_addr            src_addr;

void
ping(int signalnum)
{
    (void)signalnum;

    // printf("%x\n", sizeof(struct iphdr) + sizeof(struct icmphdr));
    send_packet.ip.ihl =
        5,  // Internet Header Length: 20 byte = 160 bit; 160 / 32 = 5
        send_packet.ip.version = 4, send_packet.ip.tos = 0,
    send_packet.ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr)),
    send_packet.ip.id = 0, send_packet.ip.frag_off = 0,
    send_packet.ip.ttl = DEFAULT_TTL, send_packet.ip.protocol = IPPROTO_ICMP,
    send_packet.ip.check = 0, send_packet.ip.saddr = src_addr.s_addr,
    send_packet.ip.daddr = dst_addr.s_addr,
    send_packet.ip.check =
        checksum((uint16_t *)&send_packet.ip, sizeof(send_packet.ip));

    send_packet.icmp.type = ICMP_ECHO, send_packet.icmp.code = 0,
    send_packet.icmp.checksum = 0, send_packet.icmp.un.echo.id = 0,
    send_packet.icmp.un.echo.sequence = 1,
    send_packet.icmp.checksum =
        checksum((uint16_t *)&send_packet.icmp, sizeof(send_packet.icmp));

    // uint8_t msg_buf[sizeof(struct iphdr) + sizeof(struct icmphdr)];
    // memcpy(msg_buf, &ip_header, sizeof(struct iphdr));
    // memcpy(msg_buf + sizeof(struct iphdr), &icmp_header, sizeof(struct icmphdr));
    // for (int i = 0; i < sizeof(ip_header); i++)
    //     printf("%02x ", msg_buf[i]);
    // printf(" | ");
    // for (int i = 0; i < sizeof(icmp_header); i++)
    //     printf("%02x ", msg_buf[sizeof(ip_header) + i]);
    // printf("\n");

    if (sendto(sock,
               &send_packet,
               sizeof(send_packet),
               0,
               destination_addrinfo->ai_addr,
               destination_addrinfo->ai_addrlen) == -1)
        die("");
    signal(SIGALRM, ping);
    alarm(1);
}

char *address = "google.com";

int
main()
{
    // gethostname();
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1)
        die("ping: socket creation failed: %s\n", strerror(errno));
    int on = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        die("ping: setsockopt failed: %s\n", strerror(errno));

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    if (getaddrinfo(address, NULL, &hints, &destination_addrinfo) != 0)
        die("ping: getaddrinfo failed: %s\n", strerror(errno));
    char destination_hostname[1048] = "";
    getnameinfo(destination_addrinfo->ai_addr,
                destination_addrinfo->ai_addrlen,
                destination_hostname,
                1048,
                NULL,
                0,
                0);
    // for (struct addrinfo *r = destination_addrinfo; r != NULL; r = r->ai_next)
    // {
    //     char hostname[1084] = "";
    //     getnameinfo(r->ai_addr, r->ai_addrlen, hostname, 1084, NULL, 0, 0);
    //     struct in_addr addr = addrinfo_to_ip(r);
    //     printf("hostname: %s (%s)\n", hostname, inet_ntoa(addr));
    // }

    dst_addr = addrinfo_to_ip(destination_addrinfo);
    inet_pton(AF_INET, "192.168.1.42", &src_addr);
    // inet_pton(AF_INET, address, &dst_addr);

    signal(SIGALRM, ping);
    ping(0);

    while (true)
    {
        if (recvfrom(sock,
                     &recv_packet,
                     sizeof(recv_packet),
                     0,
                     destination_addrinfo->ai_addr,
                     &destination_addrinfo->ai_addrlen) == -1)
            continue;
        printf("%zu bytes from %s (%s): icmp_seq=%d ttl=%d time=%.1f ms\n",
               sizeof(recv_packet),
               destination_hostname,
               inet_ntoa(dst_addr),
               1,
               1,
               1.1);
        // die("recv: %s", strerror(errno));
        // for (int i = 0; i < sizeof(recv_packet.ip); i++)
        //     printf("%02x ", ((uint8_t*)&recv_packet.ip)[i]);
        // printf(" | ");
        // for (int i = 0; i < sizeof(recv_packet.icmp); i++)
        //     printf("%02x ", ((uint8_t*)&recv_packet.ip)[sizeof(recv_packet.ip) +
        //     i]);
        // printf("\n");
    }

    // f {
    //     send();
    //     signal(sigalrm, f);
    //     alarm(interval_time);
    // }
    //
    // f()
    // while (1) {
    //     recv
    // }

    // memcpy(&ip_header, msg_buf, sizeof(ip_header));
    // memcpy(&icmp_header, msg_buf + sizeof(ip_header), sizeof(icmp_header));
    //
    // ip_header.tot_len = ntohs(ip_header.tot_len);
    // printf("ip_header.tot_len=%d\n", ip_header.tot_len);

    freeaddrinfo(destination_addrinfo);
    close(sock);
    return 0;
}
