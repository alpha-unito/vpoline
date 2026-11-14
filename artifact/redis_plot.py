import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os

FILES: dict[str, str] = {
    "Baseline":          "baseline.csv",
    "Vpoline":           "vpoline.csv",
    "Syscall intercept": "syscall_intercept.csv",
    # "SUD":               "sud.csv",
    "STRACE":            "strace.csv",
}

TOOL_ORDER = list(FILES.keys())

HATCHES: dict[str, str] = {
    "Baseline":          "x",
    "Vpoline":           "//",
    "Syscall intercept": "\\\\",
    # "SUD":               "..",
    "STRACE":            "xx",
}

COLORS: dict[str, str] = {
    "Baseline":          "#888780",
    "Vpoline":           "#378ADD",
    "Syscall intercept": "#1D9E75",
    # "SUD":               "#BA7517",
    "STRACE":            "#A32D2D",
}

FONT_SIZE = 16

def main():
    print("Loading data...")
    rows = []
    
    # Load available CSVs from the current directory
    for label, filename in FILES.items():
        if not os.path.exists(filename):
            continue
        
        try:
            df = pd.read_csv(filename)
            df["tool"] = label
            
            # Ensure the first column containing the commands is uniformly named
            if "Command" in df.columns:
                df = df.rename(columns={"Command": "name"})
            else:
                df = df.rename(columns={df.columns[0]: "name"})
            
            rows.append(df)
            print(f"✓ Loaded {label} from {filename}")
        except Exception as e:
            print(f"⚠️  Error loading {filename}: {e}")
    
    if not rows:
        print("❌ No data could be loaded. Please check the CSV files.")
        return

    # Combine all datasets
    combined_df = pd.concat(rows, ignore_index=True)
    
    # Pivot to get both RPS (Average) and StdDev by command and tool
    rps_pivot = combined_df.pivot(index="name", columns="tool", values="Average")
    std_pivot = combined_df.pivot(index="name", columns="tool", values="StdDev")
    
    available_tools = [t for t in TOOL_ORDER if t in rps_pivot.columns]
    
    # Force the logical order of commands as outputted by the bash benchmark script
    exact_order = ["PING_INLINE", "SET", "GET", "INCR", "LPUSH", "LPOP", "XADD"]
    valid_order = [c for c in exact_order if c in rps_pivot.index] + [c for c in rps_pivot.index if c not in exact_order]
    
    rps_pivot = rps_pivot.loc[valid_order, available_tools]
    std_pivot = std_pivot.loc[valid_order, available_tools]
    
    # Initialize Plot
    fig, ax = plt.subplots(figsize=(12, 5))
    
    n_tools = len(available_tools)
    n_commands = len(rps_pivot)
    bar_width = 0.15
    x = np.arange(n_commands)
    
    for tool_idx, tool in enumerate(available_tools):
        # Calculate cluster offsets
        offset = (tool_idx - n_tools/2 + 0.5) * bar_width
        positions = x + offset
        
        y_values = rps_pivot[tool].values
        y_errors = std_pivot[tool].values
        
        ax.bar(
            positions,
            y_values,
            yerr=y_errors,
            width=bar_width,
            color=COLORS[tool],
            hatch=HATCHES[tool],
            alpha=0.9,
            edgecolor="white",
            linewidth=1,
            label=tool,
            capsize=5,
            error_kw=dict(ecolor='#333333', elinewidth=2, capthick=2)
        )
    
    # X-axis configuration
    ax.set_xticks(x)
    ax.set_xticklabels(rps_pivot.index, fontsize=FONT_SIZE, weight='bold')
    
    # Y-axis configuration
    ax.tick_params(axis='y', labelsize=FONT_SIZE)
    for label in ax.get_yticklabels():
        label.set_weight('bold')
    
    ax.grid(True, which="major", linestyle="--", linewidth=1.0,
            color="#333333", alpha=0.4, axis="y")
    
    # Add environment name on the right edge, rotated
    ax.text(0.97, 0.3, "Artifact Environment", transform=ax.transAxes, fontsize=FONT_SIZE, 
         rotation=270, weight='bold', bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    # Axis labels and Legend
    ax.set_ylabel('Requests per second', fontsize=FONT_SIZE, weight='bold', labelpad=10)
    ax.set_xlabel('Redis Command', fontsize=FONT_SIZE, weight='bold', labelpad=10)

    if n_tools > 0:
        legend = ax.legend(fontsize=FONT_SIZE, ncols=len(available_tools), loc="center", bbox_to_anchor=(0.5, 1.1))
        for text in legend.get_texts():
            text.set_weight('bold')
    
    plt.tight_layout()
    
    # Output
    comparison_path = "redis_rps.pdf"
    fig.savefig(comparison_path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    
    print(f"\n✓ Saved → {comparison_path}")

if __name__ == "__main__":
    main()
