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


