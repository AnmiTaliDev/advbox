#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#define MAX_HOURS 99
#define DEFAULT_FORMAT 0
#define SIMPLE_FORMAT 1
#define MINIMAL_FORMAT 2

static volatile sig_atomic_t stop = 0;
static int display_format = DEFAULT_FORMAT;

void signal_handler(int signum) {
    stop = 1;
}

void clear_line() {
    printf("\r");
    for(int i = 0; i < 80; i++) {
        printf(" ");
    }
    printf("\r");
}

void show_help() {
    printf("Usage: countdown [OPTIONS] <time>\n");
    printf("Countdown timer with different time formats\n\n");
    printf("Time formats:\n");
    printf("  <seconds>     Direct seconds (e.g., 90)\n");
    printf("  MMmSSs        Minutes and seconds (e.g., 5m30s)\n");
    printf("  HHhMMmSSs     Hours, minutes and seconds (e.g., 1h30m15s)\n\n");
    printf("Options:\n");
    printf("  -s           Simple format (MM:SS)\n");
    printf("  -m           Minimal format (only numbers)\n");
    printf("  -h           Show this help message\n\n");
    printf("Examples:\n");
    printf("  countdown 60        # Countdown 60 seconds\n");
    printf("  countdown 5m30s     # Countdown 5 minutes 30 seconds\n");
    printf("  countdown -s 1h30m  # Countdown 1 hour 30 minutes in simple format\n");
}

// Parse time string into seconds
int parse_time(const char *time_str) {
    int hours = 0, minutes = 0, seconds = 0;
    char *str = strdup(time_str);
    char *ptr = str;
    int value = 0;

    // Check if the input is just a plain number
    char *endptr;
    int direct_seconds = (int)strtol(time_str, &endptr, 10);
    if (*endptr == '\0') {
        free(str);
        return direct_seconds;
    }

    // Parse HHhMMmSSs format
    while (*ptr) {
        if (isdigit(*ptr)) {
            value = value * 10 + (*ptr - '0');
        } else {
            switch(tolower(*ptr)) {
                case 'h':
                    hours = value;
                    value = 0;
                    break;
                case 'm':
                    minutes = value;
                    value = 0;
                    break;
                case 's':
                    seconds = value;
                    value = 0;
                    break;
                default:
                    free(str);
                    return -1;
            }
        }
        ptr++;
    }

    // Append trailing value if present
    if (value > 0) {
        seconds = value;
    }

    free(str);

    // Validate maximum values
    if (hours > MAX_HOURS || minutes > 59 || seconds > 59) {
        return -1;
    }

    return hours * 3600 + minutes * 60 + seconds;
}

void display_time(int total_seconds) {
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    switch(display_format) {
        case MINIMAL_FORMAT:
            if (hours > 0) {
                printf("\r%d:%02d:%02d", hours, minutes, seconds);
            } else {
                printf("\r%d:%02d", minutes, seconds);
            }
            break;

        case SIMPLE_FORMAT:
            if (hours > 0) {
                printf("\r%02d:%02d:%02d", hours, minutes, seconds);
            } else {
                printf("\r%02d:%02d", minutes, seconds);
            }
            break;

        default:
            if (hours > 0) {
                printf("\r[%02dh %02dm %02ds]", hours, minutes, seconds);
            } else if (minutes > 0) {
                printf("\r[%02dm %02ds]", minutes, seconds);
            } else {
                printf("\r[%02ds]", seconds);
            }
    }

    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int total_seconds;
    int opt;

    // Set up signal handler
    signal(SIGINT, signal_handler);

    // Parse options
    while ((opt = getopt(argc, argv, "smh")) != -1) {
        switch (opt) {
            case 's':
                display_format = SIMPLE_FORMAT;
                break;
            case 'm':
                display_format = MINIMAL_FORMAT;
                break;
            case 'h':
                show_help();
                return 0;
            default:
                fprintf(stderr, "Try 'countdown -h' for help\n");
                return 1;
        }
    }

    // Check that a time argument was provided
    if (optind >= argc) {
        fprintf(stderr, "Error: Time argument is required\n");
        fprintf(stderr, "Try 'countdown -h' for help\n");
        return 1;
    }

    // Parse the time argument
    total_seconds = parse_time(argv[optind]);
    if (total_seconds <= 0) {
        fprintf(stderr, "Error: Invalid time format\n");
        return 1;
    }

    // Main countdown loop
    while (total_seconds > 0 && !stop) {
        display_time(total_seconds);
        sleep(1);
        total_seconds--;
    }

    if (stop) {
        clear_line();
        printf("Countdown interrupted!\n");
        return 1;
    }

    // Clear the last line and print completion message
    clear_line();
    printf("Time's up!\n");

    return 0;
}
