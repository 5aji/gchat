// signal.hpp - polly-compatible signalfd wrapper
// Saji Champlin 2022

#pragma once
#include "filedes.hpp"
#include <sys/signalfd.h>

namespace polly {

class Signal : public FileDes<Signal> {

};
} // namespace polly
