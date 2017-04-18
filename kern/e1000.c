#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here

#define DESC_SIZE 8
#define DESC_LEN  (DESC_SIZE/8) << 7 
#define MAX_PACKET_SIZE 1518
#define BUFFER_SIZE MAX_PACKET_SIZE * DESC_SIZE

volatile uint32_t *pci_e1000;

// reserve memory for transmit descriptor array
struct tx_desc tx_desc_list[DESC_SIZE];

int transmit_pkt();

int
e1000_func_enable(struct pci_func *pcif) {
    

    pci_func_enable(pcif);

    int i;

    uint32_t base;
    uint32_t size;

    base      = pcif->reg_base[0];
    size      = pcif->reg_size[0];
    pci_e1000 = mmio_map_region(base, size);

    cprintf("[e1000_mapping_test]Status register: addr:%x, \n \
    expected content: 0x80080783; actual content:%x\n", \
    pci_e1000 + E1000_STATUS, pci_e1000[E1000_STATUS]);
    
    /* Allocate a region of memory for the transimit descriptor list.
     * Software should make sure this memory is aligned 
     * on a paragraph (16-byte) boundary. 
     * Program the Transimit Descriptor Base Address (TDBAL/TDBAH)
     * registers with the address of the region.
     * TDBAL is used for 32-bit addresses and both TDBAL and TDBAH 
     * are used for 64-bit addresses.
     */

    pci_e1000[E1000_TDBAL] = (uint32_t)tx_desc_list;
    pci_e1000[E1000_TDBAH] = (uint32_t)((uint32_t)&tx_desc_list + 32);
     //cprintf("tx_desc_list:%x, h:%x\n", (uint32_t)tx_desc_list,(uint32_t)((uint32_t)&tx_desc_list + 32));

    /* Set the Transmit Descriptor Length (TDLEN) register to the 
     * size (in bytes) of the descriptor ring.
     * This register must be 128-byte aligned.
     * E1000_TDLEN[19:7], max 0xFFF80, 64K descriptors, 1M bytes
     */ 

    //cprintf("len:%x\n",DESC_LEN);
    // set small value, 0x80, 8 descriptors, 128 bytes
    pci_e1000[E1000_TDLEN] = DESC_LEN;


    /* The transimit Descriptor Head and Tail (TDH/TDT) registers
     * are initialized (by hardware) to 0b after a power-on or
     * software initiated Ethernet controller reset.
     * Software should write 0b to both these registers to ensure 
     * this
     */

    pci_e1000[E1000_TDH] = 0x0;
    pci_e1000[E1000_TDT] = 0x0;

    /* Initialize the Transimit Control Register (TCTL) for desired
     * operation to include the following:
     *     1. Set the Enable (TCTL.EN) bit to 1b for normal 
     *        operation
     *     2. Set the Pad Short Packets (TCTL.PSP) bit to 1b.
     *     3. Configure the Collision Threshold (TCTL.CT) to
     *        the desired value. Ethernet standard is 10h.
     *        This setting only has meaning in half duplex mode.
     *     4. configure the Collision Distance (TCTL.COLD) to
     *        its expected value. 
     *        For full duplex operation, this value should be set to 40h. 
     *        For gigabit half duplex, this value should be set to 200h.
     *        For 10/100 half duplex, this value should be set to 40h
     */
    
    pci_e1000[E1000_TCTL] = E1000_TCTL_EN |
                            E1000_TCTL_PSP |
                            (E1000_TCTL_CT & (0x10 << 4)) |
                            (E1000_TCTL_COLD & (0x40 << 12));
    //cprintf("TCTL register value:%x\n", pci_e1000[E1000_TCTL]);

    /* Program the Transimit IPG (Inter Packet Gap) (TIPG) register based on 13.4.34
     * IPGT[9:0]: IPG time for back to back packet transmission
     * IPGR1[19:10]: Length of the first part of the IPG time for non back to
     *               back transmission.
     * IPGR2[29:20]: Total length of the IPG time for non back to back transmission. 
     *
     * Qustion: according to IEEE 802.3, IPGR1 should be 2/3 of IPGR2,
     * but the suggested values are 8 for IPGR1 and 6 for IPGR2, which don't
     * follow the relation IPGR1/IPGR2 = 2/3.
     */
    pci_e1000[E1000_TIPG] = 0xa | (0x8 << 10) | (0x6 << 20); 

    /* Reserve memory for memory buffers
     *     1. maximus size of an Ethernet packet is 1518 byte, there are 8 descriptors
     *     2. buffer memory should be contiguous in physical memory
     * so
     *     1. each page (4096 bytes) can have 2 buffers at most
     *     2. need 4 pages for 8 descriptors
     */
    for (i = 0; i < DESC_SIZE; i = i + 2){
        tx_desc_list[i].addr   = page2pa(page_alloc(ALLOC_ZERO));
        tx_desc_list[i+1].addr = tx_desc_list[i].addr + PGSIZE/2;
        //cprintf("i:%x, addr: %x\n", i, tx_desc_list[i].addr);
        //cprintf("i+1:%x, next_addr:%x \n", i+1, tx_desc_list[i+1].addr);
    }
     
    transmit_pkt();

    return 1;
}

/* To transimit a packet
 *   Add it to the tail of the transmit queue, which means copying the
 *   packet data into the next packet buffer and then updating the 
 *   TDT (transmit descriptor tail) register to inform the card that
 *   there's another packet in the transmit queue.
 *   
 *   Note: TDT is an index into the transmit descriptor array, not a
 *         byte offset
 */
int
transmit_pkt () {
    
    uint32_t tail;

    tail = pci_e1000[E1000_TDT];

    tx_desc_list[tail].cmd = 0x8;

    pci_e1000[E1000_TDT] += 1;

    return 0;
}
