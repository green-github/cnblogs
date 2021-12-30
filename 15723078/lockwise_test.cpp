/*
 * lockwise_test.cpp
 *
 * Testing Thread_Pool.
 *
 */

#include <cstdio>
#include <cstdlib>

#include <chrono>
#include <functional>
#include <future>
#include <ratio>
#include <thread>

#include "archery.h"
#include "lockwise_mutual_2a_pool.h"


using std::atomic;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::future;
using std::ratio;
using std::thread;


int main() {
    srand(time(0));
    duration<double, ratio<60,1>> PERIOD(0.5);
    unsigned const RAND_LIMIT = 8;
    size_t counter[11];
    time_point<steady_clock> start;
    atomic<bool> go(false);

    {
        Thread_Pool pool;

        thread t1([PERIOD, &counter, &start, &go, &pool] {        // test free function of void() 
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            void (*task)() = shoot;
            for (counter[1] = 0; steady_clock::now() - start <= PERIOD; ++counter[1]) {
                pool.submit(task);
                //pool.submit(std::bind<void(*)()>(shoot));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t2([PERIOD, &counter, &start, &go, &pool] {        // test free function of bool(size_t)
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            bool (*task)(size_t) = shoot;
            for (counter[2] = 0; steady_clock::now() - start <= PERIOD; ++counter[2]) {
                future<bool> r = pool.submit(std::bind(task, counter[2]));
                //future<bool> r = pool.submit(std::bind<bool(*)(size_t)>(shoot, counter[2]));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t3([PERIOD, &counter, &start, &go, &pool] {        // test lambda of void()
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (counter[3] = 0; steady_clock::now() - start <= PERIOD; ++counter[3]) {
                pool.submit(shootAnarrow);
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t4([PERIOD, &counter, &start, &go, &pool] {        // test lambda of bool(size_t)
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            for (counter[4] = 0; steady_clock::now() - start <= PERIOD; ++counter[4]) {
                future<bool> r = pool.submit(std::bind(shootNarrows, counter[4]));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t5([PERIOD, &counter, &start, &go, &pool] {        // test functor of void()
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (counter[5] = 0; steady_clock::now() - start <= PERIOD; ++counter[5]) {
                pool.submit(hoyt);
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t6([PERIOD, &counter, &start, &go, &pool] {        // test functor of bool(size_t)
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (counter[6] = 0; steady_clock::now() - start <= PERIOD; ++counter[6]) {
                future<bool> r = pool.submit(std::bind(hoyt, counter[6]));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t7([PERIOD, &counter, &start, &go, &pool] {        // test member function of void()
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (counter[7] = 0; steady_clock::now() - start <= PERIOD; ++counter[7]) {
                pool.submit(std::bind<void(Archer::*)()>(&Archer::shoot, &hoyt));
                //pool.submit(std::bind(static_cast<void(Archer::*)()>(&Archer::shoot), &hoyt));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t8([PERIOD, &counter, &start, &go, &pool] {        // test member function of bool(size_t)
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            Archer hoyt;
            for (counter[8] = 0; steady_clock::now() - start <= PERIOD; ++counter[8]) {
                future<bool> r = pool.submit(std::bind<bool(Archer::*)(size_t)>(&Archer::shoot, &hoyt, counter[8]));
                //future<bool> r = pool.submit(std::bind(static_cast<bool(Archer::*)(size_t)>(&Archer::shoot), &hoyt, counter[8]));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t9([PERIOD, &counter, &start, &go, &pool] {        // test std::function<> of void()
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<void()> task = static_cast<void(*)()>(shoot);
            for (counter[9] = 0; steady_clock::now() - start <= PERIOD; ++counter[9]) {
                pool.submit(task);
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        thread t10([PERIOD, &counter, &start, &go, &pool] {        // test std::function<> of bool(size_t)
            while (!go.load(memory_order_acquire))
                std::this_thread::yield();
            std::function<bool(size_t)> task = static_cast<bool(*)(size_t)>(shoot);
            for (counter[10] = 0; steady_clock::now() - start <= PERIOD; ++counter[10]) {
                future<bool> r = pool.submit(std::bind(task, counter[10]));
                std::this_thread::yield();
                //std::this_thread::sleep_for(milliseconds(rand()%RAND_LIMIT));
            }
        });

        std::fprintf(stderr, "\nReady...Go\n\nWait a moment...\n");
        std::this_thread::sleep_for(milliseconds(1000));
        start = steady_clock::now();
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

    for (unsigned i = 1; i < 11; ++i)
        counter[0] += counter[i];
    std::fprintf(stderr, "\n%zu tasks submitted in total.\n", counter[0]);
    time_point<steady_clock> end = steady_clock::now();
    std::fprintf(stderr, "\nTook %.3f seconds.\n", duration<double>(end - start).count());

    std::fprintf(stderr, "\nBye...\n");
    return 0;
}

