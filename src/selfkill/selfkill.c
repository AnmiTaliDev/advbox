#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_PROCESSES 1024
#define MAX_PATH 256
#define WAIT_TIME 2

// Structure for storing process information
typedef struct {
    pid_t pid;
    char name[256];
} ProcessInfo;

// List of processes that must not be terminated
const char *protected_processes[] = {
    "systemd", "bash", "sh", "login", "sshd",
    "gnome-session", "Xorg", "wayland",
    "selfkill", // the utility itself
    NULL
};

void show_help() {
    printf("Usage: selfkill [OPTIONS]\n");
    printf("Terminate all processes owned by current user\n\n");
    printf("Options:\n");
    printf("  -f    Force kill (SIGKILL instead of SIGTERM)\n");
    printf("  -l    List processes without killing\n");
    printf("  -v    Verbose output\n");
    printf("  -h    Show this help message\n\n");
    printf("Warning: This utility will terminate ALL non-essential\n");
    printf("user processes. Use with caution!\n");
}

// Check whether a process is protected
int is_protected(const char *name) {
    for (const char **p = protected_processes; *p; p++) {
        if (strstr(name, *p) != NULL) {
            return 1;
        }
    }
    return 0;
}

// Get process name by PID
int get_process_name(pid_t pid, char *name, size_t size) {
    char path[MAX_PATH];
    char buf[MAX_PATH];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    fp = fopen(path, "r");

    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            buf[strcspn(buf, "\n")] = 0; // Strip newline
            strncpy(name, buf, size - 1);
            name[size - 1] = '\0';
            fclose(fp);
            return 1;
        }
        fclose(fp);
    }
    return 0;
}

// Check whether a process belongs to the given user
int is_user_process(pid_t pid, uid_t uid) {
    char path[MAX_PATH];
    struct stat st;

    snprintf(path, sizeof(path), "/proc/%d", pid);
    if (stat(path, &st) == 0) {
        return st.st_uid == uid;
    }
    return 0;
}

// Build list of processes owned by the user
int get_user_processes(uid_t uid, ProcessInfo *processes, int max_procs, int verbose) {
    DIR *dir;
    struct dirent *ent;
    int count = 0;

    dir = opendir("/proc");
    if (!dir) {
        perror("Failed to open /proc");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL && count < max_procs) {
        // Only process numeric directory names (PIDs)
        if (!isdigit(*ent->d_name)) {
            continue;
        }

        pid_t pid = atoi(ent->d_name);

        // Skip the current process
        if (pid == getpid()) {
            continue;
        }

        if (is_user_process(pid, uid)) {
            processes[count].pid = pid;
            if (get_process_name(pid, processes[count].name, sizeof(processes[0].name))) {
                if (!is_protected(processes[count].name)) {
                    if (verbose) {
                        printf("Found process: %s (PID: %d)\n",
                               processes[count].name, processes[count].pid);
                    }
                    count++;
                }
            }
        }
    }

    closedir(dir);
    return count;
}

// Log the action taken
void log_action(const char *username, int process_count, int force_kill) {
    time_t now;
    char timestamp[64];
    FILE *fp;

    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fp = fopen("/var/log/selfkill.log", "a");
    if (fp) {
        fprintf(fp, "%s - User '%s' terminated %d processes (mode: %s)\n",
                timestamp, username, process_count,
                force_kill ? "SIGKILL" : "SIGTERM");
        fclose(fp);
    }
}

int main(int argc, char *argv[]) {
    ProcessInfo processes[MAX_PROCESSES];
    int force_kill = 0;
    int list_only = 0;
    int verbose = 0;
    int opt;

    // Parse options
    while ((opt = getopt(argc, argv, "flvh")) != -1) {
        switch (opt) {
            case 'f':
                force_kill = 1;
                break;
            case 'l':
                list_only = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                show_help();
                return 0;
            default:
                fprintf(stderr, "Try 'selfkill -h' for help\n");
                return 1;
        }
    }

    // Get current user information
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        fprintf(stderr, "Error: Cannot get user information\n");
        return 1;
    }

    printf("User: %s (UID: %d)\n", pw->pw_name, uid);

    // Build the process list
    int count = get_user_processes(uid, processes, MAX_PROCESSES, verbose);
    if (count < 0) {
        fprintf(stderr, "Error: Failed to get process list\n");
        return 1;
    }

    printf("Found %d terminable processes\n", count);

    if (count == 0) {
        printf("No processes to terminate\n");
        return 0;
    }

    // In list-only mode just display the processes
    if (list_only) {
        printf("\nProcess List:\n");
        for (int i = 0; i < count; i++) {
            printf("%5d: %s\n", processes[i].pid, processes[i].name);
        }
        return 0;
    }

    // Confirmation delay
    printf("\nWarning: This will terminate %d processes!\n", count);
    printf("You have %d seconds to cancel (Ctrl+C)...\n", WAIT_TIME);
    sleep(WAIT_TIME);

    // Send signals to processes
    int signal_type = force_kill ? SIGKILL : SIGTERM;
    int success_count = 0;

    for (int i = 0; i < count; i++) {
        if (kill(processes[i].pid, signal_type) == 0) {
            if (verbose) {
                printf("Terminated: %s (PID: %d)\n",
                       processes[i].name, processes[i].pid);
            }
            success_count++;
        } else if (verbose) {
            printf("Failed to terminate: %s (PID: %d) - %s\n",
                   processes[i].name, processes[i].pid, strerror(errno));
        }
    }

    // Log the result
    log_action(pw->pw_name, success_count, force_kill);

    printf("\nSuccessfully terminated %d/%d processes\n", success_count, count);

    return (success_count == count) ? 0 : 1;
}
