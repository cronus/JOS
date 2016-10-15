// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

    //cprintf("fault va:%x, err:%x, flag: %x\n", addr, err,(PGOFF(uvpt[(unsigned)addr/PGSIZE])));
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    if (!((err & FEC_WR) && ((PGOFF(uvpt[((unsigned)addr/PGSIZE)]) & PTE_COW) == PTE_COW)))
        panic("[pgfault]panic at addr:%x", addr);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	//panic("pgfault not implemented");

    if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
    memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
    if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
    if ((r = sys_page_unmap(0, PFTEMP)) < 0) 
		panic("sys_page_unmap: %e", r);
    

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
    uint32_t * addr;

    addr = (uint32_t*) (pn*PGSIZE);

	// LAB 4: Your code here.
	// panic("duppage not implemented");
    
    //cprintf("pn: %x, page:%x, content:%x, flag: %x\n", pn, addr, uvpt[pn], PGOFF(uvpt[pn]));
    if ((PGOFF(uvpt[pn]) & PTE_W) || (PGOFF(uvpt[pn]) & PTE_COW)) {
        //cprintf("writeable page: addr:%x\n", addr);
        if ((r = sys_page_map(0, (void*)addr, envid, (void*)addr, PTE_P|PTE_U|PTE_AVAIL)) < 0)
	    	panic("sys_page_map child: %e", r);
        //uvpt[pn] = uvpt[pn] | PTE_COW;
        if ((r = sys_page_map(0, (void*)addr, 0, (void*)addr, PTE_P|PTE_U|PTE_AVAIL)) < 0)
	    	panic("sys_page_map parent: %e", r);
    }
    else if (PGOFF(uvpt[pn] & PTE_P)){
        //cprintf("read only page: addr:%x\n", addr);
        if ((r = sys_page_map(0, (void*)addr, envid, (void*)addr, PTE_P|PTE_U)) < 0)
	    	panic("sys_page_map: %e", r);
    }
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");

    envid_t envid;
    uint8_t *addr;
    int r;
	extern unsigned char end[];
    extern void _pgfault_upcall(void);

    set_pgfault_handler(pgfault);
    envid = sys_exofork();
    //cprintf("envid:%x\n", envid);

    if (envid < 0)
		panic("sys_exofork: %e", envid);

    // child
    if (envid == 0) {
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
        //cprintf("enter child, envid:%x\n",sys_getenvid());
		thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // parent
    //cprintf("end:%x\n", end);
    for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE) {
        //cprintf("addr:%x, pn:%x\n", addr, (unsigned)addr/PGSIZE);
        duppage(envid, (unsigned)addr/PGSIZE);
    }

    // copy the stack we are currently running on
	if ((r = sys_page_alloc(envid, ROUNDDOWN(&addr, PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if ((r = sys_page_map(envid, ROUNDDOWN(&addr, PGSIZE), 0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, ROUNDDOWN(&addr, PGSIZE), PGSIZE);
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
    
    // create exception stack
	if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
		panic("[pgfalut]sys_page_alloc: %e", r);

    // set entry point for child pgfault handler
    sys_env_set_pgfault_upcall(envid, (uint32_t*)_pgfault_upcall);

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
