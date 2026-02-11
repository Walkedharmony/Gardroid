#include "compress.h"
#include <string.h>
#include <stdlib.h>


void psb_pixel_uncompress(const unsigned char* pInput, unsigned char* pOutput, uint32_t actualSize, uint32_t align)
{
    int i;
    int count;
    uint32_t totalBytes = 0;
    int cmdByte = 0;

    while (actualSize != totalBytes)
    {
        cmdByte = *pInput++; totalBytes++;

        if (cmdByte & PSB_LZSS_LOOKAHEAD)
        {
            count = (cmdByte ^ PSB_LZSS_LOOKAHEAD) + 3;

            for (i = 0; i < count; i++)
            {
                memcpy(pOutput, pInput, align);
                pOutput += align;
            }

            pInput += align; totalBytes += align;
        }
        else {
            count = (cmdByte + 1) * align;

            for (i = 0; i < count; i++) {
                *pOutput++ = *pInput++;
            }

            totalBytes += count;
        }
    }
}

