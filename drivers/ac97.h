
#ifndef _AC97_H
#define _AC97_H

#include <types.h>

/* Initialize AC'97 audio (PCI detection + reset + default volume) */
void ac97_init(void);

/* Set master volume 0 (mute) – 100 (max) */
void ac97_set_volume(int pct);

/* Get current volume 0-100 */
int  ac97_get_volume(void);

/* Returns true if AC'97 hardware was found */
bool ac97_available(void);

#endif
