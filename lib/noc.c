#include <ucx.h>


#define PANIC_OOM 0
inline uint32_t panic(uint32_t code){
  asm("nop");
  return 0;
}



// externs
uint16_t pktdrv_ports[MAX_TASKS];
struct queue_s* pktdrv_tqueue[MAX_TASKS];
struct queue_s* pktdrv_queue;
int32_t (*pktdrv_callback)(uint16_t *buf);

// Gets the address of the current NoC node. Nodes are identified by
// their XY address, which is compressed in a 32-bit word such that
// XY = (X << 16) | Y;
uint32_t ucx_noc_nodeid(void)
{
  uint32_t id = *((uint32_t *)NOC_NODE_ADDR);
  return id;
}

// Initializes a queue of packets using one of the available ports. May a 
// port be unavailable, the requested queue initialization will fail
// with ERR_COMM_UNFEASIBLE.
uint32_t ucx_noc_comm_create(uint16_t port)
{
  int task_id = ucx_task_id();

  // check whether this task has allocated a port before
  if (pktdrv_tqueue[task_id] != NULL) return ERR_COMM_UNFEASIBLE;

  // port already in use by another task
  for (int k = 0; k < MAX_TASKS; k++)
    if (pktdrv_ports[k] == port)
      return ERR_COMM_ERROR;

  // add a new queue to receive packets for that task
  pktdrv_tqueue[task_id] = ucx_queue_create(NOC_PACKET_SLOTS);

  // if we cannot create a new queue due to unavailable memory
  // space, return ERR_OUT_OF_MEMORY
  if (pktdrv_tqueue[task_id] == 0)
    return ERR_OUT_OF_MEMORY;
  
  // connect the port to the task and return ERR_OK
  pktdrv_ports[task_id] = port;
  return ERR_OK;
}

// Destroys a connection, removing the 
int32_t hf_comm_destroy()
{
  int task_id = ucx_task_id();
  int32_t status;

  status = _di();

  // removes all elements from the comm queue, adding them 
  // to the queue of shared packets
  while (ucx_queue_count(pktdrv_tqueue[task_id]))
    ucx_queue_enqueue(pktdrv_queue, ucx_queue_dequeue(pktdrv_tqueue[task_id]));
  
  _ei(status);

  // check whether the comm queue was gracefully destroyed. If not,
  // return ERR_COMM_ERROR
  if (ucx_queue_destroy(pktdrv_tqueue[task_id])) return ERR_COMM_ERROR;
  
  pktdrv_ports[task_id] = 0;
  return ERR_OK;
}


// Initializes the driver
void ni_init(void)
{
  //kprintf("\nKERNEL: this is core #%d", hf_cpuid());
  //printf("\nKERNEL: NoC queue init, %d packets", NOC_PACKET_SLOTS);

  pktdrv_queue = ucx_queue_create(NOC_PACKET_SLOTS);
  if (pktdrv_queue == NULL)
    panic(PANIC_OOM);

  for (int32_t i = 0; i < MAX_TASKS; i++)
    pktdrv_ports[i] = 0;

  // for (int32_t i = 0; i < NOC_PACKET_SLOTS; i++)
  // {
  //   void* ptr = hf_malloc(sizeof(int16_t) * NOC_PACKET_SIZE);
  //   if (ptr == NULL) panic(PANIC_OOM);
  //   ucx_queue_enqueue(pktdrv_queue, ptr);
  // }

  uint32_t im = _di();
  int32_t i = ni_flush(0); // NOC_PACKET_SIZE
  _ei(im);

  if (i)
  {
    // ! _irq_register(IRQ_NOC_READ, (funcptr)ni_isr);
    // ! _irq_mask_set(IRQ_NOC_READ);
    
    
    //kprintf("\nKERNEL: NoC driver registered");
  }
  else
  {
    //kprintf("\nKERNEL: NoC NI init failed");
    panic(1);
  }
}

