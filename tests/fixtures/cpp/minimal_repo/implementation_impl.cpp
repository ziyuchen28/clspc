#include "implementation_base.hpp"

struct Derived final : Base {
    int run() override {
        return 1;
    }
};
