// 8086tiny: a tiny, highly functional, highly portable PC emulator/VM
// Copyright 2013-14, Adrian Cable (adrian.cable@gmail.com) - http://www.megalith.co.uk/8086tiny

typedef unsigned short unsigned_short;
typedef unsigned char unsigned_char;

void putchr(const char c);
void writestr(const char *str);
void writenum(int n);
unsigned char get_disk(unsigned fd, unsigned offs);
void init_ram();
void init_disk();
char inp_kbhit();
char inp_getch();
void inp_poll();
void tim_draw_status(int ctr);

// Emulator system constants
#define IO_PORT_COUNT 0x1000
#define RAM_SIZE 0x10FFF0
#define REGS_BASE 0xF0000

// Graphics/timer/keyboard update delays (explained later)
#ifndef GRAPHICS_UPDATE_DELAY
#define GRAPHICS_UPDATE_DELAY 360000
#endif
#define KEYBOARD_TIMER_UPDATE_DELAY 20000

// 16-bit register decodes
#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7

#define REG_ES 8
#define REG_CS 9
#define REG_SS 10
#define REG_DS 11

#define REG_ZERO 12
#define REG_SCRATCH 13

// 8-bit register decodes
#define REG_AL 0
#define REG_AH 1
#define REG_CL 2
#define REG_CH 3
#define REG_DL 4
#define REG_DH 5
#define REG_BL 6
#define REG_BH 7

// FLAGS register decodes
#define FLAG_CF 40
#define FLAG_PF 41
#define FLAG_AF 42
#define FLAG_ZF 43
#define FLAG_SF 44
#define FLAG_TF 45
#define FLAG_IF 46
#define FLAG_DF 47
#define FLAG_OF 48

// Lookup tables in the BIOS binary
#define TABLE_XLAT_OPCODE 8
#define TABLE_XLAT_SUBFUNCTION 9
#define TABLE_STD_FLAGS 10
#define TABLE_PARITY_FLAG 11
#define TABLE_BASE_INST_SIZE 12
#define TABLE_I_W_SIZE 13
#define TABLE_I_MOD_SIZE 14
#define TABLE_COND_JUMP_DECODE_A 15
#define TABLE_COND_JUMP_DECODE_B 16
#define TABLE_COND_JUMP_DECODE_C 17
#define TABLE_COND_JUMP_DECODE_D 18
#define TABLE_FLAGS_BITFIELDS 19

// Bitfields for TABLE_STD_FLAGS values
#define FLAGS_UPDATE_SZP 1
#define FLAGS_UPDATE_AO_ARITH 2
#define FLAGS_UPDATE_OC_LOGIC 4

// Helper macros

// Decode mod, r_m and reg fields in instruction
#define DECODE_RM_REG scratch2_uint = 4 * !i_mod, \
					  op_to_addr = rm_addr = i_mod < 3 ? SEGREG(seg_override_en ? seg_override : bios_table_lookup(scratch2_uint + 3,i_rm), bios_table_lookup(scratch2_uint,i_rm), regs16[bios_table_lookup(scratch2_uint + 1,i_rm)] + bios_table_lookup(scratch2_uint + 2,i_rm) * i_data1+) : GET_REG_ADDR(i_rm), \
					  op_from_addr = GET_REG_ADDR(i_reg), \
					  i_d && (scratch_uint = op_from_addr, op_from_addr = rm_addr, op_to_addr = scratch_uint)

// Return memory-mapped register location (offset into mem array) for register #reg_id
#define GET_REG_ADDR(reg_id) (REGS_BASE + (i_w ? 2 * reg_id : 2 * reg_id + reg_id / 4 & 7))

// Returns number of top bit in operand (i.e. 8 for 8-bit operands, 16 for 16-bit operands)
#define TOP_BIT 8*(i_w + 1)

// Opcode execution unit helpers
#define OPCODE ;break; case
#define OPCODE_CHAIN ; case

// [I]MUL/[I]DIV/DAA/DAS/ADC/SBB helpers
#define MUL_MACRO(op_data_type,out_regs) (set_opcode(0x10), \
										  out_regs[i_w + 1] = (op_result = FANCY_CAST(op_data_type,*get_ram(rm_addr)) * (op_data_type)*out_regs) >> 16, \
										  regs16[REG_AX] = op_result, \
										  set_OF(set_CF(op_result - (op_data_type)op_result)))
#define DIV_MACRO(out_data_type,in_data_type,out_regs) (scratch_int = FANCY_CAST(out_data_type,*get_ram(rm_addr))) && !(scratch2_uint = (in_data_type)(scratch_uint = (out_regs[i_w+1] << 16) + regs16[REG_AX]) / scratch_int, scratch2_uint - (out_data_type)scratch2_uint) ? out_regs[i_w+1] = scratch_uint - scratch_int * (*out_regs = scratch2_uint) : pc_interrupt(0)
#define DAA_DAS(op1,op2,mask,min) set_AF((((scratch2_uint = regs8[REG_AL]) & 0x0F) > 9) || regs8[FLAG_AF]) && (op_result = regs8[REG_AL] op1 6, set_CF(regs8[FLAG_CF] || (regs8[REG_AL] op2 scratch2_uint))), \
								  set_CF((((mask & 1 ? scratch2_uint : regs8[REG_AL]) & mask) > min) || regs8[FLAG_CF]) && (op_result = regs8[REG_AL] op1 0x60)
