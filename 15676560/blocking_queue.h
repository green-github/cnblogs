/*
 * blocking_queue.h
 *
 * A generic queue supporting concurrency access.
 *   - blocking
 *   - using std::mutex with condition_variable
 *   - element type is movable
 *
 */

#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H


#include <condition_variable>
#include <mutex>
#include <queue>


using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::queue;
using std::unique_lock;


template<class T>
class Blocking_Queue {

  private:
    mutex mutable _m_;
    condition_variable _cv_;
    queue<T> _q_;

  public:
    void push(T&& element) {
        lock_guard<mutex> lk(_m_);
        _q_.push(std::move(element));
        _cv_.notify_one();
    }

    void pop(T& element) {
        unique_lock<mutex> lk(_m_);
        _cv_.wait(lk, [this]{ return !_q_.empty(); });
        element = std::move(_q_.front());
        _q_.pop();
    }

    bool empty() const {
        lock_guard<mutex> lk(_m_);
        return _q_.empty();
    }

    size_t size() const {
        lock_guard<mutex> lk(_m_);
        return _q_.size();
    }

};


#endif

