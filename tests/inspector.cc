#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <unistd.h>

#include <iostream>

#include "sdc.hh"

static void _usage_info(std::ostream & os, const char * appName) {
    os << "Usage:" << std::endl
        << "    $ " << appName << "[-m <module>] -t <type> -k <key> -p <path>" << std::endl;
}

struct AppConf {
    const char * loadModulePath;

};

static int
_parse_arguments(AppConf &, int argc, char * argv[] ) {
    // ...
}


template<typename CT, typename KT> void
_test_load(const std::string & path) {
    sdc::Documents<CT> docs;
    docs.loaders.push_back(std::make_shared<sdc::ExtCSVLoader<KT>>());
    bool added = docs.add(path);
    if(!added) {
        char errbf[256];
        snprintf(errbf, sizeof(errbf), "Can't add \"%s\"", path.c_str());
        throw std::runtime_error(errbf);
    }
}


int
main(int argc, char * argv[]) {
    

    return 0;
}

