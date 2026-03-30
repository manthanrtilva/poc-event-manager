// clanformat off
// clang-format -i -style=llvm `find . -name '*.cpp' -o -name '*.h'` && cmake
// --build build valgrind --tool=helgrind --suppressions=valgrind.supp
// ./build/poc-event-manager
// clang-format on

#include "EventPoller.h"
#include "IManager.h"
#include "PipedEvent.h"
#include "promethues-simple-server.h"

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
class OpenTracing {};

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
  std::list<Event::UPtr> txQueue{};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;
  std::array<struct epoll_event, 10> events;
  std::mutex txMutex{};

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual void _runImpl() override {
    // LOG_DEBUG_TRACE("ManagerA running main loop\n");
    // const auto count =
    //     poller.Wait(events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    // if (count > 0) {
    // } else {
    //   auto now = std::chrono::steady_clock::now();
    //   if (now - lastEventTime > std::chrono::milliseconds(EVENT_TIMEOUT_MS))
    //   {
    if (txQueue.size() > MAX_QUEUE_SIZE) {
      _prometheus->IncrementCounter(_eventTxDroppedCounter);
      return;
    }
    auto event = std::make_unique<EventA>(random_int_in_range(0, 100));
    std::lock_guard<std::mutex> lock(txMutex);
    txQueue.emplace_back(
        std::make_unique<EventA>(random_int_in_range(100, 200)));
    _prometheus->IncrementCounter(_eventTxCounter);
    txEvent.Fire();
    //     lastEventTime = now;
    //   }
    // }
  }
  virtual void _preRunImpl() override {
    // LOG_DEBUG_TRACE("ManagerA pre-run initialization\n");
  }
  virtual void Consume(std::list<Event::UPtr> &&events) override {
    // LOG_DEBUG_TRACE("ManagerA consuming batch of events\n");
    for (auto &event : events) {
      Consume(std::move(event));
    }
  }
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
    // LOG_DEBUG_TRACE("ManagerA consuming single event of type {}\n",
    // event->type);
  }
  virtual std::list<Event::UPtr> Produce() override {
    // LOG_DEBUG_TRACE("ManagerA producing events\n");
    std::list<Event::UPtr> events;
    std::lock_guard<std::mutex> lock(txMutex);
    events.swap(txQueue);
    txEvent.Consume();
    return events;
  }
  // virtual std::string_view _threadName() const { return {}; }
  virtual std::string_view _threadName() const { return "ManagerA"; }
};
class ManagerB : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  std::list<Event::UPtr> txQueue{};
  std::mutex txMutex{};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;
  std::array<struct epoll_event, 10> events;

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual void _runImpl() override {
    // LOG_DEBUG_TRACE("ManagerA running main loop\n");
    // const auto count =
    //     poller.Wait(events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    // if (count > 0) {
    // } else {
    //   auto now = std::chrono::steady_clock::now();
    //   if (now - lastEventTime > std::chrono::milliseconds(EVENT_TIMEOUT_MS))
    //   { auto event = std::make_unique<EventA>(random_int_in_range(0, 100));
    if (txQueue.size() > MAX_QUEUE_SIZE) {
      _prometheus->IncrementCounter(_eventTxDroppedCounter);
      return;
    }
    std::lock_guard<std::mutex> lock(txMutex);
    txQueue.emplace_back(
        std::make_unique<EventB>(random_int_in_range(100, 200)));
    _prometheus->IncrementCounter(_eventTxCounter);
    txEvent.Fire();
    //     lastEventTime = now;
    //   }
    // }
  }
  virtual void _preRunImpl() override {
    // LOG_DEBUG_TRACE("ManagerA pre-run initialization\n");
  }
  virtual void Consume(std::list<Event::UPtr> &&events) override {
    // LOG_DEBUG_TRACE("ManagerA consuming batch of events\n");
    for (auto &event : events) {
      Consume(std::move(event));
    }
  }
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
    // LOG_DEBUG_TRACE("ManagerA consuming single event of type {}\n",
    // event->type);
    // txQueue.emplace_back(std::move(event));
    // txEvent.Fire();
  }
  virtual std::list<Event::UPtr> Produce() override {
    // LOG_DEBUG_TRACE("ManagerA producing events\n");
    std::list<Event::UPtr> events;
    std::lock_guard<std::mutex> lock(txMutex);
    events.swap(txQueue);
    txEvent.Consume();
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerB"; }
};
class ManagerC : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  std::list<Event::UPtr> txQueue{};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;
  std::array<struct epoll_event, 10> events;
  std::mutex txMutex{};

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual void _runImpl() override {
    // LOG_DEBUG_TRACE("ManagerA running main loop\n");
    const auto count =
        poller.Wait(events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    if (count > 0) {
    } else {
      // auto now = std::chrono::steady_clock::now();
      // if (now - lastEventTime > std::chrono::milliseconds(1000)) {
      //   //   auto event = std::make_unique<EventA>(random_int_in_range(0,
      //   100)); txQueue.emplace_back(
      //       std::make_unique<EventB>(random_int_in_range(100, 200)));
      //   txEvent.Fire();
      //   lastEventTime = now;
      // }
    }
  }
  virtual void _preRunImpl() override {
    // poller
    // LOG_DEBUG_TRACE("ManagerA pre-run initialization\n");
  }
  virtual void Consume(std::list<Event::UPtr> &&events) override {
    // LOG_DEBUG_TRACE("ManagerA consuming batch of events\n");
    for (auto &event : events) {
      Consume(std::move(event));
    }
  }
  virtual void Consume(Event::UPtr &&event) override {
    std::lock_guard<std::mutex> lock(txMutex);
    txQueue.emplace_back(std::move(event));
    _prometheus->IncrementCounter(_eventRxCounter);
    _prometheus->IncrementCounter(_eventTxCounter);
    txEvent.Fire();
  }
  virtual std::list<Event::UPtr> Produce() override {
    // LOG_DEBUG_TRACE("ManagerA producing events\n");
    std::list<Event::UPtr> events;
    std::lock_guard<std::mutex> lock(txMutex);
    events.swap(txQueue);
    txEvent.Consume();
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerC"; }
};
std::atomic<bool> running{true};
void signal_handler(int signum) { running.store(false); }
#include <sys/epoll.h>
int main() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  PrometheusSimpleServer prometheusServer;

  auto managerA = std::make_shared<ManagerA>();
  auto managerB = std::make_shared<ManagerB>();
  auto managerC = std::make_shared<ManagerC>();
  EventPoller<IProducer<Event::UPtr>::WPtr, IConsumer<Event::UPtr>::WPtr>
      poller;
  poller.Add(EPOLLIN, managerA->GetSources(), managerA, managerC);
  poller.Add(EPOLLIN, managerB->GetSources(), managerB, managerC);
  poller.Add(EPOLLIN, managerC->GetSources());
  managerA->SetPrometheus(&prometheusServer);
  managerB->SetPrometheus(&prometheusServer);
  managerC->SetPrometheus(&prometheusServer);
  managerA->Run();
  // managerB->Run();
  managerC->Run();
  auto eventsProducedCounter = prometheusServer.AddCounter(
      "events_produced_total", "Total number of events produced");
  prometheusServer.Start();
  std::array<struct epoll_event, 10> events;
  while (running.load()) {
    const auto count =
        poller.Wait(events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    if (count > 0) {
      for (size_t i = 0; i < count; ++i) {
        if (events[i].data.fd == managerC->GetSources()) {
          for (auto &event : managerC->Produce()) {
            if (event->type == EventTypeA) {
              managerA->Consume(std::move(event));
              // LOG_DEBUG_TRACE("ManagerC produced EventA with data: {}\n",
              // static_cast<EventA*>(event.get())->dataA);
            } else if (event->type == EventTypeB) {
              managerB->Consume(std::move(event));
              // LOG_DEBUG_TRACE("ManagerC produced EventB with data: {}\n",
              // static_cast<EventB*>(event.get())->dataB);
            }
            prometheusServer.IncrementCounter(eventsProducedCounter);
          }
        } else {
          poller.Execute(events[i].data.fd);
        }
      }
    }
  }
  managerA->Stop();
  managerB->Stop();
  managerC->Stop();
  return 0;
}