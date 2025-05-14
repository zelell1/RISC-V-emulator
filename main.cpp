#include <iostream>
#include "simulate.hpp"

int main(int argc, char* argv[]) {
    RiscV::Simulate simulate(argc, argv);
    simulate.Start();
}

