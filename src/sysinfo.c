#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <time.h>

#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

// Структура для хранения информации о CPU
typedef struct {
    char model[256];
    int cores;
    float load[3];
} CPUInfo;

// Структура для хранения информации о памяти
typedef struct {
    unsigned long total;
    unsigned long used;
    unsigned long free;
    unsigned long shared;
    unsigned long buffers;
    unsigned long cached;
} MemInfo;

// Структура для хранения информации о диске
typedef struct {
    unsigned long total;
    unsigned long used;
    unsigned long free;
} DiskInfo;

// Получение информации о CPU
void get_cpu_info(CPUInfo *cpu) {
    FILE *fp;
    char line[256];
    
    // Получение модели CPU
    fp = fopen("/proc/cpuinfo", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *model = strchr(line, ':');
                if (model) {
                    model++; // Пропускаем двоеточие
                    while (*model == ' ') model++; // Пропускаем пробелы
                    strcpy(cpu->model, model);
                    // Удаляем символ новой строки
                    cpu->model[strlen(cpu->model)-1] = '\0';
                    break;
                }
            }
        }
        fclose(fp);
    }
    
    // Получение количества ядер
    cpu->cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    // Получение загрузки CPU
    if ((fp = fopen("/proc/loadavg", "r")) != NULL) {
        fscanf(fp, "%f %f %f", &cpu->load[0], &cpu->load[1], &cpu->load[2]);
        fclose(fp);
    }
}

// Получение информации о памяти
void get_memory_info(MemInfo *mem) {
    struct sysinfo si;
    
    if (sysinfo(&si) == 0) {
        mem->total = si.totalram;
        mem->free = si.freeram;
        mem->buffers = si.bufferram;
        mem->shared = si.sharedram;
        
        // Получение информации о кэше из /proc/meminfo
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Cached:", 7) == 0) {
                    mem->cached = ((unsigned long)atol(line + 7)) * 1024;
                    break;
                }
            }
            fclose(fp);
        }
        
        mem->used = mem->total - mem->free - mem->buffers - mem->cached;
    }
}

// Получение информации о диске
void get_disk_info(DiskInfo *disk, const char *path) {
    struct statvfs fs;
    
    if (statvfs(path, &fs) == 0) {
        disk->total = (unsigned long)fs.f_blocks * fs.f_frsize;
        disk->free = (unsigned long)fs.f_bfree * fs.f_frsize;
        disk->used = disk->total - disk->free;
    }
}

// Форматирование размера в читаемый вид
void format_size(unsigned long bytes, char *result) {
    if (bytes >= GB) {
        sprintf(result, "%.1f GB", (float)bytes / GB);
    } else if (bytes >= MB) {
        sprintf(result, "%.1f MB", (float)bytes / MB);
    } else if (bytes >= KB) {
        sprintf(result, "%.1f KB", (float)bytes / KB);
    } else {
        sprintf(result, "%lu B", bytes);
    }
}

// Вывод процентов использования в виде прогресс-бара
void print_usage_bar(float percentage, int width) {
    int filled = (int)((percentage * width) / 100.0);
    printf("[");
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            printf("#");
        } else {
            printf("-");
        }
    }
    printf("] %.1f%%", percentage);
}

void show_help() {
    printf("Usage: sysinfo [OPTIONS]\n");
    printf("Display system information\n\n");
    printf("Options:\n");
    printf("  -c    Show CPU information only\n");
    printf("  -m    Show memory information only\n");
    printf("  -d    Show disk information only\n");
    printf("  -h    Show this help message\n");
}

int main(int argc, char *argv[]) {
    int show_cpu = 1, show_mem = 1, show_disk = 1;
    int opt;
    
    // Разбор опций
    while ((opt = getopt(argc, argv, "cmdhp")) != -1) {
        switch (opt) {
            case 'c':
                show_mem = show_disk = 0;
                break;
            case 'm':
                show_cpu = show_disk = 0;
                break;
            case 'd':
                show_cpu = show_mem = 0;
                break;
            case 'h':
                show_help();
                return 0;
            default:
                fprintf(stderr, "Try 'sysinfo -h' for help\n");
                return 1;
        }
    }
    
    struct utsname un;
    if (uname(&un) < 0) {
        perror("uname");
        return 1;
    }
    
    // Получение текущего времени
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
    
    // Вывод общей информации
    printf("\n=== System Information ===\n");
    printf("Time: %s\n", time_str);
    printf("Hostname: %s\n", un.nodename);
    printf("OS: %s %s\n", un.sysname, un.release);
    printf("Architecture: %s\n\n", un.machine);
    
    if (show_cpu) {
        CPUInfo cpu = {0};
        get_cpu_info(&cpu);
        
        printf("=== CPU Information ===\n");
        printf("Model: %s\n", cpu.model);
        printf("Cores: %d\n", cpu.cores);
        printf("Load average: %.2f, %.2f, %.2f (1, 5, 15 min)\n\n",
               cpu.load[0], cpu.load[1], cpu.load[2]);
    }
    
    if (show_mem) {
        MemInfo mem = {0};
        get_memory_info(&mem);
        char total[20], used[20], free[20], cached[20], buffers[20];
        
        format_size(mem.total, total);
        format_size(mem.used, used);
        format_size(mem.free, free);
        format_size(mem.cached, cached);
        format_size(mem.buffers, buffers);
        
        printf("=== Memory Information ===\n");
        printf("Total: %s\n", total);
        printf("Used:  %s  ", used);
        print_usage_bar(100.0 * mem.used / mem.total, 30);
        printf("\n");
        printf("Free:  %s\n", free);
        printf("Cached: %s\n", cached);
        printf("Buffers: %s\n\n", buffers);
    }
    
    if (show_disk) {
        DiskInfo disk = {0};
        get_disk_info(&disk, "/");
        char total[20], used[20], free[20];
        
        format_size(disk.total, total);
        format_size(disk.used, used);
        format_size(disk.free, free);
        
        printf("=== Disk Information (/) ===\n");
        printf("Total: %s\n", total);
        printf("Used:  %s  ", used);
        print_usage_bar(100.0 * disk.used / disk.total, 30);
        printf("\n");
        printf("Free:  %s\n\n", free);
    }
    
    return 0;
}