#ifndef INC_REGISTRATIONMANAGER_HPP_
#define INC_REGISTRATIONMANAGER_HPP_

#include <memory>
#include "cstdint"
#include <tuple>
#include <algorithm>
#include <type_traits>
#include "ArrayList.hpp"
#include "Task.hpp"
#include "cyphal.hpp"

/**
 * @brief Maximum number of Cyphal subscriptions supported by the RegistrationManager.
 */
constexpr size_t NUM_SUBSCRIPTIONS = 16;
constexpr size_t NUM_PUBLICATIONS = 16;

/**
 * @brief Manages the registration and unregistration of tasks, and their subscriptions
 *        to Cyphal ports.  It also handles the interaction with the Cyphal adapters.
 */
class RegistrationManager
{
public:
    RegistrationManager() = default;
    RegistrationManager(const RegistrationManager &) = delete;
    RegistrationManager &operator=(const RegistrationManager &) = delete;

    /**
     * @brief Adds a task to the registration manager, triggering its registration.
     * @param task A shared pointer to the task to be added.
     */
    inline void add(std::shared_ptr<Task> task);

    /**
     * @brief Removes a task from the registration manager, triggering its unregistration.
     * @param task A shared pointer to the task to be removed.
     */
    inline void remove(std::shared_ptr<Task> task);

    /**
     * @brief Subscribes a task to a Cyphal port using the provided adapters.
     * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
     * @param subscription The Cyphal subscription details.
     * @param task A shared pointer to the task to be subscribed.
     * @param adapters A tuple containing the Cyphal adapter instances.
     */
    template <typename... Adapters>
    void subscribe(const CyphalSubscription &subscription, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters);

    /**
     * @brief Unsubscribes a task from a Cyphal port using the provided adapters.
     * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
     * @param subscription The Cyphal subscription details.
     * @param task A shared pointer to the task to be unsubscribed.
     * @param adapters A tuple containing the Cyphal adapter instances.
     */
    template <typename... Adapters>
    void unsubscribe(const CyphalSubscription &subscription, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters);

    /**
     * @brief publishes a task to a Cyphal port using the provided adapters.
     * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
     * @param publication The Cyphal subscription details.
     * @param task A shared pointer to the task to be published.
     * @param adapters A tuple containing the Cyphal adapter instances.
     */
    template <typename... Adapters>
    void publish(const CyphalPublication &publication, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters);

    /**
     * @brief Unpublishes a task from a Cyphal port using the provided adapters.
     * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
     * @param publication The Cyphal subscription details.
     * @param task A shared pointer to the task to be unpublished.
     * @param adapters A tuple containing the Cyphal adapter instances.
     */
    template <typename... Adapters>
    void unpublish(const CyphalPublication &publication, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters);

    /**
     * @brief Gets the list of task handlers.
     * @return A const reference to the ArrayList of task handlers.
     */
    inline const ArrayList<TaskHandler, NUM_TASK_HANDLERS> &getHandlers() const;

    /**
     * @brief Gets the list of Cyphal subscriptions.
     * @return A const reference to the ArrayList of Cyphal subscriptions.
     */
    inline const ArrayList<CyphalSubscription, NUM_SUBSCRIPTIONS> &getSubscriptions() const;
    inline const ArrayList<CyphalPublication, NUM_SUBSCRIPTIONS> &getPublications() const;

    inline bool containsTask(const std::shared_ptr<Task> &task) const;

private:
    /**
     * @brief List of task handlers.
     */
    ArrayList<TaskHandler, NUM_TASK_HANDLERS> handlers_;

    /**
     * @brief List of Cyphal subscriptions.
     */
    ArrayList<CyphalSubscription, NUM_SUBSCRIPTIONS> subscriptions_;
    ArrayList<CyphalPublication, NUM_PUBLICATIONS> publications_;
};

/**
 * @brief Adds a task to the registration manager, triggering its registration.
 * @param task A shared pointer to the task to be added.
 */
