#ifndef __iproducer_h__
#define __iproducer_h__

#include <list>   // for list
#include <memory> // for unique_ptr

/**
 * @brief Interface for producer components in the smart home system.
 *
 * The `IProducer` class serves as an interface for components that produce
 * events. It defines a method for producing events, which can be implemented by
 * various concrete producer classes. This interface allows for flexibility in
 * the types of events produced, enabling different components to generate
 * events based on their specific functionalities and requirements.
 *
 * @tparam E The type of event this producer can generate. Must be derived from
 *           or compatible with the Event base class.
 *
 * @note This is a pure virtual interface that must be implemented by concrete
 *       producer classes.
 *
 * @example
 * @code
 * class MyEventProducer : public IProducer<MyEvent> {
 * public:
 *     std::list<MyEvent> Produce() override {
 *         std::list<MyEvent> events;
 *         // Generate and add events to the list
 *         return events;
 *     }
 * };
 * @endcode
 */
template <typename E> class IProducer {
public:
  /**
   * @brief Weak pointer type for this producer interface.
   *
   * Used to maintain non-owning references to producer instances,
   * preventing circular dependencies in event handling systems.
   */
  using WPtr = std::weak_ptr<IProducer>;

  /**
   * @brief Produce events.
   *
   * This method generates and returns a list of events. Implementations
   * should create events based on the producer's specific logic and
   * current state.
   *
   * @return std::list<E> A list of events produced by this producer.
   *                      The list may be empty if no events are available.
   *
   * @note This method should be implemented to be non-blocking when possible.
   * @note The returned list contains events ready for consumption by
   *       appropriate consumers.
   */
  virtual std::list<E> Produce() = 0;

  /**
   * @brief Virtual destructor for the IProducer class.
   *
   * Ensures proper cleanup of derived classes when destroyed through
   * a base class pointer.
   */
  virtual ~IProducer() = default;
};

#endif // __iproducer_h__
