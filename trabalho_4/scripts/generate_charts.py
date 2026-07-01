#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "resultados"
CHARTS_DIR = ROOT / "graficos"

PROGRAM_LABELS = {
    "mpi_only": "MPI Puro",
    "hibrido": "Híbrido (MPI + OpenMP)",
    "hibrido_db": "Híbrido Double Buffered",
}

PROGRAM_COLORS = {
    "mpi_only": "#2563eb",     # Azul
    "hibrido": "#16a34a",      # Verde
    "hibrido_db": "#9333ea",   # Roxo
}

def parse_logs():
    data = {
        "mpi_only": {},
        "hibrido": {},
        "hibrido_db": {}
    }
    
    # Read resultados_finais.log
    log_finais = RESULTS_DIR / "resultados_finais.log"
    if log_finais.exists():
        with open(log_finais, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith("Hibrido,"):
                    # Hibrido, Nos: 1, Threads: 1, Tempo: 443.659770s
                    m = re.search(r"Nos:\s*(\d+),\s*Threads:\s*(\d+),\s*Tempo:\s*([0-9.]+)s", line)
                    if m:
                        n, t, time_s = int(m.group(1)), int(m.group(2)), float(m.group(3))
                        w = n * t
                        data["hibrido"].setdefault(w, []).append(time_s)
                elif line.startswith("MPI Only,"):
                    # MPI Only, Nos: 1, Threads: 1 (Processos=1), Tempo: 443.926319s
                    m = re.search(r"Nos:\s*(\d+).*\(Processos=(\d+)\),\s*Tempo:\s*([0-9.]+)s", line)
                    if m:
                        n, p, time_s = int(m.group(1)), int(m.group(2)), float(m.group(3))
                        w = p
                        if w == 63: w = 64 # normalize to 64 for plotting/comparison
                        data["mpi_only"].setdefault(w, []).append(time_s)

    # Read resultados_db.log
    log_db = RESULTS_DIR / "resultados_db.log"
    if log_db.exists():
        with open(log_db, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith("Hibrido DB,"):
                    # Hibrido DB, Nos: 1, Threads: 1, Tempo: 443.561391s
                    m = re.search(r"Nos:\s*(\d+),\s*Threads:\s*(\d+),\s*Tempo:\s*([0-9.]+)s", line)
                    if m:
                        n, t, time_s = int(m.group(1)), int(m.group(2)), float(m.group(3))
                        w = n * t
                        data["hibrido_db"].setdefault(w, []).append(time_s)
                        
    return data

def configure_axes(ax, workers, metric, ideal_line=True):
    ax.set_xlabel("Trabalhadores (Processos / Threads)", fontsize=11)
    ax.set_xticks(workers)
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    if ideal_line:
        if metric == "speedup":
            ax.plot(workers, workers, "--", color="#6b7280", linewidth=1.8, label="Ideal / Linear")
        elif metric == "eficiencia":
            ax.axhline(1.0, linestyle="--", color="#6b7280", linewidth=1.8, label="Ideal / 100%")

def add_point_labels(ax, xs, ys, metric):
    for x, y in zip(xs, ys):
        if x in (1, 8, 32, 64):
            if metric in ("eficiencia",):
                text = f"{y:.2f}"
            elif metric == "speedup":
                text = f"{y:.1f}x"
            else:
                text = f"{y:.1f}s"
            ax.annotate(text, (x, y), xytext=(0, 8), textcoords="offset points",
                        ha="center", fontsize=8, fontweight="bold")

def generate_plots_and_table(data):
    CHARTS_DIR.mkdir(parents=True, exist_ok=True)
    
    # Calculate medians and metrics
    workers = sorted({w for prog in data.values() for w in prog.keys()})
    
    # Baseline T1 from MPI Only W=1 (or average of W=1 across all)
    base_t1 = statistics.median(data["mpi_only"][1]) if 1 in data["mpi_only"] else 443.7
    
    summary_rows = []
    plot_data = {prog: {"w": [], "time": [], "speedup": [], "eff": []} for prog in data.keys()}
    
    for prog, w_dict in data.items():
        for w in sorted(w_dict.keys()):
            t_med = statistics.median(w_dict[w])
            spd = base_t1 / t_med
            eff = spd / w
            
            plot_data[prog]["w"].append(w)
            plot_data[prog]["time"].append(t_med)
            plot_data[prog]["speedup"].append(spd)
            plot_data[prog]["eff"].append(eff)
            
            summary_rows.append({
                "programa": PROGRAM_LABELS[prog],
                "trabalhadores": w if w != 64 or prog != "mpi_only" else 63,
                "tempo_s": f"{t_med:.4f}",
                "speedup": f"{spd:.4f}",
                "eficiencia": f"{eff:.4f}"
            })
            
    # Write CSV
    csv_path = RESULTS_DIR / "tabela_comparativa.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["programa", "trabalhadores", "tempo_s", "speedup", "eficiencia"])
        writer.writeheader()
        writer.writerows(summary_rows)
    print(f"Tabela salva em: {csv_path}")
    
    # 1. SPEEDUP PLOT
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=180)
    configure_axes(ax, workers, "speedup", ideal_line=True)
    ax.set_title("Speedup vs Número de Trabalhadores\nConjunto de Julia (128 quadros)", fontsize=14, pad=12, fontweight="bold")
    ax.set_ylabel("Speedup (x)", fontsize=11)
    
    for prog in ("mpi_only", "hibrido", "hibrido_db"):
        p = plot_data[prog]
        if not p["w"]: continue
        ax.plot(p["w"], p["speedup"], marker="o", linewidth=2.2, markersize=6,
                color=PROGRAM_COLORS[prog], label=PROGRAM_LABELS[prog])
        add_point_labels(ax, p["w"], p["speedup"], "speedup")
        
    ax.set_ylim(bottom=0)
    ax.legend(frameon=True, loc="upper left")
    fig.tight_layout()
    fig.savefig(CHARTS_DIR / "speedup.png", bbox_inches="tight")
    fig.savefig(CHARTS_DIR / "speedup.svg", bbox_inches="tight")
    plt.close(fig)
    print("Gráfico de Speedup gerado.")

    # 2. EFFICIENCY PLOT
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=180)
    configure_axes(ax, workers, "eficiencia", ideal_line=True)
    ax.set_title("Eficiência vs Número de Trabalhadores\nConjunto de Julia (128 quadros)", fontsize=14, pad=12, fontweight="bold")
    ax.set_ylabel("Eficiência", fontsize=11)
    
    for prog in ("mpi_only", "hibrido", "hibrido_db"):
        p = plot_data[prog]
        if not p["w"]: continue
        ax.plot(p["w"], p["eff"], marker="s", linewidth=2.2, markersize=6,
                color=PROGRAM_COLORS[prog], label=PROGRAM_LABELS[prog])
        add_point_labels(ax, p["w"], p["eff"], "eficiencia")
        
    ax.set_ylim(0, 1.1)
    ax.legend(frameon=True, loc="upper right")
    fig.tight_layout()
    fig.savefig(CHARTS_DIR / "eficiencia.png", bbox_inches="tight")
    fig.savefig(CHARTS_DIR / "eficiencia.svg", bbox_inches="tight")
    plt.close(fig)
    print("Gráfico de Eficiência gerado.")

    # 3. TIME PLOT
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=180)
    configure_axes(ax, workers, "tempo", ideal_line=False)
    ax.set_title("Tempo de Execução vs Número de Trabalhadores\nConjunto de Julia (128 quadros)", fontsize=14, pad=12, fontweight="bold")
    ax.set_ylabel("Tempo de Execução (s)", fontsize=11)
    
    for prog in ("mpi_only", "hibrido", "hibrido_db"):
        p = plot_data[prog]
        if not p["w"]: continue
        ax.plot(p["w"], p["time"], marker="^", linewidth=2.2, markersize=6,
                color=PROGRAM_COLORS[prog], label=PROGRAM_LABELS[prog])
        add_point_labels(ax, p["w"], p["time"], "tempo")
        
    ax.set_ylim(bottom=0)
    ax.set_yscale("log") # log scale is great to see differences at small numbers
    ax.set_ylabel("Tempo de Execução - Escala Log (s)", fontsize=11)
    ax.legend(frameon=True, loc="upper right")
    fig.tight_layout()
    fig.savefig(CHARTS_DIR / "tempo_execucao.png", bbox_inches="tight")
    fig.savefig(CHARTS_DIR / "tempo_execucao.svg", bbox_inches="tight")
    plt.close(fig)
    print("Gráfico de Tempo gerado.")

if __name__ == "__main__":
    data = parse_logs()
    generate_plots_and_table(data)
