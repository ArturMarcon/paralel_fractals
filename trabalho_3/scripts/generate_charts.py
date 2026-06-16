#!/usr/bin/env python3
import csv
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "resultados"
CHARTS_DIR = ROOT / "graficos"

PROGRAM_LABELS = {
    "mpi_arvore": "Arvore fixa",
    "mpi_balanceado": "Balanceada",
}

PROGRAM_COLORS = {
    "mpi_arvore": "#2563eb",
    "mpi_balanceado": "#dc2626",
}

METRICS = [
    ("tempo_par", "Tempo paralelo (s)", False),
    ("speedup", "Speedup", True),
    ("eficiencia", "Eficiencia", True),
    ("desbalanceamento", "Desbalanceamento", False),
    ("processos_ativos", "Processos ativos", True),
]


def parse_dataset_name(path):
    match = re.search(r"N(\d+)_D(\d+)_([A-Za-z0-9]+)", path.name)
    if not match:
        return path.stem, path.stem

    n, delta, input_mode = match.groups()
    dataset_id = f"N{n}_D{delta}_{input_mode}"
    dataset_label = f"N={int(n):,}, delta={int(delta):,}".replace(",", ".")
    return dataset_id, dataset_label


def read_rows(path):
    with path.open("r", encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source))

    for row in rows:
        row["processos"] = int(row["processos"])
        for key in ("tempo_seq", "tempo_par", "speedup", "eficiencia", "desbalanceamento"):
            row[key] = float(row[key])
        row["segmentos"] = int(row["segmentos"])
        row["processos_ativos"] = int(row["processos_ativos"])

    return rows


def grouped_by_program(rows):
    grouped = {}
    for row in rows:
        grouped.setdefault(row["programa"], []).append(row)
    for values in grouped.values():
        values.sort(key=lambda item: item["processos"])
    return grouped


def configure_axes(ax, processes, metric, ideal_line):
    ax.set_xlabel("Processos")
    ax.set_xticks(processes)
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    if ideal_line:
        if metric in ("speedup", "processos_ativos"):
            ax.plot(processes, processes, "--", color="#6b7280", linewidth=1.8, label="Ideal/referencia")
        elif metric == "eficiencia":
            ax.axhline(1.0, linestyle="--", color="#6b7280", linewidth=1.8, label="Ideal/referencia")


def add_point_labels(ax, xs, ys, metric):
    selected = {xs[0], xs[-1]}
    if 15 in xs:
        selected.add(15)

    for x, y in zip(xs, ys):
        if x not in selected:
            continue
        if metric in ("desbalanceamento", "eficiencia"):
            text = f"{y:.2f}"
        elif metric == "processos_ativos":
            text = f"{int(y)}"
        else:
            text = f"{y:.3g}"
        ax.annotate(text, (x, y), xytext=(0, 8), textcoords="offset points",
                    ha="center", fontsize=8)


def make_metric_chart(dataset_id, dataset_label, rows, metric, metric_label, ideal_line):
    grouped = grouped_by_program(rows)
    processes = sorted({row["processos"] for row in rows})
    dataset_dir = CHARTS_DIR / dataset_id
    dataset_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(8.6, 5.0), dpi=180)
    configure_axes(ax, processes, metric, ideal_line)
    ax.set_title(f"{metric_label}\n{dataset_label}", fontsize=14, pad=12)
    ax.set_ylabel(metric_label)

    for program in ("mpi_arvore", "mpi_balanceado"):
        group = grouped.get(program, [])
        if not group:
            continue
        xs = [row["processos"] for row in group]
        ys = [row[metric] for row in group]
        ax.plot(xs, ys, marker="o", linewidth=2.2, markersize=5.5,
                color=PROGRAM_COLORS[program], label=PROGRAM_LABELS[program])
        add_point_labels(ax, xs, ys, metric)

    if metric == "desbalanceamento":
        ax.set_ylim(0, 1.05)
    else:
        ax.set_ylim(bottom=0)

    ax.legend(frameon=True, loc="best")
    fig.tight_layout()

    png_path = dataset_dir / f"{metric}.png"
    svg_path = dataset_dir / f"{metric}.svg"
    fig.savefig(png_path, bbox_inches="tight")
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    return png_path, svg_path


