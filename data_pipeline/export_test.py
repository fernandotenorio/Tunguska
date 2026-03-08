import struct
import os
from glob import glob
import numpy as np
# ==============================================================================
# 1. PIECE MAPPINGS
# ==============================================================================
# White: P=0, N=1, B=2, R=3, Q=4, K=5
# Black: p=6, n=7, b=8, r=9, q=10, k=11
PIECE_MAP = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 6, 'n': 7, 'b': 8, 'r': 9, 'q': 10, 'k': 11
}
REVERSE_PIECE_MAP = {v: k for k, v in PIECE_MAP.items()}

# ==============================================================================
# 2. PACKING LOGIC (FEN -> Binary)
# ==============================================================================
def fen_to_board(fen):
    """Parses a FEN string into a 64-element array and Side To Move (STM)."""
    board = [None] * 64
    parts = fen.split()
    ranks = parts[0].split('/')
    
    # FEN starts at Rank 8 (index 7) down to Rank 1 (index 0)
    for r_idx, rank_str in enumerate(ranks):
        rank = 7 - r_idx
        file = 0
        for char in rank_str:
            if char.isdigit():
                file += int(char)
            else:
                sq = rank * 8 + file
                board[sq] = PIECE_MAP[char]
                file += 1
                
    stm = 0 if parts[1] == 'w' else 1
    return board, stm

def pack_position(fen, score, result_float):
    """Packs a position into the exact 32-byte BulletFormat."""
    board, stm = fen_to_board(fen)
    
    occupancy = 0
    pieces = bytearray(16) # 16 bytes initialized to 0
    
    piece_idx = 0
    for sq in range(64):
        if board[sq] is not None:
            # 1. Set the bit in the occupancy board
            occupancy |= (1 << sq)
            p_type = board[sq]
            
            # 2. Pack the piece type (0-11) into a 4-bit nibble
            byte_pos = piece_idx // 2
            if piece_idx % 2 == 0:
                # First piece goes into the lower 4 bits
                pieces[byte_pos] |= (p_type & 0x0F)
            else:
                # Second piece goes into the upper 4 bits
                pieces[byte_pos] |= ((p_type & 0x0F) << 4)
            
            piece_idx += 1
            
    # Convert result float (1.0, 0.5, 0.0) to uint8 (2, 1, 0)
    if result_float == 1.0: res_val = 2
    elif result_float == 0.5: res_val = 1
    else: res_val = 0
    
    # Pack to exactly 32 bytes using little-endian '<'
    # Q = uint64 (8)
    # 16s = 16 bytes (16)
    # h = int16 (2)
    # B = uint8 (1) x 2
    # H = uint16 (2) x 2
    packed = struct.pack('<Q 16s h B B H H', occupancy, pieces, int(score), res_val, stm, 0, 0)
    return packed

# ==============================================================================
# 3. UNPACKING LOGIC (Binary -> Board)
# ==============================================================================
def unpack_position(packed_data):
    """Unpacks a 32-byte binary chunk back into a board state and metadata."""
    unpacked = struct.unpack('<Q 16s h B B H H', packed_data)
    occupancy = unpacked[0]
    pieces = unpacked[1]
    score = unpacked[2]
    res_val = unpacked[3]
    stm = unpacked[4]
    
    board = [None] * 64
    piece_idx = 0
    
    # Reconstruct the board by scanning the occupancy bitboard
    for sq in range(64):
        if (occupancy & (1 << sq)) != 0:
            byte_pos = piece_idx // 2
            byte_val = pieces[byte_pos]
            
            # Extract the correct 4-bit nibble
            if piece_idx % 2 == 0:
                p_type = byte_val & 0x0F
            else:
                p_type = (byte_val >> 4) & 0x0F
                
            board[sq] = REVERSE_PIECE_MAP[p_type]
            piece_idx += 1
            
    return board, score, res_val, stm

def print_board(board):
    """Helper function to print the 64-element array as a chess board."""
    print("  +------------------------+")
    for rank in range(7, -1, -1):
        row_str = f"{rank + 1} |"
        for file in range(8):
            sq = rank * 8 + file
            if board[sq] is None:
                row_str += " . "
            else:
                row_str += f" {board[sq]} "
        row_str += "|"
        print(row_str)
    print("  +------------------------+")
    print("     A  B  C  D  E  F  G  H\n")

