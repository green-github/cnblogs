/*
 * archery.h
 *
 * Callables for testing Thread_Pool.
 *
 */

#ifndef ARCHERY_H
#define ARCHERY_H


#include <cstdio>


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


#endif

