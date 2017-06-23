#include "device.h"
#include "x86.h"

SegDesc gdt[NR_SEGMENTS];
TSS tss;

void initSeg() {
    gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_KERN);
    gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, DPL_KERN);
    gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
    gdt[SEG_KSTAK] = SEG(STA_W, 0, 0xffffffff, DPL_KERN);
    gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    gdt[SEG_TSS] = SEG16(STS_T32A, &tss, sizeof(TSS) - 1, DPL_KERN);
    gdt[SEG_TSS].s = 0;
    gdt[SEG_VIDEO] = SEG(STA_W, 0x0b8000, 0xffffffff, DPL_KERN);
    setGdt(gdt, sizeof(gdt));

    /*
     * initialize TSS
     */
    // asm volatile("movl %%esp, %0": "=r"(tss.esp0));
    // tss.esp0 = 0x200000;   // set kernel esp to 0x200,000
    tss.esp0 = (uint32_t)&pcb[0].stack[KERNEL_STACK_SIZE];
    tss.ss0 = KSEL(SEG_KDATA);
    asm volatile("ltr %%ax" ::"a"(KSEL(SEG_TSS)));

    /* set kernel segment register */
    asm volatile("movl %0, %%eax" ::"r"(KSEL(SEG_KDATA)));
    asm volatile("movw %ax, %ds");
    asm volatile("movw %ax, %es");
    asm volatile("movw %ax, %ss");
    asm volatile("movw %ax, %fs");
    asm volatile("movl %0, %%eax" ::"r"(KSEL(SEG_VIDEO)));
    asm volatile("movw %ax, %gs");
    lLdt(0);
}

void enterUserSpace(uint32_t entry) {
    /* set user segment register */
    asm volatile("movl %0, %%eax" ::"r"(USEL(SEG_UDATA)));
    asm volatile("movw %ax, %ds");
    asm volatile("movw %ax, %es");
    asm volatile("movw %ax, %fs");

    /*
     * Before enter user space
     * you should set the right segment registers here
     * and use 'iret' to jump to ring3
     */
    asm volatile("sti");
    asm volatile("pushl %0" ::"r"(USEL(SEG_UDATA)));  // %ss
    asm volatile("pushl %0" ::"r"(64 << 20));         // %esp 128MB
    asm volatile("pushfl");                           // %eflags
    asm volatile("pushl %0" ::"r"(USEL(SEG_UCODE)));  // %cs
    asm volatile("pushl %0" ::"r"(entry));            // %eip

    asm volatile("iret");  // return to user space
}

void loadUMain(void) {
    /* load user elf-file */
    struct ELFHeader *elf;
    struct ProgramHeader *ph;
    int app_file_size;

    unsigned char buf[10000];
    // unsigned char *buf = (unsigned char *)0x5000000;
    /*
    for (int i = 0; i < 100; i ++) {
            readSect((void*)(buf + 512 * i), i + 201);
    }
    */
    app_file_size = my_file_size("/boot/app");
    prints("/boot/app file size: ");
    printd(app_file_size);
    printc('\n');
    my_read(find_file_inode("/boot/app", NULL), buf, app_file_size, 0);

    elf = (struct ELFHeader *)buf;

    /* Load each program segment */
    ph = (struct ProgramHeader *)(buf + elf->phoff);
    int i;
    for (i = 0; i < elf->phnum; ++i) {
        /* Scan the program header table, load each segment into memory */
        if (ph->type == 1) {
            /* read the content of the segment from the ELF file
             * to the memory region [VirtAddr, VirtAddr + FileSiz)
             */
            unsigned int p = ph->vaddr, q = ph->off;
            while (p < ph->vaddr + ph->filesz) {
                *(unsigned char *)p = *(unsigned char *)(buf + q);
                q++;
                p++;
            }

            /* zero the memory region [VirtAddr + FileSiz, VirtAddr + MemSiz) */
            while (p < ph->vaddr + ph->memsz) {
                *(unsigned char *)p = 0;
                q++;
                p++;
            }
        }

        ph++;
    }

    int src = 0x200000, dst = 0x200000 + 0x100000;
    for (int i = 0; i < 0x100000; i++) {
        *((uint8_t *)dst + i) = *((uint8_t *)src + i);
    }

    enter_proc(elf->entry);
    // enterUserSpace(elf->entry);
}
