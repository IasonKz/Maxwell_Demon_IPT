#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

try:
    import torch
    from torch import nn
    from torch.utils.data import DataLoader, TensorDataset
except ImportError as exc:  # pragma: no cover
    raise SystemExit("PyTorch is required for ml/train_surrogate.py. Install/load torch first.") from exc


DEFAULT_TARGETS = [
    "temperature_contrast_final",
    "population_imbalance_final",
    "separation_score_final",
    "equilibrium_quality_final",
    "energy_injected_total",
    "energy_dissipated_total",
    "kinetic_energy_total_final",
    "steady_state_reached",
    "separated_steady_state_reached",
    "thermal_equilibrium_reached",
]

NUMERIC_FEATURE_CANDIDATES = [
    "num_particles", "species_count", "lx", "ly", "max_radius", "packing_fraction_target",
    "dt", "steps", "observable_interval", "collision_iterations",
    "velocity_std", "initial_left_fraction", "middle_wall_x", "opening_height", "demon_threshold",
    "restitution", "equilibrium_window", "equilibrium_required_windows", "equilibrium_temp_tol",
    "equilibrium_density_tol", "equilibrium_flux_tol", "equilibrium_slope_tol", "separated_steady_state_min_score",
    "species0_count", "species0_radius", "species0_mass", "species0_friction_gamma", "species0_drive_strength",
    "species1_count", "species1_radius", "species1_mass", "species1_friction_gamma", "species1_drive_strength",
]
CATEGORICAL_FEATURE_CANDIDATES = ["demon_mode", "scenario"]
BOOL_FEATURE_CANDIDATES = ["detect_thermal_equilibrium", "stop_at_thermal_equilibrium", "stop_at_steady_state"]
ID_COLUMNS = {"run_id", "run_dir", "output_dir", "config_path"}


def read_table(path: Path) -> pd.DataFrame:
    if path.suffix.lower() == ".parquet":
        return pd.read_parquet(path)
    return pd.read_csv(path)


def bool_to_float(series: pd.Series) -> pd.Series:
    if series.dtype == bool:
        return series.astype(float)
    return series.astype(str).str.strip().str.lower().map({"true": 1.0, "false": 0.0, "1": 1.0, "0": 0.0, "yes": 1.0, "no": 0.0}).fillna(0.0)


def choose_columns(df: pd.DataFrame, target_cols: list[str] | None) -> tuple[list[str], list[str], list[str], list[str]]:
    targets = [c for c in (target_cols or DEFAULT_TARGETS) if c in df.columns]
    if not targets:
        raise ValueError("No target columns found in dataset. Available columns: " + ", ".join(df.columns))
    numeric = [c for c in NUMERIC_FEATURE_CANDIDATES if c in df.columns and c not in targets]
    categorical = [c for c in CATEGORICAL_FEATURE_CANDIDATES if c in df.columns]
    bool_features = [c for c in BOOL_FEATURE_CANDIDATES if c in df.columns]
    return numeric, categorical, bool_features, targets


def fit_transform(df: pd.DataFrame, numeric_cols: list[str], categorical_cols: list[str], bool_cols: list[str], target_cols: list[str]) -> tuple[np.ndarray, np.ndarray, dict[str, Any]]:
    frames: list[pd.DataFrame] = []
    preprocessing: dict[str, Any] = {
        "numeric_cols": numeric_cols,
        "categorical_cols": categorical_cols,
        "bool_cols": bool_cols,
        "target_cols": target_cols,
        "categories": {},
        "numeric_fill": {},
    }

    if numeric_cols:
        num = df[numeric_cols].apply(pd.to_numeric, errors="coerce")
        fills = num.median(numeric_only=True).fillna(0.0)
        preprocessing["numeric_fill"] = {k: float(v) for k, v in fills.items()}
        frames.append(num.fillna(fills).astype(float))

    for col in bool_cols:
        frames.append(pd.DataFrame({col: bool_to_float(df[col])}))

    for col in categorical_cols:
        values = df[col].fillna("missing").astype(str)
        levels = sorted(values.unique().tolist())
        preprocessing["categories"][col] = levels
        one_hot = pd.DataFrame({f"{col}__{level}": (values == level).astype(float) for level in levels})
        frames.append(one_hot)

    if not frames:
        raise ValueError("No usable feature columns found")
    Xdf = pd.concat(frames, axis=1)
    ydf = df[target_cols].copy()
    for col in target_cols:
        if ydf[col].dtype == bool or ydf[col].astype(str).str.lower().isin(["true", "false"]).any():
            ydf[col] = bool_to_float(ydf[col])
        else:
            ydf[col] = pd.to_numeric(ydf[col], errors="coerce")
    ydf = ydf.replace([np.inf, -np.inf], np.nan).fillna(0.0)

    X = Xdf.to_numpy(dtype=np.float32)
    y = ydf.to_numpy(dtype=np.float32)

    x_mean = X.mean(axis=0)
    x_std = X.std(axis=0)
    x_std[x_std < 1e-8] = 1.0
    y_mean = y.mean(axis=0)
    y_std = y.std(axis=0)
    y_std[y_std < 1e-8] = 1.0

    preprocessing["expanded_feature_cols"] = Xdf.columns.tolist()
    preprocessing["x_mean"] = x_mean.tolist()
    preprocessing["x_std"] = x_std.tolist()
    preprocessing["y_mean"] = y_mean.tolist()
    preprocessing["y_std"] = y_std.tolist()
    return ((X - x_mean) / x_std).astype(np.float32), ((y - y_mean) / y_std).astype(np.float32), preprocessing


