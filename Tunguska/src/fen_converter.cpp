#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstdint>
#include <array>
#include <stdexcept>

// ==============================================================================
// 1. CONSTANTS AND DATA STRUCTURES
// ==============================================================================

// White: P=0, N=1, B=2, R=3, Q=4, K=5
// Black: p=6, n=7, b=8, r=9, q=10, k=11
const std::map<char, uint8_t> PIECE_MAP = {
    {'P', 0}, {'N', 1}, {'B', 2}, {'R', 3}, {'Q', 4}, {'K', 5},
    {'p', 6}, {'n', 7}, {'b', 8}, {'r', 9}, {'q', 10}, {'k', 11}
};

// This struct defines the exact 32-byte binary format.
// #pragma pack(push, 1) ensures that the compiler does not add any padding
// between the members, making the memory layout match the Python spec exactly.
#pragma pack(push, 1)
struct BulletFormat {
    uint64_t occupancy;
    std::array<uint8_t, 16> pieces;
    int16_t score;
    uint8_t result;
    uint8_t stm;
    uint16_t pad1;
    uint16_t pad2;
};
#pragma pack(pop)

// A compile-time check to ensure our struct is the correct size.
static_assert(sizeof(BulletFormat) == 32, "BulletFormat struct must be 32 bytes");

const int STOCKFISH_VALUE_NONE = 32002;

// ==============================================================================
// 2. PACKING LOGIC (FEN -> Binary)
// ==============================================================================

// Packs a FEN string, score, and result into the 32-byte BulletFormat.
BulletFormat pack_position(const std::string& fen, int16_t score, float result_float) {
    BulletFormat packed_data{}; // Initialize to all zeros

    std::array<int, 64> board;
    board.fill(-1); // -1 represents an empty square

    std::stringstream ss(fen);
    std::string fen_board, stm_str;
    ss >> fen_board >> stm_str;

    // 1. Parse the FEN board layout
    int rank = 7;
    int file = 0;
    for (char c : fen_board) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += (c - '0');
        } else {
            int sq = rank * 8 + file;
            board[sq] = PIECE_MAP.at(c);
            file++;
        }
    }

    // 2. Set Side To Move (STM)
    packed_data.stm = (stm_str == "w") ? 0 : 1;

    // 3. Populate occupancy bitboard and pieces array
    int piece_idx = 0;
    for (int sq = 0; sq < 64; ++sq) {
        if (board[sq] != -1) {
            // Set the bit in the occupancy board
            packed_data.occupancy |= (1ULL << sq);
            
            uint8_t p_type = board[sq];
            
            // Pack the piece type (0-11) into a 4-bit nibble
            int byte_pos = piece_idx / 2;
            if (piece_idx % 2 == 0) {
                // First piece goes into the lower 4 bits
                packed_data.pieces[byte_pos] |= (p_type & 0x0F);
            } else {
                // Second piece goes into the upper 4 bits
                packed_data.pieces[byte_pos] |= ((p_type & 0x0F) << 4);
            }
            piece_idx++;
        }
    }

    // 4. Set score
    packed_data.score = score;

    // 5. Convert result float (1.0, 0.5, 0.0) to uint8 (2, 1, 0)
    if (result_float == 1.0) packed_data.result = 2;
    else if (result_float == 0.5) packed_data.result = 1;
    else packed_data.result = 0;
    
    // Padding (pad1, pad2) is already zero-initialized.

    return packed_data;
}


// ==============================================================================
// 3. FILE CONVERTERS
// ==============================================================================

// Corresponds to Python's `convert_dataset`
void processEpdFile(const std::string& input_path, std::ofstream& outfile) {
    std::ifstream infile(input_path);
    if (!infile.is_open()) {
        throw std::runtime_error("Could not open input file: " + input_path);
    }

    std::cout << "Processing EPD file: " << input_path << std::endl;

    std::string line;
    uint64_t count = 0;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        std::string fen, score_str, result_str;
        size_t first_pipe = line.find('|');
        size_t second_pipe = line.find('|', first_pipe + 1);

        if (first_pipe == std::string::npos || second_pipe == std::string::npos) continue;

        fen = line.substr(0, first_pipe);
        score_str = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
        result_str = line.substr(second_pipe + 1);

        // Trim whitespace (simple version)
        fen.erase(fen.find_last_not_of(" \t") + 1);
        score_str.erase(0, score_str.find_first_not_of(" \t"));
        score_str.erase(score_str.find_last_not_of(" \t") + 1);
        result_str.erase(0, result_str.find_first_not_of(" \t"));

        try {
            int16_t score = std::stoi(score_str);
            float result = std::stof(result_str);
            
            BulletFormat packed_bytes = pack_position(fen, score, result);
            outfile.write(reinterpret_cast<const char*>(&packed_bytes), sizeof(BulletFormat));
            count++;

            if (count % 250000 == 0) {
                std::cout << "Converted " << count << " positions..." << std::endl;
            }
        } catch (const std::exception& e) {
            // Ignore lines that fail to parse
        }
    }
    std::cout << "Finished! Saved " << count << " positions." << std::endl;
}


