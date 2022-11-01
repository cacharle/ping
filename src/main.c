#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
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
#include <time.h>
#include <unistd.h>

struct ping_packet
{
    struct iphdr   ip;
    struct icmphdr icmp;
};

static int                sock = -1;
static struct ping_packet send_packet;
static struct ping_packet recv_packet;
static struct addrinfo   *destination_addrinfo;
static struct in_addr     destination_ip_addr;
static struct in_addr     source_ip_addr;
static uint16_t           current_sequence = 1;
static struct timespec    start_time;
static unsigned long      packet_count = -1;
static unsigned long      interval = 1;
static unsigned long      ttl = 64;
static double             min_time_ms = INFINITY;
static double             max_time_ms = -INFINITY;
static double             sum_time_ms = 0;
static size_t             recv_count = 0;

void
put_error(char *msg)
{
    fprintf(stderr, "ping: %s: %s\n", msg, strerror(errno));
}

void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

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

void
print_stat(int signalnum)
{
    (void)signalnum;
    // TODO: std on streaming data:
    // https://nestedsoftware.com/2018/03/27/calculating-standard-deviation-on-streaming-data-253l.23919.html
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
           min_time_ms,
           sum_time_ms / recv_count,
           max_time_ms,
           0.0);
}

void
print_stat_and_exit(int signalnum)
{
    print_stat(signalnum);
    exit(EXIT_SUCCESS);
}

void
ping(int signalnum)
{
    (void)signalnum;

    // Internet Header Length: 20 byte = 160 bit; 160 / 32 = 5
    send_packet.ip.ihl = 5;
    send_packet.ip.version = 4;
    send_packet.ip.tos = 0;
    send_packet.ip.tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
    send_packet.ip.id = 0;
    send_packet.ip.frag_off = 0;
    send_packet.ip.ttl = ttl;
    send_packet.ip.protocol = IPPROTO_ICMP;
    send_packet.ip.check = 0;
    send_packet.ip.saddr = source_ip_addr.s_addr;
    send_packet.ip.daddr = destination_ip_addr.s_addr;
    send_packet.ip.check =
        checksum((uint16_t *)&send_packet.ip, sizeof(send_packet.ip));

    send_packet.icmp.type = ICMP_ECHO;
    send_packet.icmp.code = 0;
    send_packet.icmp.checksum = 0;
    send_packet.icmp.un.echo.id = 0;
    send_packet.icmp.un.echo.sequence = htons(current_sequence);
    send_packet.icmp.checksum =
        checksum((uint16_t *)&send_packet.icmp, sizeof(send_packet.icmp));

    if (sendto(sock,
               &send_packet,
               sizeof(send_packet),
               0,
               destination_addrinfo->ai_addr,
               destination_addrinfo->ai_addrlen) == -1)
        die("sendto");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    current_sequence++;
    if (packet_count != -1)
        packet_count--;
    if (packet_count == 0)
        return;
    signal(SIGALRM, ping);
    alarm(interval);
}

char *address = "google.com";

unsigned long
parse_arg_ulong(char *arg)
{
    char *end;
    errno = 0;
    unsigned long ret = strtoul(optarg, &end, 10);
    if (ret == -1UL)
        errno = ERANGE;
    if (errno != 0)
        die("ping: invalid argument: '%s': %s\n", arg, strerror(errno));
    if (*end != '\0')
        die("ping: invalid argument: '%s'", arg);
    return ret;
}

int
main(int argc, char *argv[])
{
    int option;
    while ((option = getopt(argc, argv, "c:s:i:At:I:W:w:qp:h")) != -1)
    {
        switch (option)
        {
        case 'c':
            packet_count = parse_arg_ulong(optarg);
            if (packet_count == 0)
                die("ping: invalid argument: '%s': %s\n", optarg, strerror(ERANGE));
            break;
        case 's':
            break;
        case 'i':
            interval = parse_arg_ulong(optarg);
            if (interval > UINT_MAX)
                die("ping: invalid argument: '%s': %s\n", optarg, strerror(ERANGE));
            break;
        case 'A':
            break;
        case 't':
            ttl = parse_arg_ulong(optarg);
            if (ttl > UINT8_MAX)
                die("ping: invalid argument: '%s': %s\n", optarg, strerror(ERANGE));
            break;
        case 'I':
            break;
        case 'W':
            break;
        case 'w':
            break;
        case 'q':
            break;
        case 'p':
            break;
        case 'h':
            break;
        }
    }
    if (optind != argc - 1)
        die("ping: usage error: Destination address required\n");
    char *destination_address = argv[optind];

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
    int err = getaddrinfo(destination_address, NULL, &hints, &destination_addrinfo);
    if (err != 0)
        die("ping: %s: %s\n", destination_address, gai_strerror(err));
    char destination_hostname[1048] = "";
    getnameinfo(destination_addrinfo->ai_addr,
                destination_addrinfo->ai_addrlen,
                destination_hostname,
                1048,
                NULL,
                0,
                0);

    destination_ip_addr = addrinfo_to_ip(destination_addrinfo);
    inet_pton(AF_INET, "192.168.1.42", &source_ip_addr);

    signal(SIGQUIT, print_stat);
    signal(SIGINT, print_stat_and_exit);
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
        {
            if (errno != EINTR)  // Not interupted by alarm signal
                put_error("recvfrom");
            continue;
        }
        struct timespec end_time;
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        end_time.tv_sec -= start_time.tv_sec;
        end_time.tv_nsec -= start_time.tv_nsec;
        float time_ms =
            (float)end_time.tv_sec * 1000 + (float)end_time.tv_nsec / 1000000;
        printf("%zu bytes from %s (%s): icmp_seq=%d ttl=%d time=%.1f ms\n",
               sizeof(recv_packet),
               destination_hostname,
               inet_ntoa(destination_ip_addr),
               ntohs(recv_packet.icmp.un.echo.sequence),
               recv_packet.ip.ttl,
               time_ms);
        recv_count++;
        if (time_ms > max_time_ms)
            max_time_ms = time_ms;
        if (time_ms < min_time_ms)
            min_time_ms = time_ms;
        sum_time_ms += time_ms;
        if (packet_count == 0)
            break;
    }
    print_stat(0);

    freeaddrinfo(destination_addrinfo);
    close(sock);
    return 0;
}
