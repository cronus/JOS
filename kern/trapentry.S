/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide, T_DIVIDE)
TRAPHANDLER_NOEC(debug , T_DEBUG )
TRAPHANDLER_NOEC(nmi   , T_NMI   )
TRAPHANDLER_NOEC(brkpt , T_BRKPT )
TRAPHANDLER_NOEC(oflow , T_OFLOW )
TRAPHANDLER_NOEC(bound , T_BOUND )
TRAPHANDLER_NOEC(illop , T_ILLOP )
TRAPHANDLER_NOEC(device, T_DEVICE)
TRAPHANDLER_NOEC(dblflt, T_DBLFLT)
TRAPHANDLER(segnp, T_SEGNP)
TRAPHANDLER(stack, T_STACK)
TRAPHANDLER(gpflt, T_GPFLT)
TRAPHANDLER(pgflt, T_PGFLT)
TRAPHANDLER_NOEC(fperr  , T_FPERR  )
TRAPHANDLER_NOEC(align  , T_ALIGN  )
TRAPHANDLER_NOEC(mchk   , T_MCHK   )
TRAPHANDLER_NOEC(simderr, T_SIMDERR)

#system call
TRAPHANDLER_NOEC(enter_syscall, T_SYSCALL)

#interrupt
TRAPHANDLER_NOEC(timer,    IRQ_OFFSET + IRQ_TIMER)
TRAPHANDLER_NOEC(kbd,      IRQ_OFFSET + IRQ_KBD)
TRAPHANDLER_NOEC(serial,   IRQ_OFFSET + IRQ_SERIAL)
TRAPHANDLER_NOEC(spurious, IRQ_OFFSET + IRQ_SPURIOUS)
TRAPHANDLER_NOEC(ide,      IRQ_OFFSET + IRQ_IDE)
TRAPHANDLER_NOEC(error,    IRQ_OFFSET + IRQ_ERROR)

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:

  # Build trap frame.
  pushl %ds
  pushl %es
  pushal
  
  #load GD_KD into %ds and %es
  movw $(GD_KD), %ax
  movw %ax, %ds
  movw %ax, %es

  # Call trap(tf), where tf=%esp
  pushl %esp
  call trap
