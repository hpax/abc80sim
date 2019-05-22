#ifndef SERIAL_H
#define SERIAL_H

enum flowctrl {
    FLOW_NONE,
    FLOW_DTR,			/* DTR/DSR */
    FLOW_RTS			/* RTS/CTS */
};

int open_serial(const char *port, unsigned long speed, enum flowctrl flowctrl);

#endif /* SERIAL_H */
