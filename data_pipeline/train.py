import os
import gc
import glob
import math
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.optim.lr_scheduler import CosineAnnealingLR
from torch.utils.cpp_extension import load


# ==============================================================================
# LOAD PRE-COMPILED C++ EXTENSION
# ==============================================================================
try:
    # imports pre-compiled .pyd (Windows) or .so (Linux) file
    import nnue_loader
    FastNNUELoader = nnue_loader.FastNNUELoader
    print("Successfully imported pre-compiled C++ FastNNUELoader!\n")
except ImportError as e:
    print("ERROR: Failed to import fast_loader_ext.")
    print("Ensure that your compiled 'fast_loader_ext.pyd' or 'fast_loader_ext.so' "
          "is in the exact same directory as this script.")
    raise e

# ==============================================================================
# THE PERSPECTIVE NNUE ARCHITECTURE
# ==============================================================================
class PerspectiveNNUE(nn.Module):
    def __init__(self):
        super(PerspectiveNNUE, self).__init__()
        self.HL = 128
        self.ft = nn.Linear(768, self.HL)
        self.out = nn.Linear(2 * self.HL, 1)

    def forward(self, w_features, b_features, stm):
        w_acc = self.ft(w_features)
        w_acc = torch.clamp(w_acc, 0.0, 1.0) 
        
        b_acc = self.ft(b_features)
        b_acc = torch.clamp(b_acc, 0.0, 1.0) 

        combined = torch.zeros(w_acc.shape[0], 2 * self.HL, device=w_acc.device)
        
        white_to_move = (stm == 0.0).squeeze(-1)
        black_to_move = ~white_to_move

        if white_to_move.any():
            combined[white_to_move] = torch.cat([w_acc[white_to_move], b_acc[white_to_move]], dim=1)
            
        if black_to_move.any():
            combined[black_to_move] = torch.cat([b_acc[black_to_move], w_acc[black_to_move]], dim=1)

        evaluation = self.out(combined)
        return evaluation


# ==============================================================================
# BLENDED LOSS FUNCTION
# ==============================================================================
def blended_loss(outputs, scores, results, blend_ratio=0.7):
    score_wdl = torch.sigmoid(scores / 400.0)
    target = blend_ratio * score_wdl + (1.0 - blend_ratio) * results
    net_wdl = torch.sigmoid(outputs)
    loss = nn.MSELoss()(net_wdl, target)
    return loss

# ==============================================================================
# VALIDATION LOGIC
# ==============================================================================
def run_validation(model, val_path, batch_size, device, blend_ratio=0.7):
    """Evaluates the current model on the validation dataset."""
    if not os.path.exists(val_path):
        print(f"WARNING: Validation file '{val_path}' not found. Skipping validation.")
        return 0.0, 0.0

    model.eval() # Disable dropout, gradients, etc.
    val_loader = FastNNUELoader(val_path)
    
    val_loss = 0.0
    val_cp_mse = 0.0
    total_val_positions = 0
    val_batches = 0
    
    with torch.no_grad(): # Saves memory and accelerates computation
        while not val_loader.is_eof():
            w_indices, b_indices, stm, scores, results = val_loader.next_batch(batch_size)
            if w_indices.size(0) == 0:
                break
                
            w_indices = w_indices.to(device).long()
            b_indices = b_indices.to(device).long()
            stm, scores, results = stm.to(device), scores.to(device), results.to(device)

            bsz = w_indices.size(0)
            w_feat = torch.zeros(bsz, 769, device=device)
            b_feat = torch.zeros(bsz, 769, device=device)
            
            w_feat.scatter_(1, w_indices, 1.0)
            b_feat.scatter_(1, b_indices, 1.0)
            w_feat, b_feat = w_feat[:, :768], b_feat[:, :768]

            outputs = model(w_feat, b_feat, stm)
            
            # Compute Evaluation Loss
            loss = blended_loss(outputs, scores, results, blend_ratio=blend_ratio)
            val_loss += loss.item()
            
            # Compute Centipawn Error (Engine-intuitive metric)
            net_cp = outputs * 400.0
            cp_se = torch.sum((net_cp - scores) ** 2).item()
            val_cp_mse += cp_se
            
            total_val_positions += bsz
            val_batches += 1

    avg_val_loss = val_loss / max(1, val_batches)
    val_cp_rmse = math.sqrt(val_cp_mse / max(1, total_val_positions))
    
    print(f"Validation Validated {total_val_positions:,} positions.")
    print(f"Validation Loss:     {avg_val_loss:.5f}")
    print(f"Validation CP RMSE:  {val_cp_rmse:.2f} centipawns")
    
    del val_loader
    gc.collect()
    
    return avg_val_loss, val_cp_rmse


