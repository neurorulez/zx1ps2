winavr\bin\avr-gcc -g -DF_CPU=16000000  -Wall -Os -mmcu=atmega328p    -c -o main.o main.c
pause
winavr\bin\avr-gcc -g -DF_CPU=16000000  -Wall -Os -mmcu=atmega328p  -Wl,-Map,zxunops2.map -o zxunops2.elf main.o
pause
winavr\bin\avr-objdump -h -S zxunops2.elf > zxunops2.lst
pause
winavr\bin\avr-objcopy -j .text -j .data -O ihex zxunops2.elf zxunops2.hex