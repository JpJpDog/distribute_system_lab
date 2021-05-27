#include "rdt_util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rdt_sender.h"

short MakeChecksum(char *other, int size) {
  short checksum = 0;
  int off;
  for (off = 0; off + 1 < size; off += 2) {
    checksum += *(short *)(other + off);
  }
  if (off + 1 == size) {
    checksum += short(*(other + off)) << 8;
  }
  return ~checksum;
}

bool CheckChecksum(char *ch, int size) {
  short checksum = 0;
  int off;
  for (off = 0; off + 1 < size; off += 2) {
    checksum += *(short *)(ch + off);
  }
  if (off + 1 == size) {
    checksum += short(*(ch + off)) << 8;
  }
  return (~checksum) == 0;
}

void VirtualTimer::start(int key, double time, void (*work)(int)) {
  ASSERT(timer_n_ < max_timer_n_);
  timer_n_++;
  double cur_time = GetSimulationTime();
  double end_time = cur_time + time;
  RecordList *new_r = new RecordList(key, end_time, work), *r, *p;
  for (r = head_->next_, p = head_; r && r->r_.end_t_ < end_time; p = r, r = r->next_)
    ;
  p->next_ = new_r;
  new_r->next_ = r;
  if (p == tail_) {
    tail_ = new_r;
  }
  // fprintf(stdout, "#start timer %d\n", key);
  Sender_StartTimer(head_->next_->r_.end_t_ - cur_time);
}

void VirtualTimer::end(int key) {
  // fprintf(stdout, "#end timer %d\n", key);
  RecordList *r, *p;
  for (p = head_, r = p->next_; r && r->r_.key_ != key; p = r, r = r->next_)
    ;
  if (r == nullptr) return;
  bool end_head = (r == head_->next_);
  p->next_ = r->next_;
  timer_n_--;
  if (r == tail_) {
    tail_ = p;
  }
  if (end_head) {
    if (head_->next_) {
      Sender_StartTimer(head_->next_->r_.end_t_ - GetSimulationTime());
    } else {
      Sender_StopTimer();
    }
  }
}

void VirtualTimer::timeout() {
  // fprintf(stdout, "#timer %d timeout\n", head_->next_->r_.key_);
  RecordList *r = head_->next_;
  head_->next_ = r->next_;
  if (r == tail_) {
    tail_ = head_;
  }
  timer_n_--;
  r->r_.work_(r->r_.key_);
  if (head_ == tail_) {
    Sender_StopTimer();
  } else {
    Sender_StartTimer(head_->next_->r_.end_t_ - GetSimulationTime());
  }
}

void CircleQueueBuffer::push(char *data, int size) {
  ASSERT(r_minus(top_, base_, max_buf_size_) >= 0);
  // fprintf(stdout, "@ push %d %d, size %d\n", base_, top_, r_minus(top_, base_, max_buf_size_));
  while (r_minus(top_, base_, max_buf_size_ + 1) + size > max_buf_size_) {
    enlarge();
  }
  int top_to_end = max_buf_size_ + 1 - top_;
  if (size > top_to_end) {
    memcpy(buf_ + top_, data, top_to_end);
    memcpy(buf_, data + top_to_end, size - top_to_end);
  } else {
    memcpy(buf_ + top_, data, size);
  }
  top_ = r_add(top_, size, max_buf_size_ + 1);
}

void CircleQueueBuffer::copy(char *dst, int n) const {
  ASSERT(n <= max_buf_size_);
  int base_to_end = max_buf_size_ + 1 - base_;
  if (n > base_to_end) {
    memcpy(dst, buf_ + base_, base_to_end);
    memcpy(dst + base_to_end, buf_, n - base_to_end);
  } else {
    memcpy(dst, buf_ + base_, n);
  }
}
void CircleQueueBuffer::enlarge() {
  int new_max_size = max_buf_size_ * 2;
  char *new_buf = new char[new_max_size + 1];
  ASSERT(new_buf);
  if (top_ < base_) {
    int base_to_end = max_buf_size_ + 1 - base_;
    memcpy(new_buf, buf_ + base_, base_to_end);
    memcpy(new_buf + base_to_end, buf_, top_);
  } else {
    memcpy(new_buf, buf_ + base_, top_ - base_);
  }
  top_ = r_minus(top_, base_, max_buf_size_ + 1);
  base_ = 0;
  max_buf_size_ = new_max_size + 1;
  delete[] buf_;
  buf_ = new_buf;
}