/**
 * @brief NoC driver: network interface interrupt service routine.
 *
 * This routine is called by the second level of interrupt handling. An interrupt from the network
 * interface means a full packet has arrived. The packet header is decoded and the target port is
 * identified. A reference to an empty packet is removed from the pool of buffers (packets), the
 * contents of the empty packet are filled with flits from the hardware queue and the reference is
 * put on the target task (associated to a port) queue of packets. There is one queue per task of
 * configurable size (individual queues are elastic if size is zero, limited to the size of free
 * buffer elements from the common pool). If port 0xffff (65535) is used as the target, the packet
 * is passed to a callback. This mechanism can be used to build custom OS functions (such as user
 * defined protocols, RPC or remote system calls). Port 0 is used as a discard function, for testing
 * purposes.
 */

void ni_isr(void *arg)
{
  int32_t k;
  uint16_t *buf_ptr;

  // Since we know the size of the received packet, we ask the driver to copy only
  // the necessary bytes. The size of the packet is written by the NI to the
  // sig_recv_status signal (see drv). We need to know the size of the packet before
  // receiving it, so it can be flushed case no more room is available
  uint16_t act_pkt_size = ni_get_next_size();

  // get an slot from the global queue
  buf_ptr = ucx_queue_dequeue(pktdrv_queue);

  // if slot is valid (is NULL when no slot can be acquired)
  if (buf_ptr)
  {

    // Since we know the size of the received packet, we ask the driver to copy
    // only the necessary bytes. The size of the packet is written by the NI to
    // the sig_recv_status signal (see drv).
    ni_read_packet(buf_ptr, act_pkt_size);

    // get rid of packets which address is not the same of this cpu
    if (buf_ptr[PKT_TARGET_CPU] != ucx_noc_nodeid())
    {
      //kprintf("\nKERNEL: hardware error: this is not CPU X:%d Y:%d", (buf_ptr[PKT_TARGET_CPU] & 0xf0) >> 4, buf_ptr[PKT_TARGET_CPU] & 0xf);
      ucx_queue_enqueue(pktdrv_queue, buf_ptr);
      return;
    }

    // get the queue associated to the port indicated in the packet
    for (k = 0; k < MAX_TASKS; k++)
      if (pktdrv_ports[k] == buf_ptr[PKT_TARGET_PORT])
        break;

    // check whether some task is running for that port
    //if (k < MAX_TASKS && krnl_tcb[k].ptask)
    {

      // check whether the task has room for more packets in that queue
      if (ucx_queue_enqueue(pktdrv_tqueue[k], buf_ptr))
      {
        //kprintf("\nKERNEL: task (on port %d) queue full! dropping packet...", buf_ptr[PKT_TARGET_PORT]);
        ucx_queue_enqueue(pktdrv_queue, buf_ptr);
      }

      // no task is running for that port, return the buffer to the global queue
      // and drop the packet.
    }
    //else
    //{
      //f("\nKERNEL: no task on port %d (offender: cpu %d port %d) - dropping packet...", buf_ptr[PKT_TARGET_PORT], buf_ptr[PKT_SOURCE_CPU], buf_ptr[PKT_SOURCE_PORT]);
    //  ucx_queue_enqueue(pktdrv_queue, buf_ptr);
    //}

    // no more space in the global queue, flush
  }
  else
  {
    //kprintf("\nKERNEL: NoC queue full! dropping packet...");
    ni_flush(act_pkt_size);
  }

  return;
}

/**
 * @brief Probes for a message from a task.

 * @return channel of the first message that is waiting in queue (a value >= 0), ERR_COMM_EMPTY when no messages are
 * waiting in queue, ERR_COMM_UNFEASIBLE when no message queue (comm) was created.
 *
 * Asynchronous communication is possible using this primitive, as it first tests if there is data
 * ready for reception with hf_recv() which is a blocking primitive. The main advantage of using hf_recvprobe()
 * along with hf_recv() is that a selective receive can be performed in the right communication channel. As
 * the message is waiting at the beginning of the queue, a receive on its channel can be used to process the
 * messages in order, avoiding packet loss.
 */
int32_t hf_recvprobe(void)
{
  uint16_t id;
  int32_t k;
  uint16_t *buf_ptr;

  id = ucx_task_id();
  if (pktdrv_tqueue[id] == NULL)
    return ERR_COMM_UNFEASIBLE;

  k = ucx_queue_count(pktdrv_tqueue[id]);
  if (k)
  {
    buf_ptr = ucx_queue_peek(pktdrv_tqueue[id]);
    if (buf_ptr)
      if (buf_ptr[PKT_CHANNEL] != 0xffff)
        return buf_ptr[PKT_CHANNEL];
  }

  return ERR_COMM_EMPTY;
}

