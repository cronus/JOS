/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

//trap handler functions
void divide();
void debug();
void nmi();
void brkpt();
void oflow();
void bound();
void illop();
void device();
void dblflt();
void tss();
void segnp();
void stack();
void gpflt();
void pgflt();
void fperr();
void align();
void mchk();
void simderr();

void enter_syscall();

void timer();
void kbd();
void serial();
void spurious();
void ide();

#endif /* JOS_KERN_TRAP_H */
