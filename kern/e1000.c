#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/error.h>

// LAB 6: Your driver code here

#define TX_DESC_SIZE 8
#define TX_DESC_LEN  (TX_DESC_SIZE/8) << 7 
#define TX_PTR_MSK (0xf >> 1)
#define RX_DESC_SIZE 128
#define RX_DESC_LEN  (RX_DESC_SIZE/8) << 7 
#define RX_PTR_MSK (0xff >> 1)
#define MAX_PKT_SZ 1518

volatile uint32_t *pci_e1000;

// reserve memory for transmit descriptor array
struct tx_desc tx_desc_list[TX_DESC_SIZE];
struct rx_desc rx_desc_list[RX_DESC_SIZE];

void transmit_init();
void receive_init();

int
e1000_func_enable(struct pci_func *pcif) {
    

    pci_func_enable(pcif);

    uint32_t base;
    uint32_t size;

    base      = pcif->reg_base[0];
    size      = pcif->reg_size[0];
    pci_e1000 = mmio_map_region(base, size);

    cprintf("[e1000_mapping_test]Status register: addr:%x, \n \
    expected content: 0x80080783; actual content:%x\n", \
    pci_e1000 + E1000_STATUS, pci_e1000[E1000_STATUS]);

    transmit_init();
    receive_init();

    return 1;
}
    
