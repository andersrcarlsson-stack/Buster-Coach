// test.h — TestHarness: perft (counting leaf nodes), for the model's test gate.

#pragma once
#include "position.h"

class TestHarness { // Declaration of the Test class
public:
    int long long perft(const Position::Chess& status, int depth);
};