#define ADC_SBB_MACRO(a) OP(a##= regs8[FLAG_CF] +), \
						 set_CF(regs8[FLAG_CF] && (op_result == op_dest) || (a op_result < a(int)op_dest)), \
						 set_AF_OF_arith()

// Execute arithmetic/logic operations in emulator memory/registers
#define R_M_OP(dest,op,src) (i_w ? (op_dest = FANCY_CAST(unsigned_short,dest)), (op_source = FANCY_CAST(unsigned_short,src)), (op_result = FANCY_CAST(unsigned_short,dest) op op_source) \
								 : ((op_dest = dest), (op_result = dest op (op_source = FANCY_CAST(unsigned_char,src)))))
#define R_M_OP_PUSHEDOUT(dest,op,src) (i_w ? op_dest = FANCY_CAST(unsigned_short,dest), op_source = CAST(unsigned_short)src, op_result = FANCY_CAST(unsigned_short,dest) op op_source \
								           : (op_dest = dest, op_result = dest op (op_source = CAST(unsigned_char)src)))

#define MEM_OP(dest,op,src) R_M_OP((*get_ram(dest)),op,(*get_ram(src)))
#define OP(op) MEM_OP(op_to_addr,op,op_from_addr)

// Increment or decrement a register #reg_id (usually SI or DI), depending on direction flag and operand size (given by i_w)
#define INDEX_INC(reg_id) (regs16[reg_id] -= (2 * regs8[FLAG_DF] - 1)*(i_w + 1))

// Helpers for stack operations
#define R_M_PUSH(a) (i_w = 1, R_M_OP((*get_ram(SEGREG(REG_SS, REG_SP, --))), =, a))
#define R_M_PUSH_PUSHEDOUT(a) (i_w = 1, R_M_OP_PUSHEDOUT((*get_ram(SEGREG(REG_SS, REG_SP, --))), =, a))
#define R_M_POP(a) ((i_w = 1), (regs16[REG_SP] += 2), (R_M_OP(a, =, *get_ram(SEGREG(REG_SS, REG_SP, -2+)))))

// Convert segment:offset to linear address in emulator memory space
#define SEGREG(reg_seg,reg_ofs,op) 16 * regs16[reg_seg] + (unsigned_short)(op regs16[reg_ofs])

// Returns sign bit of an 8-bit or 16-bit operand
#define SIGN_OF_PUSHEDOUT(a) (1 & (i_w ? CAST(short)a : a) >> (TOP_BIT - 1))
#define SIGN_OF(a) (1 & (i_w ? FANCY_CAST(short,a) : a) >> (TOP_BIT - 1))

// Reinterpretation cast
#define CAST(a) *(a*)&

// fancier reinterpretation cast which preserves memory alignment
typedef struct {
    union {
		unsigned _unsigned;
        unsigned_short _unsigned_short;
        short _short;
        unsigned_char _unsigned_char;
        char _char;
    };
} __attribute__((packed,aligned(1))) memory_t;
#define FANCY_CAST(a,x) ((memory_t*)(&(x)))->_##a

// Keyboard driver for console. This may need changing for UNIX/non-UNIX platforms
#define KEYBOARD_DRIVER inp_kbhit() && ((*get_ram(0x4A6)) = inp_getch(), pc_interrupt(7))
#define SDL_KEYBOARD_DRIVER KEYBOARD_DRIVER

#ifdef GBA
#define iwram ((unsigned char*)0x03000000)
#define workram ((unsigned char*)0x02005000)
#else
unsigned_char iwram[0x7fff];
unsigned_char workram[256*1024];
#endif

// Global variable definitions
unsigned_char *regs8, i_rm, i_w, i_reg, i_mod, i_mod_size, i_d, i_reg4bit, raw_opcode_id, xlat_opcode_id, extra, rep_mode, seg_override_en, rep_override_en, trap_flag, int8_asap, scratch_uchar, io_hi_lo, *vid_mem_base, spkr_en;
unsigned_short *regs16, reg_ip, seg_override, file_index, wave_counter;
unsigned int op_source, op_dest, rm_addr, op_to_addr, op_from_addr, i_data0, i_data1, i_data2, scratch_uint, scratch2_uint, inst_counter, set_flags_type, GRAPHICS_X, GRAPHICS_Y, vmem_ctr;
int op_result, scratch_int;
unsigned_char highest_page = 0;

#define NUM_TBS 1100
#define REG_AND_BIOS_SZ 0x2100

