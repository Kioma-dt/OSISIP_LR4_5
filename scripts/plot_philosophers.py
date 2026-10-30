#!/usr/bin/env python3
"""Построение двух диаграмм по CSV экспериментов с обедающими философами."""

import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import mean

import matplotlib.pyplot as plt
from matplotlib.patches import Patch


REQUIRED_COLUMNS = {"kind", "run", "pid", "n", "eat_ms", "ops_s", "mean_wait_ms", "max_wait_ms"}


def read_results(path: Path):
    # Для каждой конфигурации храним отдельно результаты запусков (run, pid).
    groups = defaultdict(dict)

    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if reader.fieldnames is None:
            raise ValueError("CSV-файл пуст или не содержит заголовка")
        reader.fieldnames = [name.strip() for name in reader.fieldnames]

        missing = REQUIRED_COLUMNS - set(reader.fieldnames)
        if missing:
            raise ValueError(f"В CSV отсутствуют столбцы: {', '.join(sorted(missing))}")

        for line, row in enumerate(reader, start=2):
            if (row["kind"] or "").strip().upper() != "SUMMARY":
                continue

            try:
                n = int(row["n"])
                eat = int(row["eat_ms"])
                run_id = ((row["run"] or "").strip(), (row["pid"] or "").strip())
                result = {
                    "ops_s": float(row["ops_s"]),
                    "mean_wait_ms": float(row["mean_wait_ms"]),
                    "max_wait_ms": float(row["max_wait_ms"]),
                }
            except (ValueError, TypeError) as exc:
                raise ValueError(f"Некорректные числовые данные в строке {line}: {exc}") from exc

            key = (n, eat)
            if run_id in groups[key]:
                raise ValueError(
                    f"Повторный SUMMARY для n={n}, eat_ms={eat}, "
                    f"run={run_id[0]}, pid={run_id[1]} (строка {line})"
                )
            groups[key][run_id] = result

    if not groups:
        raise ValueError("Не найдено ни одной строки kind=SUMMARY")

    # Агрегируем независимые запуски каждой конфигурации.
    stats = []
    for (n, eat), runs in sorted(groups.items()):
        values = list(runs.values())
        if len(values) != 3:
            print(f"Внимание: n={n}, eat_ms={eat}: найдено {len(values)} запусков вместо 3")
        stats.append({
            "n": n,
            "eat": eat,
            "count": len(values),
            "ops": mean(v["ops_s"] for v in values),
            "mean_wait": mean(v["mean_wait_ms"] for v in values),
            "max_wait": max(v["max_wait_ms"] for v in values),
        })

    return stats


def make_positions(stats):
    # Для второй диаграммы: между различными конфигурациями одинаковый шаг.
    return list(range(len(stats)))


def format_wait_axes(ax, stats, positions):
    ax.set_xticks(positions)
    ax.set_xticklabels([f'n={s["n"]}\neat={s["eat"]} мс' for s in stats])
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.set_axisbelow(True)
    ax.set_ylim(bottom=0)


def plot_throughput(stats, output: Path):
    # Группируем по количеству философов n.
    by_n = defaultdict(dict)
    eat_values = sorted({s["eat"] for s in stats})
    n_values = sorted({s["n"] for s in stats})
    for s in stats:
        by_n[s["n"]][s["eat"]] = s["ops"]

    palette = plt.get_cmap("tab10")
    color_map = {eat: palette(i % 10) for i, eat in enumerate(eat_values)}

    group_width = 0.82
    slots = max(1, len(eat_values))
    bar_width = min(0.22, group_width / slots)
    spacing = bar_width * 0.15

    fig_width = max(9, 1.8 * len(n_values))
    fig, ax = plt.subplots(figsize=(fig_width, 6))

    group_centers = list(range(len(n_values)))

    for eat_idx, eat in enumerate(eat_values):
        offsets = []
        if slots == 1:
            offsets = [0.0] * len(group_centers)
        else:
            total_span = slots * bar_width + (slots - 1) * spacing
            start = -total_span / 2 + bar_width / 2
            offset = start + eat_idx * (bar_width + spacing)
            offsets = [offset] * len(group_centers)

        xs = [group_centers[i] + offsets[i] for i in range(len(group_centers))]
        heights = [by_n[n].get(eat) for n in n_values]

        valid_xs = [x for x, h in zip(xs, heights) if h is not None]
        valid_heights = [h for h in heights if h is not None]
        if not valid_xs:
            continue

        bars = ax.bar(valid_xs, valid_heights, width=bar_width, color=color_map[eat], label=f"eat={eat} мс")
        ax.bar_label(bars, fmt="%.2f", padding=3, fontsize=9)

    ax.set_title("Зависимость пропускной способности от числа философов")
    ax.set_xlabel("Количество философов (n)")
    ax.set_ylabel("Средняя пропускная способность, операций/с")
    ax.set_xticks(group_centers)
    ax.set_xticklabels([str(n) for n in n_values])
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    ax.set_axisbelow(True)
    ax.set_ylim(bottom=0)
    ax.legend(
        handles=[Patch(facecolor=color_map[eat], label=f"eat={eat} мс") for eat in eat_values],
        title="Время еды",
    )
    fig.tight_layout()
    fig.savefig(output, dpi=200, bbox_inches="tight")
    plt.close(fig)


def plot_wait_times(stats, positions, output: Path):
    fig, ax = plt.subplots(figsize=(max(10, 1.45 * len(stats)), 6))
    width = 0.36
    mean_bars = ax.bar(
        [x - width / 2 for x in positions],
        [s["mean_wait"] for s in stats],
        width,
        label="Среднее ожидание (среднее из 3 запусков)",
    )
    max_bars = ax.bar(
        [x + width / 2 for x in positions],
        [s["max_wait"] for s in stats],
        width,
        label="Максимальное ожидание (максимум из 3 запусков)",
    )
    ax.bar_label(mean_bars, fmt="%.2f", padding=3, fontsize=9)
    ax.bar_label(max_bars, fmt="%.2f", padding=3, fontsize=9)
    ax.set_title("Среднее и максимальное время ожидания по конфигурациям")
    ax.set_xlabel("Конфигурация")
    ax.set_ylabel("Время ожидания, мс")
    format_wait_axes(ax, stats, positions)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output, dpi=200, bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_file", type=Path, help="Путь к CSV-файлу с результатами")
    parser.add_argument("--out-dir", type=Path, help="Папка для двух PNG (по умолчанию рядом с CSV)")
    args = parser.parse_args()

    out_dir = args.out_dir if args.out_dir is not None else args.csv_file.resolve().parent
    out_dir.mkdir(parents=True, exist_ok=True)
    stats = read_results(args.csv_file)
    positions = make_positions(stats)

    throughput_file = out_dir / "throughput_vs_n.png"
    wait_file = out_dir / "wait_times_by_config.png"
    plot_throughput(stats, throughput_file)
    plot_wait_times(stats, positions, wait_file)
    print(f"Сохранено: {throughput_file}")
    print(f"Сохранено: {wait_file}")


if __name__ == "__main__":
    main()
