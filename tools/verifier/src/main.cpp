#include "poko_cli_stub.h"

int main(int argc, char** argv) {
    return poko::cli::run_stub(
        "poko-verifier", "Package signature and manifest verifier (systemplan/13)", argc, argv);
}
