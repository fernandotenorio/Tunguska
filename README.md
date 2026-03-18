# Tunguska Chess Engine
Tunguska is a UCI-compliant chess engine written in C++. It uses a bitboard representation and a Neural Network Updated Efficiently (NNUE) architecture for evaluation.

## Core Features
-   **Board Representation**: Bitboards & Magic Bitboards
-   **Search**: Iterative Deepening PVS
-   **Memory**: Zobrist Hashing & Transposition Table
-   **Evaluation**: NNUE
-   **Pruning**: Null Move, LMR, Futility, SEE
-   **Move Ordering**: MVV-LVA, Killers, History Heuristic
-   **Concurrency**: Lazy SMP with Lock-Free TT

## Evaluation (NNUE)
Tunguska's positional evaluation is handled by an NNUE network. The network weights were trained on a dataset of approximately 600 million FEN positions.

-   **Architecture**: `(768 -> 128) x 2 -> 1`
    -   Input: A simple `piece-square-color` representation of size 6 x 64 x 2 = 768.
    -   Hidden Layer: A single layer with 128 neurons and Clipped ReLU activation, with separate accumulators for each player's perspective.
    -   Output: A single neuron producing the evaluation score.
-   **Embedded Weights**: The quantized neural network weights are embedded directly into the source code. The engine does not require an external `.nnue` file to run.

## Concurrency (SMP)
Tunguska can utilize multiple CPU cores to accelerate its search.

-   **Lazy SMP**: The engine spawns worker threads that search the same position. Root move ordering is slightly randomized for each thread to diversify the search.
-   **Lock-Free Transposition Table**: The transposition table uses `std::atomic` variables, allowing multiple threads to access it concurrently without locking overhead.
-   **Threads**: The number of threads is configurable via the UCI "Threads" option, with a **default of 1 thread**.

## Building and Running
You will need a C++ compiler (g++, Clang, or MSVC) that supports C++17.

**Important Compiler Flags:** For optimal performance, it is crucial to compile with high optimization levels (e.g., `-O3` or `/O2` on Windows) and enable AVX2 instruction set support (e.g., `-mavx2`).

## Special Thanks
Special thanks go to the **Chess Programming Wiki** (`https://www.chessprogramming.org/`), which is an invaluable and indispensable resource for developers.

## License
This project is licensed under the MIT License. See the `LICENSE.md` file for details.