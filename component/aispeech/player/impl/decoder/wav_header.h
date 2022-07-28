#ifndef WAV_HEADER_H
#define WAV_HEADER_H
#ifdef __cpluspplus
extern "C" {
#endif

#include <stdint.h>

//https://www.recordingblogs.com/wiki/wave-file-format

struct riff_chunk {
    char id[4];   //The "RIFF" chunk descriptor
    uint32_t size;
    char format[4];     //"WAVE"
};

struct wave_format {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t samplerate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

//fmt chunk id是“fmt ”,注意后面有个空格
//data chunk id是"data"

struct wave_chunk {
    char id[4];
    uint32_t size;
};

#ifdef __cpluspplus
}
#endif
#endif
