#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

try:
    import torch
    from torch import nn
except ImportError as exc:  # pragma: no cover
    raise SystemExit("PyTorch is required for ml/predict.py. Install/load torch first.") from exc


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


def read_table(path: Path) -> pd.DataFrame:
    if path.suffix.lower() == ".parquet":
        return pd.read_parquet(path)
    return pd.read_csv(path)


def bool_to_float(series: pd.Series) -> pd.Series:
    if series.dtype == bool:
        return series.astype(float)
    return series.astype(str).str.strip().str.lower().map({"true": 1.0, "false": 0.0, "1": 1.0, "0": 0.0, "yes": 1.0, "no": 0.0}).fillna(0.0)


def load_checkpoint(path: Path, device: str | None = None) -> tuple[SurrogateMLP, dict[str, Any], torch.device]:
    runtime_device = torch.device(device or ("cuda" if torch.cuda.is_available() else "cpu"))
    ckpt = torch.load(path, map_location=runtime_device)
    model = SurrogateMLP(
        input_dim=int(ckpt["input_dim"]),
        output_dim=int(ckpt["output_dim"]),
        hidden=int(ckpt["hidden"]),
        depth=int(ckpt["depth"]),
        dropout=float(ckpt.get("dropout", 0.0)),
    ).to(runtime_device)
    model.load_state_dict(ckpt["state_dict"])
    model.eval()
    return model, ckpt["preprocessing"], runtime_device


def transform_dataframe(df: pd.DataFrame, preprocessing: dict[str, Any]) -> np.ndarray:
    frames: list[pd.DataFrame] = []
    for col in preprocessing.get("numeric_cols", []):
        fill = preprocessing.get("numeric_fill", {}).get(col, 0.0)
        if col in df:
            series = pd.to_numeric(df[col], errors="coerce").fillna(fill)
        else:
            series = pd.Series([fill] * len(df))
        frames.append(pd.DataFrame({col: series.astype(float)}))

    for col in preprocessing.get("bool_cols", []):
        if col in df:
            frames.append(pd.DataFrame({col: bool_to_float(df[col])}))
        else:
            frames.append(pd.DataFrame({col: np.zeros(len(df), dtype=float)}))

    for col in preprocessing.get("categorical_cols", []):
        values = df[col].fillna("missing").astype(str) if col in df else pd.Series(["missing"] * len(df))
        levels = preprocessing.get("categories", {}).get(col, [])
        frames.append(pd.DataFrame({f"{col}__{level}": (values == level).astype(float) for level in levels}))

    Xdf = pd.concat(frames, axis=1) if frames else pd.DataFrame(index=df.index)
    expected = preprocessing["expanded_feature_cols"]
    for col in expected:
        if col not in Xdf:
            Xdf[col] = 0.0
    Xdf = Xdf[expected]
    X = Xdf.to_numpy(dtype=np.float32)
    x_mean = np.array(preprocessing["x_mean"], dtype=np.float32)
    x_std = np.array(preprocessing["x_std"], dtype=np.float32)
    return ((X - x_mean) / x_std).astype(np.float32)


def predict_dataframe(df: pd.DataFrame, model: SurrogateMLP, preprocessing: dict[str, Any], device: torch.device) -> pd.DataFrame:
    X = transform_dataframe(df, preprocessing)
    with torch.no_grad():
        pred_scaled = model(torch.tensor(X, dtype=torch.float32).to(device)).cpu().numpy()
    y_mean = np.array(preprocessing["y_mean"], dtype=np.float32)
    y_std = np.array(preprocessing["y_std"], dtype=np.float32)
    pred = pred_scaled * y_std + y_mean
    target_cols = preprocessing["target_cols"]
    out = pd.DataFrame(pred, columns=[f"pred_{c}" for c in target_cols])
    # Clamp probability-like targets to [0, 1] for readability.
    for col in out.columns:
        target = col.removeprefix("pred_")
        if target.endswith("_reached"):
            out[col] = out[col].clip(0.0, 1.0)
        if any(token in target for token in ["energy", "score", "contrast", "imbalance", "time", "kinetic"]):
            out[col] = out[col].clip(lower=0.0)
        if "quality" in target:
            out[col] = out[col].clip(0.0, 1.0)
    return pd.concat([df.reset_index(drop=True), out], axis=1)


def main() -> None:
    parser = argparse.ArgumentParser(description="Predict Maxwell demon outcomes from a design/config table using a trained surrogate.")
    parser.add_argument("--model", type=Path, required=True, help="Model checkpoint .pt")
    parser.add_argument("--input", type=Path, required=True, help="Input CSV/Parquet table with design variables")
    parser.add_argument("--out", type=Path, required=True, help="Output CSV/Parquet with predicted columns")
    parser.add_argument("--device", type=str, default=None, help="Override torch device, e.g. cpu or cuda")
    args = parser.parse_args()

    model, preprocessing, device = load_checkpoint(args.model, args.device)
    df = read_table(args.input)
    result = predict_dataframe(df, model, preprocessing, device)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.out.suffix.lower() == ".parquet":
        result.to_parquet(args.out, index=False)
    else:
        result.to_csv(args.out, index=False)
    print(f"Wrote predictions for {len(result)} rows to {args.out} using {device}")


if __name__ == "__main__":
    main()
