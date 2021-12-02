// a_simple_thread_pool.cpp

#include <cstdio>

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
using std::chrono::duration;
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
    std::fprintf(stdout, "\n\t[Free Function] Let an arrow fly...\n");
}


bool shoot(size_t n) {
    std::fprintf(stdout, "\n\t[Free Function] Let %zu arrows fly...\n", n);
    return false;
}


auto shootAnarrow = [] {
    std::fprintf(stdout, "\n\t[Lambda] Let an arrow fly...\n");
};


auto shootNarrows = [](size_t n) -> bool {
    std::fprintf(stdout, "\n\t[Lambda] Let %zu arrows fly...\n", n);
    return true;
};


class Archer {

  public:
    void operator()() {
        std::fprintf(stdout, "\n\t[Functor] Let an arrow fly...\n");
    }
    bool operator()(size_t n) {
        std::fprintf(stdout, "\n\t[Functor] Let %zu arrows fly...\n", n);
        return false;
    }
    void shoot() {
        std::fprintf(stdout, "\n\t[Member Function] Let an arrow fly...\n");
    }
    bool shoot(size_t n) {
        std::fprintf(stdout, "\n\t[Member Function] Let %zu arrows fly...\n", n);
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
        size_t remaining = _queue_.size();
        while (!_queue_.empty())
            std::this_thread::yield();
        std::fprintf(stderr, "\n%zu tasks remain before destructing pool.\n", remaining);
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
    minutes PERIOD(1);
    time_point<steady_clock> start = steady_clock::now();

    {
        Thread_Pool pool;

        size_t c1 = 0;
        thread t1([&go, &pool, &c1, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            void (*task)() = shoot;
            for (c1 = 0; steady_clock::now() - start <= PERIOD; ++c1) {
                pool.submit(task);
                //pool.submit(std::bind<void(*)()>(shoot));
                std::this_thread::yield();
            }
        });

        size_t c2 = 0;
        thread t2([&go, &pool, &c2, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            bool (*task)(size_t) = shoot;
            for (c2 = 2; steady_clock::now() - start <= PERIOD; ++c2) {
                future<bool> r = pool.submit(std::bind(task, c2));
                //future<bool> r = pool.submit(std::bind<bool(*)(size_t)>(shoot, c2));
                std::this_thread::yield();
            }
        });

        size_t c3 = 0;
        thread t3([&go, &pool, &c3, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (c3 = 0; steady_clock::now() - start <= PERIOD; ++c3) {
                pool.submit(shootAnarrow);
                std::this_thread::yield();
            }
        });

        size_t c4 = 0;
        thread t4([&go, &pool, &c4, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (c4 = 2; steady_clock::now() - start <= PERIOD; ++c4) {
                future<bool> r = pool.submit(std::bind(shootNarrows, c4));
                std::this_thread::yield();
            }
        });

        size_t c5 = 0;
        thread t5([&go, &pool, &c5, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (c5 = 0; steady_clock::now() - start <= PERIOD; ++c5) {
                pool.submit(hoyt);
                std::this_thread::yield();
            }
        });

        size_t c6 = 0;
        thread t6([&go, &pool, &c6, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (c6 = 2; steady_clock::now() - start <= PERIOD; ++c6) {
                future<bool> r = pool.submit(std::bind(hoyt, c6));
                std::this_thread::yield();
            }
        });

        size_t c7 = 0;
        thread t7([&go, &pool, &c7, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (c7 = 0; steady_clock::now() - start <= PERIOD; ++c7) {
                pool.submit(std::bind<void(Archer::*)()>(&Archer::shoot, &hoyt));
                //pool.submit(std::bind(static_cast<void(Archer::*)()>(&Archer::shoot), &hoyt));
                std::this_thread::yield();
            }
        });

        size_t c8 = 0;
        thread t8([&go, &pool, &c8, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (c8 = 2; steady_clock::now() - start <= PERIOD; ++c8) {
                future<bool> r = pool.submit(std::bind<bool(Archer::*)(size_t)>(&Archer::shoot, &hoyt, c8));
                //future<bool> r = pool.submit(std::bind(static_cast<bool(Archer::*)(size_t)>(&Archer::shoot), &hoyt, c8));
                std::this_thread::yield();
            }
        });

        size_t c9 = 0;
        thread t9([&go, &pool, &c9, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<void()> task = static_cast<void(*)()>(shoot);
            for (c9 = 0; steady_clock::now() - start <= PERIOD; ++c9) {
                pool.submit(task);
                std::this_thread::yield();
            }
        });

        size_t c10 = 0;
        thread t10([&go, &pool, &c10, PERIOD, start] {
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<bool(size_t)> task = static_cast<bool(*)(size_t)>(shoot);
            for (c10 = 2; steady_clock::now() - start <= PERIOD; ++c10) {
                future<bool> r = pool.submit(std::bind(task, c10));
                std::this_thread::yield();
            }
        });

        std::fprintf(stderr, "\nReady...Go\n\nWait a moment...\n");
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

        std::fprintf(stderr, "\n%zu tasks submitted in total.\n", c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8 + c9 + c10);
    }

    time_point<steady_clock> end = steady_clock::now();
    std::fprintf(stderr, "\nTook %.3f seconds.\n", duration<double>(end - start).count());

    std::fprintf(stderr, "\nBye...\n");
    return 0;
}

