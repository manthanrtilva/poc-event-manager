#ifndef __eventpoller__h__
#define __eventpoller__h__

#include <atomic>
#include <cstring>
#include <errno.h>
#include <poll.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <type_traits>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

template <typename T, typename E>
concept IsClassOrStruct = std::is_class_v<T> && std::is_class_v<E>;

/**
 * Wrapper around epoll api.
 * \ref https://man7.org/linux/man-pages/man7/epoll.7.html
 */
template <typename Producer, typename Consumer> class EventPoller {
public:
  /**
   * Create epoll instance fd
   * \ref https://man7.org/linux/man-pages/man2/epoll_create1.2.html
   */
  EventPoller() {
    _fd = epoll_create1(0);
    if (_fd == -1) {
      auto msg = fmt::format("epoll_create1 failed with {}({})",
                             strerror(errno), errno);
      throw std::runtime_error(msg);
    }
  }
  /**
   * Add fd to epoll watch list
   * \ref https://man7.org/linux/man-pages/man2/epoll_ctl.2.html
   * \param flags event flags
   * https://man7.org/linux/man-pages/man2/epoll_ctl.2.html
   * \param fd fd to be
   * watch by epoll
   * \param data pointer to user data to retrive when event happen
   * \return return value of epoll_ctl
   */
  int Add(int flags, int fd) {
    // LOG_DEBUG_TRACE("adding fd:{}\n", fd);
    int ret{0};
    if (fd != -1) {
      struct epoll_event event;
      event.events = flags;
      event.data.fd = fd;
      ret = epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event);
    }
    return ret;
  }
  /**
   * Add a file descriptor to the event poller with associated producer and
   * consumer. \param flags Epoll flags to watch (e.g., EPOLLIN | EPOLLET)
   * \param fd File descriptor to monitor
   * \param producer Weak pointer to the producer that generates events
   * \param consumer Weak pointer to the consumer that handles events
   * \return return value of epoll_ctl
   */
  int Add(int flags, int fd, Producer producer, Consumer consumer)
    requires IsClassOrStruct<Producer, Consumer>
  {
    // LOG_DEBUG_TRACE("adding fd:{}\n", fd);
    int ret{0};
    if (fd != -1) {
      ret = Add(flags, fd);
      if (ret == 0) {
        _watchers[fd] = std::make_pair(producer, consumer);
      } else {
        // LOG_ERR("epoll_ctl failed with {}({})", strerror(errno), errno);
      }
    }
    return ret;
  }
  /**
   * Executes event handling logic for a given file descriptor.
   * If a valid producer-consumer pair is registered for the specified file
   * descriptor, this method will invoke the producer's `Produce()` method to
   * generate events, and then pass them to the consumer's `Consume()` method.
   * \param fd File descriptor for which the event occurred
   */
  void Execute(int fd) {
    if (auto it = _watchers.find(fd); it != _watchers.end()) {
      auto producer = it->second.first.lock();
      auto consumer = it->second.second.lock();
      if (producer && consumer) {
        auto &&events = producer->Produce();
        if (!events.empty()) {
          consumer->Consume(std::move(events));
        }
      } else {
        // LOG_ERR("Producer or Consumer is not available for fd:{}", fd);
      }
    } else {
      // LOG_ERR("No watchers found for fd:{}", fd);
    }
  }
  /**
   * Remove fd from epoll watch list
   * \param fd fd to be remove from epoll watch list
   * \return return value of epoll_ctl
   */
  int Remove(int fd) const {
    return epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr);
  }
  /**
   * Wait for events to be occur on epoll fd
   * \param events The buffer pointed to by events is used to return information
   * from the ready list about file descriptors in the interest list that have
   * some events available. \param maxevents Up to maxevents are returned by
   * epoll_wait \param timeout The timeout argument specifies the number of
   * milliseconds that epoll_wait() will block. \return return value of
   * epoll_wait \ref https://man7.org/linux/man-pages/man2/epoll_wait.2.html
   */
  int Wait(struct epoll_event *events, int maxevents, int timeout) const {
    return epoll_wait(_fd, events, maxevents, timeout);
  }

private:
  /**
   * File descriptor for the epoll instance.
   */
  int _fd{0};
  /**
   * Map of file descriptors to their associated producer and consumer
   */
  std::unordered_map<int, std::pair<Producer, Consumer>> _watchers{};
};
#endif // __eventpoller__h__
