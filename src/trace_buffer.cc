/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <mutex>
#include <stdexcept>

#include <dvyukov/mpmc_bounded_queue.h>
#include <gsl_p/dyn_array.h>

#include "phosphor/trace_buffer.h"
#include "utils/memory.h"

namespace phosphor {

    /*
     * TraceChunk implementation
     */

    void TraceChunk::reset() {
        next_free = 0;
    }

    bool TraceChunk::isFull() const {
        return next_free == chunk.max_size();
    }

    size_t TraceChunk::count() const {
        return next_free;
    }

    TraceEvent& TraceChunk::addEvent() {
        if (isFull()) {
            throw std::out_of_range(
                "phosphor::TraceChunk::addEvent: "
                "All events in chunk have been used");
        }
        return chunk[next_free++];
    }

    const TraceEvent& TraceChunk::operator[](const int index) const {
        return chunk[index];
    }

    TraceChunk::const_iterator TraceChunk::begin() const {
        return chunk.begin();
    }

    TraceChunk::const_iterator TraceChunk::end() const {
        return chunk.begin() + count();
    }

    /*
     * TraceBufferChunkIterator implementation
     */
    TraceBuffer::chunk_iterator::chunk_iterator(const TraceBuffer& buffer_,
                                                size_t index_)
        : buffer(buffer_), index(index_) {}

    TraceBuffer::chunk_iterator::chunk_iterator(const TraceBuffer& buffer_)
        : chunk_iterator(buffer_, 0) {}

    const TraceChunk& TraceBuffer::chunk_iterator::operator*() const {
        return buffer[index];
    }
    const TraceChunk* TraceBuffer::chunk_iterator::operator->() const {
        return &(buffer[index]);
    }
    TraceBuffer::chunk_iterator& TraceBuffer::chunk_iterator::operator++() {
        ++index;
        return *this;
    }
    bool TraceBuffer::chunk_iterator::operator==(
        const TraceBuffer::chunk_iterator& other) const {
        return &buffer == &(other.buffer) && index == other.index;
    }
    bool TraceBuffer::chunk_iterator::operator!=(
        const TraceBuffer::chunk_iterator& other) const {
        return !(*this == other);
    }

    /**
     * TraceBuffer implementation that stores events in a fixed-size
     * vector of unique pointers to BufferChunks.
     */
    class FixedTraceBuffer : public TraceBuffer {
    public:
        FixedTraceBuffer(size_t generation_, size_t buffer_size_)
            : buffer(buffer_size_), issued(0), generation(generation_) {}

        ~FixedTraceBuffer() override = default;

        TraceChunk* getChunk() override {
            size_t offset = issued++;
            if (offset >= buffer.size()) {
                return nullptr;
            }
            TraceChunk& chunk = buffer[offset];
            chunk.reset();
            return &chunk;
        }

        void returnChunk(TraceChunk& chunk) override {
            (void)chunk;
        }

        bool isFull() const override {
            return issued >= buffer.size();
        }

        size_t getGeneration() const override {
            return generation;
        }

        const TraceChunk& operator[](const int index) const override {
            return buffer[index];
        }

        size_t chunk_count() const override {
            size_t tmp{issued};
            return (buffer.size() > tmp) ? tmp : buffer.size();
        }

        chunk_iterator chunk_begin() const override {
            return chunk_iterator(*this);
        }

        chunk_iterator chunk_end() const override {
            return chunk_iterator(*this, chunk_count());
        }

        event_iterator begin() const override {
            return event_iterator(chunk_begin(), chunk_end());
        }

        event_iterator end() const override {
            return event_iterator(chunk_end(), chunk_end());
        }

    protected:
        gsl_p::dyn_array<TraceChunk> buffer;
        std::atomic<size_t> issued;
        size_t generation;
    };

    std::unique_ptr<TraceBuffer> make_fixed_buffer(size_t generation,
                                                   size_t buffer_size) {
        return utils::make_unique<FixedTraceBuffer>(generation, buffer_size);
    }

    /**
     * TraceBuffer implementation that stores events in a fixed-size
     * vector of unique pointers to BufferChunks.
     */
    class RingTraceBuffer : public TraceBuffer {
    public:
        RingTraceBuffer(size_t generation_, size_t buffer_size_)
            : actual_count(0),
              buffer(buffer_size_),
              loaned(buffer_size_),
              return_queue(upper_power_of_two(buffer_size_)),
              in_queue(0),
              generation(generation_) {}

        ~RingTraceBuffer() override = default;

        TraceChunk* getChunk() override {
            TraceChunk* chunk = reinterpret_cast<TraceChunk*>(0xDEADB33F);

            auto offset = actual_count++;

            if (offset >= buffer.size()) {
                assert(in_queue > 0);
                while (!return_queue.dequeue(chunk)) {
                }
                --in_queue;
            } else {
                chunk = &buffer[offset];
                loaned[chunk - &buffer[0]] = false;
            }

            chunk->reset();
            chunk->generation = generation;

            assert(!loaned[chunk - &buffer[0]]);
            loaned[chunk - &buffer[0]] = true;

            return chunk;
        }

        void returnChunk(TraceChunk& chunk) override {
            assert(chunk.generation == generation);
            assert(loaned[&chunk - &buffer[0]]);
            loaned[&chunk - &buffer[0]] = false;
            while (!return_queue.enqueue(&chunk))
                ;
            ++in_queue;
            assert(in_queue <= buffer.size());
        }

        bool isFull() const override {
            return false;
        }

        size_t getGeneration() const override {
            return generation;
        }

        const TraceChunk& operator[](const int index) const override {
            return buffer[index];
        }

        size_t chunk_count() const override {
            return actual_count;
        }

        chunk_iterator chunk_begin() const override {
            return chunk_iterator(*this);
        }

        chunk_iterator chunk_end() const override {
            return chunk_iterator(*this, chunk_count());
        }

        event_iterator begin() const override {
            return event_iterator(chunk_begin(), chunk_end());
        }

        event_iterator end() const override {
            return event_iterator(chunk_end(), chunk_end());
        }

    protected:
        std::atomic<size_t> actual_count;

        gsl_p::dyn_array<TraceChunk> buffer;
        gsl_p::dyn_array<std::atomic<bool>> loaned;

        dvyukov::mpmc_bounded_queue<TraceChunk*> return_queue;
        std::atomic<size_t> in_queue;

        size_t generation;

    private:
        template <typename T>
        T upper_power_of_two(T v) {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;

            if (v == 1) {
                v = 2;
            }

            return v;
        }
    };

    std::unique_ptr<TraceBuffer> make_ring_buffer(size_t generation,
                                                  size_t buffer_size) {
        return utils::make_unique<RingTraceBuffer>(generation, buffer_size);
    }
}
