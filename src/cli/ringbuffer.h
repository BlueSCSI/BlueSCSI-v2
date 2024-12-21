// Thread-safe ring buffer template class for use within the FreeRTOS 
// environment
//
// Copyright (C) 2024 akuker
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.
//
// Note: Some portions of this class may have been generated using the Claude.AI LLM

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"
#include <cstddef>
#include <optional>

template <typename T>
class RingBuffer
{
public:
    // Return status for initialization
    enum class Status
    {
        OK,
        MUTEX_INIT_FAILED,
        ALLOCATION_FAILED
    };

    RingBuffer(size_t size, T *buffer_ptr);

    // Move constructor and assignment
    RingBuffer(RingBuffer &&other) noexcept;
    RingBuffer &operator=(RingBuffer &&other) noexcept;

    ~RingBuffer();

    // Disable copy operations
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    // Write operations
    size_t write(const T *data, size_t length);
    bool write(const T &data);

    // Read operations
    size_t read(T *data, size_t length);
    std::optional<T> read();
    std::optional<T> peek() const;

    // Buffer management
    void clear();
    size_t available() const;
    bool isEmpty() const;
    bool isFull() const;
    size_t capacity() const;

    // Overflow handling
    bool hasOverflowed() const;
    void clearOverflow();
    bool checkAndClearOverflow();

private:
    const size_t bufferSize;
    T *buffer;
    volatile size_t head;
    volatile size_t tail;
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutexBuffer;
    volatile bool overflow_flag;
    ;
};
