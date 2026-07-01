#!/usr/bin/env python3
"""
Analyze MPI+OpenMP hybrid scaling CSV and produce speedup/efficiency
tables and plots comparing Pure MPI vs Hybrid Oversubscribed vs Hybrid Dedicated.

Usage:
    python analyze.py results_lad.csv
"""

import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Set matplotlib style for nice graphs
plt.style.use('seaborn-v0_8-darkgrid')
COLORS = {'pure_mpi': '#1f77b4', 'hybrid_oversubscribed': '#ff7f0e', 'hybrid_dedicated': '#2ca02c'}
LABELS = {'pure_mpi': 'MPI Puro', 'hybrid_oversubscribed': 'Híbrido (Oversubscribed)', 'hybrid_dedicated': 'Híbrido (Core Dedicado)'}

def compute_metrics(df):
    # Separate seq and par
    seq_df = df[df['kind'] == 'seq'].copy()
    if len(seq_df) == 0:
        print("Warning: No sequential baseline found. Assuming T_seq = 100 for dummy plots.")
        seq_time = 100.0
    else:
        seq_time = seq_df['total_s'].mean()
        
    print(f"Sequential Baseline Time: {seq_time:.4f}s")

    par_df = df[df['kind'] != 'seq'].copy()
    
    # Calculate means over repetitions
    grouped = par_df.groupby(['experiment', 'kind', 'np', 'workers', 'omp_threads']).agg({
        'total_frames': 'mean',
        'total_s': 'mean',
        'io_s': 'mean',
        'compute_max_s': 'mean',
        'compute_min_s': 'mean',
        'imbalance_pct': 'mean'
    }).reset_index()

    grouped['total_workers'] = grouped['workers'] * grouped['omp_threads']

    # Speedup and Efficiency
    grouped['speedup'] = seq_time / grouped['total_s']
    
    # Efficiency: speedup / total_workers
    grouped['efficiency'] = grouped['speedup'] / grouped['total_workers']

    return grouped, seq_time

