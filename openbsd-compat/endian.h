#ifndef _OPENBSD_COMPAT_ENDIAN_H_
#define _OPENBSD_COMPAT_ENDIAN_H_

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe16 OSSwapHostToBigInt16
#define htobe32 OSSwapHostToBigInt32
#define htobe64 OSSwapHostToBigInt64

#define be16toh OSSwapBigToHostInt16
#define be32toh OSSwapBigToHostInt32
#define be64toh OSSwapBigToHostInt64
#else
#include <sys/endian.h>
#endif

#endif
