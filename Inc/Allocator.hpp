#ifndef INC_ALLOCATOR_HPP_
#define INC_ALLOCATOR_HPP_

#include <memory>
#include "o1heap.h"
#include "canard.h"

#include "stdio.h"
#include "stdint.h"
#include "string.h"

// General O1HeapAllocator Definition
template <typename T>
class O1HeapAllocator {
public:
    using value_type = T;

    explicit O1HeapAllocator(O1HeapInstance* h) : heap(h) {
        // fprintf(stderr, "O1HeapAllocator<> constructed with heap %p\n", heap);
    }

    template <typename U>
    O1HeapAllocator(const O1HeapAllocator<U>& other) noexcept : heap(other.getHeap()) {}

    T* allocate(std::size_t n) {
        auto ptr = static_cast<T*>(o1heapAllocate(heap, n * sizeof(T)));
        // fprintf(stderr, "allocated <> %p %d\n", ptr, n * sizeof(T));
        // fflush(stderr);
        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        // fprintf(stderr, "deallocate <>: %p\n", p);
        // fflush(stderr);
        o1heapFree(heap, p);
    }

    void destroy(T* p) {
        // fprintf(stderr, "destroy <>: %p\n", p);
        // fflush(stderr);
        p->~T();
    }

    template <typename U>
    bool operator==(const O1HeapAllocator<U>&) const { return true; }

    template <typename U>
    bool operator!=(const O1HeapAllocator<U>&) const { return false; }

    O1HeapInstance* getHeap() const noexcept { return heap; }

    struct Deletor
	{
		Deletor(O1HeapAllocator<T> allocator) : allocator_(allocator) {};

    	void operator()(T* p) const
    	{
			if(p != nullptr) allocator_.deallocate(p, 0);
    	}

		private:
			O1HeapAllocator<T> allocator_;
    };

	Deletor getDeletor() {
		return Deletor(*this);
	}

private:
    O1HeapInstance* heap;
};

// Specialize destroy for CanardRxTransfer
template <>
inline void O1HeapAllocator<CanardRxTransfer>::destroy(CanardRxTransfer* p) {
        // fprintf(stderr, "destroy <CanardRxTransfer>: %p\n", p);
        // fflush(stderr);
    o1heapFree(heap, p->payload);
    p->~CanardRxTransfer();
}

// Use std::allocate_shared with O1HeapAllocator
template <typename T, typename Alloc, typename... Args>
std::shared_ptr<T> allocate_shared_custom(Alloc alloc, Args&&... args) {
    return std::allocate_shared<T>(alloc, std::forward<Args>(args)...);
}

#endif /* INC_ALLOCATOR_HPP_ */
