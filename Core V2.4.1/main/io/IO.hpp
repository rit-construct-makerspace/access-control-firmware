#include "common/types.hpp"

namespace IO {
    void init();
    IOState get_IO_state();
    bool set_IO_state(IOState state);
};