#pragma once

#include <cstdlib>
#include <iostream>

#define CPU_CHECK(condition, message)                                      \
    do                                                                     \
    {                                                                      \
        if (!(condition))                                                  \
        {                                                                  \
            std::cerr << "CPU_CHECK failed: " << (message)                 \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";   \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)
