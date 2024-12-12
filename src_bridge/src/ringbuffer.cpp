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
// Note: Some portions of this class were generated using the Claude.AI LLM

#include "RingBuffer.h"
#include "FreeRTOS.h"
#include "semphr.h"

template <typename T>
RingBuffer<T>::RingBuffer(size_t size, T *buffer_ptr) : bufferSize(size),
                                                        buffer(buffer_ptr),
                                                        head(0),
                                                        tail(0),
                                                        overflow_flag(false)
{
    if (!buffer)
    {
        buffer = new (std::nothrow) T[size];
    }
    mutex = xSemaphoreCreateMutex();
    // Note: If mutex creation fails, mutex will be NULL
}

template <typename T>
RingBuffer<T>::RingBuffer(RingBuffer &&other) noexcept : bufferSize(other.bufferSize),
                                                         buffer(other.buffer),
                                                         head(other.head),
                                                         tail(other.tail),
                                                         mutex(other.mutex)
{
    // Take ownership of resources
    other.buffer = nullptr;
    other.mutex = nullptr;
}

template <typename T>
RingBuffer<T>::~RingBuffer()
{
    if (buffer)
    {
        delete[] buffer;
    }
    if (mutex)
    {
        vSemaphoreDelete(mutex);
    }
}

template <typename T>
size_t RingBuffer<T>::write(const T *data, size_t length)
{
    if (!data || !length || !buffer || !mutex)
    {
        return 0;
    }

    size_t written = 0;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < length; i++)
        {
            size_t next = (head + 1) % bufferSize;
            if (next != tail)
            {
                buffer[head] = data[i];
                head = next;
                written++;
            }
            else
            {
                break; // Buffer full
            }
        }
        xSemaphoreGive(mutex);
    }
    return written;
}

template <typename T>
bool RingBuffer<T>::write(const T &data)
{
    return write(&data, 1) == 1;
}

template <typename T>
size_t RingBuffer<T>::read(T *data, size_t length)
{
    if (!data || !length || !buffer || !mutex)
    {
        return 0;
    }

    size_t read_count = 0;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        while (read_count < length && tail != head)
        {
            data[read_count++] = buffer[tail];
            tail = (tail + 1) % bufferSize;
        }
        xSemaphoreGive(mutex);
    }
    return read_count;
}

template <typename T>
std::optional<T> RingBuffer<T>::read()
{
    T value{};
    if (read(&value, 1) == 1)
    {
        return value;
    }
    return std::nullopt;
}

template <typename T>
std::optional<T> RingBuffer<T>::peek() const
{
    if (!buffer || !mutex || isEmpty())
    {
        return std::nullopt;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        T value = buffer[tail];
        xSemaphoreGive(mutex);
        return value;
    }
    return std::nullopt;
}

template <typename T>
void RingBuffer<T>::clear()
{
    if (!buffer || !mutex)
    {
        return;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        head = tail = 0;
        xSemaphoreGive(mutex);
    }
}

template <typename T>
size_t RingBuffer<T>::available() const
{
    if (!buffer || !mutex)
    {
        return 0;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        size_t count;
        if (head >= tail)
        {
            count = head - tail;
        }
        else
        {
            count = bufferSize - (tail - head);
        }
        xSemaphoreGive(mutex);
        return count;
    }
    return 0;
}

template <typename T>
bool RingBuffer<T>::isEmpty() const
{
    if (!buffer || !mutex)
    {
        return true;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        bool empty = (head == tail);
        xSemaphoreGive(mutex);
        return empty;
    }
    return true;
}

template <typename T>
bool RingBuffer<T>::isFull() const
{
    if (!buffer || !mutex)
    {
        return true;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        size_t next = (head + 1) % bufferSize;
        bool full = (next == tail);
        xSemaphoreGive(mutex);
        return full;
    }
    return true;
}

template <typename T>
size_t RingBuffer<T>::capacity() const
{
    return bufferSize - 1; // One slot always empty to distinguish full from empty
}

template <typename T>
bool RingBuffer<T>::hasOverflowed() const
{
    if (!buffer || !mutex)
    {
        return false;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        bool has_overflowed = overflow_flag;
        xSemaphoreGive(mutex);
        return has_overflowed;
    }
    return false;
}

template <typename T>
void RingBuffer<T>::clearOverflow()
{
    if (!buffer || !mutex)
    {
        return;
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        overflow_flag = false;
        xSemaphoreGive(mutex);
    }
}

template <typename T>
bool RingBuffer<T>::checkAndClearOverflow()
{
    if (!buffer || !mutex)
    {
        return false;
    }

    bool had_overflow = false;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
    {
        had_overflow = overflow_flag;
        overflow_flag = false;
        xSemaphoreGive(mutex);
    }
    return had_overflow;
}

// Explicit template instantiation for chars
template class RingBuffer<char>;
