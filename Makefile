MCU = atmega328p
F_CPU = 1000000UL
PROGRAMMER = usbasp

TARGET = 2kOS
SRCS = src/main.c src/uart.c src/timer.c src/kernel.c src/cli.c src/helpers.c src/led_task.c src/idle_task.c

CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS = -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -Wall -I./include

all: build

build: $(TARGET).elf

$(TARGET).elf: $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET).elf $(SRCS)

hex: $(TARGET).hex

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET).elf $(TARGET).hex

flash: hex
	avrdude -c $(PROGRAMMER) -p $(MCU) -U flash:w:$(TARGET).hex:i

clean:
	rm -f *.elf *.hex
