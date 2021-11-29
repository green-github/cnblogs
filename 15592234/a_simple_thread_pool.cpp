// a_simple_thread_pool.cpp

#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>


using std::atomic;
using std::atomic_flag;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::future;
using std::lock_guard;
using std::memory_order_acquire;
using std::memory_order_release;
using std::packaged_task;
using std::queue;
using std::thread;



void shoot() {
    std::printf("\n\t[Free Function] Let an arrow fly...\n");
}


bool shoot(long n) {
    std::printf("\n\t[Free Function] Let %ld arrows fly...\n", n);
    return false;
}


auto shootAnarrow = [] {
    std::printf("\n\t[Lambda] Let an arrow fly...\n");
};


auto shootNarrows = [](long n) -> bool {
    std::printf("\n\t[Lambda] Let %ld arrows fly...\n", n);
    return true;
};


class Archer {

  public:
    void operator()() {
        std::printf("\n\t[Functor] Let an arrow fly...\n");
    }
    bool operator()(long n) {
        std::printf("\n\t[Functor] Let %ld arrows fly...\n", n);
        return false;
    }
    void shoot() {
        std::printf("\n\t[Member Function] Let an arrow fly...\n");
    }
    bool shoot(long n) {
        std::printf("\n\t[Member Function] Let %ld arrows fly...\n", n);
        return true;
    }

};



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
    Lockwise_Queue<Task_Wrapper> _queue_;
    unsigned _workersize_;
    thread* _workers_;

    void work() {
        Task_Wrapper task;
        while (!_done_.load(memory_order_acquire)) {
            if (_queue_.pop(task))
                task();
            else
                std::this_thread::yield();
        }
    }

    void stop() {
        while (!_queue_.empty())
            std::this_thread::yield();
        _done_.store(true, memory_order_release);
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



int main() {

    atomic<bool> go(false);
    time_point<steady_clock> start = steady_clock::now();
    minutes PERIOD(1);

    {
        Thread_Pool pool;

        thread t1([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            void (*task)() = shoot;
            for (long x = 0; steady_clock::now() - start <= PERIOD; ++x) {
                pool.submit(task);
                //pool.submit(std::bind<void(*)()>(shoot));
                std::this_thread::yield();
            }
        });

        thread t2([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            bool (*task)(long) = shoot;
            for (long x = 2; steady_clock::now() - start <= PERIOD; ++x) {
                future<bool> r = pool.submit(std::bind(task, x));
                //future<bool> r = pool.submit(std::bind<bool(*)(long)>(shoot, x));
                std::this_thread::yield();
            }
        });

        thread t3([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (long x = 0; steady_clock::now() - start <= PERIOD; ++x) {
                pool.submit(shootAnarrow);
                std::this_thread::yield();
            }
        });

        thread t4([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (long x = 2; steady_clock::now() - start <= PERIOD; ++x) {
                future<bool> r = pool.submit(std::bind(shootNarrows, x));
                std::this_thread::yield();
            }
        });

        thread t5([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (long x = 0; steady_clock::now() - start <= PERIOD; ++x) {
                pool.submit(hoyt);
                std::this_thread::yield();
            }
        });

        thread t6([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (long x = 2; steady_clock::now() - start <= PERIOD; ++x) {
                future<bool> r = pool.submit(std::bind(hoyt, x));
                std::this_thread::yield();
            }
        });

        thread t7([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (long x = 0; steady_clock::now() - start <= PERIOD; ++x) {
                pool.submit(std::bind<void(Archer::*)()>(&Archer::shoot, &hoyt));
                //pool.submit(std::bind(static_cast<void(Archer::*)()>(&Archer::shoot), &hoyt));
                std::this_thread::yield();
            }
        });

        thread t8([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (long x = 2; steady_clock::now() - start <= PERIOD; ++x) {
                future<bool> r = pool.submit(std::bind<bool(Archer::*)(long)>(&Archer::shoot, &hoyt, x));
                //future<bool> r = pool.submit(std::bind(static_cast<bool(Archer::*)(long)>(&Archer::shoot), &hoyt, x));
                std::this_thread::yield();
            }
        });

        thread t9([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<void()> task = static_cast<void(*)()>(shoot);
            for (long x = 0; steady_clock::now() - start <= PERIOD; ++x) {
                pool.submit(task);
                std::this_thread::yield();
            }
        });

        thread t10([&go, &pool, &PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<bool(long)> task = static_cast<bool(*)(long)>(shoot);
            for (long x = 2; steady_clock::now() - start <= PERIOD; ++x) {
                future<bool> r = pool.submit(std::bind(task, x));
                std::this_thread::yield();
            }
        });

        std::printf("\nReady...\n");
        std::this_thread::sleep_for(milliseconds(1000));
        go.store(true, memory_order_release);

        t10.join();
        t9.join();
        t8.join();
        t7.join();
        t6.join();
        t5.join();
        t4.join();
        t3.join();
        t2.join();
        t1.join();
    }

    std::printf("\nBye...\n");
    return 0;
}