unsigned_char *io_ports = (unsigned_char*)(iwram + 0x7e00 - IO_PORT_COUNT);
unsigned_char *memory_translation = (unsigned_char*)(iwram + 0x7e00 - IO_PORT_COUNT - NUM_TBS);
unsigned_char *reg_and_bios = (unsigned_char*)(iwram + 0x7e00 - IO_PORT_COUNT - NUM_TBS - REG_AND_BIOS_SZ);

unsigned_char bios_table_lookup(int i, int j) {
	return regs8[regs16[0x81 + i] + j];
}

void swap_pages(int p1, int p2) {
    if (p1 == p2) return;
	int sz = 1024;
	while (sz--) {
		unsigned_char temp;
		temp = workram[p1 * 1024 + sz];
		workram[p1 * 1024 + sz] = workram[p2 * 1024 + sz];
		workram[p2 * 1024 + sz] = temp;
	}
}

int find_page_index(int x) {
    int i;
    for (i=0; i<NUM_TBS; i++) {
        if (memory_translation[i] == x) return i;
    }
    return 0;
}

void make_consistent() {
    // basically a selection sort of memory pages
    int idx1L;
    int autoincrement = 1;
    for (idx1L=0; idx1L<NUM_TBS; idx1L++) {
        if (memory_translation[idx1L] != 0) {
            int idx1R = memory_translation[idx1L];
            int idx2L = find_page_index(autoincrement);
            int idx2R = autoincrement;
            swap_pages(idx1R, idx2R);
            memory_translation[idx1L] = idx2R;
            memory_translation[idx2L] = idx1R;
            autoincrement++;
        }
    }
}

unsigned_char* get_ram(int addr) {
	if (addr >= REGS_BASE && addr < (REGS_BASE+REG_AND_BIOS_SZ)) {
		// regs and bios mem
		return reg_and_bios + addr - REGS_BASE;
	}

	int page_number = addr >> 10;
	int page_offset = addr & 0b1111111111;
	//printf("%i %i\n", page_number, page_offset);
	int found_page = 0;
	int i;

	if (page_offset == 1023) {
	    // we need to touch the neighboring page if we're at last byte
		volatile unsigned_char* r = get_ram(addr+1);
		make_consistent();
	}

	if (memory_translation[page_number] == 0) {
		// allocate a new page
		if (highest_page == 235) {
			writestr("\nmem pages exceeded, halting!");
			for(;;);
		}
		highest_page++;
		memory_translation[page_number] = highest_page;
		//writenum(highest_page);
		make_consistent();
	}

	return (workram + memory_translation[page_number] * 1024 + page_offset);
	//return &workram[addr];
}

// Helper functions

// Set carry flag
char set_CF(int new_CF)
{
	return regs8[FLAG_CF] = !!new_CF;
}

// Set auxiliary flag
char set_AF(int new_AF)
{
	return regs8[FLAG_AF] = !!new_AF;
}

// Set overflow flag
char set_OF(int new_OF)
{
	return regs8[FLAG_OF] = !!new_OF;
}

// Set auxiliary and overflow flag after arithmetic operations
char set_AF_OF_arith()
{
	set_AF((op_source ^= op_dest ^ op_result) & 0x10);
	if (op_result == op_dest)
		return set_OF(0);
	else
		return set_OF(1 & (regs8[FLAG_CF] ^ op_source >> (TOP_BIT - 1)));
}

// Assemble and return emulated CPU FLAGS register in scratch_uint
void make_flags()
{
	int i;
	scratch_uint = 0xF002; // 8086 has reserved and unused flags set to 1
	for (i = 9; i--;)
		scratch_uint += regs8[FLAG_CF + i] << bios_table_lookup(TABLE_FLAGS_BITFIELDS,i);
}

// Set emulated CPU FLAGS register from regs8[FLAG_xx] values
void set_flags(int new_flags)
{
	int i;
	for (i = 9; i--;)
		regs8[FLAG_CF + i] = !!(1 << bios_table_lookup(TABLE_FLAGS_BITFIELDS,i) & new_flags);
}

// Convert raw opcode to translated opcode index. This condenses a large number of different encodings of similar
// instructions into a much smaller number of distinct functions, which we then execute
void set_opcode(unsigned_char opcode)
{
	xlat_opcode_id = bios_table_lookup(TABLE_XLAT_OPCODE,raw_opcode_id = opcode);
	extra = bios_table_lookup(TABLE_XLAT_SUBFUNCTION,opcode);
	i_mod_size = bios_table_lookup(TABLE_I_MOD_SIZE,opcode);
	set_flags_type = bios_table_lookup(TABLE_STD_FLAGS,opcode);
}

// Execute INT #interrupt_num on the emulated machine
char pc_interrupt(unsigned_char interrupt_num)
{
	set_opcode(0xCD); // Decode like INT

	make_flags();
	R_M_PUSH(scratch_uint);
	R_M_PUSH(regs16[REG_CS]);
	R_M_PUSH(reg_ip);
	MEM_OP(REGS_BASE + 2 * REG_CS, =, 4 * interrupt_num + 2);
	R_M_OP(reg_ip, =, (*get_ram(4 * interrupt_num)));

	return regs8[FLAG_TF] = regs8[FLAG_IF] = 0;
}

