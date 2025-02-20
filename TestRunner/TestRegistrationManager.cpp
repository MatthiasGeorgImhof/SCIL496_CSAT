#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "RegistrationManager.hpp"
#include "Task.hpp" // Make sure Task.hpp is included
#include "cyphal.hpp"  // Ensure cyphal.hpp defines CyphalSubscription
#include <memory>
#include <iostream> // For cout
#include <tuple>

// Define some dummy adapter types (replace with your actual adapters)
class DummyAdapter {
public:
    DummyAdapter(int value) : value_(value), cyphalRxSubscribeCallCount(0), cyphalRxUnsubscribeCallCount(0),lastTransferKind(CyphalTransferKind::CyphalTransferKindMessage), lastPortID(0), lastExtent(0), lastTimeout(0) {}
    int getValue() const { return value_; }
    int8_t cyphalRxSubscribe(const CyphalTransferKind transfer_kind,
                                const CyphalPortID port_id,
                                const size_t extent,
                                const CyphalMicrosecond transfer_id_timeout_usec) {
        cyphalRxSubscribeCallCount++;
        lastTransferKind = transfer_kind;
        lastPortID = port_id;
        lastExtent = extent;
        lastTimeout = transfer_id_timeout_usec;
        return 1; // Dummy implementation
    }

     int8_t cyphalRxUnsubscribe(const CyphalTransferKind transfer_kind,
                                const CyphalPortID port_id)
    {
        cyphalRxUnsubscribeCallCount++;
        lastTransferKind = transfer_kind;
        lastPortID = port_id;
        lastExtent = 0;
        lastTimeout = 0;
        return 1; // Dummy implementation
    }

    int cyphalRxSubscribeCallCount;
    int cyphalRxUnsubscribeCallCount;
    CyphalTransferKind lastTransferKind;
    CyphalPortID lastPortID;
    size_t lastExtent;
    size_t lastTimeout;

private:
    int value_;
};

// Mock Task class that interacts with the RegistrationManager
class MockTask : public Task {
public:
    MockTask(uint32_t interval, uint32_t tick, CyphalPortID port_id, size_t extent, CyphalTransferKind transfer_kind, std::tuple<DummyAdapter&, DummyAdapter&>& adapters)
        : Task(interval, tick), port_id_(port_id), extent_(extent), transfer_kind_(transfer_kind), adapters_(adapters), registered_(false), unregistered_(false) {}
    ~MockTask() override {}

    void handleMessage(std::shared_ptr<CyphalTransfer> transfer) override {}
    void handleTaskImpl() override {}

     // Override the non-template virtual functions
    void registerTask(RegistrationManager* manager_ptr, std::shared_ptr<Task> task) override {
        RegistrationManager* manager = static_cast<RegistrationManager*>(manager_ptr);
        CyphalSubscription subscription = {port_id_, extent_, transfer_kind_};

         manager->subscribe(subscription, task, adapters_); // Pass the tuple
        registered_ = true;
    }

    void unregisterTask(RegistrationManager* manager_ptr, std::shared_ptr<Task> task) override {
        RegistrationManager* manager = static_cast<RegistrationManager*>(manager_ptr);
        CyphalSubscription subscription = {port_id_, extent_, transfer_kind_};

         manager->unsubscribe(subscription, task, adapters_); // Pass the tuple
        unregistered_ = true;
    }

    bool isRegistered() const { return registered_; }
    bool isUnregistered() const { return unregistered_; }

private:
    CyphalPortID port_id_;
    size_t extent_;
    CyphalTransferKind transfer_kind_;
    std::tuple<DummyAdapter&, DummyAdapter&>& adapters_;
    bool registered_;
    bool unregistered_;
};

