#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <string.h>

/* Questo è l'handler custom dell'applicazione */
void app_sigsegv_handler(int sig, siginfo_t *si, void *context) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), 
        "\n[APP HANDLER] Guarda, ho provato a mettere un numero all'indirizzo %p "
        "appena superiore alla soglia!\n", 
        si->si_addr);
    
    // Usiamo write invece di printf perché è async-signal-safe
    write(STDOUT_FILENO, buf, len);
    
    // Usciamo esplicitamente. Se non lo facessimo (o non cambiassimo il PC),
    // il processore riproverebbe in loop a eseguire l'istruzione in errore.
    _exit(1); 
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = app_sigsegv_handler;
    sa.sa_flags = SA_SIGINFO; // Fondamentale per far sì che chiami sa_sigaction e ci passi si_addr

    // Registriamo il nostro handler (vpoline dovrà intercettare questa chiamata!)
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction failed");
        return 1;
    }

    printf("[APP] SIGSEGV handler dell'applicazione registrato con successo.\n");

    // Allochiamo esattamente una pagina di memoria (tipicamente 4KB)
    long page_size = sysconf(_SC_PAGESIZE);
    int *arr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (arr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    printf("[APP] Array (1 pagina) allocato all'indirizzo di base: %p\n", (void *)arr);
    
    int elements = page_size / sizeof(int);
    printf("[APP] Ultimo indirizzo valido dell'array: %p\n", (void *)&arr[elements - 1]);

    // Puntiamo esattamente fuori dalla pagina mappata (il primo byte della pagina successiva)
    int *out_of_bounds_ptr = &arr[elements+ (4096 * 100)];
    
    printf("[APP] Provo a scrivere '42' all'indirizzo invalido: %p...\n", (void *)out_of_bounds_ptr);

    // BOOM! Segfault garantito.
    *out_of_bounds_ptr = 42;

    printf("Questo messaggio non dovrebbe mai essere stampato.\n");
    return 0;
}