#include "poko_cli_stub.h"

int main(int argc, char** argv) {
    return poko::cli::run_stub("poko-packagec",
                               "Package builder for .pk/.pkx/.pkxx (systemplan/13)", argc, argv);
}
