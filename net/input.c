#include "ns.h"

extern union Nsipc nsipcbuf;

#define PKTMAP		0x10000000

extern int sys_receive_pkt(int* length, char* rx_data);

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

    uint32_t reqno;
    int r;
    struct jif_pkt *rx_pkt;

    while (1) {
        r = sys_page_alloc(0, (void *)PKTMAP, PTE_U|PTE_W|PTE_P);
        if (r < 0)
	        panic("jif: could not allocate page of memory");
        rx_pkt = (struct jif_pkt *)PKTMAP;
        r = sys_receive_pkt(&(rx_pkt->jp_len), rx_pkt->jp_data);
        if (r == 0) {
            ipc_send(ns_envid, NSREQ_INPUT, rx_pkt, PTE_U|PTE_W|PTE_P);
        }
        //cprintf("[input]r:%x, length:%x\n", r, rx_pkt->jp_len);
        sys_page_unmap(0, (void *)rx_pkt);
    }

}
