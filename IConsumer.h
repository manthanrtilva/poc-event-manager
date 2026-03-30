#ifndef __iconsumer_h__
#define __iconsumer_h__

#include <list>   // for list
#include <memory> // for unique_ptr

/**
 * @brief Interface for consuming events in the smart home platform.
 *
 * This template interface defines the contract for components that need to
 * consume events of a specific type. Implementers of this interface can
 * process both individual events and batches of events.
 *
 * @tparam E The type of event this consumer can handle. Must be derived from
 *           or compatible with the Event base class.
 *
 * @note This is a pure virtual interface that must be implemented by concrete
 *       consumer classes.
 *
 * @example
 * @code
 * class MyEventConsumer : public IConsumer<MyEvent> {
 * public:
 *     void Consume(std::list<MyEvent> &&events) override {
 *         // Process batch of events
 *     }
 *
 *     void Consume(MyEvent &&event) override {
 *         // Process single event
 *     }
 * };
 * @endcode
 */
template <typename E> class IConsumer {
public:
  /**
   * @brief Weak pointer type for this consumer interface.
   *
   * Used to maintain non-owning references to consumer instances,
   * preventing circular dependencies in event handling systems.
   */
  using WPtr = std::weak_ptr<IConsumer>;

  /**
   * @brief Consume a batch of events.
   *
   * This method processes multiple events in a single call, which can be
   * more efficient for bulk operations or when events need to be processed
   * in a specific order or context.
   *
   * @param events A list of events to be consumed. The list is moved to
   *               avoid unnecessary copying.
   *
   * @note The events list is passed by rvalue reference for efficient
   *       transfer of ownership.
   */
  virtual void Consume(std::list<E> &&events) = 0;

  /**
   * @brief Consume a single event.
   *
   * This method processes an individual event. It's useful for real-time
   * event processing or when events can be handled independently.
   *
   * @param event The event to be consumed. The event is moved to avoid
   *              unnecessary copying.
   *
   * @note The event is passed by rvalue reference for efficient transfer
   *       of ownership.
   */
  virtual void Consume(E &&event) = 0;
};

#endif // __iconsumer_h__
