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

| Depth/Ticks | Tick/s (SharedPtr) | Tick/s (ManagedEntity) | %-Change (Tick/s) | Visit/s (SharedPtr) | Visit/s (ManagedEntity) | %-Change (Visit/s) |
|-------------|--------------------|------------------------|-------------------|---------------------|-------------------------|--------------------|
| 1/1 | 5.51932k | 5.61404k | +1.72% | 13.5682k | 49.6169k | +265.69% |
| 1/8 | 40.0529k | 38.7162k | -3.34% | 58.931k | 97.5503k | +65.53% |
| 1/64 | 152.027k | 144.844k | -4.72% | 281.184k | 252.482k | -10.21% |
| 1/200 | 178.526k | 207.465k | +16.21% | 297.221k | 371.238k | +24.90% |
| 8/1 | 1.20728k | 1.88236k | +55.92% | 2.2101k | 11.6738k | +428.20% |
| 8/8 | 3.31373k | 4.3427k | +31.05% | 4.05611k | 23.4889k | +479.10% |
| 8/64 | 4.06191k | 4.95917k | +22.09% | 5.83598k | 33.7171k | +477.75% |
| 8/200 | 3.72838k | 5.16013k | +38.40% | 5.54597k | 36.4764k | +557.71% |
| 10/1 | 352.791 | 733.677 | +107.96% | 520.786 | 2.92727k | +462.09% |
| 10/8 | 997.538 | 1.22972k | +23.28% | 1.22085k | 7.11414k | +482.72% |
| 10/64 | 987.759 | 1.34371k | +36.04% | 1.48164k | 9.09915k | +514.13% |
| 10/200 | 1040.61 | 1.3103k | +25.92% | 1.53317k | 9.05417k | +490.55% |

```
------------------------------------------------------------------------------------------------------
Benchmark                                            Time             CPU   Iterations UserCounters...
------------------------------------------------------------------------------------------------------
BM_ManagedEntitySimulation/1/1/real_time        178125 ns        62806 ns         3957 Tick_Rate=5.61404k/s Visit_Rate=49.6169k/s
BM_ManagedEntitySimulation/8/1/real_time        531247 ns       301930 ns         1200 Tick_Rate=1.88236k/s Visit_Rate=11.6738k/s
BM_ManagedEntitySimulation/10/1/real_time      1362998 ns       929080 ns          493 Tick_Rate=733.677/s Visit_Rate=2.92727k/s
BM_ManagedEntitySimulation/1/8/real_time        206632 ns        85676 ns         3287 Tick_Rate=38.7162k/s Visit_Rate=97.5503k/s
BM_ManagedEntitySimulation/8/8/real_time       1842173 ns      1628820 ns          377 Tick_Rate=4.3427k/s Visit_Rate=23.4889k/s
BM_ManagedEntitySimulation/10/8/real_time      6505534 ns      6199459 ns           96 Tick_Rate=1.22972k/s Visit_Rate=7.11414k/s
BM_ManagedEntitySimulation/1/64/real_time       441856 ns       272063 ns         1339 Tick_Rate=144.844k/s Visit_Rate=252.482k/s
BM_ManagedEntitySimulation/8/64/real_time     12905394 ns     12673454 ns           53 Tick_Rate=4.95917k/s Visit_Rate=33.7171k/s
BM_ManagedEntitySimulation/10/64/real_time    47629155 ns     47273913 ns           13 Tick_Rate=1.34371k/s Visit_Rate=9.09915k/s
BM_ManagedEntitySimulation/1/200/real_time      964016 ns       702824 ns          664 Tick_Rate=207.465k/s Visit_Rate=371.238k/s
BM_ManagedEntitySimulation/8/200/real_time    38758702 ns     38482627 ns           18 Tick_Rate=5.16013k/s Visit_Rate=36.4764k/s
BM_ManagedEntitySimulation/10/200/real_time  152636772 ns    152399085 ns            4 Tick_Rate=1.3103k/s Visit_Rate=9.05417k/s
BM_SharedPtrSimulation/1/1/real_time            181182 ns        62000 ns         3814 Tick_Rate=5.51932k/s Visit_Rate=13.5682k/s
BM_SharedPtrSimulation/8/1/real_time            828307 ns       527077 ns          744 Tick_Rate=1.20728k/s Visit_Rate=2.2101k/s
BM_SharedPtrSimulation/10/1/real_time          2834543 ns      1973297 ns          252 Tick_Rate=352.791/s Visit_Rate=520.786/s
BM_SharedPtrSimulation/1/8/real_time            199736 ns        77606 ns         3453 Tick_Rate=40.0529k/s Visit_Rate=58.931k/s
BM_SharedPtrSimulation/8/8/real_time           2414199 ns      2157760 ns          284 Tick_Rate=3.31373k/s Visit_Rate=4.05611k/s
BM_SharedPtrSimulation/10/8/real_time          8019747 ns      7454485 ns          110 Tick_Rate=997.538/s Visit_Rate=1.22085k/s
BM_SharedPtrSimulation/1/64/real_time           420978 ns       240433 ns         1398 Tick_Rate=152.027k/s Visit_Rate=281.184k/s
BM_SharedPtrSimulation/8/64/real_time         15756126 ns     15510316 ns           42 Tick_Rate=4.06191k/s Visit_Rate=5.83598k/s
BM_SharedPtrSimulation/10/64/real_time        64793118 ns     64183852 ns           10 Tick_Rate=987.759/s Visit_Rate=1.48164k/s
BM_SharedPtrSimulation/1/200/real_time         1120287 ns       746069 ns          548 Tick_Rate=178.526k/s Visit_Rate=297.221k/s
BM_SharedPtrSimulation/8/200/real_time        53642600 ns     53294080 ns           10 Tick_Rate=3.72838k/s Visit_Rate=5.54597k/s
BM_SharedPtrSimulation/10/200/real_time      192194260 ns    191665131 ns            3 Tick_Rate=1040.61/s Visit_Rate=1.53317k/s
```