// Corresponds to Python's `convert_binpack_plain`
void processStockfishPlainFile(const std::string& input_path, std::ofstream& outfile) {
    std::ifstream infile(input_path);
    if (!infile.is_open()) {
        throw std::runtime_error("Could not open input file: " + input_path);
    }

    std::cout << "Processing Stockfish plain text file: " << input_path << std::endl;

    std::string line;
    uint64_t count = 0;
    uint64_t skipped = 0;

    std::string current_fen;
    int current_score = 0;
    int current_result = 0;
    bool has_fen = false, has_score = false, has_result = false;

    while (std::getline(infile, line)) {
        if (line.empty()) continue;

        if (line.rfind("fen ", 0) == 0) {
            current_fen = line.substr(4);
            has_fen = true;
        } else if (line.rfind("score ", 0) == 0) {
            current_score = std::stoi(line.substr(6));
            has_score = true;
        } else if (line.rfind("result ", 0) == 0) {
            current_result = std::stoi(line.substr(7));
            has_result = true;
        } else if (line == "e") {
            if (has_fen && has_score && has_result) {
                // Skip unscored positions
                if (current_score == STOCKFISH_VALUE_NONE || current_score == -STOCKFISH_VALUE_NONE) {
                    skipped++;
                } else {
                    // Figure out who is to move
                    std::stringstream ss(current_fen);
                    std::string board_part, stm_part;
                    ss >> board_part >> stm_part;
                    bool is_white_to_move = (stm_part == "w");

                    // 1. SCORE CONVERSION (Relative -> Absolute)
                    int16_t abs_score = is_white_to_move ? current_score : -current_score;

                    // 2. RESULT CONVERSION (Relative -> Absolute Float)
                    float abs_result;
                    if (current_result == 1) { // STM Won
                        abs_result = is_white_to_move ? 1.0f : 0.0f;
                    } else if (current_result == -1) { // STM Lost
                        abs_result = is_white_to_move ? 0.0f : 1.0f;
                    } else { // Draw
                        abs_result = 0.5f;
                    }

                    BulletFormat packed_bytes = pack_position(current_fen, abs_score, abs_result);
                    outfile.write(reinterpret_cast<const char*>(&packed_bytes), sizeof(BulletFormat));
                    count++;

                    if (count % 250000 == 0) {
                        std::cout << "Converted " << count << " positions... (Skipped " << skipped << " unscored)" << std::endl;
                    }
                }
            }
            // Reset for the next record
            has_fen = has_score = has_result = false;
        }
    }
    
    std::cout << "\n--------------------------------------------------\n";
    std::cout << "CONVERSION COMPLETE\n";
    std::cout << "--------------------------------------------------\n";
    std::cout << "Valid positions saved: " << count << "\n";
    std::cout << "Skipped (unscored):    " << skipped << "\n";
    std::cout << std::flush;
}


// ==============================================================================
// 4. MAIN DRIVER
// ==============================================================================

void print_usage() {
    std::cerr << "Usage:\n"
              << "  ./converter epd <input.epd> <output.bin>\n"
              << "  ./converter plain <input.plain> <output.bin>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage();
        return 1;
    }

    std::string mode = argv[1];
    std::string input_path = argv[2];
    std::string output_path = argv[3];

    // Open the output file in binary mode
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file for writing: " << output_path << std::endl;
        return 1;
    }

    try {
        if (mode == "epd") {
            processEpdFile(input_path, outfile);
        } else if (mode == "plain") {
            processStockfishPlainFile(input_path, outfile);
        } else {
            std::cerr << "Error: Unknown mode '" << mode << "'\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nSuccessfully saved binary data to: " << output_path << std::endl;

    return 0;
}