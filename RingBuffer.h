//
// Created by dalaoshe on 17-7-27.
//

#ifndef TCP_TUNNEL_RINGBUFFER_H
#define TCP_TUNNEL_RINGBUFFER_H


#include <cstdint>
template <class T>
class RingBuffer {
    uint32_t buffer_size;
    uint32_t window_size;
    uint32_t current_size;
    uint32_t head;
    uint32_t tail;
    T* buffer;
public:
    RingBuffer(uint32_t buffer_size, uint32_t window_size) {
        this->buffer = new T[buffer_size];
        this->buffer_size = buffer_size;
        this->window_size = window_size;
        this->head = tail = current_size = 0;
    }
    ~RingBuffer() {
        delete buffer;
    }
    uint32_t putdData(T data) ;
    T getData(uint32_t index);
    T getTotal(uint32_t begin, uint32_t lens);
    bool aheadofHead(uint32_t index) {
        return index < head;
    }
    bool updateData(uint32_t index, T data) {
        this->buffer[index % this->buffer_size] = data;
    }
};


#endif //TCP_TUNNEL_RINGBUFFER_H
