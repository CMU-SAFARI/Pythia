#!/usr/bin/env python3
"""Python port of the rollup.pl script.

Reads trace, experiment, and metric definitions, then scans per-trace/experiment
log files to summarize metrics. Emits CSV data mirroring the Perl implementation.
"""
from __future__ import annotations

import argparse
import math
import os
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Collection, Dict, List, Tuple


def trim(value: str) -> str:
    return value.strip()


def ensure_env(var_name: str) -> None:
    if var_name not in os.environ:
        sys.exit("$PYTHIA_HOME env variable is not defined.\nHave you sourced setvars.sh?\n")


def parse_trace_file(path: str) -> List[Dict[str, str]]:
    traces: List[Dict[str, str]] = []
    current: Dict[str, str] | None = None

    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")
            if not line:
                continue
            idx = line.find("=")
            if idx == -1:
                continue
            key = line[:idx]
            value = line[idx + 1 :]
            if key == "NAME" and current is not None:
                traces.append(current)
                current = {}
            if current is None:
                current = {}
            current[key] = value

    if current is not None:
        traces.append(current)

    return traces


def parse_exp_file(path: str) -> List[Dict[str, str]]:
    exps: List[Dict[str, str]] = []
    configs: Dict[str, str] = {}

    translation = str.maketrans("", "", "$()")

    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            tokens = line.split()
            if len(tokens) >= 2 and tokens[1] == "=":
                configs[tokens[0]] = " ".join(tokens[2:])
                continue
            exp_name = tokens[0]
            args: List[str] = []
            for token in tokens[1:]:
                if token.startswith("$"):
                    key = token.translate(translation)
                    if key not in configs:
                        raise RuntimeError(f"{key} is not defined before exp {exp_name}")
                    args.append(configs[key])
                else:
                    args.append(token)
            exps.append({"NAME": exp_name, "KNOBS": " ".join(args)})

    return exps


def parse_metric_file(path: str) -> List[Dict[str, str]]:
    metrics: List[Dict[str, str]] = []
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if ":" not in line:
                continue
            name, type_ = line.split(":", 1)
            metrics.append({"NAME": trim(name), "TYPE": trim(type_)})
    return metrics


def parse_log_file(path: str, ext: str, target_keys: Collection[str] | None = None) -> Dict[str, str]:
    records: Dict[str, str] = {}
    remaining = set(target_keys) if target_keys else None
    with open(path, "r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.rstrip("\n")
            if ext == "stats":
                if "=" not in line:
                    continue
                key, value = line.split("=", 1)
                key = trim(key)
                records[key] = trim(value)
            else:
                key, sep, value = line.partition(" ")
                if not sep or " " in value:
                    continue
                key = trim(key)
                records[key] = trim(value)
            if remaining is not None and key in remaining:
                remaining.remove(key)
                if not remaining:
                    break
    return records


def tokens_to_floats(raw_value: str, treat_empty_as_zero: bool) -> List[float]:
    values: List[float] = []
    for token in raw_value.split(","):
        stripped = token.strip()
        if stripped:
            values.append(float(stripped))
        elif treat_empty_as_zero:
            values.append(0.0)
    return values


def summarize_values(metric_type: str, raw_value: str) -> str:
    if metric_type == "array":
        return raw_value

    if metric_type == "nzmean":
        data = tokens_to_floats(raw_value, treat_empty_as_zero=False)
    else:
        data = tokens_to_floats(raw_value, treat_empty_as_zero=True)

    if not data:
        return "0"

    if metric_type == "sum":
        value = sum(data)
    elif metric_type == "mean":
        value = sum(data) / len(data)
    elif metric_type == "nzmean":
        value = sum(data) / len(data)
    elif metric_type == "min":
        value = min(data)
    elif metric_type == "max":
        value = max(data)
    elif metric_type == "standard_deviation":
        value = sample_standard_deviation(data)
    elif metric_type == "variance":
        value = sample_variance(data)
    else:
        raise RuntimeError("invalid summary type")

    return format_number(value)


def sample_variance(data: List[float]) -> float:
    n = len(data)
    if n <= 1:
        return 0.0
    mean_value = sum(data) / n
    return sum((x - mean_value) ** 2 for x in data) / (n - 1)


def sample_standard_deviation(data: List[float]) -> float:
    return math.sqrt(sample_variance(data))


def format_number(value: float) -> str:
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return repr(value)


def main() -> None:
    parser = argparse.ArgumentParser(description="Summarize experiment metrics across traces.")
    parser.add_argument("--tlist", required=True, help="Trace list file")
    parser.add_argument("--exp", required=True, help="Experiment definition file")
    parser.add_argument("--mfile", required=True, help="Metric definition file")
    parser.add_argument("--ext", default="out", help="Log file extension (default: out)")
    parser.add_argument("-t", "--threads", type=int, default=1,
                        help="Number of worker threads to use (default: 1)")
    args = parser.parse_args()

    ensure_env("PYTHIA_HOME")

    trace_info = parse_trace_file(args.tlist)
    exp_info = parse_exp_file(args.exp)
    metric_info = parse_metric_file(args.mfile)
    metric_defs: List[Tuple[str, str]] = [(metric["NAME"], metric["TYPE"]) for metric in metric_info]
    metric_names = frozenset(name for name, _ in metric_defs)

    print("Trace,Exp", end="")
    for name, _ in metric_defs:
        print(f",{name}", end="")
    print(",Filter")

    workers = args.threads if args.threads and args.threads > 0 else (os.cpu_count() or 1)

    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [
            executor.submit(
                compute_trace_rollup,
                idx,
                trace,
                exp_info,
                metric_defs,
                metric_names,
                args.ext,
            )
            for idx, trace in enumerate(trace_info)
        ]

        ordered_results = sorted((future.result() for future in futures), key=lambda item: item[0])

    for _, lines in ordered_results:
        for line in lines:
            print(line)


def compute_trace_rollup(
    trace_index: int,
    trace: Dict[str, str],
    exp_info: List[Dict[str, str]],
    metric_defs: List[Tuple[str, str]],
    metric_names: Collection[str],
    ext: str,
) -> tuple[int, List[str]]:
    trace_name = trace.get("NAME", "")
    per_trace_result: Dict[str, List[str]] = {}
    all_exps_passed = True

    for exp in exp_info:
        exp_name = exp.get("NAME", "")
        log_file = f"{trace_name}_{exp_name}.{ext}"
        metric_values: List[str] = []

        if os.path.exists(log_file):
            records = parse_log_file(log_file, ext, metric_names)
            for metric_name, metric_type in metric_defs:
                if metric_name in records:
                    value = summarize_values(metric_type, records[metric_name])
                else:
                    value = "0"
                    all_exps_passed = False
                metric_values.append(value)
        else:
            all_exps_passed = False
            metric_values = ["0"] * len(metric_defs)

        per_trace_result[exp_name] = metric_values

    flag = "1" if all_exps_passed else "0"
    lines = []
    for exp in exp_info:
        exp_name = exp.get("NAME", "")
        values = ",".join(per_trace_result[exp_name])
        lines.append(f"{trace_name},{exp_name},{values},{flag}")

    return trace_index, lines


if __name__ == "__main__":
    main()