TEST_CASE("RegistrationManager: Register and Unregister Task with Tuple") {
    // Create a RegistrationManager with a dummy adapter
    DummyAdapter adapter1(42); // Create an instance of DummyAdapter
    DummyAdapter adapter2(43); // Create an instance of DummyAdapter

    RegistrationManager manager; // Pass the instance to the constructor
    CyphalPortID port_id = 123;
    size_t extent = 456;
    CyphalTransferKind transfer_kind = CyphalTransferKind::CyphalTransferKindMessage;
    std::tuple<DummyAdapter&, DummyAdapter&> adapters(adapter1, adapter2);
    std::shared_ptr<MockTask> task = std::make_shared<MockTask>(100, 0, port_id, extent, transfer_kind, adapters);

    // Initially, task should not be registered
    CHECK(task->isRegistered() == false);
    CHECK(task->isUnregistered() == false);

    // Register the task
    task->registerTask(&manager, task);

    // Now, the task should be registered (MockTask's registerTask *is* now calling subscribe)
    CHECK(task->isRegistered() == true);
    CHECK(task->isUnregistered() == false);

    // Check that cyphalRxSubscribe was called
    CHECK(std::get<0>(adapters).cyphalRxSubscribeCallCount == 1);
    CHECK(std::get<0>(adapters).lastTransferKind == transfer_kind);
    CHECK(std::get<0>(adapters).lastPortID == port_id);
    CHECK(std::get<0>(adapters).lastExtent == extent);
    CHECK(std::get<0>(adapters).lastTimeout == 1000);

        // Check that cyphalRxSubscribe was called
    CHECK(std::get<1>(adapters).cyphalRxSubscribeCallCount == 1);
    CHECK(std::get<1>(adapters).lastTransferKind == transfer_kind);
    CHECK(std::get<1>(adapters).lastPortID == port_id);
    CHECK(std::get<1>(adapters).lastExtent == extent);
    CHECK(std::get<1>(adapters).lastTimeout == 1000);


    // Unregister the task
    task->unregisterTask(&manager, task);

    // Now, the task should be unregistered
    CHECK(task->isRegistered() == true); //registered stays as true since the variable is not changed in the task class.
    CHECK(task->isUnregistered() == true);

    CHECK(std::get<0>(adapters).cyphalRxUnsubscribeCallCount == 1);
    CHECK(std::get<1>(adapters).cyphalRxUnsubscribeCallCount == 1);

    // Verify that the subscription and handler are removed from the manager
    bool found = false;

    const ArrayList<TaskHandler, NUM_TASK_HANDLERS>& handlers = manager.getHandlers(); // Use the getter
    for (size_t i = 0; i < handlers.size(); ++i) {
        if (handlers[i].port_id == port_id && handlers[i].task == task) {
            found = true;
            break;
        }
    }

    CHECK(found == false);


    found = false;
    CyphalSubscription expected_subscription = {port_id, extent, transfer_kind};
    const ArrayList<CyphalSubscription, NUM_SUBSCRIPTIONS>& subscriptions = manager.getSubscriptions(); // Use the getter
    for (size_t i = 0; i < subscriptions.size(); ++i) {
         if (subscriptions[i].port_id == port_id && subscriptions[i].extent == extent && subscriptions[i].transfer_kind == transfer_kind) {
            found = true;
            break;
        }
    }
    CHECK(found == false);
}

constexpr CyphalPortID CYPHALPORT = 129; 

void checkTransfers(const CyphalTransfer transfer1, const CyphalTransfer transfer2)
{
    CHECK(transfer1.metadata.port_id == transfer2.metadata.port_id);
    CHECK(transfer1.payload_size == transfer2.payload_size);
    CHECK(strncmp(static_cast<const char *>(transfer1.payload), static_cast<const char *>(transfer2.payload), transfer1.payload_size) == 0);
}

class BasicTaskFromBuffer : public TaskFromBuffer
{
public:
BasicTaskFromBuffer(uint32_t interval, uint32_t tick, const CyphalTransfer transfer) : TaskFromBuffer(interval, tick), transfer_(transfer) {}
    ~BasicTaskFromBuffer() override {}

    void handleTaskImpl() override 
    {
        CHECK(buffer.size() == 1);

        for(int i=0; i<buffer.size(); ++i)
        {
            auto transfer = buffer.pop();
            checkTransfers(transfer_, *transfer);
        }
    }

    void registerTask(RegistrationManager *manager, std::shared_ptr<Task> task)
    {
	    CyphalSubscription subscription = {CYPHALPORT, 2, CyphalTransferKindMessage};
        std::tuple<> adapters;
	    manager->subscribe(subscription, task, adapters);
    }

    void unregisterTask(RegistrationManager *manager, std::shared_ptr<Task> task) {}

    CyphalTransfer transfer_;
};


TEST_CASE("RegistrationManager: TaskFromBuffer") {
    constexpr CyphalPortID port_id = 123;
    CyphalTransfer transfer;
    transfer.metadata.priority = CyphalPriorityNominal;
    transfer.metadata.transfer_kind = CyphalTransferKindMessage;
    transfer.metadata.port_id = port_id;
    transfer.metadata.remote_node_id = CYPHAL_NODE_ID_UNSET;
    transfer.metadata.transfer_id = 0;
    transfer.payload_size = 5;
    char payload[6] = "hello";
    transfer.payload = payload;

    
    RegistrationManager manager;
    std::shared_ptr<BasicTaskFromBuffer> basic_task_buffer = std::make_shared<BasicTaskFromBuffer>(100, 0, transfer);
    HAL_SetTick(1000);

    basic_task_buffer->registerTask(&manager, basic_task_buffer);
    const ArrayList<TaskHandler, NUM_TASK_HANDLERS>& handlers = manager.getHandlers();
    CHECK(handlers.size() == 1);

    std::shared_ptr<CyphalTransfer> transfer_ptr = std::make_shared<CyphalTransfer>(transfer);
    CHECK(transfer_ptr.use_count() == 1);
    for(auto handler : handlers) handler.task->handleMessage(transfer_ptr);
    CHECK(transfer_ptr.use_count() == 2);
    for(auto handler : handlers) handler.task->handleTask();
    CHECK(transfer_ptr.use_count() == 1);
}