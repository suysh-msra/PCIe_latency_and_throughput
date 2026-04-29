#!/usr/bin/env python3

from __future__ import annotations

import itertools
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
SIM_BIN = BUILD / "pcie_sim"


def build() -> None:
    BUILD.mkdir(exist_ok=True)
    cmd = [
        "g++",
        "-std=c++20",
        "-O2",
        "-Wall",
        "-Wextra",
        "-pedantic",
        str(ROOT / "src" / "pcie_sim.cpp"),
        "-o",
        str(SIM_BIN),
    ]
    subprocess.run(cmd, check=True)


def run_case(**kwargs: int | float) -> dict[str, float]:
    cmd = [str(SIM_BIN)]
    for key, value in kwargs.items():
        cmd.extend([f"--{key.replace('_', '-')}", str(value)])

    out = subprocess.run(cmd, check=True, text=True, capture_output=True).stdout.strip().splitlines()
    result_line = next(line for line in out if line.startswith("results"))
    metrics: dict[str, float] = {}
    for token in result_line.split()[1:]:
      name, value = token.split("=")
      metrics[name] = float(value)
    return metrics


def format_table(rows: list[dict[str, str]]) -> str:
    headers = list(rows[0].keys())
    widths = {header: max(len(header), *(len(row[header]) for row in rows)) for header in headers}
    lines = []
    lines.append(" | ".join(header.ljust(widths[header]) for header in headers))
    lines.append("-+-".join("-" * widths[header] for header in headers))
    for row in rows:
        lines.append(" | ".join(row[header].ljust(widths[header]) for header in headers))
    return "\n".join(lines)


def sweep_payload() -> list[dict[str, str]]:
    rows = []
    for payload in [16, 32, 64, 128, 256, 512, 1024]:
        metrics = run_case(transactions=1000, payload=payload, queue_depth=8, link_gbps=8, endpoint_ns=120)
        rows.append(
            {
                "payload_B": str(payload),
                "avg_lat_ns": f"{metrics['avg_latency_ns']:.1f}",
                "p95_ns": f"{metrics['p95_latency_ns']:.1f}",
                "avg_queue_ns": f"{metrics['avg_queue_ns']:.1f}",
                "throughput_gbps": f"{metrics['throughput_gbps']:.2f}",
            }
        )
    return rows


def sweep_queue_depth() -> list[dict[str, str]]:
    rows = []
    for depth in [1, 2, 4, 8, 16, 32]:
        metrics = run_case(transactions=1000, payload=256, queue_depth=depth, link_gbps=8, endpoint_ns=120)
        rows.append(
            {
                "queue_depth": str(depth),
                "avg_lat_ns": f"{metrics['avg_latency_ns']:.1f}",
                "p95_ns": f"{metrics['p95_latency_ns']:.1f}",
                "avg_queue_ns": f"{metrics['avg_queue_ns']:.1f}",
                "throughput_gbps": f"{metrics['throughput_gbps']:.2f}",
            }
        )
    return rows


def sweep_bottleneck() -> list[dict[str, str]]:
    rows = []
    for endpoint_ns, link_gbps in itertools.product([40, 80, 120, 200], [4, 8, 16]):
        metrics = run_case(
            transactions=1000,
            payload=256,
            queue_depth=8,
            link_gbps=link_gbps,
            endpoint_ns=endpoint_ns,
        )
        bottleneck = "endpoint" if endpoint_ns > metrics["avg_serialize_ns"] else "link"
        rows.append(
            {
                "endpoint_ns": str(endpoint_ns),
                "link_gbps": str(link_gbps),
                "avg_lat_ns": f"{metrics['avg_latency_ns']:.1f}",
                "throughput_gbps": f"{metrics['throughput_gbps']:.2f}",
                "dominant_term": bottleneck,
            }
        )
    return rows


def main() -> None:
    build()
    print("Payload sweep")
    print(format_table(sweep_payload()))
    print()
    print("Queue depth sweep")
    print(format_table(sweep_queue_depth()))
    print()
    print("Bottleneck sweep")
    print(format_table(sweep_bottleneck()))


if __name__ == "__main__":
    main()
