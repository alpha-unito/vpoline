import json
import os
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

FONT_SIZE = 20

# Method label mapping for display
METHOD_LABELS = {
    "native exe": "Baseline",
    "vpoline exe (without init)": "Vpoline",
    "syscall_intercept exe (without init)": "Syscall intercept",
    "strace exe": "STRACE",
    "sud exe (without init)": "SUD",
}

# Color and Hatch mapping
COLORS = {
    "Baseline": "#888780",
    "Vpoline": "#378ADD",
    "Syscall intercept": "#1D9E75",
    "SUD": "#BA7517",
    "STRACE": "#A32D2D",
}

HATCHES = {
    "Baseline": "x",
    "Vpoline": "//",
    "Syscall intercept": "\\\\",
    "STRACE": "xx",
    "SUD": "..",
}

def load_json(filename):
    """Load benchmark data from a JSON file in the current directory."""
    if os.path.exists(filename):
        with open(filename, 'r') as f:
            return json.load(f)
    return {}

def main():
    print("Loading benchmark data...")
    dd_data = load_json("dd_benchmark_results.json")
    sha256_data = load_json("sha256sum_benchmark_results.json")

    if not dd_data and not sha256_data:
        print("Error: No benchmark JSON files found in the current directory.")
        return

    # Define desired column order
    desired_order = [
        'native exe',
        'vpoline exe (without init)',
        'strace exe',
        'syscall_intercept exe (without init)',
        'sud exe (without init)'
    ]

    # Extract and order available methods dynamically
    available_methods = set(list(dd_data.keys()) + list(sha256_data.keys()))
    cols = [c for c in desired_order if c in available_methods] + [c for c in available_methods if c not in desired_order]

    # Initialize subplots (13x9 as requested by original structure)
    fig, (ax_dd, ax_sha256) = plt.subplots(1, 2, figsize=(13, 9))
    
    n_methods = len(cols)
    bar_width = 0.12
    spacing_multiplier = 1.15
    
    def plot_subplot(ax, data, title, show_y_right=False):
        """Helper function to plot a single subplot (dd or sha256)."""
        if not data:
            return
        
        for method_idx, method in enumerate(cols):
            if method not in data:
                continue
                
            # Center the cluster of bars at x=0
            offset = (method_idx - n_methods / 2) * bar_width * spacing_multiplier + bar_width * spacing_multiplier / 2
            display_label = METHOD_LABELS.get(method, method)
            
            mean_val = data[method]['mean']
            std_val = data[method]['stddev']
            
            ax.bar(
                offset,
                mean_val,
                yerr=std_val,
                width=bar_width,
                color=COLORS.get(display_label, "#CCCCCC"),
                hatch=HATCHES.get(display_label, ""),
                alpha=0.9,
                edgecolor="white",
                linewidth=0.7,
                capsize=5,
                error_kw=dict(ecolor='#333333', elinewidth=1.5, capthick=1.5)
            )
        
        # X-axis configuration
        ax.set_xticks([0])
        ax.set_xticklabels(["Artifact\nEnvironment"], fontsize=FONT_SIZE, fontweight='bold')
        
        # Y-axis configuration
        ax.set_ylabel('Time (ms)', fontsize=FONT_SIZE, fontweight='bold', color='black')
        ax.set_title(title, fontsize=FONT_SIZE, fontweight='bold', pad=20)
        ax.tick_params(axis='y', labelsize=FONT_SIZE, labelcolor='black')
        ax.set_yscale('log')
        ax.grid(True, which='major', linestyle='--', linewidth=1.0, color='#333333', alpha=0.4, axis='y')
        
        # Spine alignment based on subplot position
        if show_y_right:
            ax.spines['left'].set_visible(False)
            ax.yaxis.set_label_position("right")
            ax.yaxis.tick_right()
        else:
            ax.spines['right'].set_visible(False)

    # Plot both benchmarks
    plot_subplot(ax_dd, dd_data, 'dd Benchmark Results', show_y_right=False)
    plot_subplot(ax_sha256, sha256_data, 'sha256sum Benchmark Results', show_y_right=True)
    
    # Custom Legend mapping
    method_handles = []
    for method in cols:
        display_label = METHOD_LABELS.get(method, method)
        patch = Patch(
            facecolor=COLORS.get(display_label, "#CCCCCC"),
            hatch=HATCHES.get(display_label, ""),
            edgecolor='white',
            linewidth=0.7,
            label=display_label
        )
        method_handles.append(patch)
    
    fig.legend(
        handles=method_handles,
        fontsize=FONT_SIZE,
        ncol=5,
        loc='lower center',
        frameon=True,
        fancybox=True,
        shadow=True,
    )

    plt.tight_layout(rect=[0, 0.08, 1, 0.96])
    
    output_file = 'benchmark_dd_sha256.pdf'
    plt.savefig(output_file, format='pdf', bbox_inches='tight', dpi=300)
    plt.close()
    
    print(f"Done! Plot generated and saved as: {output_file}")

if __name__ == "__main__":
    main()
