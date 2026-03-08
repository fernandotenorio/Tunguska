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
# 1. LOAD PRE-COMPILED C++ EXTENSION
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
# 2. THE PERSPECTIVE NNUE ARCHITECTURE
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
# 3. BLENDED LOSS FUNCTION
# ==============================================================================
def blended_loss(outputs, scores, results, blend_ratio=0.7):
    score_wdl = torch.sigmoid(scores / 400.0)
    target = blend_ratio * score_wdl + (1.0 - blend_ratio) * results
    net_wdl = torch.sigmoid(outputs)
    loss = nn.MSELoss()(net_wdl, target)
    return loss


# ==============================================================================
# 4. TRAINING LOOP
# ==============================================================================
def train(chunk_paths, checkpoint_folder, epochs=10, batch_size=8192, lr=1e-3, resume_from=None):
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

        for chunk_idx, chunk_path in enumerate(chunk_paths):
            print(f"--- Epoch {epoch+1} | Loading Chunk {chunk_idx+1}/{len(chunk_paths)}: {os.path.basename(chunk_path)} ---")
            
            # Initialize C++ Fast Loader
            loader = FastNNUELoader(chunk_path)
            model.train()
            chunk_loss = 0.0
            batch_idx = 0
            
            # Note: We don't need shuffle=True here because our `mix_and_split_bins` 
            # script already randomized the records on disk!
            while not loader.is_eof():
                # 1. Fetch batch from C++
                w_indices, b_indices, stm, scores, results = loader.next_batch(batch_size)
                
                # C++ returns CPU tensors; move to GPU
                w_indices = w_indices.to(device).long()
                b_indices = b_indices.to(device).long()
                stm = stm.to(device)
                scores = scores.to(device)
                results = results.to(device)

                # 2. Convert sparse indices to dense [batch_size, 768] arrays directly on the GPU
                bsz = w_indices.size(0)
                
                # We allocate 769 so index 768 (our empty padding value from C++) has somewhere to go
                w_feat = torch.zeros(bsz, 769, device=device)
                b_feat = torch.zeros(bsz, 769, device=device)
                
                # Instantly scatter 1.0s into the correct feature indices
                w_feat.scatter_(1, w_indices, 1.0)
                b_feat.scatter_(1, b_indices, 1.0)
                
                # Slice off the padding column so we pass exactly 768 to the model
                w_feat = w_feat[:, :768]
                b_feat = b_feat[:, :768]

                # 3. Standard Forward/Backward pass
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
                    print(f"Epoch {epoch+1} | Chunk {chunk_idx+1} | Batch {batch_idx} | Loss: {loss.item():.5f}")
            
            # To avoid division by zero on tiny files
            if batch_idx > 0:
                print(f"--- Chunk {chunk_idx+1} Complete | Average Loss: {chunk_loss / batch_idx:.5f} ---")
            
            # Sub-Epoch Cleanup & Checkpoint
            del loader
            gc.collect() 
            
            ckpt_path = os.path.join(checkpoint_folder, f"nnue_epoch_{epoch+1}_chunk_{chunk_idx+1}.pt")
            torch.save({
                'epoch': epoch, 
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'scheduler_state_dict': scheduler.state_dict(),
            }, ckpt_path)
            
        avg_loss = total_epoch_loss / max(1, batches_in_epoch)
        print(f"\n======== Epoch {epoch+1} Complete | Total Avg Loss: {avg_loss:.5f} ========")
        
        # Step the LR scheduler
        scheduler.step()
        
        # Full Epoch Checkpoint
        full_ckpt_path = os.path.join(checkpoint_folder, f"nnue_epoch_{epoch+1}_COMPLETE.pt")
        torch.save({
            'epoch': epoch + 1,
            'model_state_dict': model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'scheduler_state_dict': scheduler.state_dict(),
            'loss': avg_loss,
        }, full_ckpt_path)


# ==============================================================================
# 5. C++ ENGINE EXPORT LOGIC
# ==============================================================================
def save_npz(checkpoint_path, outfile):
    print(f"Loading checkpoint {checkpoint_path}...")
    checkpoint_dict = torch.load(checkpoint_path, map_location=torch.device('cpu'))
    state_dict = checkpoint_dict['model_state_dict']
    weights = {}
    
    weights['accumulator.weight'] = state_dict['ft.weight'].numpy()
    weights['accumulator.bias'] = state_dict['ft.bias'].numpy()
    weights['output_weights'] = state_dict['out.weight'].numpy().T
    weights['output_bias'] = state_dict['out.bias'].numpy()
    
    np.savez(f"{outfile}.npz", **weights)
    print(f"Successfully saved weights for C++ to {outfile}.npz")


def gen_all_weights():
    nets = glob.glob("../checkpoints/*.pt")
    for net in nets:
        epoch = net.split("_")[-1].replace(".pt", "")
        save_npz(net, f"net_{epoch}")


# ==============================================================================
# 6. EVALUATION
# ==============================================================================
def evaluate_checkpoint(bin_path, checkpoint_path, batch_size=8192, blend_ratio=0.7):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"\nEvaluating Checkpoint: {checkpoint_path} on {device}")

    loader = FastNNUELoader(bin_path)
    model = PerspectiveNNUE().to(device)
    
    if not os.path.exists(checkpoint_path):
        print(f"Error: Checkpoint {checkpoint_path} not found!")
        return

    checkpoint = torch.load(checkpoint_path, map_location=device)
    model.load_state_dict(checkpoint['model_state_dict'])
    model.eval()

    total_wdl_mse = 0.0
    total_cp_mse = 0.0
    total_positions = 0
    batch_idx = 0

    with torch.no_grad():
        while not loader.is_eof():
            w_indices, b_indices, stm, scores, results = loader.next_batch(batch_size)
            
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

            net_wdl = torch.sigmoid(outputs)
            score_wdl = torch.sigmoid(scores / 400.0)
            target_wdl = blend_ratio * score_wdl + (1.0 - blend_ratio) * results
            
            wdl_se = torch.sum((net_wdl - target_wdl) ** 2).item()
            total_wdl_mse += wdl_se

            net_cp = outputs * 400.0
            cp_se = torch.sum((net_cp - scores) ** 2).item()
            total_cp_mse += cp_se

            total_positions += bsz
            batch_idx += 1

            if batch_idx % 100 == 0:
                print(f"Evaluated {total_positions:,} positions...")

    final_wdl_mse = total_wdl_mse / total_positions
    final_cp_rmse = math.sqrt(total_cp_mse / total_positions)

    print("\n" + "="*40)
    print("EVALUATION RESULTS")
    print("="*40)
    print(f"Total Positions: {total_positions:,}")
    print(f"WDL Target MSE:  {final_wdl_mse:.5f}")
    print(f"Centipawn RMSE:  {final_cp_rmse:.2f} (cp margin of error)")
    print("="*40 + "\n")


if __name__ == "__main__":
    chunk_files = glob.glob("../data/train/mixed_train_chunk_*.bin")
    checkpoint_folder = "../checkpoints"

    if len(chunk_files) > 0:
        # Tweak batch size if desired. 16384 or 32768 run blazingly fast with the C++ loader.
        train(chunk_files, checkpoint_folder, epochs=10, batch_size=16384, lr=1e-3)
    else:
        print(f"No chunk files found!")