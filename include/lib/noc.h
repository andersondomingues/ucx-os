#define ERR_INVALID_CPU -201     /*!< invalid cpu */
#define ERR_IF_NOT_READY -202    /*!< network interface is not ready */
#define ERR_COMM_TIMEOUT -203    /*!< a pending communication has timed out */
#define ERR_SEQ_ERROR -204       /*!< packet sequence mismatch */
#define ERR_COMM_BUSY -205       /*!< communication channel is busy */
#define ERR_COMM_UNFEASIBLE -206 /*!< communication is not feasible */
#define ERR_COMM_ERROR -207      /*!< general communication error */
#define ERR_COMM_EMPTY -208      /*!< reception queue is empty */
#define ERR_OUT_OF_MEMORY -209
#define ERR_OK -210

#define PKT_HEADER_SIZE 8
#define PKT_TARGET_CPU 0
#define PKT_PAYLOAD 1
#define PKT_SOURCE_CPU 2
#define PKT_SOURCE_PORT 3
#define PKT_TARGET_PORT 4
#define PKT_MSG_SIZE 5
#define PKT_SEQ 6
#define PKT_CHANNEL 7


#define NOC_PACKET_SLOTS 32


#define NOC_NODE_ADDR 0xe1000000
#define NOC_NODE_ADDR_X (NOC_NODE_ADDR >> 16)
#define NOC_NODE_ADDR_Y (NOC_NODE_ADDR & 0x0000FFFF)

// memory address where the incoming packet was stored.
// packets are stored in a rounding-robin fashion memory
// storage. If the storage is full,
#define NI_PACKET_ADDR 0xe1000004

// flagout the interruption, must be set to 0 after
// receiving the packet (IRQ down)
#define NI_PACKET_IRQA 0xe1000008

#define NOC_DRIVER_STORAGE_ZERO 0xe1001000
#define NOC_DRIVER_STORAGE_SIZE 0x00001000 // <<

#define MAX_TASKS 255

// #include <ucx.h>

// ucx_task_id()

/**
 * @brief Array of associations between tasks and reception ports.
 */
extern uint16_t pktdrv_ports[MAX_TASKS];

/**
 * @brief Array of queues. Each task can have its own custom sized queue.
 */
extern struct queue_s *pktdrv_tqueue[MAX_TASKS];

/**
 * @brief Queue of free (shared) packets. The number of packets is NOC_PACKET_SLOTS.
 */
extern struct queue_s *pktdrv_queue;

/**
 * @brief Callback function pointer. Called when PKT_TARGET_PORT is 0xffff.
 */
extern int32_t (*pktdrv_callback)(uint16_t *buf);

void ni_init(void);
void ni_isr(void *arg);

#ifdef CPP
extern "C"
{
#endif

  uint32_t ucx_noc_nodeid(void);

  uint32_t ucx_noc_comm_create(uint16_t port);
  uint32_t ucx_noc_comm_destroy(uint16_t port);

  int32_t hf_recvprobe(void);
  int32_t hf_recv(uint16_t *source_cpu, uint16_t *source_port, int8_t *buf, uint16_t *size, uint16_t channel);
  int32_t hf_send(uint16_t target_cpu, uint16_t target_port, int8_t *buf, uint16_t size, uint16_t channel);
  int32_t hf_recvack(uint16_t *source_cpu, uint16_t *source_port, int8_t *buf, uint16_t *size, uint16_t channel);
  int32_t hf_sendack(uint16_t target_cpu, uint16_t target_port, int8_t *buf, uint16_t size, uint16_t channel, uint32_t timeout);

#ifdef CPP
}
#endif
