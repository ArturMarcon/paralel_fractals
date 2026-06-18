#!/usr/bin/env python3
import csv
import statistics
import sys


def parse_line(line):
    fields = {}
    for part in line.strip().split():
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key] = value
    return fields


def median(values):
    return statistics.median(values) if values else None


def sequential_time_from_log(path):
    values = []
    with open(path, "r", encoding="utf-8") as source:
        for line in source:
            fields = parse_line(line)
            if fields.get("programa") == "sequencial":
                values.append(float(fields["tempo"]))
    if not values:
        raise SystemExit(f"Nenhuma linha sequencial encontrada em {path}")
    return median(values)


def main():
    if len(sys.argv) != 4:
        print("Uso: analyze_extra_results.py log_sequencial.log log_extra.log saida.csv", file=sys.stderr)
        return 1

    seq_time = sequential_time_from_log(sys.argv[1])
    parallel = {}

    with open(sys.argv[2], "r", encoding="utf-8") as source:
        for line in source:
            fields = parse_line(line)
            program = fields.get("programa")
            if program in ("mpi_arvore", "mpi_balanceado"):
                key = (program, int(fields["processos"]))
                parallel.setdefault(key, []).append(fields)

    rows = []
    for (program, processes), entries in sorted(parallel.items()):
        times = [float(entry["tempo"]) for entry in entries]
        imbalances = [float(entry.get("desbalanceamento", "0")) for entry in entries]
        segments = [int(entry.get("segmentos", "0")) for entry in entries]
        actives = [int(entry.get("processos_ativos", "0")) for entry in entries]
        par_time = median(times)
        speedup = seq_time / par_time
        efficiency = speedup / processes
        rows.append({
            "programa": program,
            "processos": processes,
            "tempo_seq": f"{seq_time:.6f}",
            "tempo_par": f"{par_time:.6f}",
            "speedup": f"{speedup:.6f}",
            "eficiencia": f"{efficiency:.6f}",
            "desbalanceamento": f"{median(imbalances):.6f}",
            "segmentos": int(median(segments)),
            "processos_ativos": int(median(actives)),
        })

    with open(sys.argv[3], "w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=[
            "programa", "processos", "tempo_seq", "tempo_par",
            "speedup", "eficiencia", "desbalanceamento", "segmentos",
            "processos_ativos"
        ])
        writer.writeheader()
        writer.writerows(rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
