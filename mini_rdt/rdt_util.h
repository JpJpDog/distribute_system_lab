#ifndef _UTIL_H_
#define _UTIL_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rdt_struct.h"

const int kWindowSize = 10;
const int kMaxSeqN = 30;
const int kInitBufferSize = 1024 * 16;
const double kWaitTime = 0.25;
const int kHeaderSize = 5;
const int kPayloadSize = RDT_PKTSIZE - kHeaderSize;
const int kAckSize = 20;

short MakeChecksum(char *other, int size);

bool CheckChecksum(char *ch, int size);

inline int r_minus(int a, int b, int K) { return (a - b + K) % K; }

inline int r_add(int a, int b, int K) { return (a + b) % K; }

class VirtualTimer {
  struct Record {
    int key_;
    double end_t_;
    void (*work_)(int);
    Record() {}
    Record(int k, double t, void (*w)(int)) : key_(k), end_t_(t), work_(w) {}
  };
  struct RecordList {
    Record r_;
    RecordList *next_;
    RecordList() : next_(nullptr) {}
    RecordList(int k, double t, void (*w)(int)) : r_(k, t, w), next_(nullptr) {}
  };
  const int max_timer_n_;
  int timer_n_;
  RecordList *head_, *tail_;

 public:
  VirtualTimer(int max_timer_n) : max_timer_n_(max_timer_n), timer_n_(0), head_(new RecordList()), tail_(head_) {}
  ~VirtualTimer() {
    RecordList *r = head_, *r2;
    while (r) {
      r2 = r->next_;
      delete r;
      r = r2;
    }
  }
  void start(int key, double time, void (*work)(int));
  void end(int key);
  void timeout();
};

class StringManager {
 public:
  virtual ~StringManager() {}
  virtual bool empty() const = 0;
  virtual int last_size() const = 0;
  virtual void pop(int n) = 0;
  virtual void copy(char *dst, int n) const = 0;
  virtual char *last_data() const = 0;
};

class PlainStringManager : public StringManager {
  char *data_;
  const int size_;
  int cursor_;

 public:
  PlainStringManager(char *data, int size) : data_(data), size_(size), cursor_(0) {}
  inline virtual bool empty() const override { return cursor_ >= size_; }
  inline virtual int last_size() const override { return size_ - cursor_; }
  inline virtual void pop(int n) override { cursor_ += n; }
  inline virtual void copy(char *dst, int n) const override { memcpy(dst, data_ + cursor_, n); }
  inline virtual char *last_data() const override { return data_ + cursor_; }
};

// 一个循环的缓冲区，存从上层传来的但因为congestion control不能发出去的数据
class CircleQueueBuffer : public StringManager {
  int max_buf_size_;
  char *buf_;
  int top_, base_;
  void enlarge();

 public:
  CircleQueueBuffer(int max_buf_size)
      : max_buf_size_(max_buf_size), buf_(new char[max_buf_size + 1]), top_(0), base_(0) {}
  ~CircleQueueBuffer() { delete[] buf_; }
  void push(char *data, int size);
  virtual void copy(char *dst, int n) const override;
  inline virtual bool empty() const override { return top_ == base_; }
  inline virtual int last_size() const override { return r_minus(top_, base_, max_buf_size_ + 1); }
  inline virtual void pop(int n) override { base_ = r_add(base_, n, max_buf_size_ + 1); }
  inline virtual char *last_data() const {
    ASSERT(false);  //不应该用到这个
    return nullptr;
  };
};

#endif