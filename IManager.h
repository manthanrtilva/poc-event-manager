#ifndef __imanager_h__
#define __imanager_h__

#include "IConsumer.h"
#include "IProducer.h"

#include <atomic> // for atomic
#include <memory> // for unique_ptr, shared_ptr
#include <pthread.h>
#include <string_view>
#include <thread> // for thread

#include "IPrometheus.h"

/**
 * @brief Interface for manager components in the smart home system.
 *
 * The `IManager` class serves as an interface for components that manage the
 * smart home system. It extends both `IProducer` and `IConsumer`, allowing it
 * to produce and consume events. This interface is designed to facilitate the
 * management of various components within the smart home ecosystem, enabling
 * them to interact with each other through event production and consumption.
 *
 * @tparam E The type of event this manager can handle. Must be derived from
 *           or compatible with the Event base class.
 *
 * @note Managers run in their own thread and provide lifecycle management
 *       through Run() and Stop() methods.
 *
 * @example
 * @code
 * class MyManager : public IManager<MyEvent> {
 * public:
 *     std::int32_t GetSources() override {
 *         return myFileDescriptor;
 *     }
 *
 * private:
 *     void _runImpl() override {
 *         // Main loop logic
 *     }
 *
 *     void _preRunImpl() override {
 *         // Optional initialization
 *     }
 * };
 * @endcode
 */
template <typename E>
class IManager : public IProducer<E>, public IConsumer<E> {
protected:
  IPrometheus *_prometheus{nullptr};
  void *_eventRxCounter{nullptr};
  void *_eventTxCounter{nullptr};
  void *_eventTxDroppedCounter{nullptr};

public:
  void SetPrometheus(IPrometheus *prom) { _prometheus = prom; }
  /**
   * @brief Unique pointer type for IManager instances.
   */
  using UPtr = std::unique_ptr<IManager>;

  /**
   * @brief Shared pointer type for IManager instances.
   */
  using SPtr = std::shared_ptr<IManager>;

  /**
   * @brief Destructor for the IManager class.
   *
   * Checks if the manager is currently running and logs an error message if it
   * is. This helps detect improper shutdown sequences where Stop() was not
   * called before destruction.
   *
   * @warning If the manager is still running during destruction, an error
   *          message will be logged. Always call Stop() before destroying
   *          the manager instance.
   */
  virtual ~IManager() {
    if (_running.load()) {
      //   LOG_ERR("Thread running during destruction\n");
    }
  }

  /**
   * @brief Get the file descriptor for event notification.
   *
   * This function returns a file descriptor (FD) on which the Manager will
   * notify about events or state changes. This allows integration with event
   * loops, select/poll mechanisms, or other asynchronous I/O systems.
   *
   * @return std::int32_t The file descriptor for event notification
   *
   * @note The returned file descriptor should be valid and ready for
   *       monitoring (e.g., with select, poll, or epoll).
   */
  virtual std::int32_t GetSources() = 0;

  /**
   * @brief Start the manager's main control thread.
   *
   * This function starts a thread to run the main control logic. The thread
   * will first call _preRunImpl() for initialization, then continuously
   * call _runImpl() while _running is true.
   *
   * @note This method is non-blocking. The actual work is performed in
   *       a separate thread.
   * @note Only call this method once. Multiple calls may result in
   *       undefined behavior.
   *
   * @see Stop()
   * @see _preRunImpl()
   * @see _runImpl()
   */
  void Run() {
    _eventRxCounter = _prometheus->AddCounter(
        std::string{"eventRx"} + std::string{_threadName()},
        "Total number of times the manager has received events");
    _eventTxCounter = _prometheus->AddCounter(
        std::string{"eventTx"} + std::string{_threadName()},
        "Total number of times the manager has transmitted events");
    _eventTxDroppedCounter = _prometheus->AddCounter(
        std::string{"eventTxDropped"} + std::string{_threadName()},
        "Total number of times the manager has dropped events due to full "
        "queue");
    _thread = std::thread([this]() {
      auto name = _threadName();
      if (!name.empty()) {
        pthread_setname_np(pthread_self(), std::string(name).c_str());
      }
      _preRunImpl();
      while (_running.load()) {
        _runImpl();
      }
    });
  }

  /**
   * @brief Stop the manager and join the control thread.
   *
   * Ensures clean shutdown of the threaded loop started by Run().
   * Sets the _running flag to false and waits for the thread to complete.
   *
   * @note This method blocks until the thread has finished execution.
   * @note If the thread is not joinable, an error message will be logged.
   * @note It's safe to call this method multiple times.
   *
   * @see Run()
   */
  void Stop() {
    _running.store(false);
    if (_thread.joinable()) {
      _thread.join();
    } else {
      //   LOG_ERR("Thread is not joinable\n");
    }
  }

private:
  /**
   * @brief Main logic to be implemented by subclasses and run in the loop.
   *
   * This pure virtual function must be implemented by derived classes to define
   * the component's behavior inside the thread loop. This method is called
   * repeatedly while the manager is running.
   *
   * @note This method should not block indefinitely as it would prevent
   *       clean shutdown when Stop() is called.
   * @note Implementations should handle exceptions appropriately to prevent
   *       the thread from terminating unexpectedly.
   */
  virtual void _runImpl() = 0;

  /**
   * @brief Optional setup logic run once before the main loop.
   *
   * Can be overridden by derived classes to perform any necessary
   * initialization before _runImpl() starts looping. This method is called
   * exactly once when the thread starts, before entering the main loop.
   *
   * @note The default implementation does nothing.
   * @note This method runs in the manager's thread context.
   * @note Any exceptions thrown here will terminate the thread.
   */
  virtual void _preRunImpl() {}

  virtual std::string_view _threadName() const { return {}; }

  /**
   * @brief Thread for executing the main control loop.
   *
   * This thread executes the manager's main logic through _preRunImpl()
   * and repeated calls to _runImpl().
   */
  std::thread _thread{};

  /**
   * @brief Indicates whether the Manager is currently running or not.
   *
   * This flag controls the main loop execution. When set to false,
   * the thread will exit the loop and terminate.
   *
   * @note This flag is checked in the main loop condition.
   * @note Setting this to false will cause the thread to stop after
   *       the current iteration of _runImpl() completes.
   */
  std::atomic<bool> _running{true};
};

#endif // __imanager_h__
