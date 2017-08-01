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
    uint32_t putdData(T data){
        uint32_t temp = tail;
        this->buffer[tail % buffer_size] = data;
        this->current_size += 1;
        tail += 1;
        if(this->current_size < this->window_size) {

        }
        else {// >=
            head += 1;
            this->current_size -= 1;
        }

        return temp;
    }
    T getData(uint32_t index) {
        return buffer[index % buffer_size];
    }

    T getTotal(uint32_t begin, uint32_t lens) {
        T temp;
        for(uint32_t i = 0; i < lens; ++i) {
            temp = temp + this->buffer[ (begin + i) % buffer_size];
        }
        return temp;
    }

    bool aheadofHead(uint32_t index) {
        return index < head;
    }

    bool aheadoftail(uint32_t index) {
        return index < tail;
    }

    bool updateData(uint32_t index, T data) {
        this->buffer[index % this->buffer_size] = data;
    }
};


#endif //TCP_TUNNEL_RINGBUFFER_H
