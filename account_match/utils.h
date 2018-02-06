#ifndef _AM_UTILS_H
#define _AM_UTILS_H

#include <stdint.h>
#include <arpa/inet.h>

static inline uint32_t strtoip(const char *ipstr)
{
    struct in_addr in;
    inet_aton(ipstr, &in);

    return in.s_addr;
}

#endif

