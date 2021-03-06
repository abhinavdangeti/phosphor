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

#pragma once

#include "phosphor/trace_buffer.h"
#include "phosphor/trace_log.h"

namespace phosphor {
    namespace tools {

        /**
         * The JSONExport class is a tool provided to allow exporting
         * a TraceBuffer in the Chromium Tracing JSON format in a
         * chunked manner (e.g. for over the network).
         *
         * Usage:
         *
         *     auto buffer = phosphor::TraceLog::instance().getBuffer();
         *     JSONExport exporter(*buffer);
         *
         *     do {
         *         p = exporter.read(80);
         *         std::cerr << p;
         *     }  while(p.size());
         *
         * The above will write the JSON to stderr.
         *
         */
        class JSONExport {
        public:
            /**
             * Creates the export object
             */
            JSONExport(const TraceBuffer& _buffer);

            /**
             * Read 'length' worth of JSON
             *
             * @param out roughly 'length' bytes of the JSON starting from
             *            the point that was previously left off. This will
             *            return less than 'length' at the end of the buffer.
             * @param length Max size in bytes of the JSON to put into out
             * @return Number of bytes written to out
             */
            size_t read(char* out, size_t length);

            /**
             * Read 'length' worth of JSON
             *
             * @returns roughly 'length' bytes of the JSON starting from
             *          the point that was previously left off. This will
             *          return less than 'length' at the end of the buffer.
             */
            std::string read(size_t length);

        protected:
            enum class State {
                opening,
                first_event,
                other_events,
                footer,
                dead
            };

            const TraceBuffer& buffer;
            TraceBuffer::event_iterator it;
            State state = State::opening;
            std::string cache;
        };

        /**
         * Reference callback for saving a buffer to a file if tracing stops.
         *
         * This saves the buffer to file in the same thread that it is called
         * from so that it may be used after main has returned (e.g. to write
         * everything that a given process has traced when the global TraceLog
         * is destructed).
         */
        class FileStopCallback {
        public:
            /**
             * @param _file_path File path to save the buffer to on completion,
             *                   may accept the wild cards %p for PID and %d for
             *                   an ISOish timestamp 'YYYY.MM.DDTHH.MM.SS'
             */
            FileStopCallback(
                const std::string& _file_path = "phosphor.%p.json");

            /**
             * Callback method called by TraceLog
             *
             * @param log Reference to the calling TraceLog
             * @param lh The lock being held when this callback is invoked
             */
            void operator()(TraceLog& log, std::lock_guard<TraceLog>& lh);

        protected:
            std::string file_path;
            std::string generateFilePath();
        };
    }
}
