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

Additionally, we can also take with us the concept of double-buffering
from game development, meaning that we can have two different views of
the data, in a way that allows the thread performing the writes to be
isolated from the threads performing reads.

That means that as long as we know that all the readers are ready with
the back-buffer,  we can start using  freed objects memory. And  if we
keep a  queue of freed items  and the highest-most index  used, we can
consume any  freed item  as soon  as possible,  and increase  the used
space otherwise.

The fact that we will allocate data for a final immutable struct type
in a fixed-size array means that memory fragmentation will not become
an issue over time, as any item that is deallocated leaves the perfect
hole for a new item to be allocated.

For this to work effectively, we also need to assume that all data is
bitwise copiable and trivially destructible. Meaning we don't need to
worry about invoking constructors and destructors on the data.

Finally, in order to have data that refers to other data we need a
smart pointer class that, instead of an absolute address, resolves the
address from the type identifier and the index into the fixed-address
array.

The implementation strategy for this is that our approach will be to
have a memory manager that has a memmap section for each type id, with
a fixed array, a queue of freed objects and the current max index id.