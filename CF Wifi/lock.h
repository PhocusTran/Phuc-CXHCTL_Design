
#ifndef LOCK_H
#define LOCK_H

#include <stdbool.h>
#include <stdint.h>

void usb_msc_fs_lock_init();

void usb_msc_fs_lock(void);
void usb_msc_fs_free(void);;
void usb_msc_access_lock(void);
void usb_msc_access_free(void);
uint8_t end_http_task(void);

extern uint8_t usb_seize_request;
extern uint8_t http_busy;

#endif
