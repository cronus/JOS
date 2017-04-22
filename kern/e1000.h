#include <kern/pci.h>

// bocui 
#define E1000_VENDER_ID 0x8086
#define E1000_DEVICE_ID 0x100e

#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#define E1000_STATUS   (0x00008/4)  /* Device Status - RO */

#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    (0x03804/4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818/4)  /* TX Descripotr Tail - RW */
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW */
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

/* Transmit Descriptor bit definitions */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04 /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10 /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40 /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80 /* Enable Tidv register */
#define E1000_TXD_STAT_DD    0x1 /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x2 /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x4 /* Late Collisions */
#define E1000_TXD_STAT_TU    0x8 /* Transmit underrun */

#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    (0x02804/4)  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW */
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW */
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW */
#define E1000_RAL0     (0x05400/4)  /* Receive Address Low */
#define E1000_RAH0     (0x05404/4)  /* Receive Address High */ 
#define E1000_RXDCTL   (0x02828/4)  /* RX Descriptor Control queue 0 - RW */

/* Receive Control */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */

/* Receive Address */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */
#define E1000_RXD_ERR_CE        0x01    /* CRC Error */

#endif	// JOS_KERN_E1000_H


void pci_func_enable(struct pci_func *f);

int e1000_func_enable(struct pci_func *pcif);
int transmit_pkt (uint16_t length, char* tx_pkt);
int receive_pkt (uint16_t* length, char* rx_data);

/*
Transimit descriptor (TDESC) Layout - Legacy Mode
   63          48 47   40 39   36 35   32 31  24 23   16 15             0
   +--------------------------------------------------------------------+
 0 |                       Buffer address                               |
   +-------------+-------+------+--------+------+-------+---------------+
 8 |  Special    |  CSS  | RSV  | Status |  Cmd |  CSO  |    Length     |
   +-------------+-------+------+--------+------+-------+---------------+
* 
* To select legacy descriptor, bit 29 (TDESC.DEXT) should be set to 0b.
* In this case the descriptor is defined as the table above.
*     Buffer Address: 
*         Address of the transimit descriptor in the host 
*         memory. If they have the RS bit in the command 
*         byte set set (TDESC.CMD), then the DD field in the
*         status word (TDESC.STATUS) is written when the
*         hardware processes them.
*     Length: 
*         The max length associated with any single legacy descriptor 
*         is 16288 bytes.
*     CSO (Checksum Offset):
*     CMD:
*             7       6       5       4       3       2       1       0 
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*         |  IDE  |  VLE  | DEXT  |  RPS  |  RS   |  IC   | IFCS  |  EOP  |
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*             IDE: Interrupt Delay Enable
*             VLE: VLAN Packet Enable
*             DEXT: extension (0 for legacy mode)
*             RPS: Report Packet Sent
*             RS: Report Status
*             IC: Insert Checksum
*             IFCS: Insert FCS
*             EOP: End of Packet
*     Stuats:
*             3       2       1       0
*         +-------+-------+-------+-------+
*         |  RSV  |  LC   |  EC   |   DD  |
*         +-------+-------+-------+-------+
*             LC: Late Collision
*             EC: Excess Collisions
*             DD: Descriptor Done
*     RSV (Reserved)
*     CSS (Checksum Start Field):
*     Special:
*/
struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

/*
Receive descriptor (RDESC) Layout
   63          48 47       40 39       32 31  24 23   16 15             0
   +--------------------------------------------------------------------+
 0 |                       Buffer address                               |
   +-------------+-------+---------------+--------------+---------------+
 8 |  Special    |Errors     | Status    |     CSO      |    Length     |
   +-------------+-------+---------------+--------------+---------------+
*     Stuats:
*             7       6       5       4       3       2       1       0 
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*         |  PIF  | IPCS  |TCPCS  |  RSV  |  VP   | IXSM  | EOP   |   DD  |
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*             PIF:   passed in-exact filter
*             IPCS:  IP checksum Calculated on Packet
*             TCPCS: TCP Checksum Caculated on Packet
*             RSV:   reserved
*             VP:    Packet is 802.1Q
*             IXSM:  Ignore Checksum Indication
*             EOP:   End of Packet
*             DD:    Description Done
*     Errors:
*             7       6       5       4       3       2       1       0 
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*         |  RXE  | IPE   | TCPE  |  RSV  |  RSV  |  RSV  |  SE   |   CE  |
*         +-------+-------+-------+-------+-------+-------+-------+-------+
*             RXE:  RX Data Error
*             IPE:  IP Checksum Error
*             TCPE: TCP/UDP Checksum Error
*             SE:   Symbol Error
*             CE:   CRC Error or aligment Error
*/
struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t cso;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
};
