.thumb

Start:
	# source level breakpoint
	mov r11, r11
	# r0 = 04000000
	mov r0, #0x04
    lsl r0, r0, #24
	# r2 = 0
	mov r2, #0
	# disable ints (4000200 = 0 = r2)
	# notlikethis i couldn't care less
	mov r1, r0
	add r1, #0x80
	add r1, #0x80
	add r1, #0x80
	add r1, #0x80
	strh r2, [r1]
	# disable sound (4000084 = 0 = r2)
	mov r1, r0
	add r1, #0x84
	strh r2, [r1]
	# disable all DMA
	mov r1, r0
	add r1, #0xba
	strh r2, [r1]
	# notlikethis
	mov r1, r0
	add r1, #0xc6
	strh r2, [r1]
	mov r1, r0
	add r1, #0xd2
	strh r2, [r1]
	mov r1, r0
	add r1, #0xde
	strh r2, [r1]
    # set flash bank to 0
    # r5 = 0e000000
    mov r5, #0x0e
    lsl r5, r5, #24
    # r4 = 0e005555
    mov r4, #0x55
    lsl r4, r4, #8
    add r4, #0x55
    add r4, r5
    # r3 = 0e002aaa
    mov r3, #0x2a
    lsl r3, r3, #8
    add r3, #0xaa
    add r3, r5
    # magic sequence
    mov r0, #0xaa
    strb r0, [r4]
    mov r0, #0x55
    strb r0, [r3]
    mov r0, #0xb0
    strb r0, [r4]
    mov r0, #0
    strb r0, [r5]
    # r3 = 02000100
	mov r3, #0x02
    lsl r3, r3, #24
    add r3, #0x80
	add r3, #0x80
	mov r5, r3
	# r2 = 0e000000
    mov r2, #0x0e
    lsl r2, r2, #24
    # r1 = 00005000
    mov r1, #0x50
    lsl r1, r1, #8
    # copy code from save data to RAM
CopyToRAM:
    ldrb r0, [r2]
    strb r0, [r3]
    add r2, #1
    add r3, #1
    sub r1, #1
    bne CopyToRAM
	# jump there
    bx r5