// AAA and AAS instructions - which_operation is +1 for AAA, and -1 for AAS
int AAA_AAS(char which_operation)
{
	return (regs16[REG_AX] += 262 * which_operation*set_AF(set_CF(((regs8[REG_AL] & 0x0F) > 9) || regs8[FLAG_AF])), regs8[REG_AL] &= 0x0F);
}

void state_dump() {
	int i;
	for (i=0;i<14;i++) {
		writenum(regs16[i]);
	}
}

int defined_char_cast(int x) {
	int sneakyCast = x & 0x7f;
	if (x & 0x80) sneakyCast = -(256-(x & 0xff));
	return sneakyCast;
}

// Emulator entry point
int cyc;
void emu()
{
	init_ram();
	init_disk();
	tim_draw_status(0);
	
	// zero iwram
	int i, at;
	for (i=0;i<NUM_TBS;i++) memory_translation[i]=0;
	for (i=0;i<IO_PORT_COUNT;i++) io_ports[i]=0;
	
	writestr("starting\n");
	
	// regs16 and reg8 point to F000:0, the start of memory-mapped registers. CS is initialised to F000
	regs16 = (unsigned_short *)(regs8 = get_ram(REGS_BASE));
	regs16[REG_CS] = 0xF000;

	// Trap flag off
	regs8[FLAG_TF] = 0;

	regs8[REG_DL] = 0;
	regs16[REG_AX] = 0;

	reg_ip = 0x100;
	for (i=0; i<0x2000; i++) {
		*(regs8+0x100+i) = get_disk(0, i);
	}

	cyc = 0;

	// Instruction execution loop.
	for (; at = 16 * regs16[REG_CS] + reg_ip;)
	{
		// Set up variables to prepare for decoding an opcode
		set_opcode(*get_ram(at));

		// Extract i_w and i_d fields from instruction
		i_w = (i_reg4bit = raw_opcode_id & 7) & 1;
		i_d = i_reg4bit / 2 & 1;

		// Extract instruction data fields
		i_data0 = FANCY_CAST(short,*get_ram(at+1));
		i_data1 = FANCY_CAST(short,*get_ram(at+2));
		i_data2 = FANCY_CAST(short,*get_ram(at+3));

		// seg_override_en and rep_override_en contain number of instructions to hold segment override and REP prefix respectively
		if (seg_override_en)
			seg_override_en--;
		if (rep_override_en)
			rep_override_en--;

		// i_mod_size > 0 indicates that opcode uses i_mod/i_rm/i_reg, so decode them
		if (i_mod_size)
		{
			i_mod = (i_data0 & 0xFF) >> 6;
			i_rm = i_data0 & 7;
			i_reg = i_data0 / 8 & 7;

			if ((!i_mod && i_rm == 6) || (i_mod == 2))
				i_data2 = FANCY_CAST(short,*get_ram(at+4));
			else if (i_mod != 1)
				i_data2 = i_data1;
			else // If i_mod is 1, operand is (usually) 8 bits rather than 16 bits
				i_data1 = defined_char_cast(i_data1);

			DECODE_RM_REG;
		}

		cyc++;

		// Instruction execution unit
		switch (xlat_opcode_id)
		{
			OPCODE_CHAIN 0: // Conditional jump (JAE, JNAE, etc.)
				// i_w is the invert flag, e.g. i_w == 1 means JNAE, whereas i_w == 0 means JAE 
				scratch_uchar = raw_opcode_id / 2 & 7;
				int r1 = (regs8[bios_table_lookup(TABLE_COND_JUMP_DECODE_C,scratch_uchar)] ^ regs8[bios_table_lookup(TABLE_COND_JUMP_DECODE_D,scratch_uchar)]);
				int r2 = (regs8[bios_table_lookup(TABLE_COND_JUMP_DECODE_A,scratch_uchar)] || regs8[bios_table_lookup(TABLE_COND_JUMP_DECODE_B,scratch_uchar)] || r1);
				if ((i_w ^ r2)) reg_ip += defined_char_cast(i_data0);
			OPCODE 1: // MOV reg, imm
				i_w = !!(raw_opcode_id & 8);
				R_M_OP((*get_ram(GET_REG_ADDR(i_reg4bit))), =, i_data0)
			OPCODE 3: // PUSH regs16
				R_M_PUSH(regs16[i_reg4bit])
			OPCODE 4: // POP regs16
				R_M_POP(regs16[i_reg4bit])
			OPCODE 2: // INC|DEC regs16
				i_w = 1;
				i_d = 0;
				i_reg = i_reg4bit;
				DECODE_RM_REG;
				i_reg = extra
			OPCODE_CHAIN 5: // INC|DEC|JMP|CALL|PUSH
				if (i_reg < 2) // INC|DEC
					MEM_OP(op_from_addr, += 1 - 2 * i_reg +, REGS_BASE + 2 * REG_ZERO),
					op_source = 1,
					set_AF_OF_arith(),
					set_OF(op_dest + 1 - i_reg == 1 << (TOP_BIT - 1)),
					(xlat_opcode_id == 5) && (set_opcode(0x10), 0); // Decode like ADC
				else if (i_reg != 6) // JMP|CALL
					i_reg - 3 || R_M_PUSH(regs16[REG_CS]), // CALL (far)
					i_reg & 2 && R_M_PUSH_PUSHEDOUT(reg_ip + 2 + i_mod*(i_mod != 3) + 2*(!i_mod && i_rm == 6)), // CALL (near or far)
					i_reg & 1 && (regs16[REG_CS] = FANCY_CAST(short,*get_ram(op_from_addr + 2))), // JMP|CALL (far)
					R_M_OP(reg_ip, =, (*get_ram(op_from_addr))),
					set_opcode(0x9A); // Decode like CALL
				else // PUSH
					R_M_PUSH((*get_ram(rm_addr)))
			OPCODE 6: // TEST r/m, imm16 / NOT|NEG|MUL|IMUL|DIV|IDIV reg
				op_to_addr = op_from_addr;

				switch (i_reg)
				{
					OPCODE_CHAIN 0: // TEST
						set_opcode(0x20); // Decode like AND
						reg_ip += i_w + 1;
						R_M_OP((*get_ram(op_to_addr)), &, i_data2)
					OPCODE 2: // NOT
						OP(=~)
					OPCODE 3: // NEG
						OP(=-);
						op_dest = 0;
						set_opcode(0x28); // Decode like SUB
						set_CF(op_result > op_dest)
					OPCODE 4: // MUL
						i_w ? MUL_MACRO(unsigned_short, regs16) : MUL_MACRO(unsigned_char, regs8)
					OPCODE 5: // IMUL
						i_w ? MUL_MACRO(short, regs16) : MUL_MACRO(char, regs8)
					OPCODE 6: // DIV
						i_w ? DIV_MACRO(unsigned_short, unsigned, regs16) : DIV_MACRO(unsigned_char, unsigned_short, regs8)
					OPCODE 7: // IDIV
						i_w ? DIV_MACRO(short, int, regs16) : DIV_MACRO(char, short, regs8);
				}
			OPCODE 7: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP AL/AX, immed
				rm_addr = REGS_BASE;
				i_data2 = i_data0;
				i_mod = 3;
				i_reg = extra;
				reg_ip--;
			OPCODE_CHAIN 8: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP reg, immed
				op_to_addr = rm_addr;
				regs16[REG_SCRATCH] = (i_d |= !i_w) ? defined_char_cast(i_data2) : i_data2;
				op_from_addr = REGS_BASE + 2 * REG_SCRATCH;
				reg_ip += !i_d + 1;
				set_opcode(0x08 * (extra = i_reg));
			OPCODE_CHAIN 9: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP|MOV reg, r/m
				switch (extra)
				{
					OPCODE_CHAIN 0: // ADD
						OP(+=),
						set_CF(op_result < op_dest)
					OPCODE 1: // OR
						OP(|=)
					OPCODE 2: // ADC
						ADC_SBB_MACRO(+)
					OPCODE 3: // SBB
						ADC_SBB_MACRO(-)
					OPCODE 4: // AND
						OP(&=)
					OPCODE 5: // SUB
						OP(-=),
						set_CF(op_result > op_dest)
					OPCODE 6: // XOR
						OP(^=)
					OPCODE 7: // CMP
						OP(-),
						set_CF(op_result > op_dest)
					OPCODE 8: // MOV
						OP(=);
				}
			OPCODE 10: // MOV sreg, r/m | POP r/m | LEA reg, r/m
				if (!i_w) // MOV
					i_w = 1,
					i_reg += 8,
					DECODE_RM_REG,
					OP(=);
				else if (!i_d) // LEA
					seg_override_en = 1,
					seg_override = REG_ZERO,
					DECODE_RM_REG,
					R_M_OP((*get_ram(op_from_addr)), =, rm_addr);
				else // POP
					R_M_POP((*get_ram(rm_addr)))
			OPCODE 11: // MOV AL/AX, [loc]
				i_mod = i_reg = 0;
				i_rm = 6;
				i_data1 = i_data0;
				DECODE_RM_REG;
				MEM_OP(op_from_addr, =, op_to_addr)
			OPCODE 12: // ROL|ROR|RCL|RCR|SHL|SHR|???|SAR reg/mem, 1/CL/imm (80186)
				scratch2_uint = SIGN_OF((*get_ram(rm_addr))),
				scratch_uint = extra ? // xxx reg/mem, imm
					++reg_ip,
					defined_char_cast(i_data1)
				: // xxx reg/mem, CL
					i_d
						? 31 & regs8[REG_CL]
				: // xxx reg/mem, 1
					1;
				if (scratch_uint)
				{
					if (i_reg < 4) // Rotate operations
						scratch_uint %= i_reg / 2 + TOP_BIT,
						R_M_OP(scratch2_uint, =, (*get_ram(rm_addr)));
					if (i_reg & 1) // Rotate/shift right operations
						R_M_OP((*get_ram(rm_addr)), >>=, scratch_uint);
					else // Rotate/shift left operations
						R_M_OP((*get_ram(rm_addr)), <<=, scratch_uint);
					if (i_reg > 3) // Shift operations
						set_opcode(0x10); // Decode like ADC
					if (i_reg > 4) // SHR or SAR
						set_CF(op_dest >> (scratch_uint - 1) & 1);
				}

				switch (i_reg)
				{
					OPCODE_CHAIN 0: // ROL
						R_M_OP_PUSHEDOUT((*get_ram(rm_addr)), += , scratch2_uint >> (TOP_BIT - scratch_uint));
						set_OF(SIGN_OF(op_result) ^ set_CF(op_result & 1))
					OPCODE 1: // ROR
						scratch2_uint &= (1 << scratch_uint) - 1,
						R_M_OP_PUSHEDOUT((*get_ram(rm_addr)), += , scratch2_uint << (TOP_BIT - scratch_uint));
						set_OF(SIGN_OF_PUSHEDOUT(op_result * 2) ^ set_CF(SIGN_OF(op_result)))
					OPCODE 2: // RCL
						R_M_OP_PUSHEDOUT((*get_ram(rm_addr)), += (regs8[FLAG_CF] << (scratch_uint - 1)) + , scratch2_uint >> (1 + TOP_BIT - scratch_uint));
						set_OF(SIGN_OF(op_result) ^ set_CF(scratch2_uint & 1 << (TOP_BIT - scratch_uint)))
					OPCODE 3: // RCR
						R_M_OP_PUSHEDOUT((*get_ram(rm_addr)), += (regs8[FLAG_CF] << (TOP_BIT - scratch_uint)) + , scratch2_uint << (1 + TOP_BIT - scratch_uint));
						set_CF(scratch2_uint & 1 << (scratch_uint - 1));
						set_OF(SIGN_OF(op_result) ^ SIGN_OF_PUSHEDOUT(op_result * 2))
					OPCODE 4: // SHL
						set_OF(SIGN_OF(op_result) ^ set_CF(SIGN_OF_PUSHEDOUT(op_dest << (scratch_uint - 1))))
					OPCODE 5: // SHR
						set_OF(SIGN_OF(op_dest))
					OPCODE 7: // SAR
						scratch_uint < TOP_BIT || set_CF(scratch2_uint);
						set_OF(0);
						R_M_OP_PUSHEDOUT((*get_ram(rm_addr)), +=, scratch2_uint *= ~(((1 << TOP_BIT) - 1) >> scratch_uint));
				}
			OPCODE 13: // LOOPxx|JCZX
				scratch_uint = !!--regs16[REG_CX];

				switch(i_reg4bit)
				{
					OPCODE_CHAIN 0: // LOOPNZ
						scratch_uint &= !regs8[FLAG_ZF]
					OPCODE 1: // LOOPZ
						scratch_uint &= regs8[FLAG_ZF]
					OPCODE 3: // JCXXZ
						scratch_uint = !++regs16[REG_CX];
				}
				reg_ip += scratch_uint*defined_char_cast(i_data0)
			OPCODE 14: // JMP | CALL short/near
				reg_ip += 3 - i_d;
				if (!i_w)
				{
					if (i_d) // JMP far
						reg_ip = 0,
						regs16[REG_CS] = i_data2;
					else // CALL
						R_M_PUSH(reg_ip);
				}
				reg_ip += i_d && i_w ? defined_char_cast(i_data0) : i_data0
			OPCODE 15: // TEST reg, r/m
				MEM_OP(op_from_addr, &, op_to_addr)
			OPCODE 16: // XCHG AX, regs16
				i_w = 1;
				op_to_addr = REGS_BASE;
				op_from_addr = GET_REG_ADDR(i_reg4bit);
			OPCODE_CHAIN 24: // NOP|XCHG reg, r/m
				if (op_to_addr != op_from_addr)
					OP(^=),
					MEM_OP(op_from_addr, ^=, op_to_addr),
					OP(^=)
			OPCODE 17: // MOVSx (extra=0)|STOSx (extra=1)|LODSx (extra=2)
				scratch2_uint = seg_override_en ? seg_override : REG_DS;

				for (scratch_uint = rep_override_en ? regs16[REG_CX] : 1; scratch_uint; scratch_uint--)
				{
					MEM_OP(extra < 2 ? SEGREG(REG_ES, REG_DI,) : REGS_BASE, =, extra & 1 ? REGS_BASE : SEGREG(scratch2_uint, REG_SI,)),
					extra & 1 || INDEX_INC(REG_SI),
					extra & 2 || INDEX_INC(REG_DI);
				}

				if (rep_override_en)
					regs16[REG_CX] = 0
			OPCODE 18: // CMPSx (extra=0)|SCASx (extra=1)
				scratch2_uint = seg_override_en ? seg_override : REG_DS;

				if ((scratch_uint = rep_override_en ? regs16[REG_CX] : 1))
				{
					for (; scratch_uint; rep_override_en || scratch_uint--)
					{
						MEM_OP(extra ? REGS_BASE : SEGREG(scratch2_uint, REG_SI,), -, SEGREG(REG_ES, REG_DI,)),
						extra || INDEX_INC(REG_SI),
						INDEX_INC(REG_DI), rep_override_en && !(--regs16[REG_CX] && (!op_result == rep_mode)) && (scratch_uint = 0);
					}

					set_flags_type = FLAGS_UPDATE_SZP | FLAGS_UPDATE_AO_ARITH; // Funge to set SZP/AO flags
					set_CF(op_result > op_dest);
				}
			OPCODE 19: // RET|RETF|IRET
				i_d = i_w;
				R_M_POP(reg_ip);
				if (extra) // IRET|RETF|RETF imm16
					R_M_POP(regs16[REG_CS]);
				if (extra & 2) // IRET
					set_flags(R_M_POP(scratch_uint));
				else if (!i_d) // RET|RETF imm16
					regs16[REG_SP] += i_data0
			OPCODE 20: // MOV r/m, immed
				R_M_OP((*get_ram(op_from_addr)), =, i_data2)
			OPCODE 21: // IN AL/AX, DX/imm8
				io_ports[0x20] = 0; // PIC EOI
				io_ports[0x42] = --io_ports[0x40]; // PIT channel 0/2 read placeholder
				io_ports[0x3DA] ^= 9; // CGA refresh
				scratch_uint = extra ? regs16[REG_DX] : (unsigned_char)i_data0;
				scratch_uint == 0x60 && (io_ports[0x64] = 0); // Scancode read flag
				scratch_uint == 0x3D5 && (io_ports[0x3D4] >> 1 == 7) && (io_ports[0x3D5] = (((*get_ram(0x49E))*80 + (*get_ram(0x49D)) + FANCY_CAST(short,*get_ram(0x4AD))) & (io_ports[0x3D4] & 1 ? 0xFF : 0xFF00)) >> (io_ports[0x3D4] & 1 ? 0 : 8)); // CRT cursor position
				R_M_OP(regs8[REG_AL], =, io_ports[scratch_uint]);
			OPCODE 22: // OUT DX/imm8, AL/AX
				scratch_uint = extra ? regs16[REG_DX] : (unsigned_char)i_data0;
				R_M_OP(io_ports[scratch_uint], =, regs8[REG_AL]);
				scratch_uint == 0x61 && (io_hi_lo = 0, spkr_en |= regs8[REG_AL] & 3); // Speaker control
				(scratch_uint == 0x40 || scratch_uint == 0x42) && (io_ports[0x43] & 6) && ((*get_ram(0x469 + scratch_uint - (io_hi_lo ^= 1))) = regs8[REG_AL]); // PIT rate programming
				scratch_uint == 0x3D5 && (io_ports[0x3D4] >> 1 == 6) && ((*get_ram(0x4AD + !(io_ports[0x3D4] & 1))) = regs8[REG_AL]); // CRT video RAM start offset
				scratch_uint == 0x3D5 && (io_ports[0x3D4] >> 1 == 7) && (scratch2_uint = (((*get_ram(0x49E))*80 + (*get_ram(0x49D)) + FANCY_CAST(short,*get_ram(0x4AD))) & (io_ports[0x3D4] & 1 ? 0xFF00 : 0xFF)) + (regs8[REG_AL] << (io_ports[0x3D4] & 1 ? 0 : 8)) - FANCY_CAST(short,*get_ram(0x4AD)), (*get_ram(0x49D)) = scratch2_uint % 80, (*get_ram(0x49E)) = scratch2_uint / 80); // CRT cursor position
				scratch_uint == 0x3B5 && io_ports[0x3B4] == 1 && (GRAPHICS_X = regs8[REG_AL] * 16); // Hercules resolution reprogramming. Defaults are set in the BIOS
				scratch_uint == 0x3B5 && io_ports[0x3B4] == 6 && (GRAPHICS_Y = regs8[REG_AL] * 4);
			OPCODE 23: // REPxx
				rep_override_en = 2;
				rep_mode = i_w;
				seg_override_en && seg_override_en++
			OPCODE 25: // PUSH reg
				R_M_PUSH(regs16[extra])
			OPCODE 26: // POP reg
				R_M_POP(regs16[extra])
			OPCODE 27: // xS: segment overrides
				seg_override_en = 2;
				seg_override = extra;
				rep_override_en && rep_override_en++
			OPCODE 28: // DAA/DAS
				i_w = 0;
				extra ? DAA_DAS(-=, >=, 0xFF, 0x99) : DAA_DAS(+=, <, 0xF0, 0x90) // extra = 0 for DAA, 1 for DAS
			OPCODE 29: // AAA/AAS
				op_result = AAA_AAS(extra - 1)
			OPCODE 30: // CBW
				regs8[REG_AH] = -SIGN_OF(regs8[REG_AL])
			OPCODE 31: // CWD
				regs16[REG_DX] = -SIGN_OF(regs16[REG_AX])
			OPCODE 32: // CALL FAR imm16:imm16
				R_M_PUSH(regs16[REG_CS]);
				R_M_PUSH_PUSHEDOUT(reg_ip + 5);
				regs16[REG_CS] = i_data2;
				reg_ip = i_data0
			OPCODE 33: // PUSHF
				make_flags();
				R_M_PUSH(scratch_uint)
			OPCODE 34: // POPF
				set_flags(R_M_POP(scratch_uint))
			OPCODE 35: // SAHF
				make_flags();
				set_flags((scratch_uint & 0xFF00) + regs8[REG_AH])
			OPCODE 36: // LAHF
				make_flags(),
				regs8[REG_AH] = scratch_uint
			OPCODE 37: // LES|LDS reg, r/m
				i_w = i_d = 1;
				DECODE_RM_REG;
				OP(=);
				MEM_OP(REGS_BASE + extra, =, rm_addr + 2)
			OPCODE 38: // INT 3
				++reg_ip;
				pc_interrupt(3)
			OPCODE 39: // INT imm8
				reg_ip += 2;
				pc_interrupt(i_data0)
			OPCODE 40: // INTO
				++reg_ip;
				regs8[FLAG_OF] && pc_interrupt(4)
			OPCODE 41: // AAM
				if (i_data0 &= 0xFF)
					regs8[REG_AH] = regs8[REG_AL] / i_data0,
					op_result = regs8[REG_AL] %= i_data0;
				else // Divide by zero
					pc_interrupt(0)
			OPCODE 42: // AAD
				i_w = 0;
				regs16[REG_AX] = op_result = 0xFF & regs8[REG_AL] + i_data0 * regs8[REG_AH]
			OPCODE 43: // SALC
				regs8[REG_AL] = -regs8[FLAG_CF]
			OPCODE 44: // XLAT
				regs8[REG_AL] = (*get_ram(SEGREG(seg_override_en ? seg_override : REG_DS, REG_BX, regs8[REG_AL] +)))
			OPCODE 45: // CMC
				regs8[FLAG_CF] ^= 1
			OPCODE 46: // CLC|STC|CLI|STI|CLD|STD
				regs8[extra / 2] = extra & 1
			OPCODE 47: // TEST AL/AX, immed
				R_M_OP(regs8[REG_AL], &, i_data0)
			OPCODE 48: // Emulator-specific 0F xx opcodes
				switch ((char)i_data0)
				{
					OPCODE_CHAIN 0: // PUTCHAR_AL
						putchr(regs8[REG_AL]);
					OPCODE 1: // GET_RTC
						break;
					OPCODE 2: // DISK_READ
						;
						int offset = (unsigned)regs16[REG_BP] << 9;
						int size = regs16[REG_AX];
						int buffer = SEGREG(REG_ES, REG_BX,);
						while (size--) {
							*get_ram(buffer) = get_disk(regs8[REG_DL], offset);
							buffer++;
							offset++;
						}
					OPCODE 3: // DISK_WRITE
						break;
				}
		}

		// Increment instruction pointer by computed instruction length. Tables in the BIOS binary
		// help us here.
		reg_ip += (i_mod*(i_mod != 3) + 2*(!i_mod && i_rm == 6))*i_mod_size + bios_table_lookup(TABLE_BASE_INST_SIZE,raw_opcode_id) + bios_table_lookup(TABLE_I_W_SIZE,raw_opcode_id)*(i_w + 1);

		// If instruction needs to update SF, ZF and PF, set them as appropriate
		if (set_flags_type & FLAGS_UPDATE_SZP)
		{
			regs8[FLAG_SF] = SIGN_OF(op_result);
			regs8[FLAG_ZF] = !op_result;
			regs8[FLAG_PF] = bios_table_lookup(TABLE_PARITY_FLAG,(unsigned_char)op_result);

			// If instruction is an arithmetic or logic operation, also set AF/OF/CF as appropriate.
			if (set_flags_type & FLAGS_UPDATE_AO_ARITH)
				set_AF_OF_arith();
			if (set_flags_type & FLAGS_UPDATE_OC_LOGIC)
				set_CF(0), set_OF(0);
		}

		// Poll timer/keyboard every KEYBOARD_TIMER_UPDATE_DELAY instructions
		if (!(++inst_counter % KEYBOARD_TIMER_UPDATE_DELAY))
			int8_asap = 1;

		if (inst_counter % 8192 == 0) {
			tim_draw_status((inst_counter / 8192) % 16);
		}

		// Application has set trap flag, so fire INT 1
		if (trap_flag)
			pc_interrupt(1);

		trap_flag = regs8[FLAG_TF];

		inp_poll();

		// If a timer tick is pending, interrupts are enabled, and no overrides/REP are active,
		// then process the tick and check for new keystrokes
		if (int8_asap && !seg_override_en && !rep_override_en && regs8[FLAG_IF] && !regs8[FLAG_TF])
			pc_interrupt(0xA), int8_asap = 0, SDL_KEYBOARD_DRIVER;
	}
}