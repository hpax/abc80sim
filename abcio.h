#ifndef ABCIO_H
#define ABCIO_H

#include "compiler.h"

extern void io_init(void);

enum model {
    MODEL_ABC80,
    MODEL_ABC802
};

extern enum model model;
extern unsigned int kilobytes;
extern bool old_basic;

extern void abc80_mem_mode40(bool);
extern void abc80_mem_setmap(unsigned int);
extern void abc802_set_mem(bool);

extern void disk_reset(void);
extern void disk_out(int sel, int port, int value);
extern int disk_in(int sel, int port);

extern int rtc_in(int sel, int port);

extern void abc800_ctc_out(uint8_t, uint8_t);
extern uint8_t abc800_ctc_in(uint8_t);

extern void printer_reset(void);
extern void printer_out(int sel, int port, int value);
extern int printer_in(int sel, int port);

extern void keyboard_down(int sym);
extern void keyboard_up(void);

extern void abc802_vsync(void);

extern void dump_memory(bool ramonly);

extern void abc80_piob_out(uint8_t port, uint8_t v);
extern uint8_t abc80_piob_in(void);

/* Directory and filenames */
extern const char *fileop_path, *disk_path, *screen_path, *memdump_path;
extern const char *cas_file;

/* Program name for error messages */
extern const char *program_name;

#endif
