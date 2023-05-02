//extra memories (DMA)
//@TODO maybe move these to the Configuration.mk file
#define ORCA_MEMORY_BASE_1 0x50000000
#define ORCA_MEMORY_SIZE_1 0x00000080
#define ORCA_MEMORY_BASE_2 0x50000080
#define ORCA_MEMORY_SIZE_2 0x00000080

//control wires (cpu <-> dma)
#define SIGNAL_CPU_STALL    0x40410000  /* 8 bits */
#define SIGNAL_CPU_INTR     0x40410001
#define SIGNAL_SEND_STATUS  0x40410002
//0x40410003
#define SIGNAL_RECV_STATUS  0x40410004
#define SIGNAL_RECV_ADDR    0x11111111
//0x40410005
//0x40410006
//0x40410007
#define SIGNAL_PROG_SEND    0x40410008
#define SIGNAL_PROG_RECV    0x40410009
//0x4041000A
//0x4041000B
#define SIGNAL_PROG_ADDR    0x4041000C  /* 32 bits */
#define SIGNAL_PROG_SIZE    0x40410010

//tile indentifier, hardware fixed
#define MAGIC_TILE_ID       0x40411000

#define IRQ_NOC_READ 0x100
#define NI_PACKET_SIZE 64

//raise/lower macroses
#define RAISE(x) *x=0x1
#define LOWER(x) *x=0x0

int32_t ni_ready(void);
int32_t ni_flush(uint16_t pkt_size);
int32_t ni_read_packet(uint16_t *buf, uint16_t pkt_size);
int32_t ni_write_packet(uint16_t *buf, uint16_t pkt_size);
int32_t ni_get_next_size();
