#include "rdt_receiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "rdt_struct.h"
#include "rdt_util.h"

class Receiver_SlidingWindow {
 public:
  struct Content {
    char data_[kPayloadSize];
    int size_;
    bool status_;
    Content() : status_(false) {}
  };
  Content *cells_;
  int base_, base_seq_n_;

  Receiver_SlidingWindow() : cells_(new Content[kWindowSize + 1]), base_(0), base_seq_n_(0) {}
  ~Receiver_SlidingWindow() { delete[] cells_; }
  int seq_to_index(int seq_n) {
    int d_seq = r_minus(seq_n, base_seq_n_, kMaxSeqN);
    if (d_seq >= kWindowSize) {
      d_seq = r_minus(base_seq_n_, seq_n, kMaxSeqN);
      if (d_seq <= kWindowSize) {
        return -1;
      } else {
        return -2;
      }
    } else {
      return r_add(base_, d_seq, kWindowSize + 1);
    }
  }
  void put(char *data, int size, int index) {
    auto &new_content = cells_[index];
    if (new_content.status_) return;  // sender loss the ack and resend the data
    new_content.status_ = true;
    new_content.size_ = size;
    memcpy(new_content.data_, data, size);
  }
  inline bool receive_base(int seq_n) { return seq_n == base_seq_n_; }
  void get_after_base(char *first_data, int first_size, char **out_data, int *out_size) {
    int last = r_add(base_, 1, kWindowSize + 1), size = first_size, counter = 1, off = first_size;
    for (; cells_[last].status_ && last != base_; last = r_add(last, 1, kWindowSize + 1)) {
      size += cells_[last].size_;
      cells_[last].status_ = false;
      counter++;
    }
    char *data = new char[size];
    memcpy(data, first_data, first_size);
    for (int i = r_add(base_, 1, kWindowSize + 1); i != last; i = r_add(i, 1, kWindowSize + 1)) {
      memcpy(data + off, cells_[i].data_, cells_[i].size_);
      off += cells_[i].size_;
    }
    base_ = last;
    base_seq_n_ = r_add(base_seq_n_, counter, kMaxSeqN);
    *out_size = size;
    *out_data = data;
  }
};

static Receiver_SlidingWindow *sliding_window;

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
  fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
  sliding_window = new Receiver_SlidingWindow();
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
  fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
  delete sliding_window;
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
  if (pkt->data[2] != 0 || !CheckChecksum(pkt->data, std::min(pkt->data[4] + kHeaderSize, RDT_PKTSIZE))) return;
  int seq_n = pkt->data[3], data_size = pkt->data[4];
  int index = sliding_window->seq_to_index(seq_n);
  if (index == -2) return;
  packet ack_pkt;
  ack_pkt.data[2] = 1;
  ack_pkt.data[3] = seq_n;
  *(short *)ack_pkt.data = MakeChecksum(ack_pkt.data + 2, kAckSize - 2);
  //后面16位是没有用的，不加的话ack包太短，加了checksum也不安全
  // fprintf(stdout, "#receiver send %d ack\n", seq_n);
  Receiver_ToLowerLayer(&ack_pkt);
  if (index == -1) return;
  struct message msg;
  if (sliding_window->receive_base(seq_n)) {
    // fprintf(stdout, "#receiver upload from %d, ", seq_n);
    sliding_window->get_after_base(pkt->data + kHeaderSize, data_size, &msg.data, &msg.size);
    // fprintf(stdout, "#base is %d now\n", sliding_window->base_seq_n_);
    Receiver_ToUpperLayer(&msg);
    delete[] msg.data;
  } else {
    // fprintf(stdout, "#receiver buffer %d\n", seq_n);
    sliding_window->put(pkt->data + kHeaderSize, data_size, index);
  }
}
