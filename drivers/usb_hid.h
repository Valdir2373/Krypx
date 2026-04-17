
#ifndef _USB_HID_H
#define _USB_HID_H

#include <types.h>

void usb_hid_init(void);
bool usb_kbd_available(void);   /* true if USB keyboard detected */
bool usb_mouse_available(void); /* true if USB mouse detected */

/* Called from USB interrupt / polling — feeds keyboard buffer */
void usb_hid_poll(void);

#endif
