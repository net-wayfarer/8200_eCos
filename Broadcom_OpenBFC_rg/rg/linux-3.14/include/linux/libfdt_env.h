#ifndef _LIBFDT_ENV_H
#define _LIBFDT_ENV_H

#include <linux/string.h>

#include <asm/byteorder.h>

#define fdt32_to_cpu(x) be32_to_cpu(x)
#define cpu_to_fdt32(x) cpu_to_be32(x)
#define fdt64_to_cpu(x) be64_to_cpu(x)
#define cpu_to_fdt64(x) cpu_to_be64(x)

typedef uint16_t __bitwise fdt16_t;
typedef uint32_t __bitwise fdt32_t;
typedef uint64_t __bitwise fdt64_t;

#endif /* _LIBFDT_ENV_H */
