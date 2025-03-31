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
| 1/1         | 5.46209k          | 5.66755k              | +3.76%            | 5.46209k           | 40.0621k               | +633.33%          |
| 8/1         | 986.426           | 1.11274k              | +12.81%           | 986.426            | 22.0446k               | +2134.47%         |
| 10/1        | 272.325           | 343.302               | +26.08%           | 272.325            | 5.22075k               | +1816.49%         |
| 1/8         | 41.8272k          | 37.1402k              | -11.21%           | 5.2284k            | 108.579k               | +1976.61%         |
| 8/8         | 2.38805k          | 1.60123k              | -32.94%           | 298.506            | 32.918k                | +1092.57%         |
| 10/8        | 628.751           | 425.797               | -32.29%           | 78.5939            | 9.32149k               | +11768.92%        |
| 1/64        | 218.421k          | 117.382k              | -46.23%           | 3.41284k           | 287.149k               | +8309.68%         |
| 8/64        | 2.97532k          | 1.74707k              | -41.31%           | 46.4894            | 40.888k                | +87920.91%        |
| 10/64       | 731.811           | 441.863               | -39.63%           | 11.4345            | 9.98197k               | +87188.92%        |
| 1/200       | 353.78k           | 143.336k              | -59.50%           | 1.7689k            | 355.595k               | +20096.61%        |
| 8/200       | 3.05117k          | 1.75731k              | -42.42%           | 15.2558            | 41.5516k               | +271.59%          |
| 10/200      | 756.997           | 440.76                | -41.77%           | 3.78499            | 10.1948k               | +269.43%          |

```
2025-03-30T18:53:54-04:00
Running ./src/benchmark/cpioo_benchmark
Run on (16 X 1614.12 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 512 KiB (x8)
  L3 Unified 4096 KiB (x2)
Load Average: 1.27, 1.60, 3.06
***WARNING*** Library was built as DEBUG. Timings may be affected.
Allocated superbuffer 0 with index 0 and capacity 126
------------------------------------------------------------------------------------------------------
Benchmark                                            Time             CPU   Iterations UserCounters...
------------------------------------------------------------------------------------------------------
BM_ManagedEntitySimulation/1/1/real_time        176443 ns        61696 ns         4033 Tick_Rate=5.66755k/s Visit_Rate=40.0621k/s
BM_ManagedEntitySimulation/8/1/real_time        898683 ns       688744 ns          773 Tick_Rate=1.11274k/s Visit_Rate=22.0446k/s
BM_ManagedEntitySimulation/10/1/real_time      2912888 ns      2565444 ns          241 Tick_Rate=343.302/s Visit_Rate=5.22075k/s
BM_ManagedEntitySimulation/1/8/real_time        215400 ns        93044 ns         3261 Tick_Rate=37.1402k/s Visit_Rate=108.579k/s
BM_ManagedEntitySimulation/8/8/real_time       4996148 ns      4777370 ns          136 Tick_Rate=1.60123k/s Visit_Rate=32.918k/s
BM_ManagedEntitySimulation/10/8/real_time     18788312 ns     18417552 ns           37 Tick_Rate=425.797/s Visit_Rate=9.32149k/s
BM_ManagedEntitySimulation/1/64/real_time       545230 ns       342231 ns         1197 Tick_Rate=117.382k/s Visit_Rate=287.149k/s
BM_ManagedEntitySimulation/8/64/real_time     36632771 ns     36356033 ns           19 Tick_Rate=1.74707k/s Visit_Rate=40.888k/s
BM_ManagedEntitySimulation/10/64/real_time   144841215 ns    144569879 ns            5 Tick_Rate=441.863/s Visit_Rate=9.98197k/s
BM_ManagedEntitySimulation/1/200/real_time     1395325 ns       957123 ns          488 Tick_Rate=143.336k/s Visit_Rate=355.595k/s
BM_ManagedEntitySimulation/8/200/real_time   113810431 ns    113552606 ns            6 Tick_Rate=1.75731k/s Visit_Rate=41.5516k/s
BM_ManagedEntitySimulation/10/200/real_time  453762023 ns    453214687 ns            2 Tick_Rate=440.76/s Visit_Rate=10.1948k/s
BM_SharedPtrSimulation/1/1/real_time            183080 ns        60025 ns         3827 Tick_Rate=5.46209k/s Visit_Rate=5.46209k/s
BM_SharedPtrSimulation/8/1/real_time           1013760 ns       647714 ns          709 Tick_Rate=986.426/s Visit_Rate=986.426/s
BM_SharedPtrSimulation/10/1/real_time          3672079 ns      2577225 ns          194 Tick_Rate=272.325/s Visit_Rate=272.325/s
BM_SharedPtrSimulation/1/8/real_time            191263 ns        69290 ns         3658 Tick_Rate=41.8272k/s Visit_Rate=5.2284k/s
BM_SharedPtrSimulation/8/8/real_time           3350014 ns      2950479 ns          206 Tick_Rate=2.38805k/s Visit_Rate=298.506/s
BM_SharedPtrSimulation/10/8/real_time         12723638 ns     11736257 ns           55 Tick_Rate=628.751/s Visit_Rate=78.5939/s
BM_SharedPtrSimulation/1/64/real_time           293011 ns       169326 ns         2365 Tick_Rate=218.421k/s Visit_Rate=3.41284k/s
BM_SharedPtrSimulation/8/64/real_time         21510302 ns     21113757 ns           32 Tick_Rate=2.97532k/s Visit_Rate=46.4894/s
BM_SharedPtrSimulation/10/64/real_time        87454256 ns     86387926 ns            8 Tick_Rate=731.811/s Visit_Rate=11.4345/s
BM_SharedPtrSimulation/1/200/real_time          565324 ns       413853 ns         1183 Tick_Rate=353.78k/s Visit_Rate=1.7689k/s
BM_SharedPtrSimulation/8/200/real_time        65548712 ns     65085210 ns           11 Tick_Rate=3.05117k/s Visit_Rate=15.2558/s
BM_SharedPtrSimulation/10/200/real_time      264201695 ns    262977622 ns            3 Tick_Rate=756.997/s Visit_Rate=3.78499/s
```