#pragma once

// Note: This class was generated using the Claude.AI LLM

#include "FreeRTOS.h"
#include "semphr.h"
#include <cstddef>
#include <optional>

template<typename T>
class RingBuffer {
public:
    // Return status for initialization
    enum class Status {
        OK,
        MUTEX_INIT_FAILED,
        ALLOCATION_FAILED
    };

    // // Static creation method instead of constructor
    // static RingBuffer<T> create(size_t size);
    
    RingBuffer(size_t size, T* buffer_ptr);

    // Move constructor and assignment
    RingBuffer(RingBuffer&& other) noexcept;
    RingBuffer& operator=(RingBuffer&& other) noexcept;
    
    ~RingBuffer();

    // Disable copy operations
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Write operations
    size_t write(const T* data, size_t length);
    bool write(const T& data);

    // Read operations
    size_t read(T* data, size_t length);
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
    T* buffer;
    volatile size_t head;
    volatile size_t tail;
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutexBuffer;
    volatile bool overflow_flag;;
};

// #pragma once

// #include "FreeRTOS.h"
// #include "semphr.h"
// #include <cstddef>
// #include <array>
// #include <optional>

// template<typename T>
// class RingBuffer {
// public:
//     explicit RingBuffer(size_t size);
//     ~RingBuffer();

//     // Disable copy constructor and assignment operator
//     RingBuffer(const RingBuffer&) = delete;
//     RingBuffer& operator=(const RingBuffer&) = delete;

//     // Write operations
//     size_t write(const T* data, size_t length);
//     bool write(const T& data);

//     // Read operations
//     size_t read(T* data, size_t length);
//     std::optional<T> read();
//     std::optional<T> peek() const;

//     // Buffer management
//     void clear();
//     size_t available() const;
//     bool isEmpty() const;
//     bool isFull() const;
//     size_t capacity() const;

// private:
//     const size_t bufferSize;
//     T* buffer;
//     volatile size_t head;
//     volatile size_t tail;
//     SemaphoreHandle_t mutex;
//     StaticSemaphore_t mutexBuffer;
// };