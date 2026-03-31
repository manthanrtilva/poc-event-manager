#ifndef __lockefreequeue_h__
#define __lockefreequeue_h__

#include <atomic>
#include <list>
#include <utility>
#include <valgrind/helgrind.h>

template <typename T> class LockeFreeQueue {
  struct Node {
    T data;
    std::atomic<Node *> next;
    template <typename... Args>
    Node(Args &&...args)
        : data(std::forward<Args>(args)...), next(nullptr) {}
  };

  Node *head_;
  std::atomic<Node *> tail_;

public:
  LockeFreeQueue() : head_(new Node()), tail_(head_) {}

  ~LockeFreeQueue() {
    Node *node = head_;
    while (node) {
      Node *next = node->next.load(std::memory_order_relaxed);
      delete node;
      node = next;
    }
  }

  LockeFreeQueue(const LockeFreeQueue &) = delete;
  LockeFreeQueue &operator=(const LockeFreeQueue &) = delete;

  template <typename... Args> void emplace_back(Args &&...args) {
    Node *newNode = new Node(std::forward<Args>(args)...);
    Node *prev = tail_.exchange(newNode, std::memory_order_acq_rel);
    ANNOTATE_HAPPENS_BEFORE(&prev->next);
    prev->next.store(newNode, std::memory_order_release);
  }

  void swap(std::list<T> &out) {
    Node *cur = head_;
    Node *next = cur->next.load(std::memory_order_acquire);
    while (next) {
      ANNOTATE_HAPPENS_AFTER(&cur->next);
      delete cur;
      cur = next;
      out.emplace_back(std::move(cur->data));
      next = cur->next.load(std::memory_order_acquire);
    }
    head_ = cur;
  }
};

#endif // __lockefreequeue_h__