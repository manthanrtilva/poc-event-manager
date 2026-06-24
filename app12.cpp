#include "EventPoller.h"
#include "IManager.h"
#include "PipedEvent.h"
#include "promethues-simple-server.h"
#include <rigtorp/SPSCQueue.h>

#include <array>
#include <csignal>
#include <iostream>
#include <mutex>
#include <random>
#include <sys/epoll.h>

#define POLL_WAIT_TIMEOUT_MS 1
#define EVENT_TIMEOUT_MS 1
#define MAX_QUEUE_SIZE 3500000
std::atomic<std::int32_t> AutoId::counter{0};
class Event {
public:
  enum Type {
    STATUS,
    CONTROL,
    ERROR,
    SERIAL,
  };
  using UPtr = std::unique_ptr<Event>;
  Event(Type type, std::int32_t target) : type(type), target(target) {}
  Event(const Event &e) = delete;
  Event &operator=(const Event &e) = delete;
  virtual UPtr Clone() const = 0;
  virtual ~Event() = default;
  const Type type;
  const std::int32_t target{0};
  virtual std::string to_string() const {
    return fmt::format("Event{{type:{}, target:{}}}", static_cast<int>(type),
                       target);
  }
};
class StatusEvent : public Event {
public:
  StatusEvent(std::int32_t target, std::string &&json)
      : Event{Type::STATUS, target}, json{std::move(json)} {}
  UPtr Clone() const override {
    std::string copy = json;
    return std::make_unique<StatusEvent>(target, std::move(copy));
  }
  virtual std::string to_string() const override {
    return fmt::format("StatusEvent{{{}, json:{}}}", Event::to_string(), json);
  }

private:
  const std::string json;
};
class ControlEvent : public Event {
public:
  ControlEvent(std::int32_t target, std::string &&json)
      : Event{Type::CONTROL, target}, json{std::move(json)} {}
  UPtr Clone() const override {
    std::string copy = json;
    return std::make_unique<ControlEvent>(target, std::move(copy));
  }
  virtual std::string to_string() const override {
    return fmt::format("ControlEvent{{{}, json:{}}}", Event::to_string(), json);
  }

private:
  const std::string json;
};
class SerialEvent : public Event {
public:
  SerialEvent(std::vector<std::uint8_t> &&data)
      : Event{Type::SERIAL, 0}, data{std::move(data)} {}
  UPtr Clone() const override {
    std::vector<std::uint8_t> copy{data};
    return std::make_unique<SerialEvent>(std::move(copy));
  }
  virtual std::string to_string() const override {
    return fmt::format("SerialEvent{{{}, data_size:{}}}", Event::to_string(),
                       data.size());
  }

private:
  const std::vector<std::uint8_t> data;
};

