import os
import pandas as pd
import matplotlib.pyplot as plt

# Explicit file mapping to ensure exact match regardless of minor filename differences
FILE_MAPPING = {
    "baseline.csv": "Baseline",
    "vpoline_mem.csv": "Vpoline",
    "syscall_mem.csv": "Syscall Intercept"
}

PALETTES = {
    "Baseline": "#5d5d58",            
    "Vpoline": "#1f6fbf",            
    "Syscall Intercept": "#127a56"     
}

MARKERS = {
    "Baseline": 'o',
    "Vpoline": 'x',
    "Syscall Intercept": "v"
}

LABELS = {
    "Baseline": "Baseline",
    "Vpoline": 'CAPIO (Vpoline)',
    "Syscall Intercept": "CAPIO (Syscall Intercept)"
}

LINESTYLES = ['-', '--', ':', '-.', '-', '--']

TITLE_FS = 10
LABEL_FS = 10
TICK_FS = 10
LEGEND_FS = 10

def main():
    plt.rcParams.update({'font.size': TICK_FS})
    
    cwd = os.getcwd()
    print(f"Loading data from current directory: {cwd}")
    
    # Pre-pass to get all sizes for consistent X-ticks across the plot
    all_sizes = set()
    loaded_files = []
    
    for filename in FILE_MAPPING.keys():
        filepath = os.path.join(cwd, filename)
        if os.path.exists(filepath):
            df = pd.read_csv(filepath)
            if 'size_mb' in df.columns:
                all_sizes.update(df['size_mb'].tolist())
                loaded_files.append(filename)
            
    if not loaded_files:
        print("Error: No valid CAPIO CSV files found in the current directory.")
        return
        
    all_sizes = sorted(list(all_sizes))
    
    # Setup single plot
    fig, ax = plt.subplots(figsize=(7, 4.5), constrained_layout=True)
    
    handles_list = []
    labels_list = []
    
    for i, filename in enumerate(FILE_MAPPING.keys()):
        filepath = os.path.join(cwd, filename)
        if not os.path.exists(filepath):
            continue
            
        df = pd.read_csv(filepath)
        df = df.sort_values('size_mb')
        
        key = FILE_MAPPING[filename]
        marker = MARKERS[key]
        linestyle = LINESTYLES[i % len(LINESTYLES)]
        
        y_vals = df['avg_total_s']
        y_err = df['stddev_s'] if 'stddev_s' in df.columns else None
        
        line = ax.errorbar(
            df['size_mb'], y_vals,
            yerr=y_err,
            label=LABELS[key],
            color=PALETTES[key], marker=marker, markersize=5,
            markeredgewidth=1.2, linewidth=1.5, linestyle=linestyle,
            capsize=3, elinewidth=1.0
        )
        
        handles_list.append(line)
        labels_list.append(LABELS[key])
        print(f"✓ Loaded {filename}")

    # Scale styling
    ax.set_xscale('log', base=2)
    ax.set_yscale('log')
    ax.grid(True, which='major', axis='y', linestyle='--', linewidth=0.6,
            color='#555555', alpha=0.55)
    
    # X-axis ticks configuration
    ax.set_xticks(all_sizes)
    ax.set_xticklabels(
        [str(int(s)) if s == int(s) else str(s) for s in all_sizes],
        rotation=45, fontsize=TICK_FS
    )
    ax.tick_params(axis='both', labelsize=TICK_FS, length=3, width=0.8)
    for spine in ax.spines.values():
        spine.set_linewidth(0.8)

    # Subplot environment title inside the plot area (top-left corner)
    ax.text(0.03, 0.96, "Artifact Environment", transform=ax.transAxes,
            fontsize=TITLE_FS, fontweight='bold',
            verticalalignment='top', horizontalalignment='left',
            bbox=dict(boxstyle='round,pad=0.28', facecolor='white',
                      edgecolor='0.3', linewidth=0.6, alpha=0.92))

    # Give the shared log y-axis a little headroom above the tallest curve
    ymin, ymax = ax.get_ylim()
    ax.set_ylim(ymin, ymax * 1.5)

    # Legend positioned outside the axes area
    legend = ax.legend(
        handles_list, labels_list, loc='upper center', bbox_to_anchor=(0.5, 1.18),
        ncols=3, fontsize=LEGEND_FS,
        frameon=True, fancybox=True, handlelength=2.5, columnspacing=1.2
    )
    legend.get_frame().set_linewidth(0.8)
    
    # Axis labels
    ax.set_ylabel('Time (s)', fontweight='bold', fontsize=LABEL_FS)
    ax.set_xlabel('File Size (MB)', fontweight='bold', fontsize=LABEL_FS)

    # Save the figure
    output_filename = "capio_benchmarks.pdf"
    plt.savefig(output_filename, bbox_inches='tight', dpi=300)
    plt.close(fig)
    
    print(f"\nPlot generated successfully! Saved to: {output_filename}")

if __name__ == "__main__":
    main()
