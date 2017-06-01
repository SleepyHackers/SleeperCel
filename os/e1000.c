#include "types.h"
#include "serial.h"
#include "pci.h"

#define INTEL_VEND     0x8086  // Vendor ID for Intel
#define E1000_DEV      0x100E  // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs
#define E1000_I217     0x153A  // Device ID for Intel I217
#define E1000_82577LM  0x10EA  // Device ID for Intel 82577LM


// I have gathered those from different Hobby online operating systems instead of getting them one by one from the manual

#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_CTRL_EXT    0x0018
#define REG_IMASK       0x00D0
#define REG_RCTRL       0x0100
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818

#define REG_TCTRL       0x0400
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818


#define REG_RDTR         0x2820 // RX Delay Timer Register
#define REG_RXDCTL       0x3828 // RX Descriptor Control
#define REG_RADV         0x282C // RX Int. Absolute Delay Timer
#define REG_RSRPD        0x2C00 // RX Small Packet Detect Interrupt



#define REG_TIPG         0x0410      // Transmit Inter Packet Gap
#define ECTRL_SLU        0x40        //set link up


#define RCTL_EN                         (1 << 1)    // Receiver Enable
#define RCTL_SBP                        (1 << 2)    // Store Bad Packets
#define RCTL_UPE                        (1 << 3)    // Unicast Promiscuous Enabled
#define RCTL_MPE                        (1 << 4)    // Multicast Promiscuous Enabled
#define RCTL_LPE                        (1 << 5)    // Long Packet Reception Enable
#define RCTL_LBM_NONE                   (0 << 6)    // No Loopback
#define RCTL_LBM_PHY                    (3 << 6)    // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF                 (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER              (1 << 8)    // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH               (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36                      (0 << 12)   // Multicast Offset - bits 47:36
#define RCTL_MO_35                      (1 << 12)   // Multicast Offset - bits 46:35
#define RCTL_MO_34                      (2 << 12)   // Multicast Offset - bits 45:34
#define RCTL_MO_32                      (3 << 12)   // Multicast Offset - bits 43:32
#define RCTL_BAM                        (1 << 15)   // Broadcast Accept Mode
#define RCTL_VFE                        (1 << 18)   // VLAN Filter Enable
#define RCTL_CFIEN                      (1 << 19)   // Canonical Form Indicator Enable
#define RCTL_CFI                        (1 << 20)   // Canonical Form Indicator Bit Value
#define RCTL_DPF                        (1 << 22)   // Discard Pause Frames
#define RCTL_PMCF                       (1 << 23)   // Pass MAC Control Frames
#define RCTL_SECRC                      (1 << 26)   // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))


// Transmit Command

#define CMD_EOP                         (1 << 0)    // End of Packet
#define CMD_IFCS                        (1 << 1)    // Insert FCS
#define CMD_IC                          (1 << 2)    // Insert Checksum
#define CMD_RS                          (1 << 3)    // Report Status
#define CMD_RPS                         (1 << 4)    // Report Packet Sent
#define CMD_VLE                         (1 << 6)    // VLAN Packet Enable
#define CMD_IDE                         (1 << 7)    // Interrupt Delay Enable


// TCTL Register

#define TCTL_EN                         (1 << 1)    // Transmit Enable
#define TCTL_PSP                        (1 << 3)    // Pad Short Packets
#define TCTL_CT_SHIFT                   4           // Collision Threshold
#define TCTL_COLD_SHIFT                 12          // Collision Distance
#define TCTL_SWXOFF                     (1 << 22)   // Software XOFF Transmission
#define TCTL_RTLC                       (1 << 24)   // Re-transmit on Late Collision

#define TSTA_DD                         (1 << 0)    // Descriptor Done
#define TSTA_EC                         (1 << 1)    // Excess Collisions
#define TSTA_LC                         (1 << 2)    // Late Collision
#define LSTA_TU                         (1 << 3)    // Transmit Underrun

