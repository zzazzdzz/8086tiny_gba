clean:
	rm *.bin
	rm 8086_abs.txt

gba:
	arm-none-eabi-gcc -c main.c -mthumb-interwork -mno-unaligned-access -DSTANDALONE -mthumb -O2 -o main.o
	arm-none-eabi-gcc main.o -mthumb-interwork -mthumb -specs=gba.specs -o main.elf
	arm-none-eabi-objcopy -O binary main.elf main.gba
	gbafix main.gba

sav:
	arm-none-eabi-gcc -c main.c -mthumb-interwork -mno-unaligned-access -mthumb -Os -o main.o
	arm-none-eabi-gcc main.o -mthumb-interwork -mthumb -specs=gba_emer.specs -o main.elf
	arm-none-eabi-objcopy --dump-section .init=8086_init.bin main.elf
	arm-none-eabi-objcopy --dump-section .text=8086_text.bin main.elf
	arm-none-eabi-objcopy --dump-section .fini=8086_fini.bin main.elf
	arm-none-eabi-objcopy --dump-section .rodata=8086_rodata.bin main.elf
	arm-none-eabi-objcopy --dump-section .eh_frame=8086_eh_frame.bin main.elf
	arm-none-eabi-objcopy --dump-section .data=8086_data.bin main.elf
	arm-none-eabi-objcopy --dump-section .init_array=8086_init_array.bin main.elf
	arm-none-eabi-objcopy --dump-section .fini_array=8086_fini_array.bin main.elf
	objdump -t main.elf > 8086_abs.txt
	python3 make_8086_image.py
	arm-none-eabi-as -march=armv4t stub.s -o stub.o
	arm-none-eabi-objcopy -O binary stub.o stub.dmp
	python3 save_construct.py
