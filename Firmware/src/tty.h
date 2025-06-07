#ifndef _PROGRAMMER_H_
#define _PROGRAMMER_H_

#include "usbd_lib.h"
#include "hardware.h"

char tty_ep0_in(config_pack_t *req, void **data, uint16_t *size);
char tty_ep0_out(config_pack_t *req, uint16_t offset, uint16_t rx_size);
void tty_init();
void tty_poll();

#endif
