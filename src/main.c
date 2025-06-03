#define GBA

// yea i know i'm writing c like assembly lol
// do i look like i care?
#include "term.c"
#include "8086tiny.c"

#define REG_KEYINPUT (* (volatile unsigned short*) 0x4000130)
#define KEY_A        0x0001
#define KEY_B        0x0002
#define KEY_SELECT   0x0004
#define KEY_START    0x0008
#define KEY_RIGHT    0x0010
#define KEY_LEFT     0x0020
#define KEY_UP       0x0040
#define KEY_DOWN     0x0080
#define KEY_R        0x0100
#define KEY_L        0x0200
#define KEY_MASK     0xFC00

#define SIODATA  ((volatile unsigned char*)0x4000120)

void joybus_init() {
    // RCNT = 0xc000
    *((volatile unsigned short*)(SIODATA + 0x14)) = 0xc000;
    // JOYSTAT = 0
    *((volatile unsigned int*)(SIODATA + 0x38)) = 0;
}

unsigned char joy_error = 0;

unsigned char joybus_common(unsigned int x) {
    // JOYTRANS = x
    *((volatile unsigned int*)(SIODATA + 0x34)) = x;
    // JOYRECV = 0
    *((volatile unsigned int*)(SIODATA + 0x30)) = 0;
    // acknowledge everything
    *((volatile unsigned int*)(SIODATA + 0x20)) = 7;
    // extra code for very bad amulators
    *((volatile unsigned int*)(SIODATA + 0x20)) = 0;
    // JOYSTAT = 0x10
    *((volatile unsigned int*)(SIODATA + 0x38)) = 0x10;
    // wait for response
    int retries = 100000;
    while (retries--) {
        unsigned int r = *((volatile unsigned int*)(SIODATA + 0x20));
        if (r & 0b00000110) break;
    }
    // clear JOYSTAT
    *((volatile unsigned int*)(SIODATA + 0x38)) = 0;
    // get response from JOYRECV
    unsigned int result = *((volatile unsigned int*)(SIODATA + 0x30));
    // JOYTRANS = JOYRECV = #0
    *((volatile unsigned int*)(SIODATA + 0x34)) = 0;
    *((volatile unsigned int*)(SIODATA + 0x30)) = 0;
    // return what we got
    if ((result >> 8) == 0x42069) {
        joy_error = 0;
        return result & 0xff;
    } else {
        joy_error = 1;
        return 0;
    }
}

#ifdef STANDALONE
    #include "inc_fd.c"
    #include "inc_bios.c"
    unsigned char get_disk(unsigned fd, unsigned offs) {
        if (fd == 1) return ___fd_img[offs];
        else if (fd == 0) {
            if (offs >= ___bios_len) return 0;
            return ___bios[offs];
        }
        else return 0;
    }
#else
unsigned char get_disk(unsigned fd, unsigned offs) {
    unsigned char r;
    int retries = 10;
    while (retries--) {
        if (fd == 0) r = joybus_common(0x69000000 | offs);
        if (fd == 1) r = joybus_common(0x42000000 | offs);
        if (fd == 13) r = joybus_common(0xb3515420);
        if (!joy_error) break;
    }
    if (retries == -1 && fd != 13) {
        writestr("\ninval joy fd read at ");
        writenum(fd);
        writenum(offs);
        writestr(", halting!");
        for(;;);
    }
    return r;
}
#endif

int kb_buf, kb_cnt, kb_store;

void init_ram() {
    kb_buf = 0;
    kb_cnt = 0;
    kb_store = 0;
}

void init_disk() {
}

int get_key(unsigned short k) {
    return !(k & (REG_KEYINPUT | KEY_MASK));
}

void inp_draw_status() {
    putchr_base(4, '0' + ((kb_buf >> 6) & 1), 0xff00, 0x0033);
    putchr_base(5, '0' + ((kb_buf >> 5) & 1), 0xff00, 0x0033);
    putchr_base(6, '0' + ((kb_buf >> 4) & 1), 0xff00, 0x0033);
    putchr_base(7, '0' + ((kb_buf >> 3) & 1), 0xff00, 0x0033);
    putchr_base(8, '0' + ((kb_buf >> 2) & 1), 0xff00, 0x0033);
    putchr_base(9, '0' + ((kb_buf >> 1) & 1), 0xff00, 0x0033);
    putchr_base(10, '0' + ((kb_buf >> 0) & 1), 0xff00, 0x0033);
}

void tim_draw_status(int ctr) {
    unsigned char wave[16] = {
        0b00000000, 0b00000001, 0b00000011, 0b00000111, 
        0b00001111, 0b00011111, 0b00111111, 0b01111111, 
        0b11111111, 0b11111110, 0b11111100, 0b11111000, 
        0b11110000, 0b11100000, 0b11000000, 0b10000000,
    };
    int i;
    for (i=0; i<8; i++) {
        putchr_base(16+i, 16 + 4*((wave[ctr] >> i) & 1), 0xff00, 0x0033);
    }
}

void inp_poll() {
    if (get_key(KEY_A)) {
        kb_buf <<= 1;
        kb_buf |= 1;
        kb_cnt++;
        inp_draw_status();
    }
    if (get_key(KEY_B)) {
        kb_buf <<= 1;
        kb_cnt++;
        inp_draw_status();
    }
    if (get_key(KEY_SELECT)) {
        kb_buf = 0;
        kb_cnt = 0;
        inp_draw_status();
    }
    kb_buf = kb_buf & 0b1111111;
    if (get_key(KEY_START)) {
        putchr_base(12, 'P', 0xff00, 0x0033);
        putchr_base(13, 'N', 0xff00, 0x0033);
        putchr_base(14, 'D', 0xff00, 0x0033);
        kb_store = kb_buf;
        kb_buf = 0;
        kb_cnt = 0;
        inp_draw_status();
    }
    while (get_key(KEY_A) || get_key(KEY_B) || get_key(KEY_START));
}

char inp_kbhit() {
    return kb_store;
}

char inp_getch() {
    int r = kb_store;
    kb_store = 0;
    putchr_base(12, ' ', 0xff00, 0x0033);
    putchr_base(13, ' ', 0xff00, 0x0033);
    putchr_base(14, ' ', 0xff00, 0x0033);
    return r;
}

int main(void){
    term_init();
    cls();
    draw_topbar();
    
    writestr("8086tiny on gba by zzazz\n");

    #ifndef STANDALONE
    writestr("looking for joybus...");
    joybus_init();
    while(1) {
        if (get_disk(13, 37) == 0x69) break;
        putchr('.');
    }
    writestr("got it\n");
    #endif

    emu();
}