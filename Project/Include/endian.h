#ifndef __ENDIAN_H
#define __ENDIAN_H

#include "stdint.h"
#define sw16(A) ((((u16)(A) & 0xff00) >> 8) | (((u16)(A) & 0x00ff) << 8))
#define sw32(A) ((((u32)(A) & 0xff000000) >> 24)\
               | (((u32)(A) & 0x00ff0000) >> 8)\
               | (((u32)(A) & 0x0000ff00) << 8)\
               | (((u32)(A) & 0x000000ff) << 24))
#define sw64(A) ((uint64_t)(\
				(((uint64_t)(A)& (uint64_t)0x00000000000000ffULL) << 56) | \
				(((uint64_t)(A)& (uint64_t)0x000000000000ff00ULL) << 40) | \
				(((uint64_t)(A)& (uint64_t)0x0000000000ff0000ULL) << 24) | \
				(((uint64_t)(A)& (uint64_t)0x00000000ff000000ULL) << 8) | \
				(((uint64_t)(A)& (uint64_t)0x000000ff00000000ULL) >> 8) | \
				(((uint64_t)(A)& (uint64_t)0x0000ff0000000000ULL) >> 24) | \
				(((uint64_t)(A)& (uint64_t)0x00ff000000000000ULL) >> 40) | \
				(((uint64_t)(A)& (uint64_t)0xff00000000000000ULL) >> 56) ))

#define u64 uint64_t
#define SW16(A)	sw16(((u16)A))	
#define SW32(A)	sw32(((u32)A))
#define SW64(A)	sw64(((u64)A))	

#endif
