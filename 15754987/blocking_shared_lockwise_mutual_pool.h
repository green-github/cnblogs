/*
 * blocking_shared_lockwise_mutual_pool.h
 *
 * A simple thread pool accepting callables as tasks and using:
 *   - a pool task queue saving all submitted tasks
 *   - several mutual task queues within worker threads
 *   - a scheduler thread assigning tasks in pool queue to worker queues
 *
 * Mutual task queue means its tasks would be popped into the other worker
 * threads.
 *
 */

#ifndef BLOCKING_SHARED_LOCKWISE_MUTUAL_POOL_H
#define BLOCKING_SHARED_LOCKWISE_MUTUAL_POOL_H


#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>

#include "blocking_queue.h"
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
    Blocking_Queue<Task_Wrapper> _poolqueue_;
    thread _scheduler_;
    unsigned _workersize_;
    thread* _workers_;
    Lockwise_Queue<Task_Wrapper>* _workerqueues_;

    void work(unsigned index) {
        Task_Wrapper task;
        while (!_done_.load(memory_order_acquire)) {
            if (_workerqueues_[index].pop(task))
                task();
            else
                for (unsigned i = 0; i < _workersize_; ++i)
                    if (_workerqueues_[(index + i + 1) % _workersize_].pop(task)) {
                        task();
                        break;
                    }
            while (_suspend_.load(memory_order_acquire))
                std::this_thread::yield();
        }
    }

    void stop() {
        size_t remaining = 0;
        _suspend_.store(true, memory_order_release);
        remaining = _poolqueue_.size();
        for (unsigned i = 0; i < _workersize_; ++i)
            remaining += _workerqueues_[i].size();
        _suspend_.store(false, memory_order_release);
        while (!_poolqueue_.empty())
            std::this_thread::yield();
        for (unsigned i = 0; i < _workersize_; ++i)
            while (!_workerqueues_[i].empty())
                std::this_thread::yield();
        std::fprintf(stderr, "\n%zu tasks remain before destructing pool.\n", remaining);
        _done_.store(true, memory_order_release);
        _poolqueue_.push([] {});
        for (unsigned i = 0; i < _workersize_; ++i)
            if (_workers_[i].joinable())
                _workers_[i].join();
        if (_scheduler_.joinable())
            _scheduler_.join();
        delete[] _workers_;
        delete[] _workerqueues_;
    }

    void schedule() {
        Task_Wrapper task;
        while (!_done_.load(memory_order_acquire)) {
            _poolqueue_.pop(task);
            _workerqueues_[rand() % _workersize_].push(std::move(task));
        }
    }

  public:
    Thread_Pool() : _suspend_(false), _done_(false) {
        try {
            _workersize_ = thread::hardware_concurrency();
            _workers_ = new thread[_workersize_]();
            _workerqueues_ = new Lockwise_Queue<Task_Wrapper>[_workersize_]();
            for (unsigned i = 0; i < _workersize_; ++i)
                _workers_[i] = thread(&Thread_Pool::work, this, i);
            _scheduler_ = thread(&Thread_Pool::schedule, this);
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
        _poolqueue_.push(std::move(task));
        return r;
    }

};


#endif

