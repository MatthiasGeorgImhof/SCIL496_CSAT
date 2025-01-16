#ifndef CYPHAL_HPP_INCLUDED
#define CYPHAL_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include <memory>

// Versioning - Follow the approach used in canard and serard
#define CYPHAL_VERSION_MAJOR 0
#define CYPHAL_VERSION_MINOR 1
#define CYPHAL_CYPHAL_SPECIFICATION_VERSION_MAJOR 1
#define CYPHAL_CYPHAL_SPECIFICATION_VERSION_MINOR 0

// Error Codes - Follow the approach used in canard and serard
#define CYPHAL_ERROR_INVALID_ARGUMENT 2
#define CYPHAL_ERROR_OUT_OF_MEMORY 3
#define CYPHAL_ERROR_ANONYMOUS 4

/// Parameter ranges are inclusive; the lower bound is zero for all. See Cyphal/CAN Specification for background.
#define CYPHAL_SUBJECT_ID_MAX 8191U
#define CYPHAL_SERVICE_ID_MAX 511U
#define CYPHAL_NODE_ID_MAX 127U
#define CYPHAL_PRIORITY_MAX 7U
#define CYPHAL_TRANSFER_ID_BIT_LENGTH 5U
#define CYPHAL_TRANSFER_ID_MAX ((1U << CYPHAL_TRANSFER_ID_BIT_LENGTH) - 1U)

#define CYPHAL_NODE_ID_UNSET 255U

// Time
typedef uint64_t CyphalMicrosecond;
#define CYPHAL_DEFAULT_TRANSFER_ID_TIMEOUT_USEC 2000000UL


// Enums
enum class CyphalPriority : uint8_t {
    Exceptional = 0,
    Immediate = 1,
    Fast = 2,
    High = 3,
    Nominal = 4,
    Low = 5,
    Slow = 6,
    Optional = 7,
};

enum class CyphalTransferKind : uint8_t {
    Message = 0,
    Response = 1,
    Request = 2,
};
constexpr int CYPHAL_NUM_TRANSFER_KINDS = 3;

// Core Data Structures
typedef uint16_t CyphalPortID;
typedef uint8_t CyphalNodeID;
typedef uint8_t CyphalTransferID;

struct CyphalTransferMetadata {
    CyphalPriority priority;
    CyphalTransferKind transfer_kind;
    CyphalPortID port_id;
    CyphalNodeID remote_node_id;
    CyphalTransferID transfer_id;
};

struct CyphalRxTransfer {
    CyphalTransferMetadata metadata;
    CyphalMicrosecond timestamp_usec;
    size_t payload_size;
    void* payload;
};


// Forward declarations for adapter structures
class CyphalInstance;
class CyphalRxSubscription;
class CyphalTxQueue;
class CyphalFrame;
class CyphalTxQueueItem;
template <typename Adapter>
class CyphalImpl;


// Memory Allocator Types
using CyphalMemoryAllocate = void* (*)(CyphalInstance* ins, size_t amount);
using CyphalMemoryFree = void (*)(CyphalInstance* ins, void* pointer);


// Hardware Filter support - this will likely be CAN specific.
struct CyphalFilter {
    uint32_t extended_can_id;
    uint32_t extended_mask;
};


class CyphalInstance {
public:

    template <typename Adapter>
    friend struct CyphalImpl;

    CyphalInstance(const CyphalMemoryAllocate memory_allocate, const CyphalMemoryFree memory_free) :
        memory_allocate_(memory_allocate),
        memory_free_(memory_free) {
    }

    virtual ~CyphalInstance() = default;

    void* user_reference = nullptr;
    CyphalNodeID node_id = CYPHAL_NODE_ID_UNSET;

    CyphalMemoryAllocate memory_allocate() { return memory_allocate_; }
    CyphalMemoryFree memory_free() { return memory_free_; }
  public:
    void* get_impl() {return impl_;};
private:
    CyphalMemoryAllocate memory_allocate_;
    CyphalMemoryFree memory_free_;

    void* impl_ = nullptr;
};



class CyphalTxQueue {
public:
    CyphalTxQueue(CyphalInstance& instance, const size_t capacity, const size_t mtu_bytes) : instance_(instance) {
        this->capacity = capacity;
        this->mtu_bytes = mtu_bytes;
    }
    virtual ~CyphalTxQueue() = default;
    size_t capacity;
    size_t mtu_bytes;
    void* user_reference = nullptr;
    CyphalInstance& instance() { return instance_; };
 public:
        void* get_impl() { return impl_;}
private:
    void* impl_ = nullptr;
    CyphalInstance& instance_;
    template <typename Adapter>
    friend struct CyphalImpl;
};

