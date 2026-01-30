/***************************************************************************************
* Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
* Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
*
* MCSim is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#ifndef __FIXEDQUEUE_H
#define __FIXEDQUEUE_H
#include <vector>
#include <cstddef>
#include <deque>
#include <algorithm>

template <typename T>
class FixedQueue : public std::deque<T> {
private:
    size_t max_capacity;
public:
    explicit FixedQueue(size_t cap) : max_capacity(cap) {};
    
    bool push(const T& item) {
        if (full()) return false;
        this->push_back(item);
        return true;
    }

    void pop() {
        if (!empty()) {
            this->pop_front();
        }
    }

    const T& front() const {
        return std::deque<T>::front();
    }

    bool empty() const {
        return std::deque<T>::empty();
    }

    bool full() const {
        return this->size() >= max_capacity;
    }

    size_t size() const {
        return std::deque<T>::size();
    }

    bool contains(auto equal, auto x) const {
        return std::any_of(this->begin(), this->end(),
            [&](const T item) { return equal(item, x); });
    }

    size_t capacity() const {
        return max_capacity;
    }

    template<typename Iterator>
    void erase(Iterator it) {
        std::deque<T>::erase(it);
    }

    template<typename Predicate>
    void remove_if(Predicate pred) {
        this->erase(std::remove_if(this->begin(), this->end(), pred), this->end());
    }
};
#endif