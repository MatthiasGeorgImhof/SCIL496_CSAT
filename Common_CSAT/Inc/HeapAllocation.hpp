#ifndef __HEAP_ALLOCATION_HPP__
#define __HEAP_ALLOCATION_HPP__
#include <cstddef>
#include <cstdint>
#include "o1heap.h"
#include "canard.h"
#include "serard.h"
#include "udpard.h"

#ifdef __arm____
#include "stm32l4xx_hal.h"
#else
#include "mock_hal.h"
#endif

typedef struct
{
    size_t capacity;
    size_t allocated;
    size_t peak_allocated;
    size_t peak_request_size;
    uint64_t oom_count;
} HeapDiagnostics;

template <size_t HeapSize = 65536>
class HeapAllocation
{
private:
	static uint8_t o1heap_buffer[HeapSize] __attribute__((aligned(O1HEAP_ALIGNMENT)));
	static O1HeapInstance *o1heap;

	static void disableCANInterrupts()
	{
		HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
		HAL_NVIC_DisableIRQ(CAN1_RX1_IRQn);
	}

	static void enableCANInterrupts()
	{
		HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
		HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
	}

	static void *safeAllocate(const size_t size)
	{
		disableCANInterrupts();
		void *ptr = o1heapAllocate(o1heap, size);
		enableCANInterrupts();
		return ptr;
	}
	
	static void *unsafeAllocate(const size_t size)
	{
		return o1heapAllocate(o1heap, size);
	}

	static void safeDeallocate(void *const pointer)
	{
		disableCANInterrupts();
		o1heapFree(o1heap, pointer);
		enableCANInterrupts();
	}

	static void unsafeDeallocate(void *const pointer)
	{
		o1heapFree(o1heap, pointer);
	}

public:
	static void initialize()
	{
		o1heap = o1heapInit(o1heap_buffer, HeapSize);
	}

	static void *canardMemoryAllocate(CanardInstance *const /*canard*/, const size_t size)
	{
		return safeAllocate(size);
	}

	static void canardMemoryDeallocate(CanardInstance *const /*canard*/, void *const pointer)
	{
		safeDeallocate(pointer);
	}

	static void *serardMemoryAllocate(void *const /*user_reference*/, const size_t size)
	{
		return safeAllocate(size);
	}

	static void serardMemoryDeallocate(void *const /*user_reference*/, const size_t /*size*/, void *const pointer)
	{
		safeDeallocate(	pointer);
	}

	static void *udpardMemoryAllocate(void *const /*user_reference*/, const size_t size)
	{ 
		return safeAllocate(size);
	}
	
	static void udpardMemoryDeallocate(void *const /*user_reference*/, const size_t /*size*/, void *const pointer)
	{ 
		safeDeallocate(pointer);
	};

	static O1HeapInstance *getO1Heap()
	{
		return o1heap;
	}

HeapDiagnostics getDiagnostics() const
{
    O1HeapInstance* inst = getO1Heap();
    // In production you might assert; in tests we just return zeros if uninitialized.
    if (inst == nullptr)
    {
        return HeapDiagnostics{0, 0, 0, 0, 0};
    }

    const O1HeapDiagnostics o1diag = o1heapGetDiagnostics(inst);
    return HeapDiagnostics{
        o1diag.capacity,
        o1diag.allocated,
        o1diag.peak_allocated,
        o1diag.peak_request_size,
        o1diag.oom_count
    };
}
};

template <size_t HeapSize>
uint8_t HeapAllocation<HeapSize>::o1heap_buffer[HeapSize];

template <size_t HeapSize>
O1HeapInstance* HeapAllocation<HeapSize>::o1heap = nullptr;
#endif // __HEAP_ALLOCATION_HPP__