def make_time_comparison_chart(dataset_id, dataset_label, rows):
    grouped = grouped_by_program(rows)
    processes = sorted({row["processos"] for row in rows})
    dataset_dir = CHARTS_DIR / dataset_id
    dataset_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(8.6, 5.0), dpi=180)
    ax.set_title(f"Tempo paralelo por versao\n{dataset_label}", fontsize=14, pad=12)
    ax.set_xlabel("Processos")
    ax.set_ylabel("Tempo paralelo (s)")
    ax.set_xticks(processes)
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    for program in ("mpi_arvore", "mpi_balanceado"):
        group = grouped.get(program, [])
        xs = [row["processos"] for row in group]
        ys = [row["tempo_par"] for row in group]
        ax.plot(xs, ys, marker="o", linewidth=2.2, markersize=5.5,
                color=PROGRAM_COLORS[program], label=PROGRAM_LABELS[program])

    ax.legend(frameon=True, loc="best")
    fig.tight_layout()

    png_path = dataset_dir / "comparacao_tempo.png"
    svg_path = dataset_dir / "comparacao_tempo.svg"
    fig.savefig(png_path, bbox_inches="tight")
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    return png_path, svg_path


def make_balance_gain_chart(dataset_id, dataset_label, rows):
    grouped = grouped_by_program(rows)
    fixed = {row["processos"]: row for row in grouped.get("mpi_arvore", [])}
    balanced = {row["processos"]: row for row in grouped.get("mpi_balanceado", [])}
    processes = sorted(set(fixed) & set(balanced))
    gains = [fixed[p]["tempo_par"] / balanced[p]["tempo_par"] for p in processes]

    dataset_dir = CHARTS_DIR / dataset_id
    dataset_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(8.6, 5.0), dpi=180)
    ax.set_title(f"Ganho do balanceamento\n{dataset_label}", fontsize=14, pad=12)
    ax.set_xlabel("Processos")
    ax.set_ylabel("Tempo arvore fixa / tempo balanceada")
    ax.set_xticks(processes)
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.axhline(1.0, linestyle="--", color="#6b7280", linewidth=1.8, label="Sem ganho")
    ax.plot(processes, gains, marker="o", linewidth=2.2, markersize=5.5,
            color="#16a34a", label="Ganho")
    ax.set_ylim(bottom=0)

    for p, gain in zip(processes, gains):
        if p in (1, 15, 31):
            ax.annotate(f"{gain:.2f}x", (p, gain), xytext=(0, 8),
                        textcoords="offset points", ha="center", fontsize=8)

    ax.legend(frameon=True, loc="best")
    fig.tight_layout()

    png_path = dataset_dir / "ganho_balanceamento.png"
    svg_path = dataset_dir / "ganho_balanceamento.svg"
    fig.savefig(png_path, bbox_inches="tight")
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    return png_path, svg_path


