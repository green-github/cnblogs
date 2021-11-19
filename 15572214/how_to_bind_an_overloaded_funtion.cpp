// how_to_bind_an_overloaded_funtion.cpp

#include <cstdio>

#include <functional>


void shoot() {                  // #1
    std::printf("\n\t[Free Function] Let an arrow fly... Hit!\n");
}


bool shoot(unsigned n) {        // #2
    std::printf("\n\t[Free Function] Let %d arrows fly... All missed!\n", n);
    return false;
}


class Archer {
  public:
    void shoot() {              // #3
        std::printf("\n\t[Member Function] Let an arrow fly... Missed!\n");
    }
    bool shoot(unsigned n) {    // #4
        std::printf("\n\t[Member Function] Let %d arrows fly... All hit!\n", n);
        return true;
    }
};


int main() {

    // For overloaded free functions
    // bind #1
    std::bind(shoot)();                                                                 // NG
    //std::bind<void(*)()>(shoot)();                                                    // OK
    //std::bind(static_cast<void(*)()>(shoot))();                                       // OK

    // bind #2
    std::bind(shoot, 3)();                                                              // NG
    //std::bind<bool(*)(unsigned)>(shoot, 3)();                                         // OK
    //std::bind(static_cast<bool(*)(unsigned)>(shoot), 3)();                            // OK

    // For overloaded member functions
    Archer hoyt;
    // bind #3
    std::bind(&Archer::shoot, &hoyt)();                                                 // NG
    //std::bind<void(Archer::*)()>(&Archer::shoot, &hoyt)();                            // OK
    //std::bind(static_cast<void(Archer::*)()>(&Archer::shoot), &hoyt)();               // OK

    // bind #4
    std::bind(&Archer::shoot, &hoyt, 3)();                                              // NG
    //std::bind<bool(Archer::*)(unsigned)>(&Archer::shoot, &hoyt, 5)();                 // OK
    //std::bind(static_cast<bool(Archer::*)(unsigned)>(&Archer::shoot), &hoyt, 5)();    // OK

    std::printf("\nBye...\n");
    return 0;
}