# ==============================================================================
# TRAINING LOOP WITH VALIDATION
# ==============================================================================
def train(chunk_paths, val_path, checkpoint_folder, epochs=10, batch_size=16384, lr=1e-3, resume_from=None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Starting Training on: {device}")

    os.makedirs(checkpoint_folder, exist_ok=True)

    model = PerspectiveNNUE().to(device)
    optimizer = optim.Adam(model.parameters(), lr=lr)
    
    # Cosine Annealing Scheduler
    scheduler = CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-5)

    start_epoch = 0

    # --- RESUME FROM CHECKPOINT LOGIC ---
    if resume_from is not None and os.path.exists(resume_from):
        print(f"Loading checkpoint '{resume_from}'...")
        checkpoint = torch.load(resume_from, map_location=device)
        model.load_state_dict(checkpoint['model_state_dict'])
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        start_epoch = checkpoint['epoch']
        
        if 'scheduler_state_dict' in checkpoint:
            scheduler.load_state_dict(checkpoint['scheduler_state_dict'])
        else:
            for _ in range(start_epoch):
                scheduler.step()
                
        print(f"Successfully resumed from Epoch {start_epoch}")
    # ------------------------------------

    for epoch in range(start_epoch, epochs):
        current_lr = scheduler.get_last_lr()[0]
        print(f"\n{'='*50}")
        print(f"STARTING EPOCH {epoch+1}/{epochs} | CURRENT LR: {current_lr:.6f}")
        print(f"{'='*50}\n")
        
        # Randomize order of chunks per epoch
        np.random.shuffle(chunk_paths) 
        total_epoch_loss = 0.0
        batches_in_epoch = 0

        # --- 4A. TRAINING PHASE ---
        for chunk_idx, chunk_path in enumerate(chunk_paths):
            print(f"--- Epoch {epoch+1} | Loading Chunk {chunk_idx+1}/{len(chunk_paths)}: {os.path.basename(chunk_path)} ---")
            
            loader = FastNNUELoader(chunk_path)
            model.train() # Set to train mode
            chunk_loss = 0.0
            batch_idx = 0
            
            while not loader.is_eof():
                w_indices, b_indices, stm, scores, results = loader.next_batch(batch_size)
                if w_indices.size(0) == 0:
                    break
                
                w_indices = w_indices.to(device).long()
                b_indices = b_indices.to(device).long()
                stm, scores, results = stm.to(device), scores.to(device), results.to(device)

                bsz = w_indices.size(0)
                w_feat = torch.zeros(bsz, 769, device=device)
                b_feat = torch.zeros(bsz, 769, device=device)
                
                w_feat.scatter_(1, w_indices, 1.0)
                b_feat.scatter_(1, b_indices, 1.0)
                w_feat, b_feat = w_feat[:, :768], b_feat[:, :768]

                optimizer.zero_grad()
                outputs = model(w_feat, b_feat, stm)
                loss = blended_loss(outputs, scores, results, blend_ratio=0.7) 
                
                loss.backward()
                optimizer.step()

                chunk_loss += loss.item()
                total_epoch_loss += loss.item()
                batches_in_epoch += 1
                batch_idx += 1

                if batch_idx % 250 == 0:
                    print(f"Epoch {epoch+1} | Chunk {chunk_idx+1} | Batch {batch_idx} | Train Loss: {loss.item():.5f}")
            
            if batch_idx > 0:
                print(f"--- Chunk {chunk_idx+1} Complete | Average Train Loss: {chunk_loss / batch_idx:.5f} ---")
            
            del loader
            gc.collect() 
            
            # Sub-Epoch Checkpoint
            # ckpt_path = os.path.join(checkpoint_folder, f"nnue_epoch_{epoch+1}_chunk_{chunk_idx+1}.pt")
            # torch.save({
            #     'epoch': epoch, 
            #     'model_state_dict': model.state_dict(),
            #     'optimizer_state_dict': optimizer.state_dict(),
            #     'scheduler_state_dict': scheduler.state_dict(),
            # }, ckpt_path)

        # --- 4B. VALIDATION PHASE ---
        print(f"\n--- Running Validation for Epoch {epoch+1} ---")
        avg_val_loss, val_cp_rmse = run_validation(model, val_path, batch_size, device, blend_ratio=0.7)

        # --- 4C. EPOCH COMPLETION ---
        avg_train_loss = total_epoch_loss / max(1, batches_in_epoch)
        print(f"\n======== Epoch {epoch+1} Complete ========")
        print(f"Train Loss: {avg_train_loss:.5f} | Val Loss: {avg_val_loss:.5f} | Val CP Error: {val_cp_rmse:.2f}")
        print("========================================")
        
        scheduler.step()
        
        # Save Full Epoch Checkpoint with Validation Metrics Included
        full_ckpt_path = os.path.join(checkpoint_folder, f"nnue_epoch_{epoch+1}_COMPLETE.pt")
        torch.save({
            'epoch': epoch + 1,
            'model_state_dict': model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'scheduler_state_dict': scheduler.state_dict(),
            'train_loss': avg_train_loss,
            'val_loss': avg_val_loss,
            'val_cp_rmse': val_cp_rmse
        }, full_ckpt_path)