def make_cross_dataset_chart(datasets, metric, metric_label):
    out_dir = CHARTS_DIR / "comparativos"
    out_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(9.4, 5.2), dpi=180)
    ax.set_title(f"{metric_label} por tamanho do vetor", fontsize=14, pad=12)
    ax.set_xlabel("Processos")
    ax.set_ylabel(metric_label)
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    markers = {"mpi_arvore": "o", "mpi_balanceado": "s"}
    palette = ["#2563eb", "#dc2626", "#16a34a", "#9333ea", "#f97316", "#0891b2"]
    all_processes = set()

    color_idx = 0
    for idx, (dataset_id, dataset_label, rows) in enumerate(datasets):
        grouped = grouped_by_program(rows)
        for program in ("mpi_arvore", "mpi_balanceado"):
            group = grouped.get(program, [])
            if not group:
                continue
            xs = [row["processos"] for row in group]
            ys = [row[metric] for row in group]
            all_processes.update(xs)
            color = palette[color_idx % len(palette)]
            color_idx += 1
            ax.plot(
                xs,
                ys,
                marker=markers[program],
                linestyle="-",
                linewidth=2.0,
                markersize=5.2,
                color=color,
                label=f"{PROGRAM_LABELS[program]} - {dataset_label}",
            )

    if all_processes:
        ax.set_xticks(sorted(all_processes))
    if metric == "desbalanceamento":
        ax.set_ylim(0, 1.05)
    else:
        ax.set_ylim(bottom=0)

    ax.legend(frameon=True, fontsize=8, loc="best")
    fig.tight_layout()

    png_path = out_dir / f"{metric}_por_tamanho.png"
    svg_path = out_dir / f"{metric}_por_tamanho.svg"
    fig.savefig(png_path, bbox_inches="tight")
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    return png_path, svg_path


def make_cross_balance_gain_chart(datasets):
    out_dir = CHARTS_DIR / "comparativos"
    out_dir.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(9.4, 5.2), dpi=180)
    ax.set_title("Ganho do balanceamento por tamanho do vetor", fontsize=14, pad=12)
    ax.set_xlabel("Processos")
    ax.set_ylabel("Tempo arvore fixa / tempo balanceada")
    ax.grid(True, axis="y", alpha=0.25)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.axhline(1.0, linestyle="--", color="#6b7280", linewidth=1.8, label="Sem ganho")

    palette = ["#2563eb", "#dc2626", "#16a34a", "#9333ea", "#f97316"]
    all_processes = set()

    for idx, (_dataset_id, dataset_label, rows) in enumerate(datasets):
        grouped = grouped_by_program(rows)
        fixed = {row["processos"]: row for row in grouped.get("mpi_arvore", [])}
        balanced = {row["processos"]: row for row in grouped.get("mpi_balanceado", [])}
        processes = sorted(set(fixed) & set(balanced))
        gains = [fixed[p]["tempo_par"] / balanced[p]["tempo_par"] for p in processes]
        all_processes.update(processes)
        ax.plot(processes, gains, marker="o", linewidth=2.2, markersize=5.5,
                color=palette[idx % len(palette)], label=dataset_label)

    if all_processes:
        ax.set_xticks(sorted(all_processes))
    ax.set_ylim(bottom=0)
    ax.legend(frameon=True, loc="best")
    fig.tight_layout()

    png_path = out_dir / "ganho_balanceamento_por_tamanho.png"
    svg_path = out_dir / "ganho_balanceamento_por_tamanho.svg"
    fig.savefig(png_path, bbox_inches="tight")
    fig.savefig(svg_path, bbox_inches="tight")
    plt.close(fig)
    return png_path, svg_path


def main():
    CHARTS_DIR.mkdir(parents=True, exist_ok=True)
    csv_files = sorted(RESULTS_DIR.glob("tabela_*.csv"))
    if not csv_files:
        raise SystemExit(f"Nenhum CSV encontrado em {RESULTS_DIR}")

    generated = []
    datasets = []
    for csv_path in csv_files:
        dataset_id, dataset_label = parse_dataset_name(csv_path)
        rows = read_rows(csv_path)
        datasets.append((dataset_id, dataset_label, rows))
        for metric, metric_label, ideal_line in METRICS:
            generated.extend(make_metric_chart(dataset_id, dataset_label, rows, metric, metric_label, ideal_line))
        generated.extend(make_time_comparison_chart(dataset_id, dataset_label, rows))
        generated.extend(make_balance_gain_chart(dataset_id, dataset_label, rows))

    if len(datasets) >= 2:
        for metric, metric_label, _ideal_line in METRICS:
            generated.extend(make_cross_dataset_chart(datasets, metric, metric_label))
        generated.extend(make_cross_balance_gain_chart(datasets))

    for path in generated:
        print(path)


if __name__ == "__main__":
    main()