class CyphalRxSubscription {
public:
    virtual ~CyphalRxSubscription() = default;
    void* user_reference = nullptr;
    CyphalMicrosecond transfer_id_timeout_usec;
    size_t extent;
    CyphalPortID port_id;

    // Private
    void* impl = nullptr;
 public:
          void* get_impl() {return impl;};
    template <typename Adapter>
   friend struct CyphalImpl;
};

class CyphalFrame {
public:
    virtual ~CyphalFrame() = default;
    size_t payload_size = 0;
    const void* payload = nullptr;
    void* impl = nullptr;
   public:
          void* get_impl() { return impl; }
    template <typename Adapter>
    friend struct CyphalImpl;
};

 class CyphalTxQueueItem {
 public:
     virtual ~CyphalTxQueueItem() = default;
     CyphalMicrosecond tx_deadline_usec;
     CyphalFrame frame;
     void* impl = nullptr; // This line MUST be present
 public:
     void* get_impl() { return impl;}
     const void* get_impl() const {return impl;};
     template <typename Adapter>
     friend struct CyphalImpl;
 };

// =====================  API Functions  =====================
template <typename Adapter>
struct CyphalImpl : public CyphalInstance {
public:
    CyphalImpl(const CyphalMemoryAllocate memory_allocate, const CyphalMemoryFree memory_free) : CyphalInstance(memory_allocate, memory_free) {
        impl_ = Adapter::init(memory_allocate, memory_free);
    }


    ~CyphalImpl() override { if (impl_) Adapter::deinit(impl_); }


    CyphalTxQueue tx_init(const size_t capacity, const size_t mtu_bytes) {
        CyphalTxQueue queue(*this, capacity, mtu_bytes);
        queue.impl_ = Adapter::tx_init(capacity, mtu_bytes);
        return queue;
    }


    int32_t tx_push(CyphalTxQueue& que,
                    const CyphalMicrosecond tx_deadline_usec,
                    const CyphalTransferMetadata& metadata,
                    const size_t payload_size,
                    const void* const payload) {

        return Adapter::tx_push(&que, this, tx_deadline_usec, &metadata, payload_size, payload);
    }

    const CyphalTxQueueItem* tx_peek(const CyphalTxQueue& que) {

        return Adapter::tx_peek(&que);
    }

    CyphalTxQueueItem* tx_pop(CyphalTxQueue& que, const CyphalTxQueueItem& item) {

        return Adapter::tx_pop(&que, &item);
    }


    int8_t rx_accept(const CyphalMicrosecond timestamp_usec,
        const CyphalFrame& frame,
        const uint8_t redundant_iface_index,
        CyphalRxTransfer& out_transfer,
        CyphalRxSubscription** const out_subscription) {

        return Adapter::rx_accept(this, timestamp_usec, &frame, redundant_iface_index, &out_transfer, out_subscription);

    }

    int8_t rx_subscribe(const CyphalTransferKind transfer_kind,
                    const CyphalPortID port_id,
                    const size_t extent,
                    const CyphalMicrosecond transfer_id_timeout_usec,
                    CyphalRxSubscription& out_subscription) {
        out_subscription.impl = Adapter::rx_subscribe(this, transfer_kind, port_id, extent, transfer_id_timeout_usec, &out_subscription);
        return out_subscription.impl ? 1 : 0;
    }


    int8_t rx_unsubscribe(const CyphalTransferKind transfer_kind,
                            const CyphalPortID port_id) {
        return Adapter::rx_unsubscribe(this, transfer_kind, port_id);
    }

    int8_t rx_get_subscription(const CyphalTransferKind transfer_kind,
                            const CyphalPortID port_id,
                            CyphalRxSubscription** const out_subscription) {
        return Adapter::rx_get_subscription(this, transfer_kind, port_id, out_subscription);
    }


    CyphalFilter make_filter_for_subject(const CyphalPortID subject_id) {
        return Adapter::make_filter_for_subject(subject_id);
    }

    CyphalFilter make_filter_for_service(const CyphalPortID service_id, const CyphalNodeID local_node_id) {
        return Adapter::make_filter_for_service(service_id, local_node_id);
    }

    CyphalFilter make_filter_for_services(const CyphalNodeID local_node_id) {
        return Adapter::make_filter_for_services(local_node_id);
    }

    CyphalFilter consolidate_filters(const CyphalFilter& a, const CyphalFilter& b) {
        return  Adapter::consolidate_filters(&a, &b);
    }
};


#endif