// like component manager
class ManagerA : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  rigtorp::SPSCQueue<Event::UPtr> txQueue{MAX_QUEUE_SIZE};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;
  std::uint32_t counter{0};

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual std::list<Event::Type> WillConsume() const override {
    return {Event::Type::CONTROL, Event::Type::SERIAL};
  }
  virtual void _runImpl() override {
    if (counter++ % 2 == 0) {
      auto event = std::make_unique<StatusEvent>(0, R"({"status":"ok"})");
      std::cout << "ManagerA produced event: " << event->to_string()
                << std::endl;

      if (txQueue.try_emplace(std::move(event))) {
        _prometheus->IncrementCounter(_eventTxCounter);
        txEvent.Fire();
      } else {
        _prometheus->IncrementCounter(_eventTxDroppedCounter);
      }
    } else {
      auto event =
          std::make_unique<SerialEvent>(std::vector<std::uint8_t>{0, 1, 2, 3});
      std::cout << "ManagerA produced event: " << event->to_string()
                << std::endl;
      if (txQueue.try_emplace(std::move(event))) {
        _prometheus->IncrementCounter(_eventTxCounter);
        txEvent.Fire();
      } else {
        _prometheus->IncrementCounter(_eventTxDroppedCounter);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  virtual void _preRunImpl() override {}
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
    std::cout << "ManagerA consumed event: " << event->to_string() << std::endl;
  }
  virtual std::list<Event::UPtr> Produce() override {
    std::list<Event::UPtr> events;
    txEvent.Consume();
    Event::UPtr event;
    while (txQueue.empty() == false) {
      events.push_back(std::move(*txQueue.front()));
      txQueue.pop();
    }
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerA"; }
};

// like serial manager
class ManagerB : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  rigtorp::SPSCQueue<Event::UPtr> txQueue{MAX_QUEUE_SIZE};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual std::list<Event::Type> WillConsume() const override {
    return {Event::Type::SERIAL};
  }
  virtual void _runImpl() override {
    auto event =
        std::make_unique<SerialEvent>(std::vector<std::uint8_t>{0, 1, 2, 3});
    std::cout << "ManagerB produced event: " << event->to_string() << std::endl;
    if (txQueue.try_emplace(std::move(event))) {
      _prometheus->IncrementCounter(_eventTxCounter);
      txEvent.Fire();
    } else {
      _prometheus->IncrementCounter(_eventTxDroppedCounter);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  virtual void _preRunImpl() override {}
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
    std::cout << "ManagerB consumed event: " << event->to_string() << std::endl;
  }
  virtual std::list<Event::UPtr> Produce() override {
    std::list<Event::UPtr> events;
    txEvent.Consume();
    Event::UPtr event;
    while (txQueue.empty() == false) {
      events.push_back(std::move(*txQueue.front()));
      txQueue.pop();
    }
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerB"; }
};
// like mqtt manager
class ManagerC : public IManager<Event::UPtr> {
  PipedEvent txEvent{};
  rigtorp::SPSCQueue<Event::UPtr> txQueue{MAX_QUEUE_SIZE};
  std::chrono::steady_clock::time_point lastEventTime{
      std::chrono::steady_clock::now()};
  EventPoller<void *, void *> poller;

public:
  std::int32_t GetSources() override { return txEvent.GetSource(); }
  virtual std::list<Event::Type> WillConsume() const override {
    return {Event::Type::STATUS};
  }
  virtual void _runImpl() override {
    auto event = std::make_unique<ControlEvent>(0, R"({"status":"ok"})");
    std::cout << "ManagerC produced event: " << event->to_string() << std::endl;
    if (txQueue.try_emplace(std::move(event))) {
      _prometheus->IncrementCounter(_eventTxCounter);
      txEvent.Fire();
    } else {
      _prometheus->IncrementCounter(_eventTxDroppedCounter);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
  virtual void _preRunImpl() override {}
  virtual void Consume(Event::UPtr &&event) override {
    _prometheus->IncrementCounter(_eventRxCounter);
    std::cout << "ManagerC consumed event: " << event->to_string() << std::endl;
  }
  virtual std::list<Event::UPtr> Produce() override {
    std::list<Event::UPtr> events;
    txEvent.Consume();
    Event::UPtr event;
    while (txQueue.empty() == false) {
      events.push_back(std::move(*txQueue.front()));
      txQueue.pop();
    }
    return events;
  }
  virtual std::string_view _threadName() const { return "ManagerC"; }
};
class EventRouter {
public:
  EventRouter() {
    _fd = epoll_create1(0);
    if (_fd == -1) {
      auto msg = fmt::format("epoll_create1 failed with {}({})",
                             strerror(errno), errno);
      throw std::runtime_error(msg);
    }
  }
  void AddProducer(std::int32_t fd, IProducer<Event::UPtr>::WPtr producer) {
    producers[fd] = producer;
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
      auto msg =
          fmt::format("epoll_ctl failed with {}({})", strerror(errno), errno);
      throw std::runtime_error(msg);
    }
  }
  void AddConsumer(std::list<Event::Type> types,
                   IConsumer<Event::UPtr>::WPtr consumer) {
    for (auto &type : types) {
      consumers[type].push_back(consumer);
    }
  }
  size_t WaitAndRoute() {
    const auto count =
        epoll_wait(_fd, events.data(), events.size(), POLL_WAIT_TIMEOUT_MS);
    if (count > 0) {
      for (size_t i = 0; i < count; ++i) {
        const int fd = events[i].data.fd;
        // Find the producer and consumer associated with this fd and route
        // events
        if (auto it = producers.find(fd); it != producers.end()) {
          if (auto producer = it->second.lock()) {
            for (auto &event : producer->Produce()) {
              if (auto consumerIt = consumers.find(event->type);
                  consumerIt != consumers.end()) {
                for (auto &consumerWPtr : consumerIt->second) {
                  if (auto consumer = consumerWPtr.lock();
                      consumer &&
                      consumer->GetAutoId() != producer->GetAutoId()) {
                    consumer->Consume(event->Clone());
                  }
                }
              }
            }
          }
        }
      }
    }
    return count;
  }

private:
  std::array<struct epoll_event, 10> events;
  std::unordered_map<int, IProducer<Event::UPtr>::WPtr> producers;
  std::unordered_map<Event::Type, std::list<IConsumer<Event::UPtr>::WPtr>>
      consumers;
  int _fd{0};
};
std::atomic<bool> running{true};
void signal_handler(int signum) { running.store(false); }
int main(int argc, const char **argv) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  PrometheusSimpleServer prometheusServer;
  auto managerA = std::make_shared<ManagerA>();
  auto managerB = std::make_shared<ManagerB>();
  auto managerC = std::make_shared<ManagerC>();
  EventRouter router;

  router.AddProducer(managerA->GetSources(), managerA);
  router.AddProducer(managerB->GetSources(), managerB);
  router.AddProducer(managerC->GetSources(), managerC);
  router.AddConsumer(managerA->WillConsume(), managerA);
  router.AddConsumer(managerB->WillConsume(), managerB);
  router.AddConsumer(managerC->WillConsume(), managerC);

  managerA->SetPrometheus(&prometheusServer);
  managerA->Run();
  managerB->SetPrometheus(&prometheusServer);
  managerB->Run();
  managerC->SetPrometheus(&prometheusServer);
  managerC->Run();
  prometheusServer.Start();
  std::array<struct epoll_event, 10> events;
  while (running.load()) {
    auto count = router.WaitAndRoute();
    if (count == -1 && errno != EINTR) {
      // auto msg = fmt::format("epoll_wait failed with {}({})",
      // strerror(errno), errno); throw std::runtime_error(msg);
      running.store(false);
    } else if (count == 0) {
      // timeout, can perform other tasks like trim database
    }
  }
  managerA->Stop();
  managerB->Stop();
  managerC->Stop();
  return 0;
}
