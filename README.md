# Memory_Management

In this project a memory management library called sbmem.a is implemented
(buddy memory allocation from shared memory). Processes uses the library to
allocate memory space dynamically, instead of using the well-known malloc function.
A shared memory object, i.e., a shared memory segment, is created first.
Memory space allocations are made from that segment to the requesting
processes. The library implements the necessary initialization, allocation and
deallocation routines. The library keeps track of the allocated and free spaces in
the memory segment. For that the Buddy memory allocation algoritm is used.
