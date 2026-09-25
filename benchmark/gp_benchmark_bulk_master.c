#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NUM_RUNS 100
#define NUM_INSTRUCTIONS 15

struct instruction_stats {
    char name[32];
    double slow_paths[NUM_RUNS];
    double fast_means[NUM_RUNS];
};

void run_benchmark_set(const char *cmd, const char *out_csv_path, const char *label) {
    struct instruction_stats stats[NUM_INSTRUCTIONS];

    for(int i = 0; i < NUM_INSTRUCTIONS; i++) {
        stats[i].name[0] = '\0';
    }

    printf("\n==========================================================\n");
    printf("[*] Executing %s: %d benchmark runs...\n", label, NUM_RUNS);
    printf("[*] Command: %s\n", cmd);
    printf("==========================================================\n");

    for (int run = 0; run < NUM_RUNS; run++) {
        if (run > 0 && run % 500 == 0) {
            printf("    Completed %d/%d runs...\n", run, NUM_RUNS);
        }

        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
            perror("Failed to run benchmark");
            exit(EXIT_FAILURE);
        }

        char line[256];
        int instr_idx = 0;

        while (fgets(line, sizeof(line), fp) != NULL) {
            char insn[32];
            double slow, fast;

            if (sscanf(line, "%[^,],%lf,%lf", insn, &slow, &fast) == 3) {
                if (run == 0) {
                    strncpy(stats[instr_idx].name, insn, sizeof(stats[instr_idx].name)-1);
                }

                stats[instr_idx].slow_paths[run] = slow;
                stats[instr_idx].fast_means[run] = fast;
                instr_idx++;
            }
        }
        pclose(fp);
    }

    printf("[*] All runs finished. Computing stats for %s...\n\n", label);

    FILE *out = NULL;
    if (out_csv_path != NULL) {
        out = fopen(out_csv_path, "w");
        if (!out) {
            perror("Failed to open output CSV file");
        } else {
            fprintf(out, "Insn,Slow Path (ns),sp_stddev,Fast Path (ns),fp_stddev\n");
        }
    }

    printf("%-10s,%-20s,%-20s,%-20s,%-20s\n", "Insn","Slow Path (ns)","sp_stddev","Fast Path (ns)","fp_stddev");

    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (strlen(stats[i].name) == 0) continue;

        double sum_slow = 0.0, sum_fast = 0.0;

        for (int r = 0; r < NUM_RUNS; r++) {
            sum_slow += stats[i].slow_paths[r];
            sum_fast += stats[i].fast_means[r];
        }
        double mean_slow = sum_slow / NUM_RUNS;
        double mean_fast = sum_fast / NUM_RUNS;

        double var_slow = 0.0, var_fast = 0.0;
        for (int r = 0; r < NUM_RUNS; r++) {
            var_slow += pow(stats[i].slow_paths[r] - mean_slow, 2);
            var_fast += pow(stats[i].fast_means[r] - mean_fast, 2);
        }

        double stddev_slow = sqrt(var_slow / (NUM_RUNS - 1));
        double stddev_fast = sqrt(var_fast / (NUM_RUNS - 1));

        /* outputs on screen */
        printf("%s,%f,%f,%f,%f\n",
            stats[i].name,
            mean_slow, stddev_slow,
            mean_fast, stddev_fast);

        /* outputs on CSV */
        if (out) {
            fprintf(out, "%s,%f,%f,%f,%f\n",
                stats[i].name,
                mean_slow, stddev_slow,
                mean_fast, stddev_fast);
        }
    }

    if (out) {
        fclose(out);
        printf("\n[+] Results successfully saved to: %s\n", out_csv_path);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <baseline_csv_path> <vpoline_csv_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *baseline_csv = argv[1];
    const char *vpoline_csv = argv[2];

    run_benchmark_set("~/vpoline/build/bin/gp_benchmark_bulk", baseline_csv, "Baseline (native)");

    run_benchmark_set("LD_PRELOAD=~/vpoline/build/libvpoline.so ~/vpoline/build/bin/gp_benchmark_bulk", vpoline_csv, "vpoline (LD_PRELOAD)");

    return EXIT_SUCCESS;
}
