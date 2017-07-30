//
// Created by dalaoshe on 17-7-27.
//

#include "RingBuffer.h"


template <class T>
uint32_t RingBuffer<T>::putdData(T data) {
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

template <class T>
T RingBuffer<T>::getData(uint32_t index) {
    return buffer[index % buffer_size];
}

template <class T>
T RingBuffer<T>::getTotal(uint32_t begin, uint32_t lens) {
    T temp;
    for(uint32_t i = begin; i < begin + lens; ++i) {
        temp += this->buffer[i % buffer_size];
    }
    return temp;
}