#define e1k_NUM_RX_DESC 32
#define e1k_NUM_TX_DESC 8

typedef struct e1k_rx e1k_rx;
typedef struct e1k_tx e1k_tx;

struct e1k_rx {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint16_t checksum;
  volatile uint8_t status;
  volatile uint8_t errors;
  volatile uint16_t special;
} __attribute__((packed));

struct e1k_tx {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint8_t cso;
  volatile uint8_t cmd;
  volatile uint8_t status;
  volatile uint8_t css;
  volatile uint16_t special;
} __attribute__((packed));

uint8_t mmio_read8 (uint64_t p_address)
{
  return *((volatile uint8_t*)(p_address));
}
uint16_t mmio_read16 (uint64_t p_address)
{
  return *((volatile uint16_t*)(p_address));

}
uint32_t mmio_read32 (uint64_t p_address)
{
  return *((volatile uint32_t*)(p_address));

}
uint64_t mmio_read64 (uint64_t p_address)
{
  return *((volatile uint64_t*)(p_address));
}
void mmio_write8 (uint64_t p_address,uint8_t p_value)
{
  (*((volatile uint8_t*)(p_address)))=(p_value);
}
void mmio_write16 (uint64_t p_address,uint16_t p_value)
{
  (*((volatile uint16_t*)(p_address)))=(p_value);
}
void mmio_write32 (uint64_t p_address,uint32_t p_value)
{
  (*((volatile uint32_t*)(p_address)))=(p_value);

}
void mmio_write64 (uint64_t p_address,uint64_t p_value)
{
  (*((volatile uint64_t*)(p_address)))=(p_value);
}

/* void outportb (uint16_t p_port,uint8_t p_data)
 * 
 * This method outputs a byte to a hardware port.
 * It uses an inline asm with the volatile keyword
 * to disable compiler optimization.
 * 
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 * 
 * Notice the input constraint
 *      "dN" (port) : indicates using the DX register to store the 
 *                  value of port in it
 *      "a"  (data) : store the value of data into 
 * 
 * The above constraint will instruct the compiler to generate assembly
 * code that looks like that
 *      mov    %edi,%edx
 *      mov    %esi,%eax
 *      out    %eax,(%dx)
 * 
 * According the ABI, the edi will have the value of p_port and esi will have
 * the value of the p_data
 * 
 */
void outportb (uint16_t p_port,uint8_t p_data)
{
  asm volatile ("outb %1, %0" : : "dN" (p_port), "a" (p_data));
}

/* void outportw (uint16_t p_port,uint16_t p_data)
 * 
 * This method outputs a word to a hardware port.
 * 
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 * 
 */


void outportw (uint16_t p_port,uint16_t p_data)
{
  asm volatile ("outw %1, %0" : : "dN" (p_port), "a" (p_data));
}

/* void outportl (uint16_t p_port,uint32_t p_data)
 * 
 * This method outputs a double word to a hardware port.
 * 
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 * 
 */


extern void outportl (uint16_t p_port,uint32_t p_data);
 

/* uint8_t inportb( uint16_t p_port)
 * 
 * This method reads a byte from a hardware port.
 * 
 *  p_port: the port number to read the byte from.
 *  return value : a byte read from the port p_port.
 * 
 * Notice the output constraint "=a", this tells the compiler 
 * to expect the save the value of register AX into the variable l_ret
 * The register AX should contain the result of the inb instruction.
 * 
 * 
 */

uint8_t inportb( uint16_t p_port)
{
  uint8_t l_ret;
  asm volatile("inb %1, %0" : "=a" (l_ret) : "dN" (p_port));
  return l_ret;
}

/* uint16_t inportw( uint16_t p_port)
 * 
 * This method reads a word from a hardware port.
 * 
 *  p_port: the port number to read the word from.
 *  return value : a word read from the port p_port.
 * 
 */


uint16_t inportw( uint16_t p_port)
{
  uint16_t l_ret;
  asm volatile ("inw %1, %0" : "=a" (l_ret) : "dN" (p_port));
  return l_ret;
}