def plot_strong_scaling(df, seq_time):
    strong = df[df['experiment'] == 'strong'].copy()
    if strong.empty:
        return

    # Sort by total_workers to plot correctly
    strong = strong.sort_values('total_workers')

    # Speedup Plot
    plt.figure(figsize=(10, 6))
    workers_set = sorted(strong['total_workers'].unique())
    
    if len(workers_set) > 0:
        # Ideal line: speedup = total_workers
        plt.plot(workers_set, workers_set, 'k--', label='Speedup Ideal', linewidth=2)

    for kind in ['pure_mpi', 'hybrid_oversubscribed', 'hybrid_dedicated']:
        sub = strong[strong['kind'] == kind]
        if not sub.empty:
            plt.plot(sub['total_workers'], sub['speedup'], marker='o', 
                     color=COLORS[kind], label=LABELS[kind], linewidth=2, markersize=8)

    plt.title('Strong Scaling - Speedup', fontsize=16, fontweight='bold')
    plt.xlabel('Total de Workers (Threads/Processos de Computação)', fontsize=14)
    plt.ylabel('Speedup (T_seq / T_par)', fontsize=14)
    plt.xticks(workers_set)
    plt.legend(fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    os.makedirs('plots', exist_ok=True)
    plt.savefig('plots/strong_speedup.png', dpi=300)
    plt.close()

    # Efficiency Plot
    plt.figure(figsize=(10, 6))
    
    if len(workers_set) > 0:
        plt.plot(workers_set, [1.0] * len(workers_set), 'k--', label='Eficiência Ideal (100%)', linewidth=2)

    for kind in ['pure_mpi', 'hybrid_oversubscribed', 'hybrid_dedicated']:
        sub = strong[strong['kind'] == kind]
        if not sub.empty:
            plt.plot(sub['total_workers'], sub['efficiency'], marker='s', 
                     color=COLORS[kind], label=LABELS[kind], linewidth=2, markersize=8)

    plt.title('Strong Scaling - Eficiência', fontsize=16, fontweight='bold')
    plt.xlabel('Total de Workers (Threads/Processos de Computação)', fontsize=14)
    plt.ylabel('Eficiência (Speedup / Total Workers)', fontsize=14)
    plt.ylim(0, 1.1)
    plt.xticks(workers_set)
    plt.legend(fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('plots/strong_efficiency.png', dpi=300)
    plt.close()

def plot_weak_scaling(df):
    weak = df[df['experiment'] == 'weak'].copy()
    if weak.empty:
        return

    weak = weak.sort_values('total_workers')
    
    # We define weak efficiency relative to the baseline performance of 1 worker
    # We find the baseline time for 1 worker for each kind
    plt.figure(figsize=(10, 6))
    workers_set = sorted(weak['total_workers'].unique())
    
    if len(workers_set) > 0:
        plt.plot(workers_set, [1.0] * len(workers_set), 'k--', label='Eficiência Ideal (100%)', linewidth=2)

    for kind in ['pure_mpi', 'hybrid_oversubscribed', 'hybrid_dedicated']:
        sub = weak[weak['kind'] == kind].copy()
        if not sub.empty:
            # Baseline is the time at minimum worker count (usually 1 or 2)
            base_workers = sub['total_workers'].min()
            base_time = sub[sub['total_workers'] == base_workers]['total_s'].values[0]
            
            sub['weak_efficiency'] = base_time / sub['total_s']
            
            plt.plot(sub['total_workers'], sub['weak_efficiency'], marker='^', 
                     color=COLORS[kind], label=LABELS[kind], linewidth=2, markersize=8)

    plt.title('Weak Scaling - Eficiência', fontsize=16, fontweight='bold')
    plt.xlabel('Total de Workers (Threads/Processos de Computação)', fontsize=14)
    plt.ylabel('Eficiência Fraca (T_base / T_N)', fontsize=14)
    plt.ylim(0, 1.1)
    plt.xticks(workers_set)
    plt.legend(fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('plots/weak_efficiency.png', dpi=300)
    plt.close()

def print_markdown_tables(df):
    print("\n### Tabelas de Resultados Consolidados")
    print("\n#### Strong Scaling")
    strong = df[df['experiment'] == 'strong'].sort_values(['kind', 'total_workers'])
    print("| Kind | NP | Workers | OMP Threads | Total Workers | Tempo (s) | Speedup | Eficiência | Desbalanceamento |")
    print("|---|---|---|---|---|---|---|---|---|")
    for _, row in strong.iterrows():
        print(f"| {row['kind']} | {row['np']} | {row['workers']} | {row['omp_threads']} | {row['total_workers']} | {row['total_s']:.4f} | {row['speedup']:.2f}x | {row['efficiency']*100:.1f}% | {row['imbalance_pct']:.1f}% |")

    print("\n#### Weak Scaling")
    weak = df[df['experiment'] == 'weak'].sort_values(['kind', 'total_workers'])
    print("| Kind | NP | Workers | OMP Threads | Total Workers | Tempo (s) | Desbalanceamento |")
    print("|---|---|---|---|---|---|---|")
    for _, row in weak.iterrows():
        print(f"| {row['kind']} | {row['np']} | {row['workers']} | {row['omp_threads']} | {row['total_workers']} | {row['total_s']:.4f} | {row['imbalance_pct']:.1f}% |")
    print("\n")

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze.py <results_csv>")
        sys.exit(1)

    csv_file = sys.argv[1]
    if not os.path.exists(csv_file):
        print(f"File {csv_file} not found.")
        sys.exit(1)

    df = pd.read_csv(csv_file)
    grouped, seq_time = compute_metrics(df)

    plot_strong_scaling(grouped, seq_time)
    plot_weak_scaling(grouped)
    print_markdown_tables(grouped)

if __name__ == '__main__':
    main()
