# PoC - C++ Inside Out Objects

This is an experiment on immutable data storage for C++ inspired on
the Perl approach called "inside-out objects", where the object has
its memory stored in a separate storage from the object itself,
allowing better memory access patterns.

The idea of bringing this to C++ comes from reflections on Data-Driven
design, and also after some early experiments with using shared points
that made me realize that the reference count being close to the data
itself means that we end up introducig a lot of page-faults on
unrelated threads.

The target use case for this proof of concept is on situations where
we are representing the data in the system as a series of "frames",
this is particularly relevant on the context of games. In situations
like this, it's possible to think about the data in a "transactional"
way. Meaning that all threads committing changes to the data will do
so atomically, and that every thread reading the data should be
presented with a fully immutable data representation.

The fact that we will allocate data for a final immutable struct type
in a fixed-size array means that memory fragmentation will not become
an issue over time, as any item that is deallocated leaves the perfect
hole for a new item to be allocated.

Since this scenario usually involves situations where the ownership is
shared, the smart pointer class also implements a reference count, but
instead of holding the reference itself, we presume that most places
will be looking at the reference, not making copies of it, which means
very few places need to touch the memory with the refcount, such that
the pages being loaded are all truly immutable, which should be a
significant benefit for a multi-threaded environment.

## Benchmarks

There's a benchmark in this repo that compares rapidly changing data, in
an system with a producer thread and a consumer thread, such as what you
would see in a game where the entire game state is represented as a tree
of immutable objects.

The results are actually quite interesting:

| Depth/Ticks | Tick/s (SharedPtr) | Tick/s (ManagedEntity) | %-Change (Tick/s) | Visit/s (SharedPtr) | Visit/s (ManagedEntity) | %-Change (Visit/s) | Objects/s (SharedPtr) | Objects/s (ManagedEntity) | %-Change (Objects/s) |
|-------------|--------------------|-----------------------|-------------------|---------------------|-------------------------|--------------------|----------------------|--------------------------|----------------------|
| 1/1 | 37.8287k | 38.2467k | +1.10% | 37.8287k | 38.2467k | +1.10% | 1.45714 | 2.2138k | +151827.75% |
| 1/8 | 36.5264k | 37.1962k | +1.83% | 292.211k | 297.569k | +1.83% | 351.521 | 27.0184 | -92.31% |
| 1/64 | 38.208k | 34.7076k | -9.16% | 2.44531M | 2.22128M | -9.16% | 86.6855k | 488.995k | +464.10% |
| 1/512 | 26.5379k | 23.2289k | -12.47% | 2.26456M | 1.9822M | -12.47% | 6.25942M | 7.403M | +18.27% |
| 1/2000 | 23.4671k | 18.1464k | -22.67% | 2.23496M | 1.72823M | -22.67% | 6.67414M | 8.24933M | +23.60% |
| 8/1 | 226.076k | 256.863k | +13.62% | 28.2595k | 32.1079k | +13.62% | 11.1305k | 40.6797k | +265.48% |
| 8/8 | 539.516k | 1075.3k | +99.31% | 56.7911k | 113.189k | +99.31% | 67.6326k | 1023.4k | +1413.18% |
| 8/64 | 670.683k | 1.56345M | +133.11% | 61.4952k | 143.354k | +133.11% | 97.7966k | 1.6135M | +1549.85% |
| 8/512 | 564.577k | 1.58118M | +180.06% | 57.1498k | 160.056k | +180.06% | 92.3637k | 1.58791M | +1619.19% |
| 8/2000 | 578.474k | 1.52971M | +164.44% | 58.7134k | 155.261k | +164.44% | 92.2861k | 1.67611M | +1716.21% |
| 15/1 | 411.499k | 1.47046M | +257.34% | 297.111 | 1061.7 | +257.34% | 298.339 | 3.21305k | +976.98% |
| 15/8 | 651.292k | 2.17487M | +233.93% | 474.53 | 1.5846k | +233.93% | 546.556 | 8.70343k | +1492.41% |
| 15/64 | 736.963k | 1.54302M | +109.38% | 536.259 | 1.1228k | +109.38% | 635.97 | 4.45084k | +599.85% |
| 15/512 | 691.455k | 946.716k | +36.92% | 501.262 | 686.311 | +36.92% | 576.648 | 1.6702k | +189.64% |
| 15/2000 | 699.908k | 811.668k | +15.97% | 507.342 | 588.353 | +15.97% | 591.814 | 1.53972k | +160.17% |