# ==============================================================================
# C++ ENGINE EXPORT LOGIC (QUANTIZED)
# ==============================================================================
def save_npz(checkpoint_path, outfile):
    print(f"Loading checkpoint {checkpoint_path}...")
    checkpoint_dict = torch.load(checkpoint_path, map_location=torch.device('cpu'))
    state_dict = checkpoint_dict['model_state_dict']
    
    # Quantization multipliers
    QA = 255.0
    SCALE = 400.0
    
    weights = {}
    
    # 1. Accumulator (Force C-Contiguous Memory Layout!)
    ft_weight = np.ascontiguousarray(state_dict['ft.weight'].numpy().T) * QA
    weights['accumulator.weight'] = np.ascontiguousarray(
        np.clip(np.round(ft_weight), -32768, 32767).astype(np.int16)
    )
    
    ft_bias = np.ascontiguousarray(state_dict['ft.bias'].numpy()) * QA
    weights['accumulator.bias'] = np.ascontiguousarray(
        np.clip(np.round(ft_bias), -32768, 32767).astype(np.int16)
    )
    
    # 2. Output Layer (Force C-Contiguous Memory Layout!)
    out_weight = np.ascontiguousarray(state_dict['out.weight'].numpy().T) * SCALE
    weights['output_weights'] = np.ascontiguousarray(
        np.clip(np.round(out_weight), -32768, 32767).astype(np.int16)
    )
    
    # 3. Output Bias
    out_bias = np.ascontiguousarray(state_dict['out.bias'].numpy()) * QA * SCALE
    weights['output_bias'] = np.ascontiguousarray(
        np.round(out_bias).astype(np.int32)
    )
    
    np.savez(f"{outfile}.npz", **weights)
    print(f"Successfully saved INT QUANTIZED weights to {outfile}.npz")


def save_weights_header(checkpoint_path, outfile):
    print(f"Loading checkpoint {checkpoint_path}...")
    checkpoint_dict = torch.load(checkpoint_path, map_location=torch.device('cpu'))
    state_dict = checkpoint_dict['model_state_dict']
    
    # Quantization multipliers
    QA = 255.0
    SCALE = 400.0
    
    # 1. Accumulator
    ft_weight = np.ascontiguousarray(state_dict['ft.weight'].numpy().T) * QA
    acc_weight = np.clip(np.round(ft_weight), -32768, 32767).astype(np.int16)
    
    ft_bias = np.ascontiguousarray(state_dict['ft.bias'].numpy()) * QA
    acc_bias = np.clip(np.round(ft_bias), -32768, 32767).astype(np.int16)
    
    # 2. Output Layer
    out_weight = np.ascontiguousarray(state_dict['out.weight'].numpy().T) * SCALE
    out_weights = np.clip(np.round(out_weight), -32768, 32767).astype(np.int16)
    
    # 3. Output Bias
    out_b = np.ascontiguousarray(state_dict['out.bias'].numpy()) * QA * SCALE
    out_bias = np.round(out_b).astype(np.int32)

    print("Generating nnue_weights.h (this might take a few seconds)...")
    
    with open(f"{outfile}", "w") as f:
        f.write("// AUTO-GENERATED NNUE WEIGHTS\n")
        f.write("// DO NOT INCLUDE THIS FILE ANYWHERE EXCEPT IN nnue_loader.cpp!\n\n")
        
        # Write Accumulator Weight (2D Array)
        f.write(f"alignas(64) const int16_t NNUENetwork::accumulator_weight[{acc_weight.shape[0]}][{acc_weight.shape[1]}] = {{\n")
        for row in acc_weight:
            f.write("    {" + ", ".join(map(str, row)) + "},\n")
        f.write("};\n\n")
        
        # Write Accumulator Bias (1D Array)
        f.write(f"alignas(64) const int16_t NNUENetwork::accumulator_bias[{acc_bias.shape[0]}] = {{\n")
        f.write("    " + ", ".join(map(str, acc_bias)) + "\n")
        f.write("};\n\n")
        
        # Write Output Weights (1D Array)
        f.write(f"alignas(64) const int16_t NNUENetwork::output_weights[{out_weights.size}] = {{\n")
        f.write("    " + ", ".join(map(str, out_weights.flatten())) + "\n")
        f.write("};\n\n")
        
        # Write Output Bias (Single Value)
        val = out_bias.item() if out_bias.size == 1 else out_bias[0]
        f.write(f"const int32_t NNUENetwork::output_bias = {val};\n")
        
        # We don't need weights_loaded anymore, but we can set it to true just in case
        f.write("bool NNUENetwork::weights_loaded = true;\n")
        
    print(f"Successfully saved INT QUANTIZED weights to {outfile}")


def gen_all_weights():
    nets = glob.glob("../checkpoints/*_COMPLETE.pt")
    for net in nets:
        epoch = net.split("_")[-2]
        save_npz(net, f"net_{epoch}")


if __name__ == "__main__":
    chunk_files = glob.glob("../data/train/mixed_train_chunk_*.bin")
    val_file = chunk_files[0]
    train_files = chunk_files[1:]
    checkpoint_folder = "../checkpoints"

    if len(chunk_files) > 0:
        # Tweak batch size if desired. 16384 or 32768 run blazingly fast with the C++ loader.
        train(train_files, val_file, checkpoint_folder, epochs=10, batch_size=16384, lr=1e-3)
    else:
        print(f"No chunk files found!")