/**
 * @brief Receives a message from a task (blocking receive).
 *
 * @param source_cpu is a pointer to a variable which will hold the source cpu
 * @param source_port is a pointer to a variable which will hold the source port
 * @param buf is a pointer to a buffer to hold the received message
 * @param size a pointer to a variable which will hold the size (in bytes) of the received message
 * @param channel is the selected message channel of this message (must be the same as in the sender)
 *
 * @return ERR_OK when successful, ERR_COMM_UNFEASIBLE when no message queue (comm) was
 * created and ERR_SEQ_ERROR when received packets arrive out of order, so the message
 * is corrupted.
 *
 * A message is build from packets received on the ni_isr() routine. Packets are decoded and
 * combined in a complete message, returning the message, its size and source identification
 * to the calling task. The buffer where the message will be stored must be large enough or
 * we will have a problem that may not be noticed before its too late.
 */
int32_t hf_recv(uint16_t *source_cpu, uint16_t *source_port, int8_t *buf, uint16_t *size, uint16_t channel)
{
  uint16_t id, seq = 0, packet = 0, packets, payload_bytes;
  uint32_t status;
  int32_t i, k, p = 0, error = ERR_OK;
  uint16_t *buf_ptr;

  // printf("hf_recv: 0x%x\n", (uint32_t)buf);

  id = ucx_task_id();

  if (pktdrv_tqueue[id] == NULL)
    return ERR_COMM_UNFEASIBLE;

  while (1)
  {
    k = ucx_queue_count(pktdrv_tqueue[id]);
    if (k)
    {
      buf_ptr = ucx_queue_peek(pktdrv_tqueue[id]);
      if (buf_ptr)
        if (buf_ptr[PKT_CHANNEL] == channel && buf_ptr[PKT_SEQ] == seq + 1)
          break;

      status = _di();
      buf_ptr = ucx_queue_dequeue(pktdrv_tqueue[id]);
      ucx_queue_enqueue(pktdrv_tqueue[id], buf_ptr);
      _ei(status);
    }
  }

  // printf("queue size: 0x%x\n", ucx_queue_count(pktdrv_tqueue[id]);

  status = _di();
  buf_ptr = ucx_queue_dequeue(pktdrv_tqueue[id]);
  _ei(status);

  *source_cpu = buf_ptr[PKT_SOURCE_CPU];
  *source_port = buf_ptr[PKT_SOURCE_PORT];
  *size = buf_ptr[PKT_MSG_SIZE];
  seq = buf_ptr[PKT_SEQ];


  // 4 bytes per flit
  uint32_t packet_size = PKT_MSG_SIZE * 4; 

  payload_bytes = (packet_size - PKT_HEADER_SIZE) * sizeof(uint16_t);
  packets = (*size % payload_bytes == 0) ? (*size / payload_bytes) : (*size / payload_bytes + 1);

  while (++packet < packets)
  {
    if (buf_ptr[PKT_SEQ] != seq++)
      error = ERR_SEQ_ERROR;

    for (i = PKT_HEADER_SIZE; i < packet_size; i++)
    {
      buf[p++] = (uint8_t)(buf_ptr[i] >> 8);
      buf[p++] = (uint8_t)(buf_ptr[i] & 0xff);
    }
    status = _di();
    ucx_queue_enqueue(pktdrv_queue, buf_ptr);
    _ei(status);

    i = 0;
    while (1)
    {
      k = ucx_queue_count(pktdrv_tqueue[id]);
      if (k)
      {
        buf_ptr = ucx_queue_peek(pktdrv_tqueue[id]);
        if (buf_ptr)
          if (buf_ptr[PKT_CHANNEL] == channel && buf_ptr[PKT_SEQ] == seq)
            break;

        status = _di();
        buf_ptr = ucx_queue_dequeue(pktdrv_tqueue[id]);
        ucx_queue_enqueue(pktdrv_tqueue[id], buf_ptr);
        _ei(status);
        if (i++ > NOC_PACKET_SLOTS << 3)
          break;
      }
    }
    status = _di();
    buf_ptr = ucx_queue_dequeue(pktdrv_tqueue[id]);
    _ei(status);
  }

  if (buf_ptr[PKT_SEQ] != seq++)
    error = ERR_SEQ_ERROR;

  // TODO: treat endianess up to the size of the packet
  // for (i = PKT_HEADER_SIZE; i < packet_size && p < *size; i++){
  for (i = PKT_HEADER_SIZE; i < buf_ptr[PKT_PAYLOAD] + 1 && p < *size; i++)
  {
    buf[p++] = (uint8_t)(buf_ptr[i] >> 8);
    buf[p++] = (uint8_t)(buf_ptr[i] & 0xff);
  }

  status = _di();
  ucx_queue_enqueue(pktdrv_queue, buf_ptr);
  _ei(status);

  return error;
}