# ==============================================================================
# 4. FILE CONVERTER
# ==============================================================================
def convert_dataset(input_folder, output_bin):
    """Reads 'fen | score | result' from txt and writes to a .bin file."""
    
    epd_files = glob(os.path.join(input_folder, "*.epd"))

    count = 0
    with open(output_bin, 'wb') as outfile:
        for fl in epd_files:
            epd_lines = open(fl).readlines()
            for line in epd_lines:
                line = line.strip()
                if not line: continue
                
                parts = line.split('|')
                if len(parts) < 3: continue
                
                fen = parts[0].strip()
                score = int(parts[1].strip())
                result = float(parts[2].strip())
                
                packed_bytes = pack_position(fen, score, result)
                outfile.write(packed_bytes)
                count += 1
                
                if count % 100000 == 0:
                    print(f"Converted {count} positions...")
                
    print(f"Done! Saved {count} positions to {output_bin}")

# ==============================================================================
# converts the plain format given by stockfish convert tool
# ==============================================================================
def convert_binpack_plain(input_txt, output_bin):
    """
    Reads a plain text representation of a Stockfish binpack.
    Converts relative scores/results into absolute (White's perspective) 
    and writes to our custom 32-byte .bin format.
    """
    count = 0
    skipped = 0
    
    current_fen = None
    current_score = None
    current_result = None
    
    print(f"Starting conversion of {input_txt}...")
    
    with open(input_txt, 'r') as infile, open(output_bin, 'wb') as outfile:
        for line in infile:
            line = line.strip()
            if not line: continue
            
            if line.startswith('fen '):
                current_fen = line[4:].strip()
            elif line.startswith('score '):
                current_score = int(line[6:].strip())
            elif line.startswith('result '):
                current_result = int(line[7:].strip())
            elif line == 'e':
                # Reached the end of a block, process the position
                if current_fen and current_score is not None and current_result is not None:
                    
                    # 32002 is Stockfish's internal "VALUE_NONE" (unscored position). 
                    # We must skip these as they don't have a valid evaluation.
                    if current_score == 32002 or current_score == -32002:
                        skipped += 1
                    else:
                        # Figure out who is to move
                        parts = current_fen.split()
                        stm = parts[1]
                        is_white = (stm == 'w')
                        
                        # =========================================================
                        # 1. SCORE CONVERSION (Relative -> Absolute)
                        # Binpack score is relative to STM. We need White's perspective.
                        # =========================================================
                        abs_score = current_score if is_white else -current_score
                        
                        # =========================================================
                        # 2. RESULT CONVERSION (Relative -> Absolute Float)
                        # Relative: 1 (Win for STM), 0 (Draw), -1 (Loss for STM)
                        # Absolute: 1.0 (White Win), 0.5 (Draw), 0.0 (Black Win)
                        # =========================================================
                        if current_result == 1:     # Side to move Won
                            abs_result = 1.0 if is_white else 0.0
                        elif current_result == -1:  # Side to move Lost
                            abs_result = 0.0 if is_white else 1.0
                        else:                       # Draw (0)
                            abs_result = 0.5
                            
                        # Pack it into our 32-byte binary format
                        packed_bytes = pack_position(current_fen, abs_score, abs_result)
                        outfile.write(packed_bytes)
                        count += 1
                        
                        if count % 250000 == 0:
                            print(f"Converted {count} positions... (Skipped {skipped} unscored)")
                            
                # Reset for the next record
                current_fen = None
                current_score = None
                current_result = None

    print("-" * 50)
    print("CONVERSION COMPLETE")
    print("-" * 50)
    print(f"Valid positions saved: {count:,}")
    print(f"Skipped (Score=32002): {skipped:,}")
    print(f"Saved to: {output_bin}\n")

# ==============================================================================
# 5. TEST SUITE
# ==============================================================================
def test_packing_logic():
    print("=== RUNNING PACK/UNPACK TEST ===\n")
    
    # 1. Setup Test Data
    test_fen = "r1bq1rk1/ppp1bppp/2np1n2/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7"
    test_score = 45      # +0.45 centipawns
    test_result = 0.5    # Draw
    
    print("1. ORIGINAL DATA:")
    print(f"FEN:    {test_fen}")
    print(f"Score:  {test_score}")
    print(f"Result: {test_result}\n")
    
    # 2. Pack to Binary
    packed_bytes = pack_position(test_fen, test_score, test_result)
    print(f"2. PACKED TO BINARY: {len(packed_bytes)} bytes")
    print(f"Raw hex: {packed_bytes.hex()}\n")
    
    # 3. Unpack from Binary
    unpacked_board, unpacked_score, unpacked_res, unpacked_stm = unpack_position(packed_bytes)
    
    # 4. Output Results
    print("3. UNPACKED BOARD:")
    print_board(unpacked_board)
    
    res_str = "1.0 (White Win)" if unpacked_res == 2 else "0.5 (Draw)" if unpacked_res == 1 else "0.0 (Black Win)"
    stm_str = "White" if unpacked_stm == 0 else "Black"
    
    print("4. UNPACKED METADATA:")
    print(f"Score:         {unpacked_score}")
    print(f"Result:        {res_str}")
    print(f"Side to move:  {stm_str}\n")
    
    # 5. Assertions
    assert unpacked_score == test_score, "Score decoding failed!"
    assert unpacked_res == 1, "Result decoding failed!"
    assert unpacked_stm == 0, "STM decoding failed!"
    print("SUCCESS: Packing and unpacking logic is 100% mathematically flawless! ✅")



