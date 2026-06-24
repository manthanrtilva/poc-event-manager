#ifndef __iconsumer_h__
#define __iconsumer_h__

#include <list>   // for list
#include <memory> // for unique_ptr

#include "AutoId.h"

template <typename, typename = void>
struct has_element_type_type : std::false_type {};
template <typename E>
struct has_element_type_type<E, std::void_t<typename E::element_type::Type>>
    : std::true_type {};

template <typename E, bool = has_element_type_type<E>::value>
class IConsumerWillConsume {
public:
  virtual ~IConsumerWillConsume() = default;
};

template <typename E> class IConsumerWillConsume<E, true> {
public:
  virtual std::list<typename E::element_type::Type> WillConsume() const {
    return {};
  }
  virtual ~IConsumerWillConsume() = default;
};

template <typename E>
class IConsumer : public IConsumerWillConsume<E>, virtual public AutoId {
public:
  using WPtr = std::weak_ptr<IConsumer>;

  virtual void Consume1(std::list<E> &&events) {
    for (auto &event : events) {
      Consume(std::move(event));
    }
  }
  virtual void Consume(E &&event) = 0;
  virtual ~IConsumer() = default;
};

#endif // __iconsumer_h__
