/*
 * lockwise_queue.h
 *
 * A generic queue supporting concurrency access.
 *   - nonblocking
 *   - using spin-lock mutex without condition_variable
 *   - element type is movable
 *
 */

#ifndef LOCKWISE_QUEUE_H
#define LOCKWISE_QUEUE_H


#include <atomic>
#include <mutex>
#include <queue>


using std::atomic_flag;
using std::lock_guard;
using std::memory_order_acquire;
using std::memory_order_release;
using std::queue;


template<class T>
class Lockwise_Queue {

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
    queue<T> _q_;

  public:
    void push(T&& element) {
        lock_guard<Spinlock_Mutex> lk(_m_);
        _q_.push(std::move(element));
    }

    bool pop(T& element) {
        lock_guard<Spinlock_Mutex> lk(_m_);
        if (_q_.empty())
            return false;
        element = std::move(_q_.front());
        _q_.pop();
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

