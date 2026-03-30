#ifndef __pipedevent__h__
#define __pipedevent__h__

#include <atomic>
#include <cstring>
#include <errno.h>
#include <poll.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

#include <fmt/format.h>

/**
 * Encapsulates a pair of file descriptors for event signaling using a pipe.
 */
class PipedEvent {
public:
  /**
   * Constructs a PipedEvent.
   * Initializes the pipe, throwing a std::runtime_error if creation fails.
   */
  PipedEvent() {
    /**
     * \todo use pipe2
     */
    if (pipe(_fds) == -1) {
      /**
       * \todo use strerror_r thread safe
       */
      auto msg = fmt::format("pipe failed with {}({})", strerror(errno), errno);
      throw std::runtime_error(msg);
    }
  }
  /**
   * Signals the event.
   * Writes a byte to the pipe if it has not already been fired.
   */
  void Fire() const {
    if (_fds[0] != 0 && !_fired) {
      _fired.store(true);
      auto unused = write(_fds[1], _fds, 1);
    }
  }
  /**
   * Consumes the event signal.
   * Reads a byte from the pipe and resets the fired state.
   */
  void Consume() const {
    if (_fds[0] != 0 && _fired) {
      char val;
      auto unused = read(_fds[0], &val, 1);
      _fired.store(false);
    }
  }
  /**
   * Gets the source file descriptor.
   * \return Reference to the source file descriptor.
   */
  const int &GetSource() const { return _fds[0]; }
  /**
   * Gets the sink file descriptor.
   * \return Reference to the sink file descriptor.
   */
  const int &GetSink() const { return _fds[1]; }
  /**
   * Closes the source file descriptor.
   * \return 0 on success, -1 on failure.
   */
  int CloseSource() {
    if (_fds[0] == 0) {
      return 0;
    }
    auto ret = close(_fds[0]);
    if (ret == 0) {
      _fds[0] = 0;
    }
    return ret;
  }
  /**
   * Closes the sink file descriptor.
   * \return 0 on success, -1 on failure.
   */
  int CloseSink() {
    if (_fds[1] == 0) {
      return 0;
    }
    auto ret = close(_fds[1]);
    if (ret == 0) {
      _fds[1] = 0;
    }
    return ret;
  }

  /**
   * Gets both Source and Sink file descriptors as a pair.
   * \return A pair containing the Source and Sink file descriptors.
   */
  std::pair<int, int> GetFds() const {
    return std::make_pair(_fds[0], _fds[1]);
  }

  /**
   * Destructor closes both Source and Sink file descriptors.
   */
  ~PipedEvent() {
    CloseSource();
    CloseSink();
  }

private:
  /**
   * Stores file descriptors for the Source and Sink.
   */
  int _fds[2] = {-1, -1};
  /**
   * An Atomic flag indicating if the event has been fired or not.
   */
  mutable std::atomic<bool> _fired{false};
};
#endif // __pipedevent__h__