inline void RegistrationManager::add(std::shared_ptr<Task> task)
{
    task->registerTask(this, task);
}

/**
 * @brief Removes a task from the registration manager, triggering its unregistration.
 * @param task A shared pointer to the task to be removed.
 */
inline void RegistrationManager::remove(std::shared_ptr<Task> task)
{
    task->unregisterTask(this, task);
}

inline bool RegistrationManager::containsTask(const std::shared_ptr<Task> &task) const
{
    return std::any_of(handlers_.begin(), handlers_.end(),
                       [&](const auto &handler)
                       {
                           return handler.task == task; // Compare the shared_ptr directly
                       });
}

/**
 * @brief Subscribes a task to a Cyphal port using the provided adapters.
 * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
 * @param subscription The Cyphal subscription details.
 */
template <typename... Adapters>
void RegistrationManager::subscribe(const CyphalSubscription &subscription, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters)
{
    TaskHandler handler = {subscription.port_id, task};
    handlers_.push(handler);

    subscriptions_.push(subscription);

    bool all_successful = true;
    std::apply([&](auto &...adapter)
               { ([&]()
                  {
            int8_t res = (adapter.cyphalRxSubscribe(subscription.transfer_kind, subscription.port_id, subscription.extent, 1000) > 0);
            all_successful = all_successful && res; }(), ...); }, adapters);
}

/**
 * @brief Unsubscribes a task from a Cyphal port using the provided adapters.
 * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
 * @param subscription The Cyphal subscription details.
 */
template <typename... Adapters>
void RegistrationManager::unsubscribe(const CyphalSubscription &subscription, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters)
{
    handlers_.removeIf([&](const TaskHandler &handler)
                       { return handler.port_id == subscription.port_id && handler.task == task; });

    subscriptions_.removeIf([&](const CyphalSubscription &sub)
                            { return sub.port_id == subscription.port_id && sub.extent == subscription.extent && sub.transfer_kind == subscription.transfer_kind; });

    bool all_successful = true;
    std::apply([&](auto &...adapter)
               { ([&]()
                  {
            int8_t res = (adapter.cyphalRxUnsubscribe(subscription.transfer_kind, subscription.port_id) > 0);
            all_successful = all_successful && res; }(), ...); }, adapters);
}

/**
 * @brief publishes a task to a Cyphal port using the provided adapters.
 * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
 * @param publication The Cyphal publication details.
 */
template <typename... Adapters>
void RegistrationManager::publish(const CyphalPublication &publication, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters)
{
    publications_.push(publication);
}

/**
 * @brief Unpublishes a task from a Cyphal port using the provided adapters.
 * @tparam Adapters Variadic template parameter representing the Cyphal adapter types.
 * @param publication The Cyphal publication details.
 */
template <typename... Adapters>
void RegistrationManager::unpublish(const CyphalPublication &publication, std::shared_ptr<Task> task, std::tuple<Adapters...> &adapters)
{
    publications_.removeIf([&](const CyphalPublication &pub)
                            { return pub.port_id == publication.port_id && pub.extent == publication.extent && pub.transfer_kind == publication.transfer_kind; });
}

/**
 * @brief Gets the list of task handlers.
 * @return A const reference to the ArrayList of task handlers.
 */
inline const ArrayList<TaskHandler, NUM_TASK_HANDLERS> &RegistrationManager::getHandlers() const { return handlers_; }

/**
 * @brief Gets the list of Cyphal subscriptions.
 * @return A const reference to the ArrayList of Cyphal subscriptions.
 */
inline const ArrayList<CyphalSubscription, NUM_SUBSCRIPTIONS> &RegistrationManager::getSubscriptions() const { return subscriptions_; }
inline const ArrayList<CyphalPublication, NUM_PUBLICATIONS> &RegistrationManager::getPublications() const { return publications_; }

#endif