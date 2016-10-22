/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
    //cprintf("first para: %x, sec: %x\n", s, len);
    user_mem_assert(curenv, s, len, 0);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
    // bocui Q: how to deal with sys_exofork will appear to return 0?
    // bocui A: tweak reg eax to 0

	// LAB 4: Your code here.
	//panic("sys_exofork not implemented");
    
    struct Env * new_user_env;
    struct Env * parent_env;
    int r;
    r = env_alloc(&new_user_env, curenv->env_id);
    if (r == 0) {
        new_user_env->env_status = ENV_NOT_RUNNABLE;
        envid2env(new_user_env->env_parent_id, &parent_env, 1);
        new_user_env->env_tf         = parent_env->env_tf;
        new_user_env->env_tf.tf_regs.reg_eax = 0;
        return new_user_env->env_id;
    }
    else {
        return r;
    }
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	//panic("sys_env_set_status not implemented");

    struct Env * this_env;
    int r;
    //cprintf("status:%d\n",status);
    if ((status != ENV_RUNNABLE) && (status != ENV_NOT_RUNNABLE)) {
        return -E_INVAL;
    }
    r = envid2env(envid, &this_env, 1);
    //cprintf("r:%d\n",r);
    if (r == 0) {
        this_env->env_status = status;
        return r;
    }
    else {
        return r;
    }
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	// panic("sys_env_set_trapframe not implemented");

    struct Env * thisenv;
    int r;
    
    r = envid2env(envid, &thisenv, 1);
    if (r == 0) {
        thisenv->env_tf = *tf;
        thisenv->env_tf.tf_cs     |= 3;
        thisenv->env_tf.tf_eflags |= FL_IF;

        return r;
    }
    else {
        return r;
    }
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	//panic("sys_env_set_pgfault_upcall not implemented");
    
    struct Env * this_env;
    int r;

    r = envid2env(envid, &this_env, 1);
    if (r == 0) {
        this_env->env_pgfault_upcall = func;
    }
    return r;
   
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	//panic("sys_page_alloc not implemented");

    struct Env * this_env;
    struct PageInfo * new_page;
    int r;
    
    //cprintf("[sys_page_alloc]1env id:%x, va:%x, thisenv content:%x\n", curenv->env_id, va, *(uint32_t*)0x804004);
    if (((uint32_t)va >= UTOP) || 
        (((perm & (PTE_U|PTE_P)) != (PTE_U|PTE_P)) ||
        //(perm != (PTE_U|PTE_P|PTE_AVAIL)) &&
        //(perm != (PTE_U|PTE_P|PTE_W)) && 
        ((perm | PTE_SYSCALL) != PTE_SYSCALL))) {
        return -E_INVAL;
    }
    r = envid2env(envid, &this_env, 1);
    if (r == 0) {
        new_page = page_alloc(ALLOC_ZERO);
        if (new_page != NULL) {
            //cprintf("[sys_page_alloc]3env id:%x, va:%x, thisenv content:%x, new page:%x, ref:%x\n", curenv->env_id, va, *(uint32_t*)0x804004, new_page, new_page->pp_ref);
            return page_insert(this_env->env_pgdir, new_page, va, perm);
        }
        else {
            return -E_NO_MEM;
        }
    }
    else {
        return r;
    }
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	//panic("sys_page_map not implemented");

    struct Env * src_env;
    struct Env * dst_env;
    pte_t * src_pte_ptr;
    struct PageInfo * src_page;


    int r_src, r_dst;

    if (((uint32_t)srcva >= UTOP) || 
        ((uint32_t)dstva >= UTOP) ||
        (((perm & (PTE_U|PTE_P))!= (PTE_U|PTE_P)) ||
        //(perm != (PTE_U|PTE_P|PTE_AVAIL)) &&
        //(perm != (PTE_U|PTE_P|PTE_W)) && 
        ((perm | PTE_SYSCALL)!= PTE_SYSCALL))) {
        return -E_INVAL;
    }

    r_src = envid2env(srcenvid, &src_env, 1);
    r_dst = envid2env(dstenvid, &dst_env, 0);
    //cprintf("r_src:%d, destid,:%x, r_dst:%d, return:%d\n", r_src, dstenvid, r_dst, r_src|r_dst);
    if ((r_src == 0) && (r_dst == 0)) {
        if ((src_page = page_lookup(src_env->env_pgdir, srcva, &src_pte_ptr))) {
            if ((perm & PTE_W) && !(PGOFF(*src_pte_ptr) & PTE_W)) {
                return -E_INVAL;
            }
            //cprintf("[sys_page_map]dstva:%x, srccontent:%x, perm:%x\n", dstva, *src_pte_ptr, perm);
            return page_insert(dst_env->env_pgdir, src_page, dstva, perm);
        }
        else {
            //cprintf("2\n");
            return -E_INVAL;
        }
    }
    else {
        //cprintf("3\n");
        return r_src | r_dst;
    }
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	//panic("sys_page_unmap not implemented");

    struct Env * this_env;
    int r;

    if ((uint32_t)va >= UTOP) {
        return -E_INVAL;
    }
    r = envid2env(envid, &this_env, 1);
    if (r == 0) {
        //cprintf("[sys_page_umap]va:%x\n", va);
        page_remove(this_env->env_pgdir, va);
        return 0;
    }
    else {
        return r;
    }
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	// panic("sys_ipc_try_send not implemented");
    
    struct Env * tgt_env;
    int r;
    pte_t * src_pte_ptr;
    pte_t * tgt_pte_ptr;

    r = envid2env(envid, &tgt_env, 0);
    src_pte_ptr = pgdir_walk(curenv->env_pgdir, srcva, 0);
    tgt_pte_ptr = pgdir_walk(tgt_env->env_pgdir, tgt_env->env_ipc_dstva, 1);

    bool perm_check =  (((perm & (PTE_U|PTE_P)) != (PTE_U|PTE_P)) ||
                       //(perm != (PTE_U|PTE_P|PTE_AVAIL)) &&
                       //(perm != (PTE_U|PTE_P|PTE_W)) && 
                       ((perm | PTE_SYSCALL) != PTE_SYSCALL));
    //check srcva
    if ((((uint32_t)srcva < UTOP) && ((uint32_t)srcva % PGSIZE)) ||
        (((uint32_t)srcva < UTOP) && perm_check) ||
        (((uint32_t)srcva < UTOP) && (src_pte_ptr == NULL)) ||
        (((uint32_t)srcva < UTOP) && ((perm) & PTE_W) && ((PGOFF(*src_pte_ptr) & PTE_W) != PTE_W))) {
        return -E_INVAL;
    }

    if (tgt_pte_ptr == NULL) {
        return -E_NO_MEM;
    }

    //cprintf("[try_send]2\n");
    if (r == 0) {
        if (tgt_env->env_ipc_recving) {
            tgt_env->env_ipc_recving = 0;
            tgt_env->env_ipc_from    = curenv->env_id;
            tgt_env->env_ipc_value   = value;
            if ((srcva < (void*)UTOP) && (tgt_env->env_ipc_dstva < (void*)UTOP)) {
                tgt_env->env_ipc_perm    = perm;
                sys_page_map(tgt_env->env_ipc_from, srcva,
                             envid, tgt_env->env_ipc_dstva, perm);
                //cprintf("map, src:%x, destva: %x, content:%x,r:%d\n", srcva,tgt_env->env_ipc_dstva, (*(uint32_t*)tgt_env->env_ipc_dstva),r);
            }
            else {
                tgt_env->env_ipc_perm    = 0;
            }
            tgt_env->env_tf.tf_regs.reg_eax = 0;
            tgt_env->env_status             = ENV_RUNNABLE;
            //cprintf("\n[sys_ipc_try_send]cpu:%d, %x send successfully, set %x runnable, set return value eax:%x\n",curenv->env_cpunum, curenv->env_id, tgt_env->env_id, tgt_env->env_tf.tf_regs.reg_eax);
            return 0;
        }
        else {
            return -E_IPC_NOT_RECV;
        }
    }
    else {
        return r;
    }
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	// panic("sys_ipc_recv not implemented");

    // check dstva
    if (((uint32_t)dstva < UTOP) && ((uint32_t)dstva % PGSIZE)) {
        return -E_INVAL;
    }
    
    curenv->env_ipc_recving = 1;
    if ((uint32_t)dstva < UTOP) {
        curenv->env_ipc_dstva   = dstva;
    } 
    while (curenv->env_ipc_recving){
        //cprintf("\n[sys_ipc_recv]cpu:%d, set %x not runnable\n", curenv->env_cpunum, curenv->env_id, curenv->env_cpunum);
        curenv->env_status = ENV_NOT_RUNNABLE; 
        sched_yield();
    }

	return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	panic("sys_time_msec not implemented");
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	//panic("syscall not implemented");

    //cprintf("syscall no: %x\n", syscallno);
    //cprintf("no: %x, a1:%x, a2:%x\n", syscallno, a1, a2);
	switch (syscallno) {
        case SYS_cputs:
            //cprintf("a1:%x, a2:%d\n", a1, a2);
            sys_cputs((char*)a1, a2);
            return 0;
        case SYS_cgetc:
            return sys_cgetc();
        case SYS_getenvid:
            return sys_getenvid();
        case SYS_env_destroy:
            return sys_env_destroy(a1);
        // bocui: for lab 4, exe 6
        case SYS_yield:
            sys_yield();
            return 0;
        // bocui: for lab 4 exe 7
        case SYS_exofork:
            return sys_exofork();
        case SYS_env_set_status:
            return sys_env_set_status(a1, a2);
        case SYS_page_alloc:
            return sys_page_alloc(a1, (void*)a2, a3);
        case SYS_page_map:
            return sys_page_map(a1, (void*)a2, a3, (void*)a4, a5);
        case SYS_page_unmap:
            return sys_page_unmap(a1, (void*)a2);
        // bocui: for lab 4 exe 8
        case SYS_env_set_pgfault_upcall:
            return sys_env_set_pgfault_upcall(a1, (void*)a2);
        // bocui: for lab 4 exe 15
        case SYS_ipc_try_send:
            return sys_ipc_try_send(a1, a2, (void*)a3, (unsigned)a4);
        case SYS_ipc_recv:
            return sys_ipc_recv((void*)a1);
        // bocui: for lab5 exe 7
        case SYS_env_set_trapframe:
            return sys_env_set_trapframe((envid_t)a1, (struct Trapframe*)a2);
        // old default till lab4
	    //default:
		//    return -E_NO_SYS;
        // new default in lab5
	    default:
	    	return -E_INVAL;
	}
}

