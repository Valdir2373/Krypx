
#ifndef _ACPI_H
#define _ACPI_H

#include <types.h>

void acpi_init(void);
void acpi_shutdown(void);
void acpi_reboot(void);
bool acpi_available(void);

#endif