class SurrogateMLP(nn.Module):
    def __init__(self, input_dim: int, output_dim: int, hidden: int, depth: int, dropout: float):
        super().__init__()
        layers: list[nn.Module] = []
        dim = input_dim
        for _ in range(depth):
            layers.append(nn.Linear(dim, hidden))
            layers.append(nn.ReLU())
            if dropout > 0:
                layers.append(nn.Dropout(dropout))
            dim = hidden
        layers.append(nn.Linear(dim, output_dim))
        self.net = nn.Sequential(*layers)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def split_indices(n: int, test_fraction: float, seed: int) -> tuple[np.ndarray, np.ndarray]:
    idx = list(range(n))
    random.Random(seed).shuffle(idx)
    if n < 5:
        return np.array(idx), np.array(idx)
    n_test = max(1, int(round(n * test_fraction)))
    test = np.array(idx[:n_test], dtype=int)
    train = np.array(idx[n_test:], dtype=int)
    if len(train) == 0:
        train = test
    return train, test


def metrics_from_predictions(y_true: np.ndarray, y_pred: np.ndarray, targets: list[str]) -> dict[str, dict[str, float]]:
    metrics: dict[str, dict[str, float]] = {}
    for i, target in enumerate(targets):
        truth = y_true[:, i]
        pred = y_pred[:, i]
        rmse = float(np.sqrt(np.mean((truth - pred) ** 2)))
        mae = float(np.mean(np.abs(truth - pred)))
        denom = float(np.sum((truth - np.mean(truth)) ** 2))
        r2 = float(1.0 - np.sum((truth - pred) ** 2) / denom) if denom > 1e-12 else float("nan")
        metrics[target] = {"rmse": rmse, "mae": mae, "r2": r2}
    return metrics


