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

---------------------|-------------------------|--------------------|
| Depth/Ticks | Tick/s (SharedPtr) | Tick/s (ManagedEntity) | %-Change (Tick/s) | Visit/s (SharedPtr) | Visit/s (ManagedEntity) | %-Change (Visit/s) |
|-------------|--------------------|------------------------|-------------------|---------------------|-------------------------|--------------------|
| 1/1 | 37.459k | 38.213k | +2.01% | 186.784 | 3.37998k | +1709.57% |
| 1/8 | 299.076k | 303.453k | +1.46% | 14.3101k | 1.99678k | -86.05% |
| 1/64 | 2.33488M | 2.24199M | -3.98% | 72.5592k | 126.245k | +73.99% |
| 1/512 | 2.76475M | 2.28316M | -17.42% | 6.64838M | 6.79573M | +2.22% |
| 1/2000 | 2.58336M | 2.30162M | -10.91% | 6.95764M | 7.27152M | +4.51% |
| 8/1 | 27.3378k | 31.2711k | +14.39% | 14.3744k | 42.1447k | +193.19% |
| 8/8 | 50.8016k | 108.822k | +114.21% | 53.6824k | 1049.41k | +1854.85% |
| 8/64 | 76.0643k | 146.696k | +92.86% | 102.194k | 1.646M | +1510.66% |
| 8/512 | 61.9295k | 168.744k | +172.48% | 90.1897k | 1.56162M | +1631.48% |
| 8/2000 | 67.7613k | 159.8k | +135.83% | 94.997k | 1.7113M | +1701.43% |
| 15/1 | 326.351 | 1066.75 | +226.87% | 332.827 | 3.07806k | +824.82% |
| 15/8 | 638.343 | 1.6006k | +150.74% | 725.39 | 8.9282k | +1130.81% |
| 15/64 | 523.81 | 1055.59 | +101.52% | 587.649 | 4.54172k | +672.86% |
| 15/512 | 523.186 | 620.469 | +18.59% | 596.759 | 1.59965k | +168.06% |
| 15/2000 | 526.039 | 594.934 | +13.10% | 586.797 | 1.48823k | +153.62% |
