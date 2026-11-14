import json
from pathlib import Path

import matplotlib.pyplot as plt
plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42
import numpy as np
import pandas as pd

# Always load JSON files from and save the output PDF to the script's directory
SCRIPT_DIR = Path(__file__).resolve().parent

FILES: dict[str, str] = {
    "Baseline":          "baseline_syscall_latency.json",
    "Vpoline":           "vpoline_syscall_latency.json",
    "KPROBES":           "kprobes_syscall_latency.json",
    "Syscall intercept": "gekko_syscall_latency.json",
    "SUD":               "sud_syscall_latency.json",
    "STRACE":            "strace_syscall_latency.json",
}

TOOL_ORDER = list(FILES.keys())

HATCHES: dict[str, str] = {
    "Baseline":          "x",
    "Vpoline":           "//",
    "Syscall intercept": "\\\\",
    "SUD":               "..",
    "STRACE":            "xx",
    "KPROBES":           ".",
}

COLORS: dict[str, str] = {
    "Baseline":          "#888780",
    "Vpoline":           "#378ADD",
    "KPROBES":           "#A47DAB",
    "Syscall intercept": "#1D9E75",
    "SUD":               "#BA7517",
    "STRACE":            "#A32D2D",
}

BAR_WIDTH = 0.15
FIGURE_SIZE = (14, 7)
FONT_SIZE = 20


def load_data(data_dir: Path) -> pd.DataFrame:
    """Load all tool JSON data from the script directory."""
    rows = []

    for label, filename in FILES.items():
        file_path = data_dir / filename

        # Handle filename variations (e.g., "kprobe" vs "kprobes", "sycall" vs "syscall")
        if not file_path.exists():
            alternatives = [
                data_dir / filename.replace("kprobes_", "kprobe_"),
                data_dir / filename.replace("kprobe_", "kprobes_"),
                data_dir / filename.replace("syscall", "sycall"),
                data_dir / filename.replace("sycall", "syscall"),
                data_dir / filename.replace("gekko_", "gekkko_"),
                data_dir / filename.replace("gekko_", "syscall_intercept_"),
                ]
            for alt in alternatives:
                if alt.exists():
                    file_path = alt
                    break

        if not file_path.exists():
            print(f"⚠️  Warning: {file_path.name} not found for {label}")
            continue

        try:
            content = json.loads(file_path.read_text())
            for entry in content:
                rows.append({**entry, "tool": label})
            print(f"✓ Loaded {file_path.name} ({len(content)} entries)")
        except Exception as e:
            print(f"⚠️  Error loading {file_path.name}: {e}")

    return pd.DataFrame(rows)


def build_pivots(df: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Build pivot tables for averages and standard deviations."""
    avg_pivot = df.pivot(index="name", columns="tool", values="avg_ns")
    std_pivot = df.pivot(index="name", columns="tool", values="stddev_ns")

    # Only keep tools that are present in this dataset
    available_tools = [t for t in TOOL_ORDER if t in avg_pivot.columns]
    if "Baseline" in avg_pivot.columns:
        avg_pivot = avg_pivot[available_tools].sort_values(by="Baseline")
    else:
        avg_pivot = avg_pivot[available_tools]
    std_pivot = std_pivot[available_tools].loc[avg_pivot.index]

    return avg_pivot, std_pivot


def log_error_bars(values, errors):
    """Convert linear error bars to logarithmic scale."""
    values = np.asarray(values, dtype=float)
    errors = np.asarray(errors, dtype=float)

    upper = np.log10(values + errors) - np.log10(values)
    lower = np.log10(values) - np.log10(np.maximum(values - errors, 1e-10))

    return lower, upper


def main():
    print(f"Loading data from directory: {SCRIPT_DIR}")
    df = load_data(SCRIPT_DIR)

    if df.empty:
        print("❌ No data could be loaded. Make sure the JSON files are in the same directory as this script.")
        return

    print("\nGenerating comparison plot...")
    avg, std = build_pivots(df)

    syscall_names = list(avg.index)
    x = np.arange(len(syscall_names))
    available_tools = [t for t in TOOL_ORDER if t in avg.columns]
    n_tools = len(available_tools)

    fig, ax = plt.subplots(1, 1, figsize=FIGURE_SIZE)

    for tool_idx, tool in enumerate(available_tools):
        offset = (tool_idx - n_tools / 2 + 0.5) * BAR_WIDTH
        positions = x + offset

        tool_values = avg[tool].values
        tool_errors = std[tool].values
        lower_err, upper_err = log_error_bars(tool_values, tool_errors)

        ax.bar(
            positions,
            tool_values,
            width=BAR_WIDTH,
            color=COLORS[tool],
            hatch=HATCHES[tool],
            alpha=0.9,
            edgecolor="white",
            linewidth=0.5,
            label=tool,
            yerr=[lower_err, upper_err],
            error_kw=dict(ecolor="black", linewidth=1, capsize=2, alpha=0.7),
        )

    ax.set_yscale("log")
    ax.grid(
        True, which="major", linestyle="--", linewidth=1.0,
        color="#333333", alpha=0.4, axis="y"
    )

    ax.set_xticks(x)
    ax.set_xticklabels(syscall_names, fontsize=FONT_SIZE, rotation=45, ha="right", weight="bold")

    ax.tick_params(axis="y", labelsize=FONT_SIZE)
    for label in ax.get_yticklabels():
        label.set_weight("bold")

    if n_tools > 0:
        legend = ax.legend(fontsize=FONT_SIZE, ncols=3, loc="upper center", bbox_to_anchor=(0.5, 1.25))
        for text in legend.get_texts():
            text.set_weight("bold")

    fig.text(-0.01, 0.5, "Nanoseconds", va="center", rotation="vertical", fontsize=FONT_SIZE, weight="bold")

    plt.subplots_adjust(left=0.06, right=0.85, top=0.95, bottom=0.1)
    comparison_path = SCRIPT_DIR / "syscall_latency.pdf"
    fig.savefig(comparison_path, dpi=300, bbox_inches="tight")
    plt.close(fig)

    print(f"✓ Saved → {comparison_path}")
    print("\n✅ Plot generated successfully!")


if __name__ == "__main__":
    main()