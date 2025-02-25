#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_NUMBERS 1000

void show_help() {
    printf("Usage: randnum [OPTIONS] <min> <max>\n");
    printf("Generate random numbers in specified range\n\n");
    printf("Options:\n");
    printf("  -n <count>   Number of random numbers to generate (default: 1, max: 1000)\n");
    printf("  -u           Ensure unique numbers (no duplicates)\n");
    printf("  -s           Sort output numbers\n");
    printf("  -c           Output in comma-separated format\n");
    printf("  -h           Show this help message\n\n");
    printf("Example:\n");
    printf("  randnum 1 100          # Generate one random number between 1 and 100\n");
    printf("  randnum -n 5 1 10      # Generate 5 random numbers between 1 and 10\n");
    printf("  randnum -u -n 3 1 5    # Generate 3 unique numbers between 1 and 5\n");
}

// Компаратор для qsort
int compare(const void* a, const void* b) {
    return (*(long*)a - *(long*)b);
}

// Проверка, существует ли число в массиве
int number_exists(long* numbers, int count, long number) {
    for(int i = 0; i < count; i++) {
        if(numbers[i] == number) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    long min = 0, max = 0;
    int count = 1;
    int unique = 0;
    int sort = 0;
    int comma = 0;
    int opt;
    
    // Инициализация генератора случайных чисел
    srand(time(NULL) ^ getpid());
    
    // Разбор опций
    while ((opt = getopt(argc, argv, "n:usch")) != -1) {
        switch (opt) {
            case 'n':
                count = atoi(optarg);
                if (count < 1 || count > MAX_NUMBERS) {
                    fprintf(stderr, "Error: Count must be between 1 and %d\n", MAX_NUMBERS);
                    return 1;
                }
                break;
            case 'u':
                unique = 1;
                break;
            case 's':
                sort = 1;
                break;
            case 'c':
                comma = 1;
                break;
            case 'h':
                show_help();
                return 0;
            default:
                fprintf(stderr, "Try 'randnum -h' for help\n");
                return 1;
        }
    }
    
    // Проверка оставшихся аргументов
    if (argc - optind != 2) {
        fprintf(stderr, "Error: Min and max values required\n");
        fprintf(stderr, "Try 'randnum -h' for help\n");
        return 1;
    }
    
    // Получение min и max значений
    min = atol(argv[optind]);
    max = atol(argv[optind + 1]);
    
    // Проверка диапазона
    if (min >= max) {
        fprintf(stderr, "Error: Max must be greater than min\n");
        return 1;
    }
    
    // Проверка возможности генерации уникальных чисел
    if (unique && (max - min + 1) < count) {
        fprintf(stderr, "Error: Range too small for %d unique numbers\n", count);
        return 1;
    }
    
    // Выделение памяти для чисел
    long* numbers = malloc(count * sizeof(long));
    if (!numbers) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    // Генерация чисел
    for(int i = 0; i < count; i++) {
        long range = max - min + 1;
        long number;
        
        do {
            // Используем более точный метод для больших диапазонов
            if (range > RAND_MAX) {
                number = min + ((long)rand() * RAND_MAX + rand()) % range;
            } else {
                number = min + rand() % range;
            }
        } while (unique && number_exists(numbers, i, number));
        
        numbers[i] = number;
    }
    
    // Сортировка если требуется
    if (sort) {
        qsort(numbers, count, sizeof(long), compare);
    }
    
    // Вывод результатов
    for(int i = 0; i < count; i++) {
        if (comma && i > 0) {
            printf(", ");
        }
        printf("%ld", numbers[i]);
        if (!comma) {
            printf("\n");
        }
    }
    
    if (comma) {
        printf("\n");
    }
    
    // Освобождение памяти
    free(numbers);
    return 0;
}