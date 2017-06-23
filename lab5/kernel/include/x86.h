#ifndef __X86_H__
#define __X86_H__

#include "x86/cpu.h"
#include "x86/memory.h"
#include "x86/io.h"
#include "x86/irq.h"
#include "x86/pcb.h"
#include "x86/semaphore.h"
#include "x86/fs.h"

void initSeg(void);
void loadUMain(void);

#endif
