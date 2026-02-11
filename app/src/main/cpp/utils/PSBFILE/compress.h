#ifndef _COMPRESS_H_
#define _COMPRESS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#define PSB_LZSS_LOOKSHIFT		7
#define PSB_LZSS_LOOKAHEAD		( 1 << PSB_LZSS_LOOKSHIFT )

void psb_pixel_uncompress(const unsigned char* pInput, unsigned char* pOutput, uint32_t actualSize, uint32_t align);


#ifdef __cplusplus
};
#endif

#endif