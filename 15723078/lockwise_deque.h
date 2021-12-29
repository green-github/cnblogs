/*
 * lockwise_deque.h
 *
 * A generic deque supporting concurrency access.
 *   - nonblocking
 *   - using spin-lock mutex without condition_variable
 *   - element type is movable
 *
 */

#ifndef LOCKWISE_DEQUE_H
#define LOCKWISE_DEQUE_H


#include <atomic>
#include <mutex>
#include <deque>


using std::atomic_flag;
using std::lock_guard;
using std::memory_order_acquire;
using std::memory_order_release;
using std::deque;


template<class T>
class Lockwise_Deque {

  private:
    struct Spinlock_Mutex {
        atomic_flag _af_;
        Spinlock_Mutex() : _af_(false) {}
        void lock() {
            while (_af_.test_and_set(memory_order_acquire));
        }
        void unlock() {
            _af_.clear(memory_order_release);
        }
    } mutable _m_;
    deque<T> _q_;

  public:
    void push(T&& element) {
        lock_guard<Spinlock_Mutex> lk(_m_);
        _q_.push_back(std::move(element));
    }

    bool pop(T& element) {
        lock_guard<Spinlock_Mutex> lk(_m_);
        if (_q_.empty())
            return false;
        element = std::move(_q_.front());
        _q_.pop_front();
        return true;
    }

    bool pull(T& element) {
        lock_guard<Spinlock_Mutex> lk(_m_);
        if (_q_.empty())
            return false;
        element = std::move(_q_.back());
        _q_.pop_back();
        return true;
    }

    bool empty() const {
        lock_guard<Spinlock_Mutex> lk(_m_);
        return _q_.empty();
    }

    size_t size() const {
        lock_guard<Spinlock_Mutex> lk(_m_);
        return _q_.size();
    }

};


#endif

