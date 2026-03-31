// clanformat off
// clang-format -i -style=llvm `find . -name '*.cpp' -o -name '*.h'` && cmake
// --build build valgrind --tool=helgrind --suppressions=valgrind.supp
// ./build/poc-event-manager
// clang-format on

#include "EventPoller.h"
#include "IManager.h"
#include "PipedEvent.h"
#include "promethues-simple-server.h"
#include "List.h"

#include <array>
#include <csignal>
#include <mutex>
#include <random>
#include <sys/epoll.h>

#define POLL_WAIT_TIMEOUT_MS 1
#define EVENT_TIMEOUT_MS 1
#define MAX_QUEUE_SIZE 5000
enum EventType {
  EventTypeA,
  EventTypeB,
};

struct Event {
  Event(EventType type) : type(type) {}
  const EventType type;
  using UPtr = std::unique_ptr<Event>;
  using SPtr = std::shared_ptr<Event>;
  Event(const Event &e) = delete;
  Event &operator=(const Event &e) = delete;
  virtual ~Event() {}
};
struct EventA : public Event {
  EventA(int data) : Event{EventTypeA}, dataA(data) {}
  int dataA{0};
};
struct EventB : public Event {
  EventB(int data) : Event{EventTypeB}, dataB(data) {}
  int dataB{0};
};
int random_int_in_range(int min, int max) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(min, max);
  return dis(gen);
}

class ManagerA : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  list<Event::UPtr> txQueue{};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;
  std::array<struct epoll_event, 10> events;

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual void _runImpl() override {
    auto event = std::make_unique<EventA>(random_int_in_range(0, 100));
    txQueue->emplace_back(
        std::make_unique<EventA>(random_int_in_range(100, 200)));
    _prometheus->IncrementCounter(_eventTxCounter);
    txEvent.Fire();
  }
  virtual void _preRunImpl() override {
  }
  virtual void Consume(std::list<Event::UPtr> &&events) override {
    for (auto &event : events) {
      Consume(std::move(event));
    }
  }
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
  }
  virtual std::list<Event::UPtr> Produce() override {
    std::list<Event::UPtr> events;
    txQueue->swap(events);
    // events.swap(txQueue);
    txEvent.Consume();
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerA"; }
};
std::atomic<bool> running{true};
void signal_handler(int signum) { running.store(false); }
int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  PrometheusSimpleServer prometheusServer;
  auto managerA = std::make_shared<ManagerA>();
  EventPoller<IProducer<Event::UPtr>::WPtr, IConsumer<Event::UPtr>::WPtr>
      poller;
  poller.Add(EPOLLIN, managerA->GetSources(), managerA, managerA);
  managerA->SetPrometheus(&prometheusServer);
  managerA->Run();
  prometheusServer.Start();
  std::array<struct epoll_event, 10> events;
  while (running.load()) {
    const auto count =
        poller.Wait(events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    if (count > 0) {
      for (size_t i = 0; i < count; ++i) {
        poller.Execute(events[i].data.fd);
      }
    }
  }
  managerA->Stop();
  return 0;
}
