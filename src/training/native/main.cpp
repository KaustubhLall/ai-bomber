#include "training/native/trainer.h"

#include <torch/torch.h>

#include <cstdlib>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

bool load_cuda_runtime() {
#ifdef _WIN32
    /* MSVC can discard the CUDA import library because dispatcher operators
       are registered by DLL initialization rather than direct symbol calls. */
    if (!LoadLibraryW(L"torch_cuda.dll")) {
        std::cerr << "Could not load torch_cuda.dll (error " << GetLastError()
                  << "). Use tools/run_native_alphazero.ps1.\n";
        return false;
    }
#endif
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" ||
            std::string(argv[1]) == "-h") {
            bomber::az::print_native_help();
            return EXIT_SUCCESS;
        }
        if (!load_cuda_runtime() || !torch::cuda::is_available()) {
            std::cerr << "CUDA is required for the native trainer.\n";
            return EXIT_FAILURE;
        }
        at::globalContext().setAllowTF32CuDNN(true);
        at::globalContext().setAllowTF32CuBLAS(true);

        const std::string command = argv[1];
        if (command == "benchmark") {
            bomber::az::benchmark_model(argc, argv, 2);
            return EXIT_SUCCESS;
        }
        if (command == "train" || command == "evaluate" || command == "gates") {
            auto config = bomber::az::parse_train_config(argc, argv, 2);
            /* gates is read-only like evaluate: same checkpoint-loading/manifest-inheritance
               path, no run-dir lock (see the ProcessLock comment in trainer.cpp's Impl
               constructor). */
            config.evaluation_only = command == "evaluate" || command == "gates";
            bomber::az::Trainer trainer(std::move(config));
            if (command == "train") trainer.run();
            else if (command == "evaluate") trainer.evaluate_only();
            else trainer.gates();
            return EXIT_SUCCESS;
        }
        std::cerr << "Unknown command: " << command << "\n\n";
        bomber::az::print_native_help();
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
