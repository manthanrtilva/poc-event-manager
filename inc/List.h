#ifndef __list__h__
#define __list__h__

#include <list>
#include <mutex>
/**
 * This class is based on widely used pattern described in https://www.stroustrup.com/wrapper.pdf
 */
/*
 * Functional object, which unlock mutex when get called
 */
template <typename Mutex> struct Unlock {
  /**
   * Reference to a mutex object that will be unlocked.
   */
  Mutex &_mutex;
  /**
   * Unlocks the referenced mutex when invoked.
   */
  void operator()() const { _mutex.unlock(); }
};
template <typename T, typename Mutex> class Wrapper;

/*
 * Object of this class allows to transparently call methods of proxied object.
 * Mutex locked by Wrapper class will be unlocked by destructor of CallProxy class' object
 */
template <typename T, typename Suf> class CallProxy {
public:
  /**
   * Constructs a CallProxy, storing a reference to the data and a suffix callable.
   */
  CallProxy(T &data, Suf suffix) : _data{data}, _suffix{suffix} {}
  /**
   * Destructor calls the stored suffix callable, typically used for unlocking a mutex.
   */
  ~CallProxy() { _suffix(); }
  /**
   * Copy constructor is deleted
   */
  CallProxy(const CallProxy &) = delete;
  /**
   * Copy assignment operator is deleted
   */
  CallProxy &operator=(const CallProxy &) = delete;
  /**
   * Move constructor is deleted
   */
  CallProxy(CallProxy &&) = delete;
  /**
   * Move assignment operator is deleted
   */
  CallProxy &&operator=(CallProxy &&) = delete;
  /**
   * Allows pointer-style access to call methods on the proxied object by returning its pointer.
   * \return pointer to the proxied object
   */
  T *operator->() const { return &_data; }

private:
  /**
   * Proxied object's reference, whose methods are accessible through this proxy.
   */
  T &_data;
  /**
   * Callable that is called by the destructor, typically to unlock a mutex.
   */
  Suf _suffix;
};

/*
 * Wrap the type/class to make its method thread safe
 * Ref https://www.stroustrup.com/wrapper.pdf for more details
 */
template <typename T, typename Mutex> class Wrapper {
public:
  /**
   * Default Constructor
   */
  Wrapper() = default;
  /**
   * Copy constructor is deleted
   */
  Wrapper(const Wrapper &) = delete;
  /**
   * Copy assignment operator is deleted
   */
  Wrapper &operator=(const Wrapper &) = delete;
  /**
   * Move constructor is deleted
   */
  Wrapper(Wrapper &&) = delete;
  /**
   * Move assignment operator is deleted
   */
  Wrapper &&operator=(Wrapper &&) = delete;
  /**
   * Provides thread-safe access to the proxied object's methods.
   * This operator locks the internal mutex to ensure exclusive access
   * to the wrapped object, creating a CallProxy that unlocks the mutex
   * when the proxy goes out of scope. This allows safe method calls on
   * the proxied object in a multi-threaded environment.
   * \return A CallProxy object that safely lets you call methods on the wrapped object,
   * automatically unlocking the mutex when done.
   */
  CallProxy<T, Unlock<Mutex>> operator->() {
    _mutex.lock();
    return CallProxy<T, Unlock<Mutex>>(_data, Unlock{_mutex});
  }

private:
  /**
   * Instance of the object of Type T being wrapped for thread-safe method access.
   */
  T _data{};
  /**
   * Mutex used to ensure exclusive access to the wrapped object's methods.
   */
  Mutex _mutex{};
};

/**
 * A thread-safe list using Wrapper to secure method access.
 * This alias creates a thread-safe version of `std::list` by wrapping it with
 * the `Wrapper` class to ensure its methods are accessed safely in concurrent scenarios.
 */
template <typename T> using list = Wrapper<std::list<T>, std::mutex>;

/**
 * Move an item from a list to end of other list
 * \param to list on which item will be appended
 * \param from list from which item will be moved
 * \param at location of item on from which will be moved
 */
template <typename T> inline void Append(list<T> &to, std::list<T> &from, typename std::list<T>::iterator at) {
  auto queue = to.operator->();
  queue->splice(queue->end(), from, at);
}

/**
 * Move all items from a list to end of other list
 * \param to list on which items will be appended
 * \param from list from which item will be moved
 */
template <typename T> inline void Append(list<T> &to, std::list<T> &&from) {
  auto queue = to.operator->();
  queue->splice(queue->end(), std::move(from));
}
#endif // __list__h__