/**
 * @brief Sends a message to a task (blocking send).
 *
 * @param target_cpu is the target processor
 * @param target_port is the target task port
 * @param buf is a pointer to a buffer that holds the message
 * @param size is the size (in bytes) of the message
 * @param channel is the selected message channel of this message (must be the same as in the receiver)
 *
 * @return ERR_OK
 *
 * A message is broken into packets containing a header and part of the message as the payload.
 * The packets are injected, one by one, in the network through the network interface.
 */
int32_t hf_send(uint16_t target_cpu, uint16_t target_port, int8_t *buf, uint16_t size, uint16_t channel)
{
  #define NOC_PACKET_SIZE 64

  uint16_t packet = 0, packets, payload_bytes, id;
  int32_t i, p = 0;
  uint16_t out_buf[NOC_PACKET_SIZE];

  id = ucx_task_id();
  if (pktdrv_tqueue[id] == NULL)
    return ERR_COMM_UNFEASIBLE;

  payload_bytes = (NOC_PACKET_SIZE - PKT_HEADER_SIZE) * sizeof(uint16_t);
  packets = (size % payload_bytes == 0) ? (size / payload_bytes) : (size / payload_bytes + 1);

  while (++packet < packets)
  {
    out_buf[PKT_TARGET_CPU] = ucx_noc_nodeid();
    out_buf[PKT_PAYLOAD] = NOC_PACKET_SIZE - 2;
    out_buf[PKT_SOURCE_CPU] = id;
    out_buf[PKT_SOURCE_PORT] = pktdrv_ports[id];
    out_buf[PKT_TARGET_PORT] = target_port;
    out_buf[PKT_MSG_SIZE] = size;
    out_buf[PKT_SEQ] = packet;
    out_buf[PKT_CHANNEL] = channel;

    for (i = PKT_HEADER_SIZE; i < NOC_PACKET_SIZE; i++, p += 2)
      out_buf[i] = ((uint8_t)buf[p] << 8) | (uint8_t)buf[p + 1];

    ni_write_packet(out_buf, NOC_PACKET_SIZE);
  }

  out_buf[PKT_TARGET_CPU] = ucx_noc_nodeid();

  // Last packet, which most of the time is the only packet, must have size equals
  // to the number of flits in the payload plus two, instead of NOC_PACKET_SIZE.
  // COMMENTED OUT >> out_buf[PKT_PAYLOAD] = NOC_PACKET_SIZE - 2;
  out_buf[PKT_SOURCE_CPU] = ucx_task_id();
  out_buf[PKT_SOURCE_PORT] = pktdrv_ports[id];
  out_buf[PKT_TARGET_PORT] = target_port;
  out_buf[PKT_MSG_SIZE] = size;
  out_buf[PKT_SEQ] = packet;
  out_buf[PKT_CHANNEL] = channel;

  for (i = PKT_HEADER_SIZE; i < NOC_PACKET_SIZE && (p < size); i++, p += 2)
    out_buf[i] = ((uint8_t)buf[p] << 8) | (uint8_t)buf[p + 1];

  // Actual size of payload fixed here (total data minus the first two flits).
  out_buf[PKT_PAYLOAD] = i - 2;

  // We can save some processing time by leaving garbage at the end of the packet.
  // Since we never read from these addresses, there is no reason to fill them up.
  // COMMENTED OUT >> for(; i < NOC_PACKET_SIZE; i++)
  // COMMENTED OUT >>     out_buf[i] = 0xdead;

  // Here, we configure the NI to send the actual size of data instead of
  // a whole packet.
  // COMMENTED OUT >> ni_write_packet(out_buf, NOC_PACKET_SIZE);
  // uint32_t im = _di();
  ni_write_packet(out_buf, i);
  //_ei(im);

  // CPU becomes stalled during network operations, so there is no need to hold
  // the process.
  // COMMENTED OUT >> delay_ms(1);

  return ERR_OK;
}

