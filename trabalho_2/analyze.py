#!/usr/bin/env python3
"""Analyze MPI master-worker scaling CSV and produce speedup/efficiency
tables and plots.

Usage:
    python analyze.py results_lad.csv
        -> writes plots/*.png next to the CSV
        -> prints markdown-ready tables to stdout

Strong scaling:
    Speedup_strong(W)    = T_seq / T(W)
    Efficiency_strong(W) = Speedup_strong(W) / W       (W = num. workers)

Weak scaling (per professor's instruction: count only workers):
    Speedup_weak(W)    = T(1 worker) / T(W)            (ideal = 1.0)
    Efficiency_weak(W) = Speedup_weak(W)               (same; ideal = 1.0)
"""

import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) != 2:
        print("usage: analyze.py <results.csv>", file=sys.stderr)
        sys.exit(1)

    csv_path = Path(sys.argv[1])
    df = pd.read_csv(csv_path)

    # Median across repetitions per configuration.
    agg = (
        df.groupby(["experiment", "kind", "np", "workers", "total_frames"], as_index=False)
        .agg(
            total_s=("total_s", "median"),
            io_s=("io_s", "median"),
            compute_max_s=("compute_max_s", "median"),
            compute_min_s=("compute_min_s", "median"),
            imbalance_pct=("imbalance_pct", "median"),
        )
    )

    # ---------------- Strong scaling ----------------
    strong = agg[agg["experiment"] == "strong"].copy()
    t_seq_row = strong[strong["kind"] == "seq"]
    if t_seq_row.empty:
        sys.exit("No sequential row found in strong experiment.")
    t_seq = float(t_seq_row["total_s"].iloc[0])

    strong_par = strong[strong["kind"] == "par"].sort_values("workers").copy()
    strong_par["speedup"] = t_seq / strong_par["total_s"]
    strong_par["efficiency"] = strong_par["speedup"] / strong_par["workers"]

    # ---------------- Weak scaling ----------------
    # Speedup uses T_seq measured for the SAME workload (joined by total_frames),
    # not T(W=1). This is the correct weak metric when work-per-worker is fixed
    # but per-frame cost grows with frame_id (Julia zoom). Falls back to the
    # T(W=1)/T(W) metric if no matching seq row exists.
    weak_all = agg[agg["experiment"] == "weak"].copy()
    weak = weak_all[weak_all["kind"] == "par"].sort_values("workers").copy()
    seq_map = (
        weak_all[weak_all["kind"] == "seq"]
        .set_index("total_frames")["total_s"]
        .to_dict()
    )
    if seq_map:
        weak["t_seq"] = weak["total_frames"].map(seq_map)
        weak["speedup"] = weak["t_seq"] / weak["total_s"]
        weak["efficiency"] = weak["speedup"] / weak["workers"].clip(lower=1)
    else:
        t1_row = weak[weak["workers"] == 1]
        if t1_row.empty:
            sys.exit("No 1-worker row found in weak experiment.")
        t1 = float(t1_row["total_s"].iloc[0])
        weak["t_seq"] = float("nan")
        weak["speedup"] = t1 / weak["total_s"]
        weak["efficiency"] = weak["speedup"]

    # ---------------- Tables ----------------
    print(f"Sequential baseline: T_seq = {t_seq:.4f}s\n")

    print("### Strong scaling")
    print("| np | workers | frames | T(s) | Speedup | Eficiência | Imbalance% |")
    print("|----|---------|--------|------|---------|------------|------------|")
    for _, r in strong_par.iterrows():
        print(
            f"| {int(r['np'])} | {int(r['workers'])} | {int(r['total_frames'])} | "
            f"{r['total_s']:.4f} | {r['speedup']:.3f} | {r['efficiency']:.3f} | "
            f"{r['imbalance_pct']:.2f} |"
        )

    print(f"\n### Weak scaling")
    print("| np | workers | frames | T_seq (s) | T_par (s) | Speedup | Eficiência | Imbalance% |")
    print("|----|---------|--------|-----------|-----------|---------|------------|------------|")
    for _, r in weak.iterrows():
        t_seq_str = f"{r['t_seq']:.4f}" if r["t_seq"] == r["t_seq"] else "—"
        print(
            f"| {int(r['np'])} | {int(r['workers'])} | {int(r['total_frames'])} | "
            f"{t_seq_str} | {r['total_s']:.4f} | {r['speedup']:.3f} | {r['efficiency']:.3f} | "
            f"{r['imbalance_pct']:.2f} |"
        )

    # ---------------- Plots ----------------
    outdir = csv_path.parent / "plots"
    outdir.mkdir(exist_ok=True)

    def save_plot(fname):
        plt.tight_layout()
        plt.savefig(outdir / fname, dpi=150)
        plt.close()

    plt.figure(figsize=(6, 4.5))
    plt.plot(strong_par["workers"], strong_par["speedup"], "o-", linewidth=2, markersize=8, label="medido")
    plt.plot(strong_par["workers"], strong_par["workers"], "k--", alpha=0.5, label="ideal (linear)")
    plt.xlabel("Trabalhadores")
    plt.ylabel("Speedup")
    plt.title("Speedup forte")
    plt.legend()
    plt.grid(True, alpha=0.3)
    save_plot("strong_speedup.png")

    plt.figure(figsize=(6, 4.5))
    plt.plot(strong_par["workers"], strong_par["efficiency"], "o-", linewidth=2, markersize=8)
    plt.axhline(1.0, color="k", linestyle="--", alpha=0.5, label="ideal")
    plt.xlabel("Trabalhadores")
    plt.ylabel("Eficiência")
    plt.title("Eficiência forte")
    plt.ylim(0, 1.2)
    plt.legend()
    plt.grid(True, alpha=0.3)
    save_plot("strong_efficiency.png")

    plt.figure(figsize=(6, 4.5))
    plt.plot(weak["workers"], weak["efficiency"], "o-", linewidth=2, markersize=8)
    plt.axhline(1.0, color="k", linestyle="--", alpha=0.5, label="ideal")
    plt.xlabel("Trabalhadores")
    plt.ylabel("Eficiência (fraca)")
    plt.title("Eficiência fraca")
    plt.ylim(0, 1.2)
    plt.legend()
    plt.grid(True, alpha=0.3)
    save_plot("weak_efficiency.png")

    plt.figure(figsize=(6, 4.5))
    plt.plot(strong_par["workers"], strong_par["imbalance_pct"], "o-", linewidth=2, markersize=8, label="forte")
    plt.plot(weak["workers"], weak["imbalance_pct"], "s-", linewidth=2, markersize=8, label="fraca")
    plt.xlabel("Trabalhadores")
    plt.ylabel("Desbalanceamento (%)")
    plt.title("Desbalanceamento: (max − min) / max")
    plt.legend()
    plt.grid(True, alpha=0.3)
    save_plot("imbalance.png")

    print(f"\nPlots saved to: {outdir}")


if __name__ == "__main__":
    main()
