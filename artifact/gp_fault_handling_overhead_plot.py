import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter
import os

FONT_SIZE = 18

def main():
    intercepted_file = 'vpoline.csv'
    baseline_file = 'baseline.csv'

    if not os.path.exists(intercepted_file) or not os.path.exists(baseline_file):
        print(f"Error: Missing files. Ensure {baseline_file} and {intercepted_file} are in the current directory.")
        return

    print("Loading and processing data...")
    
    # Dynamic loading, tolerant to different separators
    df_intercepted = pd.read_csv(intercepted_file, sep=None, engine='python')
    df_not_intercepted = pd.read_csv(baseline_file, sep=None, engine='python')

    # Force standard column names based on position (0 to 4)
    # to bypass any header discrepancies (e.g., 'Insn' vs 'Instruction')
    standard_cols = ['Instruction', 'Slow_Path', 'sp_stddev', 'Fast_Path', 'fp_stddev']
    df_intercepted.columns = standard_cols
    df_not_intercepted.columns = standard_cols

    # Merge datasets using 'Instruction' as the key
    df = df_intercepted.merge(
        df_not_intercepted,
        on='Instruction',
        suffixes=('_intercepted', '_not_intercepted')
    )

    # Calculate overhead (Fast Path)
    df['Overhead'] = df['Fast_Path_intercepted'] - df['Fast_Path_not_intercepted']
    df = df.sort_values(by='Fast_Path_intercepted', ascending=True)

    # Calculate bar widths in logarithmic scale (log10)
    log_baseline = np.log10(df['Fast_Path_not_intercepted'])
    log_overhead = np.log10(df['Fast_Path_not_intercepted'] + df['Overhead']) - log_baseline

    # Calculate Slow Path width
    # (Baseline Slow_Path_not_intercepted is ignored as requested)
    slow_tip = df['Fast_Path_not_intercepted'] + df['Overhead'] + df['Slow_Path_intercepted']
    log_slowpath = np.log10(slow_tip) - log_baseline - log_overhead

    # Error bars in logarithmic scale
    xerr_slowpath_log = np.log10(slow_tip + df['sp_stddev_intercepted']) - np.log10(slow_tip)
    fast_tip = df['Fast_Path_not_intercepted'] + df['Overhead']
    xerr_fastpath_log = np.log10(fast_tip + df['fp_stddev_intercepted']) - np.log10(fast_tip)

    # Initialize Single Plot
    fig, ax = plt.subplots(figsize=(12, 6))

    # Baseline bar (Blue)
    ax.barh(df['Instruction'], log_baseline,
            label='Baseline',
            color='#294669', hatch='//', edgecolor='white', linewidth=1.2)

    # JIT overhead bar (Orange)
    ax.barh(df['Instruction'], log_overhead, left=log_baseline,
            label='JIT Interception Overhead',
            color='#F69D39', hatch='\\\\', edgecolor='white', linewidth=1.2)

    # SIGSEGV / Slow path bar (Red)
    ax.barh(df['Instruction'], log_slowpath, left=log_baseline + log_overhead,
            label='SIGSEGV interception Overhead',
            color='#E63946', hatch='xx', edgecolor='white', linewidth=1.2)

    # Error bars Fast Path (Orange)
    ax.errorbar(
        log_baseline + log_overhead, np.arange(len(df)), xerr=xerr_fastpath_log,
        fmt='none', ecolor='black', elinewidth=2, capsize=5, capthick=2, alpha=0.7, zorder=10
    )

    # Error bars Slow Path (Red)
    ax.errorbar(
        log_baseline + log_overhead + log_slowpath, np.arange(len(df)), xerr=xerr_slowpath_log,
        fmt='none', ecolor='black', elinewidth=2, capsize=5, capthick=2, alpha=0.7, zorder=10
    )

    # Y-axis Ticks and Labels styling
    ax.tick_params(axis='y', labelsize=FONT_SIZE)
    for label in ax.get_yticklabels():
        label.set_weight('bold')

    # X-axis Ticks, Labels and Grid styling
    ax.grid(axis='x', which='major', linestyle='--', linewidth=1.0, color='#333333', alpha=0.55)
    ax.tick_params(axis='x', labelsize=FONT_SIZE)

    def log_to_actual(x, pos):
        return f'{10**x:.0f}'
    
    ax.xaxis.set_major_formatter(FuncFormatter(log_to_actual))
    ax.set_xlabel('Execution Time (ns)', fontweight='bold', fontsize=FONT_SIZE)
    ax.set_ylabel('Instruction', fontweight='bold', fontsize=FONT_SIZE)

    # Legend positioned above the plot
    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, fontsize=FONT_SIZE, ncols=3,
               loc='upper center', bbox_to_anchor=(0.5, 1.10),
               frameon=True, borderaxespad=0)

    # Save the figure
    plt.tight_layout()
    output_file = 'interception_overhead_GP.pdf'
    plt.savefig(output_file, format='pdf', dpi=300, bbox_inches='tight')
    plt.close()

    print(f"Done! Plot generated and saved as: {output_file}")

if __name__ == "__main__":
    main()
