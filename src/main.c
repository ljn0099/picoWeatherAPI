#include "./http/server.h"
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include "database/database.h"

static volatile int keepRuning = 1;

static void handle_signal(int sigNum) {
    (void)sigNum;
    keepRuning = 0;
}

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initizalize libsodium\n");
        return EXIT_FAILURE;
    }

    if (!init_db_vars()) {
        fprintf(stderr, "Failed to initialize db env vars\n");
        return EXIT_FAILURE;
    }

    if(!init_pool()) {
        fprintf(stderr, "Failed to initialize db\n");
        return EXIT_FAILURE;
    }

    const char *apiPortStr = getenv("API_PORT");
    int apiPort;
    if (apiPortStr)
        apiPort = atoi(apiPortStr);
    else
        apiPort = 8080;

    int nThreads = sysconf(_SC_NPROCESSORS_ONLN);

    if (http_server_init(apiPort, nThreads) != 0) {
        fprintf(stderr, "Failed to initialize HTTP server\n");
        free_pool();
        return EXIT_FAILURE;
    }

    printf("Weather REST API server running on port %d...\n", apiPort);

    while (keepRuning) {
        http_server_process();
    }

    http_server_cleanup();
    free_pool();
    printf("\nServer shutdown complete\n");

    return EXIT_SUCCESS;
}
