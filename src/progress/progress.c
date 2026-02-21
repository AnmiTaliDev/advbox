#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define DEFAULT_WIDTH 50
#define MIN_WIDTH 10
#define MAX_WIDTH 150
#define UPDATE_DELAY 100000  // 100ms

static int progress_width = DEFAULT_WIDTH;
static volatile sig_atomic_t stop = 0;

void signal_handler(int signum) {
    stop = 1;
}

void clear_line() {
    printf("\r");
    for(int i = 0; i < progress_width + 30; i++) {
        printf(" ");
    }
    printf("\r");
}

void show_help() {
    printf("Usage: progress [OPTIONS] <value>\n");
    printf("Shows an animated progress bar (0-100%%)\n\n");
    printf("Options:\n");
    printf("  -w <width>   Set progress bar width (10-150, default: 50)\n");
    printf("  -h           Show this help message\n\n");
    printf("Example:\n");
    printf("  progress 75          # Show 75%% progress\n");
    printf("  progress -w 100 50   # Show 50%% progress with width 100\n");
}

void draw_progress(int percentage) {
    int filled = (progress_width * percentage) / 100;
    int empty = progress_width - filled;

    printf("\r[");

    // Filled portion
    for(int i = 0; i < filled; i++) {
        printf("=");
    }

    // Progress cursor
    if (percentage < 100) {
        printf(">");
        empty--;
    }

    // Empty portion
    for(int i = 0; i < empty; i++) {
        printf(" ");
    }

    printf("] %3d%%", percentage);
    fflush(stdout);
}

int animate_progress(int target_percentage) {
    int current = 0;
    signal(SIGINT, signal_handler);

    while (current < target_percentage && !stop) {
        current++;
        draw_progress(current);
        usleep(UPDATE_DELAY);
    }

    if (stop) {
        clear_line();
        return 1;
    }

    // Show the final result
    draw_progress(target_percentage);
    printf("\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int percentage = 0;
    int i = 1;

    // Parse arguments
    while (i < argc) {
        if (strcmp(argv[i], "-h") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Width value is missing\n");
                return 1;
            }

            progress_width = atoi(argv[i + 1]);

            if (progress_width < MIN_WIDTH || progress_width > MAX_WIDTH) {
                fprintf(stderr, "Error: Width must be between %d and %d\n",
                        MIN_WIDTH, MAX_WIDTH);
                return 1;
            }

            i += 2;
        }
        else {
            percentage = atoi(argv[i]);
            i++;
        }
    }

    // Validate percentage value
    if (percentage < 0 || percentage > 100) {
        fprintf(stderr, "Error: Percentage must be between 0 and 100\n");
        return 1;
    }

    // If no percentage was provided
    if (argc == 1) {
        show_help();
        return 1;
    }

    return animate_progress(percentage);
}
