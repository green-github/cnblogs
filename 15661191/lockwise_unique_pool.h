/*
 * lockwise_unique_pool.h
 *
 * A simple thread pool using a unique task queue within each worker thread,
 * accepting callables as tasks.
 *
 */

#ifndef LOCKWISE_UNIQUE_POOL_H
#define LOCKWISE_UNIQUE_POOL_H


#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>

#include "lockwise_queue.h"


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

    atomic<bool> _suspend_;
    atomic<bool> _done_;
    unsigned _workersize_;
    thread* _workers_;
    Lockwise_Queue<Task_Wrapper>* _workerqueues_;

    void work(unsigned index) {
        Task_Wrapper task;
        while (!_done_.load(memory_order_acquire)) {
            if (_workerqueues_[index].pop(task))
                task();
            while (_suspend_.load(memory_order_acquire))
                std::this_thread::yield();
        }
    }

    void stop() {
        size_t remaining = 0;
        _suspend_.store(true, memory_order_release);
        for (unsigned i = 0; i < _workersize_; ++i)
            remaining += _workerqueues_[i].size();
        _suspend_.store(false, memory_order_release);
        for (unsigned i = 0; i < _workersize_; ++i)
            while (!_workerqueues_[i].empty())
                std::this_thread::yield();
        std::fprintf(stderr, "\n%zu tasks remain before destructing pool.\n", remaining);
        _done_.store(true, memory_order_release);
        for (unsigned i = 0; i < _workersize_; ++i)
            if (_workers_[i].joinable())
                _workers_[i].join();
        delete[] _workers_;
        delete[] _workerqueues_;
    }

  public:
    Thread_Pool() : _suspend_(false), _done_(false) {
        try {
            _workersize_ = thread::hardware_concurrency();
            _workers_ = new thread[_workersize_]();
            _workerqueues_ = new Lockwise_Queue<Task_Wrapper>[_workersize_]();
            for (unsigned i = 0; i < _workersize_; ++i)
                _workers_[i] = thread(&Thread_Pool::work, this, i);
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
        _workerqueues_[std::rand() % _workersize_].push(std::move(task));
        return r;
    }

};


#endif