def plot_diagnostics(out_dir: Path, history: list[dict[str, float]], targets: list[str], y_true: np.ndarray, y_pred: np.ndarray) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    if history:
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.plot([h["epoch"] for h in history], [h["train_loss"] for h in history], label="train")
        ax.plot([h["epoch"] for h in history], [h["val_loss"] for h in history], label="validation")
        ax.set_xlabel("epoch")
        ax.set_ylabel("MSE loss")
        ax.set_title("Training curve")
        ax.legend()
        fig.savefig(out_dir / "training_loss.png", dpi=160, bbox_inches="tight")
        plt.close(fig)

    # Plot the most important target if available, otherwise the first target.
    target = "separation_score_final" if "separation_score_final" in targets else targets[0]
    i = targets.index(target)
    fig, ax = plt.subplots(figsize=(5, 5))
    ax.scatter(y_true[:, i], y_pred[:, i], alpha=0.7)
    low = min(float(y_true[:, i].min()), float(y_pred[:, i].min()))
    high = max(float(y_true[:, i].max()), float(y_pred[:, i].max()))
    ax.plot([low, high], [low, high], linewidth=1)
    ax.set_xlabel(f"true {target}")
    ax.set_ylabel(f"predicted {target}")
    ax.set_title("Predicted vs true")
    fig.savefig(out_dir / "predicted_vs_true.png", dpi=160, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Train a GPU-ready PyTorch surrogate model from Maxwell demon simulation summaries.")
    parser.add_argument("--data", type=Path, required=True, help="Dataset CSV or Parquet")
    parser.add_argument("--out", type=Path, required=True, help="Output model checkpoint .pt")
    parser.add_argument("--targets", nargs="*", default=None, help="Optional explicit target columns")
    parser.add_argument("--epochs", type=int, default=300)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--hidden", type=int, default=128)
    parser.add_argument("--depth", type=int, default=3)
    parser.add_argument("--dropout", type=float, default=0.05)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--test-fraction", type=float, default=0.2)
    parser.add_argument("--seed", type=int, default=123)
    args = parser.parse_args()

    df = read_table(args.data)
    numeric_cols, categorical_cols, bool_cols, target_cols = choose_columns(df, args.targets)
    X, y, preprocessing = fit_transform(df, numeric_cols, categorical_cols, bool_cols, target_cols)
    train_idx, test_idx = split_indices(len(df), args.test_fraction, args.seed)

    # Keep raw target values for reporting.
    y_mean = np.array(preprocessing["y_mean"], dtype=np.float32)
    y_std = np.array(preprocessing["y_std"], dtype=np.float32)
    y_raw = y * y_std + y_mean

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    torch.manual_seed(args.seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(args.seed)

    model = SurrogateMLP(X.shape[1], y.shape[1], hidden=args.hidden, depth=args.depth, dropout=args.dropout).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    loss_fn = nn.MSELoss()

    train_ds = TensorDataset(torch.tensor(X[train_idx]), torch.tensor(y[train_idx]))
    val_x = torch.tensor(X[test_idx], dtype=torch.float32).to(device)
    val_y = torch.tensor(y[test_idx], dtype=torch.float32).to(device)
    loader = DataLoader(train_ds, batch_size=min(args.batch_size, len(train_ds)), shuffle=True)

    history: list[dict[str, float]] = []
    best_val = math.inf
    best_state: dict[str, torch.Tensor] | None = None
    patience = max(30, args.epochs // 8)
    stale = 0

    for epoch in range(1, args.epochs + 1):
        model.train()
        losses = []
        for xb, yb in loader:
            xb = xb.to(device)
            yb = yb.to(device)
            optimizer.zero_grad(set_to_none=True)
            loss = loss_fn(model(xb), yb)
            loss.backward()
            optimizer.step()
            losses.append(float(loss.detach().cpu()))
        model.eval()
        with torch.no_grad():
            val_loss = float(loss_fn(model(val_x), val_y).detach().cpu())
        train_loss = float(np.mean(losses)) if losses else val_loss
        history.append({"epoch": epoch, "train_loss": train_loss, "val_loss": val_loss})
        if val_loss < best_val:
            best_val = val_loss
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
            stale = 0
        else:
            stale += 1
        if stale >= patience:
            break

    if best_state is not None:
        model.load_state_dict(best_state)
    model.eval()
    with torch.no_grad():
        pred_scaled = model(torch.tensor(X[test_idx], dtype=torch.float32).to(device)).cpu().numpy()
    pred_raw = pred_scaled * y_std + y_mean
    true_raw = y_raw[test_idx]
    metrics = metrics_from_predictions(true_raw, pred_raw, target_cols)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    checkpoint = {
        "state_dict": model.state_dict(),
        "input_dim": X.shape[1],
        "output_dim": y.shape[1],
        "hidden": args.hidden,
        "depth": args.depth,
        "dropout": args.dropout,
        "preprocessing": preprocessing,
        "metrics": metrics,
        "device_used": str(device),
        "n_rows": int(len(df)),
        "train_rows": int(len(train_idx)),
        "test_rows": int(len(test_idx)),
    }
    torch.save(checkpoint, args.out)
    metrics_path = args.out.with_suffix(".metrics.json")
    metrics_path.write_text(json.dumps(metrics, indent=2, allow_nan=True))
    plot_diagnostics(args.out.parent, history, target_cols, true_raw, pred_raw)

    print(f"Saved model to {args.out}")
    print(f"Device used: {device}; rows: {len(df)}; train/test: {len(train_idx)}/{len(test_idx)}")
    print(json.dumps(metrics, indent=2, allow_nan=True))


if __name__ == "__main__":
    main()
