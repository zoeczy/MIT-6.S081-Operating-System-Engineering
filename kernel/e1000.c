#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// 配置E1000直接从RAM中transmit/receive packet （DMA）
// buffer in e1000
// array of descriptor(一个地址) in ram 
// 使用mbufalloc（）分配mbuf packet buffers 
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

// top部分，是用户进程，或者内核的其他部分调用的接口。
// 一般有read/write接口，这些接口可以被更高层级的代码调用。
int
e1000_transmit(struct mbuf *m)
{
  // 内存->网卡buf
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  acquire(&e1000_lock);
  uint32 transmit_ring_tail = regs[E1000_TDT];
  if((tx_ring[transmit_ring_tail].status & E1000_TXD_STAT_DD )== 0)
    // check if the the ring is overflowing.
    // 如果没有设置DD标志位, e1000没有完成之前的传输
    {release(&e1000_lock);return -1;}
  else if (tx_mbufs[transmit_ring_tail] != 0)
    //tx_ring在上一个环中存buf的位置，释放掉，再存新的。
    {mbuffree(tx_mbufs[transmit_ring_tail]);}

  tx_mbufs[transmit_ring_tail] = m;
  tx_ring[transmit_ring_tail].addr = (uint64)m->head;
  tx_ring[transmit_ring_tail].length = m->len;
  tx_ring[transmit_ring_tail].status = tx_ring[transmit_ring_tail].status | E1000_TXD_STAT_DD;
  // EOP 表示该 buffer 含有一个完整的 packet
  // RS 告诉网卡在发送完成后，设置 status 中的 E1000_TXD_STAT_DD 位，表示发送完成。
  // 其他cmd位感觉是硬件设置
  tx_ring[transmit_ring_tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

  // -----------------------------checksum-----------------------------------
  // checksum字段是packet中的一部分
  // checksum计算在Ethernet controller上执行
  // 假设IPOFLD（ip_offload）有效，controller计算IP的checksum并提示一个pass/fail状态给软件（软件设置recv.descriptor.error位）
  // 相似的，有TCP/UDP checksum
  // -------------------------------------------------------------------------

  //ensure that each mbuf is eventually freed, but only after the E1000 has finished transmitting the packet 
  //(the E1000 sets the E1000_TXD_STAT_DD bit in the descriptor to indicate this). 
  regs[E1000_TDT] = (transmit_ring_tail + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}


// 注意recv()中不需要lock，因为recv()是在bottom half的interrupt handler中，
// 只有一个进程在跑这个handler，因此不存在共享的数据结构。
static void
e1000_recv(void)
{
  // 网卡buf-内存
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).

  // TODO:detect when received packets are available

  // uint32 recv_ring_head = regs[E1000_RDH];
  while(1){
    uint32 recv_ring_tail = (regs[E1000_RDT]+ 1) % RX_RING_SIZE;
    if((rx_ring[recv_ring_tail].status & E1000_RXD_STAT_DD) ==0)
      // 检查是否有新的packet等待接收
      {break;}
    // Update length & Deliver the mbuf to the network stack
    struct mbuf* m = rx_mbufs[recv_ring_tail];
    // rx_ring[recv_ring_tail].length= rx_mbufs[recv_ring_tail]->len;
    rx_mbufs[recv_ring_tail]->len = rx_ring[recv_ring_tail].length;
    net_rx(m);
    
    // alloc new buffer in rx_buf 代替刚刚已经传给内存之后没用的buf
    struct mbuf* m_new = mbufalloc(0);
    rx_mbufs[recv_ring_tail] = m_new;
    // link rx_buf in rc_ring(description)
    rx_ring[recv_ring_tail].addr = (uint64)rx_mbufs[recv_ring_tail]->head;
    rx_ring[recv_ring_tail].status = 0;  // clear
    //
    regs[E1000_RDT] = recv_ring_tail;
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
