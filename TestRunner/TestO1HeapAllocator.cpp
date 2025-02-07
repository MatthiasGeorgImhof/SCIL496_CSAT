#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "Allocator.hpp"

#include <iostream>

TEST_CASE("O1HeapAllocator with int and shared_ptr") {
    char buffer[4192] __attribute__ ((aligned (256)));
    O1HeapInstance* heap = o1heapInit(buffer, 4192);
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(heap);
    size_t allocated = diagnostics.allocated;
    O1HeapAllocator<int> intAllocator(heap);
    {
        auto intPtr = allocate_shared_custom<int>(intAllocator, 100);
        REQUIRE(intPtr != nullptr);
        CHECK(*intPtr == 100);
        diagnostics = o1heapGetDiagnostics(heap);
        CHECK(allocated != diagnostics.allocated);
    }
    diagnostics = o1heapGetDiagnostics(heap);
    CHECK(allocated == diagnostics.allocated);
}

TEST_CASE("O1HeapAllocator with int and unique_ptr") {
    char buffer[4192] __attribute__ ((aligned (256)));
    O1HeapInstance* heap = o1heapInit(buffer, 4192);
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(heap);
    size_t allocated = diagnostics.allocated;
    O1HeapAllocator<int> intAllocator(heap);
    {    
        auto intPtr = allocate_unique_custom<int>(intAllocator, 100);
        REQUIRE(intPtr != nullptr);
        CHECK(*intPtr == 100);
        diagnostics = o1heapGetDiagnostics(heap);
        CHECK(allocated != diagnostics.allocated);
    }
    diagnostics = o1heapGetDiagnostics(heap);
    CHECK(allocated == diagnostics.allocated);
}

TEST_CASE("O1HeapAllocator with CanardRxTransfer and shared_ptr") {
    char buffer[4192] __attribute__ ((aligned (256)));
    O1HeapInstance* heap = o1heapInit(buffer, 4192);
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(heap);
    size_t allocated = diagnostics.allocated;
    O1HeapAllocator<CanardRxTransfer> CanardRxTransferAllocator(heap);
    {    
        auto CanardRxTransferPtr = allocate_shared_custom<CanardRxTransfer>(CanardRxTransferAllocator);
        REQUIRE(CanardRxTransferPtr != nullptr);
        diagnostics = o1heapGetDiagnostics(heap);
        CHECK(allocated != diagnostics.allocated);
        if (CanardRxTransferPtr) CanardRxTransferPtr->payload = o1heapAllocate(heap, 100);
    }
    diagnostics = o1heapGetDiagnostics(heap);
    CHECK(allocated == diagnostics.allocated);
}

TEST_CASE("O1HeapAllocator with CanardRxTransfer and unique_ptr") {
    char buffer[4192] __attribute__ ((aligned (256)));
    O1HeapInstance* heap = o1heapInit(buffer, 4192);
    O1HeapDiagnostics diagnostics = o1heapGetDiagnostics(heap);
    size_t allocated = diagnostics.allocated;
    O1HeapAllocator<CanardRxTransfer> CanardRxTransferAllocator(heap);
    {    
        auto CanardRxTransferPtr = allocate_unique_custom<CanardRxTransfer>(CanardRxTransferAllocator);
        REQUIRE(CanardRxTransferPtr != nullptr);
        diagnostics = o1heapGetDiagnostics(heap);
        CHECK(allocated != diagnostics.allocated);
        if (CanardRxTransferPtr) CanardRxTransferPtr->payload = o1heapAllocate(heap, 100);
    }
    diagnostics = o1heapGetDiagnostics(heap);
    CHECK(allocated == diagnostics.allocated);
}


TEST_CASE("O1HeapAllocator allocation and deallocation") {
    char buffer[4192] __attribute__ ((aligned (256)));
    O1HeapInstance* heap = o1heapInit(buffer, 4192);
    REQUIRE(heap != nullptr);
    O1HeapAllocator<int> intAllocator(heap);

    int* ptr = intAllocator.allocate(5);
    for (int i = 0; i < 5; ++i) {
        ptr[i] = i;
    }

    for (int i = 0; i < 5; ++i) {
        CHECK(ptr[i] == i);
    }

    intAllocator.deallocate(ptr, 5);
}