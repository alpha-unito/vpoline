#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 5

// int64_t global_vars[NUM_THREADS] = {100, 100, 100, 100, 100};

int64_t global_var_0 = 100;
int64_t global_var_1 = 100;
int64_t global_var_2 = 100;
int64_t global_var_3 = 100;
int64_t global_var_4 = 100;

pthread_barrier_t start_barrier;

// int64_t global_var_64 = 100;

/* Spinlock custom per forzare l'esecuzione concorrente in user-space */
// volatile int current_turn = 0;

void* thread_func(void* arg)
{
    int id = *(int*)arg;


    pthread_barrier_wait(&start_barrier);
    /*
     * I thread rimangono intrappolati qui in user-space finché non è il loro turno.
     * Questo massimizza la probabilità che ricevano il SIGUSR1 mentre sono attivi.
     */
    // while (current_turn != id) {
    //     // busy wait
    // }
    //
    // /* Reset per far funzionare l'aritmetica di controllo per i thread successivi */
    // /* resetting global variable */
    // global_var_64 = 100;


    printf("\n[*] Thread %d starting LD/SD test with GP-relative access...\n", id);
    int exit_status = EXIT_SUCCESS;
    int64_t expected_load = 100;
    int64_t store_val     = 200;

    // int64_t val64 = global_vars[id];
    int64_t val64 = 0;
    switch(id) {
        case 0: val64 = global_var_0; break;
        case 1: val64 = global_var_1; break;
        case 2: val64 = global_var_2; break;
        case 3: val64 = global_var_3; break;
        case 4: val64 = global_var_4; break;
    }

    if (val64 != expected_load) {
        printf("[FAIL] Thread %d: LD emulation failed. Read: %ld, Expected: %ld\n",
                           id, val64, expected_load);
        exit_status = EXIT_FAILURE;
    } else {
        printf("[ OK ] Thread %d: LD emulation success\n", id);
    }

    // global_vars[id] = store_val;

    switch(id) {
        case 0: global_var_0 = store_val; break;
        case 1: global_var_1 = store_val; break;
        case 2: global_var_2 = store_val; break;
        case 3: global_var_3 = store_val; break;
        case 4: global_var_4 = store_val; break;
    }

    int64_t final_check = 0;
    switch(id) {
        case 0: final_check = global_var_0; break;
        case 1: final_check = global_var_1; break;
        case 2: final_check = global_var_2; break;
        case 3: final_check = global_var_3; break;
        case 4: final_check = global_var_4; break;
    }

    if (final_check != store_val) {
        printf("[FAIL] Thread %d: SD emulation failed. In memory: %ld, Expected: %ld\n",
                   id, final_check, store_val);
        exit_status = EXIT_FAILURE;
    } else {
        printf("[ OK ] Thread %d: SD emulation success\n", id);
    }

    // for (int i = 0; i < 2; i++) {
    //     int64_t expected_load = (i == 0) ? 100 : 200;
    //     int64_t store_val     = (i == 0) ? 200 : 300;
    //
    //     /*
    //      * NOTA: Dal Thread 1 in poi, anche quando i == 0 l'esecuzione passerà in
    //      * FAST-PATH perché l'istruzione in memoria è GIA' STATA patchata dal Thread 0.
    //      * La stringa stamperà comunque "SLOW-PATH" per via dell'if, ma strutturalmente
    //      * non entreranno mai nel sigsegv_handler.
    //      */
    //     // const char* path_name = (i == 0) ? "SLOW-PATH (sigsegv_handler) " : "FAST-PATH (gp_fault_handler)";
    //
    //     /* ld reg, offset(gp) */
    //     int64_t val64 = global_var_64;
    //
    //     if (val64 != expected_load) {
    //         printf("[FAIL] Thread %d: LD emulation failed at iteration %d. Read: %ld, Expected: %ld\n",
    //                id, i, val64, expected_load);
    //         exit_status = EXIT_FAILURE;
    //     } else {
    //         printf("[ OK ] Thread %d: LD emulation success at iteration %d\n", id, i);
    //     }
    //
    //     /* sd reg, offset(gp) */
    //     global_var_64 = store_val;
    //
    //     if (global_var_64 != store_val) {
    //         printf("[FAIL] Thread %d: Fallito SD in %s. In memoria: %ld, Atteso: %ld\n",
    //                id, path_name, global_var_64, store_val);
    //         exit_status = EXIT_FAILURE;
    //     } else {
    //         printf("[ OK ] Thread %d: Corretta emulazione SD in %s\n", id, path_name);
    //     }
    // }
    //
    //
    // /* Passa il turno al prossimo thread e sblocca il suo while */
    // current_turn++;
    return (void*)(intptr_t)exit_status;
}


int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    pthread_barrier_init(&start_barrier, NULL, NUM_THREADS);
    printf("Starting multithreading Stop-The-World handling with %d threads...\n", NUM_THREADS);

    /* thread creation */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]) != 0) {
            perror("Thread creation failed");
            return EXIT_FAILURE;
        }
    }

    /* waiting for thread termination */
    for (int i = 0; i < NUM_THREADS; i++) {
        void* status;
        pthread_join(threads[i], &status);
    }

    pthread_barrier_destroy(&start_barrier);
    printf("\nTest completed\n");
    return 0;
}