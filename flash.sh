#!/usr/bin/env bash
# Smart Desk Coach — build + flash for Nucleo-G474RE
set -e

# Generate audio.c: embed raw PCM clips as C arrays using xxd.
# xxd -i produces 'unsigned char name[] = {...}; unsigned int name_len = N;'
# sed renames the symbols to match the extern declarations in audio.h.
xxd -i audio_files/output_1.bin \
  | sed 's/unsigned char/const uint8_t/g;
         s/unsigned int/const uint32_t/g;
         s/audio_files_output_1_bin/audio_1/g' > audio.c

xxd -i audio_files/output_2.bin \
  | sed 's/unsigned char/const uint8_t/g;
         s/unsigned int/const uint32_t/g;
         s/audio_files_output_2_bin/audio_2/g' >> audio.c

xxd -i audio_files/output_3.bin \
  | sed 's/unsigned char/const uint8_t/g;
         s/unsigned int/const uint32_t/g;
         s/audio_files_output_3_bin/audio_3/g' >> audio.c

arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -Wall -ffreestanding -nostdlib \
  -T link.ld -o deskcoach.elf startup.c main.c audio.c
arm-none-eabi-objcopy -O binary deskcoach.elf deskcoach.bin
arm-none-eabi-size deskcoach.elf
st-flash write deskcoach.bin 0x08000000
echo "Flashed. Open serial @115200 to see state:  screen \$(ls /dev/cu.usbmodem*) 115200"
