#!/usr/bin/env bash
# Smart Desk Coach — build + flash for Nucleo-G474RE
set -e
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -Wall -ffreestanding -nostdlib \
  -T link.ld -o deskcoach.elf startup.c main.c
arm-none-eabi-objcopy -O binary deskcoach.elf deskcoach.bin
arm-none-eabi-size deskcoach.elf
st-flash write deskcoach.bin 0x08000000
echo "Flashed. Open serial @115200 to see state:  screen \$(ls /dev/cu.usbmodem*) 115200"
