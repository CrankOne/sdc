#include <gtest/gtest.h>
#include "sdc-config.h"

#ifdef ROOT_FOUND
#   include <TSystem.h>
#endif

int main(int argc, char * argv[]) {
    #ifdef ROOT_FOUND
    gSystem->ResetSignals();  // disable stupid ROOT signal handlers
    #endif

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

