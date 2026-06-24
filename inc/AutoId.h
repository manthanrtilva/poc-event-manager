#ifndef __autoid_h__
#define __autoid_h__
#include <atomic>
class AutoId {
  static std::atomic<std::int32_t> counter;
  const std::int32_t id{counter.fetch_add(1, std::memory_order_relaxed)};

public:
  AutoId() {}
  std::int32_t GetAutoId() const { return id; }
};
#endif // __autoid_h__