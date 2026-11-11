#include "poko_cli_stub.h"

int main(int argc, char** argv) {
    return poko::cli::run_stub("poko-assetc",
                               "Asset compiler CLI (systemplan/05)", argc, argv);
}
