#include "rdt_sender.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "rdt_struct.h"
#include "rdt_util.h"

// packet structure
// 2                  | 1       | 1                       | 1
// checksum | type | seq_number | pkt_size

//输入字符串，得到包
static void Sender_PktMake(StringManager *str, int seq_n, packet *pkt) {
  ASSERT(!str->empty());
  pkt->data[2] = 0;
  pkt->data[3] = seq_n;
  int last_size = str->last_size();
  if (last_size > kPayloadSize) {
    pkt->data[4] = kPayloadSize;
    str->copy(pkt->data + kHeaderSize, kPayloadSize);
    *(short *)pkt->data = MakeChecksum(pkt->data + 2, kPayloadSize + kHeaderSize - 2);
    str->pop(kPayloadSize);
  } else {
    pkt->data[4] = last_size;
    str->copy(pkt->data + kHeaderSize, last_size);
    *(short *)pkt->data = MakeChecksum(pkt->data + 2, last_size + kHeaderSize - 2);
    str->pop(last_size);
  }
}

//控制提供seqence number，备份没有ack的数据
class Sender_SlidingWindow {
 public:
  int base_, top_, base_seq_n_;
  struct Cell {
    bool status_;
    packet backup_;
    Cell() : status_(false) {}
  };
  Cell *cells_;

  Sender_SlidingWindow() : base_(0), top_(0), base_seq_n_(0), cells_(new Cell[kWindowSize + 1]) {}
  ~Sender_SlidingWindow() { delete[] cells_; }
  bool empty() const { return base_ == top_; }
  int send() {
    if (r_minus(top_, base_, kWindowSize + 1) >= kWindowSize) {  // full
      return -1;
    }
    int seq_n = r_add(base_seq_n_, r_minus(top_, base_, kWindowSize + 1), kMaxSeqN);
    cells_[top_].status_ = true;
    top_ = r_add(top_, 1, kWindowSize + 1);
    return seq_n;
  }
  void backup(packet *pkt, int seq_n) {
    int backup_i = r_add(base_, r_minus(seq_n, base_seq_n_, kMaxSeqN), kWindowSize + 1);
    ASSERT(cells_[backup_i].status_);
    memcpy(cells_[backup_i].backup_.data, pkt, sizeof(*pkt));
  }
  void get_backup(packet *pkt, int seq_n) const {
    int backup_i = r_add(base_, r_minus(seq_n, base_seq_n_, kMaxSeqN), kWindowSize + 1);
    ASSERT(cells_[backup_i].status_);
    memcpy(pkt, cells_[backup_i].backup_.data, sizeof(*pkt));
  }
  bool ack(int seq_n) {
    int tmp = r_minus(seq_n, base_seq_n_, kMaxSeqN);
    if (tmp >= kWindowSize) {
      return false;
    }
    int ack_i = r_add(base_, tmp, kWindowSize + 1);
    cells_[ack_i].status_ = false;
    if (ack_i != base_) {
      return false;
    }
    int i, counter = 1;
    for (i = r_add(ack_i, 1, kWindowSize + 1); !cells_[i].status_ && i != top_; i = r_add(i, 1, kWindowSize + 1)) {
      counter++;
    }
    base_ = i;
    base_seq_n_ = r_add(base_seq_n_, counter, kMaxSeqN);
    return true;
  }
};

static Sender_SlidingWindow *sliding_window;
static CircleQueueBuffer *buffer;
static VirtualTimer *virtual_timer;

/* sender initialization, called once at the very beginning */
void Sender_Init() {
  fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
  sliding_window = new Sender_SlidingWindow();
  buffer = new CircleQueueBuffer(kInitBufferSize);
  virtual_timer = new VirtualTimer(kWindowSize);
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final() {
  fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
  delete sliding_window;
  delete buffer;
  delete virtual_timer;
}

static void Sender_ReSend(int seq_n) {
  // fprintf(stdout, "#sender resend %d\n", seq_n);
  packet pkt;
  sliding_window->get_backup(&pkt, seq_n);
  virtual_timer->start(seq_n, kWaitTime, Sender_ReSend);
  Sender_ToLowerLayer(&pkt);
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg) {
  // fprintf(stdout, "#sender receive %d data\n", msg->size);
  if (!buffer->empty()) {
    // fprintf(stdout, "#sender buffer %d size\n", msg->size);
    ASSERT(!sliding_window->empty());
    buffer->push(msg->data, msg->size);
    return;
  }
  PlainStringManager data(msg->data, msg->size);
  packet pkt;
  int seq_n;
  while (!data.empty() && (seq_n = sliding_window->send()) != -1) {
    // fprintf(stdout, "#sender send %d\n", seq_n);
    Sender_PktMake(&data, seq_n, &pkt);
    sliding_window->backup(&pkt, seq_n);
    virtual_timer->start(seq_n, kWaitTime, Sender_ReSend);
    Sender_ToLowerLayer(&pkt);
  }
  if (!data.empty()) {
    // fprintf(stdout, "#send buffer %d size\n", data.last_size());
    buffer->push(data.last_data(), data.last_size());
  }
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *ack_pkt) {
  packet pkt;
  if (ack_pkt->data[2] != 1 || !CheckChecksum(ack_pkt->data, kAckSize)) return;
  int ack_seq_n = ack_pkt->data[3];
  // fprintf(stdout, "#sender receive %d ack", ack_seq_n);
  virtual_timer->end(ack_seq_n);
  if (!sliding_window->ack(ack_seq_n)) {
    return;  // ignore a invalid ack
  }
  // fprintf(stdout, ", #base is %d now\n", sliding_window->base_seq_n_);
  int seq_n;
  while (!buffer->empty()) {
    if ((seq_n = sliding_window->send()) == -1) break;
    // fprintf(stdout, "#sender from buffer send %d\n", seq_n);
    if (seq_n == 4) {
      static int debug_counter = 0;
      debug_counter++;
    }
    Sender_PktMake(buffer, seq_n, &pkt);
    sliding_window->backup(&pkt, seq_n);
    virtual_timer->start(seq_n, kWaitTime, Sender_ReSend);
    Sender_ToLowerLayer(&pkt);
  }
}

/* event handler, called when the timer expires */
void Sender_Timeout() { virtual_timer->timeout(); }