/**
 * @brief Receives a message from a task (blocking receive) with acknowledgement.
 *
 * @param source_cpu is a pointer to a variable which will hold the source cpu
 * @param source_port is a pointer to a variable which will hold the source port
 * @param buf is a pointer to a buffer to hold the received message
 * @param size a pointer to a variable which will hold the size (in bytes) of the received message
 * @param channel is the selected message channel of this message (must be the same as in the sender)
 *
 * @return ERR_OK when successful, ERR_COMM_UNFEASIBLE when no message queue (comm) was
 * created and ERR_SEQ_ERROR when received packets arrive out of order, so the message
 * is corrupted.
 *
 * A message is build from packets received on the ni_isr() routine. Packets are decoded and
 * combined in a complete message, returning the message, its size and source identification
 * to the calling task. The buffer where the message will be stored must be large enough or
 * we will have a problem that may not be noticed before its too late. After the reception
 * of the whole message is completed, an acknowledgement is sent to the sender task. This works
 * as a flow control mechanism, avoiding buffer/queue overflows common to the raw protocol.
 * Message channel 32767 will be used for the flow control mechanism. This routine must be used
 * exclusively with hf_sendack().
 */
int32_t hf_recvack(uint16_t *source_cpu, uint16_t *source_port, int8_t *buf, uint16_t *size, uint16_t channel)
{
  int32_t error;

  error = hf_recv(source_cpu, source_port, buf, size, channel);
  if (error == ERR_OK)
  {
    hf_send(*source_cpu, *source_port, "ok", 3, 0x7fff);
  }

  return error;
}

/**
 * @brief Sends a message to a task (blocking send) with acknowledgement.
 *
 * @param target_cpu is the target processor
 * @param target_port is the target task port
 * @param buf is a pointer to a buffer that holds the message
 * @param size is the size (in bytes) of the message
 * @param channel is the selected message channel of this message (must be the same as in the receiver)
 * @param timeout is the time (in ms) that the sender will wait for a reception acknowledgement
 *
 * @return ERR_OK
 *
 * A message is broken into packets containing a header and part of the message as the payload.
 * The packets are injected, one by one, in the network through the network interface. After that, the
 * sender will wait for an acknowledgement from the receiver. This works as a flow control mechanism,
 * avoiding buffer/queue overflows common to the raw protocol. Message channel 32767 will be used for
 * the flow control mechanism. This routine should be used exclusively with hf_recvack().
 */
int32_t hf_sendack(uint16_t target_cpu, uint16_t target_port, int8_t *buf, uint16_t size, uint16_t channel, uint32_t timeout)
{
  uint16_t id, source_cpu, source_port;
  int32_t error, k;
  uint32_t time;
  int8_t ack[4];
  uint16_t *buf_ptr;

  error = hf_send(target_cpu, target_port, buf, size, channel);
  if (error == ERR_OK)
  {
    id = ucx_task_id();
    time = _read_us() / 1000;
    while (1)
    {
      k = ucx_queue_count(pktdrv_tqueue[id]);
      if (k)
      {
        buf_ptr = ucx_queue_peek(pktdrv_tqueue[id]);
        if (buf_ptr)
          if (buf_ptr[PKT_CHANNEL] == 0x7fff && buf_ptr[PKT_MSG_SIZE] == 3)
            break;
      }
      if (((_read_us() / 1000) - time) > timeout)
        return ERR_COMM_TIMEOUT;
    }
    hf_recv(&source_cpu, &source_port, ack, &size, 0x7fff);
  }

  return error;
}