def mix_and_split_bins(input_bins, output_prefix, num_chunks=2):
    """
    Takes a list of .bin files, mixes them globally, and splits them 
    into `num_chunks` equal-sized .bin chunks to fit inside RAM constraints.
    """
    bullet_dtype = np.dtype([
        ('occupancy', np.uint64),
        ('pieces', (np.uint8, 16)),
        ('score', np.int16),
        ('result', np.uint8),
        ('stm', np.uint8),
        ('pad1', np.uint16),
        ('pad2', np.uint16)
    ])
    
    # 1. Open temporary files for out-of-core distribution
    temp_files = [open(f"{output_prefix}_temp_{i}.bin", 'wb') for i in range(num_chunks)]
    
    for in_file in input_bins:
        print(f"Distributing records from {in_file}...")
        # Open source file in read-only memmap
        mmap_data = np.memmap(in_file, dtype=bullet_dtype, mode='r')
        
        # Read in 5 Million record blocks (~160MB at a time)
        chunk_size = 5_000_000 
        for offset in range(0, len(mmap_data), chunk_size):
            block = mmap_data[offset:offset+chunk_size]
            
            # Generate random destination chunks for each record in the block
            dests = np.random.randint(0, num_chunks, size=len(block))
            
            # Vectorized write to temp files
            for i in range(num_chunks):
                mask = (dests == i)
                if np.any(mask):
                    temp_files[i].write(block[mask].tobytes())
                    
    for f in temp_files:
        f.close()
        
    print("\nDistribution done! Now performing perfect in-memory shuffles for each chunk...")
    
    # 2. Perfect shuffle each chunk individually
    for i in range(num_chunks):
        temp_name = f"{output_prefix}_temp_{i}.bin"
        final_name = f"{output_prefix}_chunk_{i}.bin"
        print(f"Shuffling and saving {final_name}...")
        
        # This will load exactly 1/num_chunks of the data into RAM
        chunk_data = np.fromfile(temp_name, dtype=bullet_dtype)
        np.random.shuffle(chunk_data)
        chunk_data.tofile(final_name)
        
        # Clean up temp file
        os.remove(temp_name)
        
    print("\nAll datasets mixed, shuffled, and split successfully!")


if __name__ == "__main__":
  
    binpacks = [
        # ("../data/binpacks/test80-2024-02-feb-2tb7p.min-v2.v6.plain", "../data/train/test80-2024-02-feb-2tb7p.min-v2.v6.bin"),
        # ("../data/binpacks/test80-2024-03-mar-2tb7p.min-v2.v6.plain", "../data/train/test80-2024-03-mar-2tb7p.min-v2.v6.bin"),
        # ("../data/binpacks/test80-2024-04-apr-2tb7p.min-v2.v6.plain", "../data/train/test80-2024-04-apr-2tb7p.min-v2.v6.bin"),
        # ("../data/binpacks/test80-2024-05-may-2tb7p.min-v2.v6.plain", "../data/train/test80-2024-05-may-2tb7p.min-v2.v6.bin"),
        # ("../data/binpacks/test80-2024-06-jun-2tb7p.min-v2.v6.plain", "../data/train/test80-2024-06-jun-2tb7p.min-v2.v6.bin")
    ]
    # for p, b in binpacks:
    #     convert_binpack_plain(p, b)

    mix_and_split_bins(
        [
            "../data/train/test80-2024-02-feb-2tb7p.min-v2.v6.bin",
            "../data/train/test80-2024-03-mar-2tb7p.min-v2.v6.bin",
            "../data/train/test80-2024-04-apr-2tb7p.min-v2.v6.bin",
            "../data/train/test80-2024-05-may-2tb7p.min-v2.v6.bin",
            "../data/train/test80-2024-06-jun-2tb7p.min-v2.v6.bin"
        ],
        "../data/train/mixed_train",
        4
    )