extern uint32_t inportl( uint16_t p_port);

typedef struct e1k e1k;
struct e1k {
  uint8_t bar_type;                    // Type of BOR0
  uint16_t io_base;                    // IO Base Address
  uint64_t  mem_base;                  // MMIO Base Address
  uint8_t eeprom_exists;               // A flag indicating if eeprom exists
  uint8_t mac [6];                     // A buffer for storing the mack address
  e1k_rx* rx_descs[E1000_NUM_RX_DESC]; // Receive Descriptor Buffers
  e1k_tx* tx_descs[E1000_NUM_TX_DESC]; // Transmit Descriptor Buffers
  uint16_t rx_cur;                     // Current Receive Descriptor Buffer
  uint16_t tx_cur;                     // Current Transmit Descriptor Buffer
};

// Send Commands and read results From NICs either using MMIO or IO Ports
void e1k_write_command(e1k* ecfg, uint16_t p_address, uint32_t p_value);
uint32_t e1k_read_command(e1k* ecfg, uint16_t p_address);
bool e1k_detect_eeprom(e1k* ecfg); // Return true if EEProm exist, else it returns false and set the eerprom_existsdata member
uint32_t e1k_read_eeprom(e1k* ecfg, uint8_t addr); // Read 4 bytes from a specific EEProm Address
int e1k_read_mac(e1k* ecfg);       // Read MAC Address
void e1k_start_link (e1k* ecfg);           // Start up the network
void e1k_rx_init(e1k* ecfg);               // Initialize receive descriptors an buffers
void e1k_tx_init(e1k* ecfg);               // Initialize transmit descriptors an buffers
void e1k_enable_interrupt(e1k *ecfg);      // Enable Interrupts
void e1k_handle_receive(e1k *ecfg);        // Handle a packet reception.

void e1k_write_command(e1k* ecfg, uint16_t p_address, uint32_t p_value) {
  if (!ecfg->bar_type) {
    mmio_write(ecfg->mem_base + p_address, p_value);
    return;
  }
  outportl(ecfg->io_base, p_address);
  outportl(ecfg->io_base + 4, p_value);
}

uint32_t e1k_read_command(e1k* ecfg, uint16_t p_address) {
  if (!ecfg->bar_type) {
    return mmio_read(ecfg->mem_base+p_address);
  }
  outportl(ecfg->io_base, p_address);
  inportl(ecfg->io_base + 4);
}

int e1k_detect_eeprom(e1k* ecfg) {
  uint32_t val = 0;
  uint16_t i;

  ecfg->eeprom_exists = 0; 
  e1k_write_command(ecfg, REG_EEPROM, 0x1);
  for (i = 0; i < 1000 && (ecfg->eeprom_exists != true); i++) {
    val = e1k_read_command(ecfg, REG_EEPROM);
    if (val & 0x10) {
      ecfg->eeprom_exists = 1;
      return 0;
    }
  }
  return 1;
}

uint32_t e1k_read_eeprom(e1k* ecfg, uint8_t addr) {
  uint16_t data = 0;
  uint32_t tmp = 0;

  if (ecfg->eeprom_exists) {
    e1k_write_command(ecfg, REG_EEPROM, (1) | ((uint32_t)(addr) << 8));
    while (!((tmp = e1k_read_command(REG_EEPROM)) & (1 << 4))) { continue; }
  }
  else {
    e1k_write_command(ecfg, REG_EEPROM, (1) | ((uint32_t)(addr) << 2));
    while (!((tmp = e1k_read_command(REG_EEPROM)) & (1 << 1))) { continue; }
  }
  data = (uint16_t)((tmp >> 16) & 0xFFFF);
  return data;
}

