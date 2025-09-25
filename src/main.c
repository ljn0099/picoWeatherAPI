#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "./http/server.h"
#include "./core/weather.h"
#include "./db/database.h"

static volatile int keep_running = 1;

static void handle_signal(int signum) {
    (void)signum;
    keep_running = 0;
}

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (weather_init_db() != DB_OK) {
        fprintf(stderr, "Failed to initialize database\n");
        return EXIT_FAILURE;
    }

    const char *apiPortStr = getenv("API_PORT");
    int apiPort;
    if (apiPortStr)
        apiPort = atoi(apiPortStr);
    else
        apiPort = 8080;

    if (http_server_init(apiPort) != 0) {
        fprintf(stderr, "Failed to initialize HTTP server\n");
        weather_close_db();
        return EXIT_FAILURE;
    }

    printf("Todo REST API server running on port %d...\n", apiPort);

    while (keep_running) {
        http_server_process();
    }

    http_server_cleanup();
    weather_close_db();
    printf("\nServer shutdown complete\n");

    return EXIT_SUCCESS;
}
