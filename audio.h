#pragma once
/* audio.h — extern declarations for pre-recorded PCM clips.
 * audio.c is generated at build time by flash.sh using xxd -i.
 * Raw 8-bit unsigned PCM, 8000 Hz sample rate.
 */
#include <stdint.h>

extern const uint8_t  audio_1[];
extern const uint8_t  audio_2[];
extern const uint8_t  audio_3[];
extern const uint32_t audio_1_len;
extern const uint32_t audio_2_len;
extern const uint32_t audio_3_len;
