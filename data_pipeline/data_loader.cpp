#include <torch/extension.h>
#include <fstream>
#include <vector>
#include <string>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward64)
#endif

inline int numberOfTrailingZeros(uint64_t bb) {
#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, bb);
    return index;
#else
    return __builtin_ctzll(bb);
#endif
}

// Match the exact 32-byte layout of your Python script
#pragma pack(push, 1)
struct BulletRecord {
    uint64_t occupancy;
    uint8_t pieces[16];
    int16_t score;
    uint8_t result;
    uint8_t stm;
    uint16_t pad1;
    uint16_t pad2;
};
#pragma pack(pop)

class FastNNUELoader {
private:
    std::ifstream file;
    size_t file_size;

public:
    FastNNUELoader(const std::string& path) {
        file.open(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) throw std::runtime_error("Cannot open file: " + path);
        file_size = file.tellg();
        file.seekg(0, std::ios::beg);
    }

    // Returns true if EOF is reached
    bool is_eof() {
        return file.peek() == EOF;
    }

    // Reads a batch and returns a tuple of Tensors
    std::vector<torch::Tensor> next_batch(int batch_size) {
        // Allocate zero-filled tensors. 32 is the max pieces on a chess board.
        // We initialize with 768, which will act as our "padding" index for empty squares.
        auto w_indices = torch::full({batch_size, 32}, 768, torch::kInt32);
        auto b_indices = torch::full({batch_size, 32}, 768, torch::kInt32);
        auto stm_t = torch::zeros({batch_size, 1}, torch::kFloat32);
        auto score_t = torch::zeros({batch_size, 1}, torch::kFloat32);
        auto result_t = torch::zeros({batch_size, 1}, torch::kFloat32);

        auto w_acc = w_indices.accessor<int32_t, 2>();
        auto b_acc = b_indices.accessor<int32_t, 2>();
        auto stm_acc = stm_t.accessor<float, 2>();
        auto score_acc = score_t.accessor<float, 2>();
        auto result_acc = result_t.accessor<float, 2>();

        std::vector<BulletRecord> buffer(batch_size);
        file.read(reinterpret_cast<char*>(buffer.data()), batch_size * sizeof(BulletRecord));
        int actual_read = file.gcount() / sizeof(BulletRecord);

        for (int i = 0; i < actual_read; i++) {
            const auto& row = buffer[i];

            // 1. Target and STM Extraction (with perspective flipping)
            float score = static_cast<float>(row.score);
            float result = (row.result == 2) ? 1.0f : ((row.result == 1) ? 0.5f : 0.0f);
            float stm = static_cast<float>(row.stm);

            if (stm == 1.0f) { // If Black to move, flip to Black's perspective
                score = -score;
                result = 1.0f - result;
            }

            stm_acc[i][0] = stm;
            score_acc[i][0] = score;
            result_acc[i][0] = result;

            // 2. Ultra-fast Bitboard Extraction
            uint64_t occ = row.occupancy;
            int piece_idx = 0;
            int list_idx = 0; // Number of pieces found so far on this board

            // __builtin_ctzll finds the lowest set bit instantly. 
            // occ &= occ - 1 clears that bit. 
            // This loops EXACTLY as many times as there are pieces (e.g., 20 pieces = 20 loops, not 64).
            while (occ) {
                int sq = numberOfTrailingZeros(occ);
                
                uint8_t byte_val = row.pieces[piece_idx / 2];
                int p_type = (piece_idx % 2 == 0) ? (byte_val & 0x0F) : (byte_val >> 4);

                // White perspective
                int w_idx = p_type * 64 + sq;
                w_acc[i][list_idx] = w_idx;

                // Black perspective (flip ranks + flip colors)
                int b_p_type = (p_type + 6) % 12;
                int b_sq = sq ^ 56;
                int b_idx = b_p_type * 64 + b_sq;
                b_acc[i][list_idx] = b_idx;

                list_idx++;
                piece_idx++;
                occ &= occ - 1; 
            }
        }

        // If we hit the end of the file and didn't get a full batch, slice the tensors
        if (actual_read < batch_size) {
            w_indices = w_indices.slice(0, 0, actual_read);
            b_indices = b_indices.slice(0, 0, actual_read);
            stm_t = stm_t.slice(0, 0, actual_read);
            score_t = score_t.slice(0, 0, actual_read);
            result_t = result_t.slice(0, 0, actual_read);
        }

        return {w_indices, b_indices, stm_t, score_t, result_t};
    }
};

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    pybind11::class_<FastNNUELoader>(m, "FastNNUELoader")
        .def(pybind11::init<const std::string&>())
        .def("next_batch", &FastNNUELoader::next_batch)
        .def("is_eof", &FastNNUELoader::is_eof);
}