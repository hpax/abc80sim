#ifndef ABCIO_H
#define ABCIO_H

#include "compiler.h"
#include "hostfile.h"
#include "options.h"
#include "debug.h"

extern void mem_init(enum memflags flags, const char *memfile);
extern void io_init(void);

typedef uint8_t (*in_port_handler)(uint16_t addr);
typedef void (*out_port_handler)(uint16_t addr, uint8_t value);

extern void register_ioport(uint16_t base, uint16_t mask, uint16_t valid,
			    out_port_handler out, in_port_handler in);
extern void register_inport(uint16_t base, uint16_t mask, uint16_t valid,
			    in_port_handler in);
extern void register_outport(uint16_t base, uint16_t mask, uint16_t valid,
			     out_port_handler out);


/* ABC-bus select codes */
#define BUS_MASK 0x3f
#define NO_SELECT (BUS_MASK+1)

typedef uint8_t (*abcbus_in_handler)(uint8_t sel, uint16_t addr);
typedef void (*abcbus_out_handler)(uint8_t sel, uint16_t addr, uint8_t value);
typedef void (*abcbus_select_handler)(uint8_t sel, uint8_t newsel);
typedef void (*abcbus_reset_handler)(uint8_t sel);

struct abcbus_dev {
    abcbus_out_handler    out;
    abcbus_in_handler     in;
    abcbus_select_handler select;
    abcbus_reset_handler  reset;
    uint16_t portmask;
};
extern void register_abcbus_dev(uint8_t sel, const struct abcbus_dev *dev);

extern void abc80_mem_mode80(bool);
extern void abc80_64k_control_out(uint16_t addr, uint8_t val);
extern uint8_t abc80_sram_in(uint16_t addr);
extern void abc80_sram_out(uint16_t addr, uint8_t val);
extern void abc80_sram_control_out(uint16_t addr, uint8_t val);

extern void abc800_set_mem(bool);

extern void disk_register_devices(void);
extern bool valid_drive_name(const char *drive);
extern int disk_mount(const char *drive, const char *filename);

extern void rtc_init(void);

/* ABC806 RTC */
extern uint8_t abc806_rtc_in(uint16_t port);
extern void abc806_rtc_out(uint16_t port, uint8_t value);

extern void abc800_ctc_out(uint16_t, uint8_t);
extern uint8_t abc800_ctc_in(uint16_t);
extern void abc800_ctc_init(void);

extern void printer_init(void);
extern void dart_pr_out(uint16_t port, uint8_t v);
extern uint8_t dart_pr_in(uint16_t port);

extern void keyboard_down(int sym);
extern void keyboard_up(void);
extern bool faketype;
extern void cursor_enable_hook(void);

extern void abc800_vsync(void);

extern void abc80_piob_out(uint16_t port, uint8_t v);
extern uint8_t abc80_piob_in(uint16_t port);
extern void abc80_cas_init(void);

extern void abc800_sio_cas_out(uint16_t port, uint8_t v);
extern uint8_t abc800_sio_cas_in(uint16_t port);
extern void abc800_cas_init(void);

/*
 * Z80 has fixed priorities based on the device daisy chain, but the vectors
 * can be different, so we identify interrupts by their priority level.
 */
enum abc80_irq {
    IRQ80_PIOA,
    IRQ80_PIOB
};
enum abc800_irq {
    IRQ800_DARTA,
    IRQ800_DARTB,
    IRQ800_SIOA,
    IRQ800_SIOB,
    IRQ800_CTC0,
    IRQ800_CTC1,
    IRQ800_CTC2,
    IRQ800_CTC3
};

/* Directory and filenames */
extern const char *fileop_path, *disk_path, *screen_path, *memdump_path,
    *cas_path;
extern struct file_list cas_files;
extern struct file_list script_files;

/* Program name for error messages */
extern const char *program_name;

#endif
