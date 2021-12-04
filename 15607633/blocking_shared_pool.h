/*
 * blocking_shared_pool.h
 *
 * A simple thread pool using a shared task queue among worker threads
 * accepts callables as tasks.
 *
 */

#ifndef BLOCKING_SHARED_POOL_H
#define BLOCKING_SHARED_POOL_H


#include <cstdio>

#include <atomic>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>

#include "blocking_queue.h"


using std::atomic;
using std::future;
using std::memory_order_acquire;
using std::memory_order_release;
using std::packaged_task;
using std::thread;


class Thread_Pool {

  private:

    struct Task_Wrapper {

        struct Task_Base {
            virtual ~Task_Base() {}
            virtual void call() = 0;
        };
        template<class T>
        struct Task : Task_Base {
            T _t_;
            Task(T&& t) : _t_(std::move(t)) {}
            void call() { _t_(); }
        };

        Task_Base* _ptr_;

        Task_Wrapper() : _ptr_(nullptr) {};
        // support move
        Task_Wrapper(Task_Wrapper&& other) {
            _ptr_ = other._ptr_;
            other._ptr_ = nullptr;
        }
        Task_Wrapper& operator=(Task_Wrapper&& other) {
            _ptr_ = other._ptr_;
            other._ptr_ = nullptr;
            return *this;
        }
        // no copy
        Task_Wrapper(Task_Wrapper&) = delete;
        Task_Wrapper& operator=(Task_Wrapper&) = delete;
        ~Task_Wrapper() {
            if (_ptr_) delete _ptr_;
        }
        template<class T>
        Task_Wrapper(T&& t) : _ptr_(new Task<T>(std::move(t))) {}

        void operator()() const {
            _ptr_->call();
        }

    };

    atomic<bool> _done_;
    Blocking_Queue<Task_Wrapper> _queue_;
    unsigned _workersize_;
    thread* _workers_;

    void work() {
        Task_Wrapper task;
        while (!_done_.load(memory_order_acquire)) {
            _queue_.pop(task);
            task();
        }
    }

    void stop() {
        size_t remaining = _queue_.size();
        while (!_queue_.empty())
            std::this_thread::yield();
        std::fprintf(stderr, "\n%zu tasks remain before destructing pool.\n", remaining);
        _done_.store(true, memory_order_release);
        for (unsigned i = 0; i < _workersize_; ++i)
            _queue_.push([] {});
        for (unsigned i = 0; i < _workersize_; ++i) {
            if (_workers_[i].joinable())
                _workers_[i].join();
        }
        delete[] _workers_;
    }

  public:
    Thread_Pool() : _done_(false) {
        try {
            _workersize_ = thread::hardware_concurrency();
            _workers_ = new thread[_workersize_];
            for (unsigned i = 0; i < _workersize_; ++i) {
                _workers_[i] = thread(&Thread_Pool::work, this);
            }
        } catch (...) {
            stop();
            throw;
        }
    }
    ~Thread_Pool() {
        stop();
    }

    template<class Callable>
    future<typename std::result_of<Callable()>::type> submit(Callable c) {
        typedef typename std::result_of<Callable()>::type R;
        packaged_task<R()> task(c);
        future<R> r = task.get_future();
        _queue_.push(std::move(task));
        return r;
    }

};


#endif

