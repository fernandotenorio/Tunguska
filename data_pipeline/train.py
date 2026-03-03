import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
import math

# ==============================================================================
# 1. DATA DEFINITION & DATALOADER
# ==============================================================================

bullet_dtype = np.dtype([
    ('occupancy', np.uint64),
    ('pieces', (np.uint8, 16)),
    ('score', np.int16),
    ('result', np.uint8),
    ('stm', np.uint8),
    ('pad1', np.uint16),
    ('pad2', np.uint16)
])

class PerspectiveNNUEDataset(Dataset):
    def __init__(self, bin_path):
        super().__init__()
        self.data = np.memmap(bin_path, dtype=bullet_dtype, mode='r')
        self.length = len(self.data)

    def __len__(self):
        return self.length

    def __getitem__(self, idx):
        row = self.data[idx]
        
        w_features = torch.zeros(768, dtype=torch.float32)
        b_features = torch.zeros(768, dtype=torch.float32)
        
        occupancy = int(row['occupancy'])
        pieces = row['pieces']
        
        piece_idx = 0
        for sq in range(64):
            if (occupancy & (1 << sq)) != 0:
                byte_val = pieces[piece_idx // 2]
                if piece_idx % 2 == 0:
                    p_type = byte_val & 0x0F
                else:
                    p_type = (byte_val >> 4) & 0x0F
                
                # --- WHITE'S PERSPECTIVE ---
                w_idx = p_type * 64 + sq
                w_features[w_idx] = 1.0
                
                # --- BLACK'S PERSPECTIVE ---
                b_p_type = (p_type + 6) % 12
                b_sq = sq ^ 56
                b_idx = b_p_type * 64 + b_sq
                b_features[b_idx] = 1.0
                
                piece_idx += 1

        score = float(row['score'])
        if row['result'] == 2: result = 1.0
        elif row['result'] == 1: result = 0.5
        else: result = 0.0

        stm = float(row['stm'])

        # ---> FLIP TARGETS FOR BLACK'S PERSPECTIVE <---
        if stm == 1.0: 
            score = -score              # Ensure the score is relative to US
            result = 1.0 - result       # Ensure the result is relative to US

        return w_features, b_features, torch.tensor([stm], dtype=torch.float32), \
               torch.tensor([score], dtype=torch.float32), torch.tensor([result], dtype=torch.float32)

# ==============================================================================
# 2. THE PERSPECTIVE NNUE ARCHITECTURE
# ==============================================================================

class PerspectiveNNUE(nn.Module):
    def __init__(self):
        super(PerspectiveNNUE, self).__init__()
        self.HL = 64
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
# 4. TRAINING LOOP WITH FULL CHECKPOINTING
# ==============================================================================

def train(bin_path, checkpoint_folder, epochs=10, batch_size=8192, lr=1e-3, resume_from=None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Starting Training on: {device}")

    # Create checkpoints directory
    os.makedirs(checkpoint_folder, exist_ok=True)

    dataset = PerspectiveNNUEDataset(bin_path)
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True, num_workers=4)

    model = PerspectiveNNUE().to(device)
    optimizer = optim.Adam(model.parameters(), lr=lr)

    start_epoch = 0

    # --- RESUME FROM CHECKPOINT LOGIC ---
    if resume_from is not None and os.path.exists(resume_from):
        print(f"Loading checkpoint '{resume_from}'...")
        checkpoint = torch.load(resume_from, map_location=device)
        model.load_state_dict(checkpoint['model_state_dict'])
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        start_epoch = checkpoint['epoch']
        print(f"Successfully resumed from Epoch {start_epoch}")
    # ------------------------------------

    for epoch in range(start_epoch, epochs):
        model.train()
        total_loss = 0.0
        
        for batch_idx, (w_feat, b_feat, stm, scores, results) in enumerate(dataloader):
            w_feat = w_feat.to(device)
            b_feat = b_feat.to(device)
            stm = stm.to(device)
            scores = scores.to(device)
            results = results.to(device)

            optimizer.zero_grad()
            outputs = model(w_feat, b_feat, stm)
            loss = blended_loss(outputs, scores, results, blend_ratio=0.7) 
            
            loss.backward()
            optimizer.step()

            total_loss += loss.item()

            if batch_idx % 100 == 0:
                print(f"Epoch {epoch+1}/{epochs} | Batch {batch_idx}/{len(dataloader)} | Loss: {loss.item():.5f}")

        avg_loss = total_loss / len(dataloader)
        print(f"--- Epoch {epoch+1} Complete | Average Loss: {avg_loss:.5f} ---")
        
        # --- FULL CHECKPOINT SAVING LOGIC ---
        checkpoint = {
            'epoch': epoch + 1,
            'model_state_dict': model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'loss': avg_loss,
        }
        
        ckpt_path = os.path.join(checkpoint_folder, f"nnue_epoch_{epoch+1}.pt")
        torch.save(checkpoint, ckpt_path)
        print(f"Saved Full Checkpoint to {ckpt_path}\n")
        # ------------------------------------



def save_npz(checkpoint_path, outfile):
    print(f"Loading checkpoint {checkpoint_path}...")
    
    # 1. Load the checkpoint dictionary (force CPU to avoid device mismatch)
    checkpoint_dict = torch.load(checkpoint_path, map_location=torch.device('cpu'))
    
    # 2. Extract ONLY the model weights from our new checkpoint structure
    state_dict = checkpoint_dict['model_state_dict']
    
    # 3. Map the new keys and shapes to your old C++ expectations
    weights = {}
    
    # ft.weight -> accumulator.weight
    weights['accumulator.weight'] = state_dict['ft.weight'].numpy()
    
    # ft.bias -> accumulator.bias
    weights['accumulator.bias'] = state_dict['ft.bias'].numpy()
    
    # out.weight -> output_weights
    weights['output_weights'] = state_dict['out.weight'].numpy().T
    
    # out.bias -> output_bias
    weights['output_bias'] = state_dict['out.bias'].numpy()
    
    # 4. Save to .npz
    np.savez(f"{outfile}.npz", **weights)
    print(f"Successfully saved weights for C++ to {outfile}.npz")


def evaluate_checkpoint(bin_path, checkpoint_path, batch_size=8192, blend_ratio=0.7):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"\nEvaluating Checkpoint: {checkpoint_path} on {device}")

    # Load dataset
    dataset = PerspectiveNNUEDataset(bin_path)
    # shuffle=False is better for pure evaluation
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=False, num_workers=4)

    # Initialize model and load weights
    model = PerspectiveNNUE().to(device)
    
    if not os.path.exists(checkpoint_path):
        print(f"Error: Checkpoint {checkpoint_path} not found!")
        return

    checkpoint = torch.load(checkpoint_path, map_location=device)
    model.load_state_dict(checkpoint['model_state_dict'])
    
    # Put model in evaluation mode (disables gradients, dropout, etc.)
    model.eval()

    total_wdl_mse = 0.0
    total_cp_mse = 0.0
    total_positions = 0

    # torch.no_grad() speeds up computation and saves memory since we aren't training
    with torch.no_grad():
        for batch_idx, (w_feat, b_feat, stm, scores, results) in enumerate(dataloader):
            w_feat = w_feat.to(device)
            b_feat = b_feat.to(device)
            stm = stm.to(device)
            scores = scores.to(device)
            results = results.to(device)

            # Get network outputs
            outputs = model(w_feat, b_feat, stm)

            # --- 1. WDL Metrics (What the network trained to minimize) ---
            net_wdl = torch.sigmoid(outputs)
            score_wdl = torch.sigmoid(scores / 400.0)
            target_wdl = blend_ratio * score_wdl + (1.0 - blend_ratio) * results # Blended target
            
            # Squared error sum for the batch
            wdl_se = torch.sum((net_wdl - target_wdl) ** 2).item()
            total_wdl_mse += wdl_se

            # --- 2. Centipawn Metrics (Intuitive Engine Metric) ---
            # Because our loss function uses (scores / 400.0), the network learns
            # to output values where 1.0 network unit = 400 centipawns.
            net_cp = outputs * 400.0
            
            # Squared error sum for Centipawns
            cp_se = torch.sum((net_cp - scores) ** 2).item()
            total_cp_mse += cp_se

            total_positions += scores.size(0)

            if batch_idx % 100 == 0:
                print(f"Evaluated {total_positions}/{len(dataset)} positions...")

    # Calculate final MSE and RMSE
    final_wdl_mse = total_wdl_mse / total_positions
    final_wdl_rmse = math.sqrt(final_wdl_mse)
    
    final_cp_mse = total_cp_mse / total_positions
    final_cp_rmse = math.sqrt(final_cp_mse)

    print("\n" + "="*40)
    print("EVALUATION RESULTS")
    print("="*40)
    print(f"Total Positions: {total_positions:,}")
    print("-" * 40)
    print(f"WDL Target MSE:  {final_wdl_mse:.5f}")
    print(f"WDL Target RMSE: {final_wdl_rmse:.5f} (probability margin of error)")
    print("-" * 40)
    print(f"Centipawn MSE:   {final_cp_mse:.2f}")
    print(f"Centipawn RMSE:  {final_cp_rmse:.2f} (cp margin of error)")
    print("="*40 + "\n")


def gen_all_weights():
    from glob import glob
    nets = glob("../checkpoints/*.pt")
    for net in nets:
        epoch = net.split("_")[-1].replace(".pt", "")
        save_npz(net, f"net_{epoch}")

if __name__ == "__main__":
    training_file = "../data/train/train.bin"
    checkpoint_folder = "../checkpoints"

    # evaluate_checkpoint(training_file, os.path.join(checkpoint_folder, "nnue_epoch_10.pt"))
    
    if os.path.exists(training_file):
        train(training_file, checkpoint_folder, epochs=10, batch_size=8192, lr=1e-3)
        
        # To resume from epoch 5
        # train(training_file, epochs=10, batch_size=8192, lr=1e-3, resume_from="checkpoints/nnue_epoch_5.pt")
    else:
        print(f"File {training_file} not found.")