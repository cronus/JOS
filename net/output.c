#include "ns.h"
#include <lib/syscall.c>

extern union Nsipc nsipcbuf;

#define PKTMAP		0x10000000
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver

    /* Specifically, the output helper environment's
     * goal is to do the following:
     *   1.accept NSREQ_OUTPUT IPC messages from the 
     *     core network server
     *   2.send the packets accompanying these IPC 
     *     message to the network device driver using
     *     the system call
     */

    envid_t whom;
    uint32_t reqno;
    int perm;
    //int r;

    int r = sys_page_alloc(0, (void *)PKTMAP, PTE_U|PTE_W|PTE_P);
    if (r < 0)
	    panic("jif: could not allocate page of memory");
    struct jif_pkt *tx_pkt = (struct jif_pkt *)PKTMAP;

    while (1) {
        // read packet from network server
        reqno = ipc_recv(&whom, tx_pkt, &perm);

	    //cprintf("ns req %d from %08x, length:%x, pkt:%x\n", reqno, whom, tx_pkt->jp_len, tx_pkt->jp_data);
        // send the packet to the device driver
        sys_transmit_pkt(tx_pkt->jp_len, tx_pkt->jp_data);
    }

    sys_page_unmap(0, (void *)tx_pkt);
}
