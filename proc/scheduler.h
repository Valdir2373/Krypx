
#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <types.h>
#include <proc/process.h>

#define TIMESLICE_MS  20   


void scheduler_init(void);


void scheduler_add(process_t *proc);


void scheduler_remove(process_t *proc);


void schedule(void);


void scheduler_enable(void);
void scheduler_disable(void);

#endif 