void transmit_init() {
    
    int i;
    /* Allocate a region of memory for the transimit descriptor list.
     * Software should make sure this memory is aligned 
     * on a paragraph (16-byte) boundary. 
     * Program the Transimit Descriptor Base Address (TDBAL/TDBAH)
     * registers with the address of the region.
     * TDBAL is used for 32-bit addresses and both TDBAL and TDBAH 
     * are used for 64-bit addresses.
     */
    
    // Hw can only recognize pa, should be physical addr
    pci_e1000[E1000_TDBAL] = PADDR(tx_desc_list);
    pci_e1000[E1000_TDBAH] = 0x0;
    //cprintf("tx_desc_list:%x, h:%x\n", pci_e1000[E1000_TDBAL], pci_e1000[E1000_TDBAH]);

    /* Set the Transmit Descriptor Length (TDLEN) register to the 
     * size (in bytes) of the descriptor ring.
     * This register must be 128-byte aligned.
     * E1000_TDLEN[19:7], max 0xFFF80, 64K descriptors, 1M bytes
     */ 

    //cprintf("len:%x\n",TX_DESC_LEN);
    // set small value, 0x80, 8 descriptors, 128 bytes
    pci_e1000[E1000_TDLEN] = TX_DESC_LEN;


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
    for (i = 0; i < TX_DESC_SIZE; i = i + 2){
        tx_desc_list[i].addr   = page2pa(page_alloc(ALLOC_ZERO));
        tx_desc_list[i+1].addr = tx_desc_list[i].addr + PGSIZE/2;
        //cprintf("i:%x, addr: %x\n", i, tx_desc_list[i].addr);
        //cprintf("i+1:%x, next_addr:%x \n", i+1, tx_desc_list[i+1].addr);
    }

    /* Set all the DD bit to 1
     */
    for (i = 0; i < TX_DESC_SIZE; i = i + 1){
        tx_desc_list[i].status   = 0x1;
    }
    
    cprintf("Transmit Initialization Done!\n");
     
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
int transmit_pkt (uint16_t length, char* tx_pkt) {
    
    uint32_t  tail;
    uint8_t* pkt_buf;

    int i;

    tail         = pci_e1000[E1000_TDT];
    pkt_buf      = KADDR(tx_desc_list[tail].addr);
    
    //cprintf("[transmit pkt]status: %x\n", tx_desc_list[tail].status);

    // check DD is set to make sure next descriptor is available
    if ((tx_desc_list[tail].status & E1000_TXD_STAT_DD) == E1000_TXD_STAT_DD) {
        
        // clean descriptor done
        tx_desc_list[tail].status = 0x0;

        //for (i = 0; i < MAX_PKT_SZ/4; i++) {
        //    pkt_buf[i] = 0x5a5a5a5a;
        //}
        for (i = 0; i < length; i++) {
            pkt_buf[i] = tx_pkt[i];
        }

        tx_desc_list[tail].length = length;
        tx_desc_list[tail].cmd    = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;

        pci_e1000[E1000_TDT] = (tail + 1) & TX_PTR_MSK;

        //cprintf("[transmit pkt]TDH, %x, TDT, %x, status: %x\n\n", pci_e1000[E1000_TDH], pci_e1000[E1000_TDT], tx_desc_list[tail].status);
        return 0;
    }
    else {
        return -E_TX_BUF_FULL;
    }
}



void receive_init() {

    int i;
    /* Program the Recieve address Registers (RAL/RAH) with the desired 
     * Ethernet addres
     */
    pci_e1000[E1000_RAL0] = 0x12005452;
    pci_e1000[E1000_RAH0] = E1000_RAH_AV | 0x5634;

    /* Allocate a region of memory for the receive descriptor list.
     * Software should insure this memory is aligned ona paragraph boundary.
     * Program the Receive Descriptor Base Address (RDBAL/RDBAH) registers
     * with the address of the region.
     */

    // Hw can only recognize pa, should be physical addr
    pci_e1000[E1000_RDBAL] = PADDR(rx_desc_list);
    pci_e1000[E1000_RDBAH] = 0x0;
    //cprintf("rx_desc_list:%x, h:%x\n", pci_e1000[E1000_RDBAL], pci_e1000[E1000_RDBAH]);
    

    /* Set the Receive Descriptor Length (RDLEN) register to the size (in bytes)
     * of descriptor ring.
     * This register must be 128-byte aligned
     */

    // set small value, 0x80, 8 descriptors, 128 bytes
    pci_e1000[E1000_RDLEN] = RX_DESC_LEN;
    
    /* The Receive Descriptor Head and Tail registers initialized (by hardware) to 0b 
     * after a power-on or a software-initialized Ethernet controller reset. Receive 
     * buffers of approriate size should be allocated and pointers to these buffers
     * should be stored in the receive descriptor ring.
     * Software initializes the Receive Descriptor Head (RDH) register and 
     * Receive Descriptor Tail (RDT) with the appropriate head and tail addresses. 
     * Head should point to the first valid receive descriptor in the descriptor ring 
     * and tail should point to one descriptor beyond the last valid descriptor
     * in the descriptor ring
     */
    pci_e1000[E1000_RDH] = 0x0;
    pci_e1000[E1000_RDT] = 0x80 - 1;

    /* Program the Receive Control (RCTL) register with appropriate values for
     * desired operation to include:
     *     1. Set the receiver Enable (RCTL.EN) to 1b for normal operation.
     *     2. Set the Long packet Enable bit to 1b when processing packets
     *        greater than the standard Ethernet packet size 
     *     Note: 2 is not needed since the buffer pointed to by the descripter
     *           is greater than the max packet size
     *     3. Loopback Mode (RCTL.LBM) should be set to 00b for normal operation
     *     4. Configure the Receive minimum threshold size (RCTL.RDMTS) bits
     *        to the desired value.
     *     5. Configure the Multicase Offset (RCTL.MO) bits to the desired value
     *     6. Set the Broadcase Accept Mode (RCTL.BAM) bit to 1b allowing the 
     *        hardware to accept broadcast packets.
     *     7. Configure the Receive Buffer Size (RCTL.BSIZE) bits to reflect the 
     *        size of the receive buffers software provides to hardware.
     *        Also configure the Buffer Extension Size (RCTL.BSEX) bits if receive
     *        buffers needs to be larger than 2048 bytes
     *     8. Set the Strip Ethernet CRC (RCTL.SECRC) bit if the desire is for 
     *        hardware to strip the CRC prior to DMA-ing the receive packet to host
     *        memory
     */
    pci_e1000[E1000_RCTL] = E1000_RCTL_EN |
                            E1000_RCTL_LBM_NO |
                            E1000_RCTL_BAM |
                            E1000_RCTL_SZ_2048 |
                            E1000_RCTL_SECRC;

    //cprintf("RCTL:%x\n", pci_e1000[E1000_RCTL]);


    /* Reserve memory for memory buffers
     *     1. maximus size of an Ethernet packet is 1518 byte, there are 8 descriptors
     *     2. buffer memory should be contiguous in physical memory
     * so
     *     1. each page (4096 bytes) can have 2 buffers at most
     *     2. need 4 pages for 8 descriptors
     */
    for (i = 0; i < RX_DESC_SIZE; i = i + 2){
        rx_desc_list[i].addr   = page2pa(page_alloc(ALLOC_ZERO));
        rx_desc_list[i+1].addr = rx_desc_list[i].addr + PGSIZE/2;
        //cprintf("i:%x, addr: %x\n", i, rx_desc_list[i].addr);
        //cprintf("i+1:%x, next_addr:%x \n", i+1, rx_desc_list[i+1].addr);
    }
    cprintf("Receive Initialization Done!\n");

}

int receive_pkt (uint16_t* length, char* rx_data) {
    
    int tail, next_tail, head;
    uint8_t* pkt_buf;

    int i;

    tail = pci_e1000[E1000_RDT];
    head = pci_e1000[E1000_RDH];

    next_tail = (tail + 1) & RX_PTR_MSK;
    pkt_buf   = KADDR(rx_desc_list[next_tail].addr);
    if ((rx_desc_list[next_tail].status & E1000_RXD_STAT_DD) == E1000_RXD_STAT_DD) {
        //cprintf("[receive_pkt]next_tail:%x, status:%x\n", next_tail, rx_desc_list[next_tail].status);

        rx_desc_list[next_tail].status = 0x0;
        *length   = rx_desc_list[next_tail].length;
        for (i = 0; i < rx_desc_list[next_tail].length; i++) {
            rx_data[i] = pkt_buf[i];
        }
        pci_e1000[E1000_RDT] = next_tail;

        //cprintf("[receive_pkt]head:%x,tail:%x\n", pci_e1000[E1000_RDH], pci_e1000[E1000_RDT]);
        return 0;
    }
    else {
        return -E_RX_BUF_EMPTY;
    }

}