int e1k_read_mac(e1k* ecfg) {
  uint32_t temp;
  uint8_t* mem_base_mac8, i;
  uint32_t* mem_base_mac32;

  if (ecfg->eeprom_exists) {
    temp = e1k_read_eeprom(0);
    mac[0] = temp & oxff; mac[1] = temp >>8;
    temp = e1k_read_eeprom(0);
    mac[2] = temp & oxff; mac[3] = temp >>8;
    temp = e1k_read_eeprom(0);
    mac[4] = temp & oxff; mac[5] = temp >>8;
    return 0;
  }
  mem_base_mac8 = (uint8_t*)(mem_base+0x5400);
  mem_base_mac32 = (uint32_t*)(mem_base+0x5400);
  if (!mem_base_mac32[0]) { return -1; }
  for (i = 0; i < 6; i++) {
    mac[i] = mem_base_mac8[i];
  }
  return 0;
}

int e1k_rx_init(task_data* task, e1k* ecfg) {
  uint32_t i;
  e1k_rx* descs = malloc(sizeof(e1000_rx_desc) * E1000_NUM_RX_DESC + 16);
  if (!descs) { return -1; }
  for (i = 0; i < E1000_NUM_RX_DESC; i++) {
    ecfg->rx_descs[i] = descs + i * 16;
    ecfg->rx_descs[i]->addr = (uint64_t)(uint8_t*)malloc(8192 + 16);
    ecfg->rx_descs[i]->status = 0;
  }
  e1k_write_command(ecfg, REG_TXDESCLO, (uint32_t)((uint64_t)descs >> 32));
  e1k_write_command(ecfg, REG_TXDESCHI, (uint32_t)((uint64_t)descs & 0xFFFFFFFF));
  e1k_write_command(ecfg, REG_RXDESCLO, (uint64_t)descs);
  e1k_write_command(ecfg, REG_TXDESCHI, 0);
  e1k_write_command(ecfg, REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);
  e1k_write_command(ecfg, REG_RXDESCHEAD, 0);
  e1k_write_command(ecfg, REG_RXDESCTAIL, E1000_NUM_RX_DESC-1);
  ecfg->rx_cur = 0;
  e1k_write_command(ecfg,
                    REG_RCTRL, RCTL_EN | RCTL_SBP | RCTL_UPE |
                    RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM |
                    RCTL_SECRC | RCTL_BSIZE_2048);
  return 0;
}

int e1k_tx_init(task_data* task, e1k* ecfg) {
  uint32_t i;
  e1k_tx* descs = malloc(sizeof(e1000_tx_desc) * E1000_NUM_TX_DESC + 16);
  if (!descs) { return -1; }
  for (i = 0; i < E1000_NUM_TX_DESC; i++) {
    ecfg->tx_descs[i] = descs + i * 16;
    ecfg->tx_descs[i].addr = 0;
    ecfg->tx_descs[i].cmd = 0;
    ecfg->tx_descs[i].status = TSTA_DD;
  }

  e1k_write_command(ecfg, REG_TXDESCHI, (uint32_t)((uint64_t)descs >> 32));
  e1k_write_command(ecfg, REG_TXDESCLO, (uint32_t)((uint64_t)descs & 0xFFFFFFFF));
  e1k_write_command(ecfg, REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);
  e1k_write_command(ecfg, REG_TXDESCHEAD, 0);
  e1k_write_command(ecfg, REG_TXDESCTAIL, 0);
  ecfg->tx_cur = 0;
  e1k_write_command(ecfg, REG_TCTRL, 0b0110000000000111111000011111010);
  e1k_write_command(ecfg, REG_TIPG, 0x0060200A);
  return 0;
}

void e1k_enable_interrupt(e1k* ecfg) {
  e1k_write_command(ecfg, REG_IMASK, 0x1F6DC);
  e1k_write_command(ecfg, REG_IMASK, 0xff & ~4);
  e1k_read_command(ecfg, 0xc0);
}

e1k* new_e1k(pci_device* dev) {
  e1k* ecfg = kmalloc(sizeof(*ecfg));
  if (!ecfg) { return NULL; }

  
  
  
}
