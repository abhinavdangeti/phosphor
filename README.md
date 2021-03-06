# Phosphor

[![License](https://img.shields.io/github/license/couchbaselabs/phosphor.svg)](LICENSE.txt)

Phosphor is a high-performance event tracing framework for C++11 applications
designed for use in Couchbase Server - specifically KV Engine and ForestDB.

Event tracing is implemented in an application by instrumenting it as
described in [phosphor.h](include/phosphor/phosphor.h). You can then enable
and manage tracing using the management API described in
[trace_log.h](include/phosphor/trace_log.h).

## Example
The following is an example of hypothetical instrumentation in memcached:

    void performSet(ENGINE_HANDLE* engine, const char* key, const char* value) {
        TRACE_EVENT_START("Memcached:Operation", "performSet", key)
        // Perform a set operation
        TRACE_EVENT_END0("Memcached:Operation", "performSet")
    }

The following is an example of enabling tracing with a fixed-style 5MB buffer:


    phosphor::TraceLog::getInstance().start(TraceConfig(BufferMode::fixed, 5))

Once the trace buffer becomes full you can retrieve it and iterate over it:

    auto trace_buffer = phosphor::TraceLog::getInstance().getBuffer()
    for(auto& event : *trace_buffer) {
        std::cout << event << '\n';
    }

## Build

Phosphor is written in C++11 and requires a mostly conforming compiler. Many
early C++11 implementations used slow or even inaccurate clock sources which can
cause issues with tracing.

Phosphor can be built with CMake, a top level Makefile is provided for
convenience. A simple `make compile` and `make test` will compile and run the
tests.

Phosphor can be built and tested with code coverage by running `make covered`.

## Documentation

Documentation for Phosphor can be found on
[labs.couchbase.com](http://labs.couchbase.com/phosphor/). The documentation is
generated by Doxygen and can be done using `make docs` with the top level
Makefile.
