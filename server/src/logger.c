#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

int logger_init(const char *path) {
    log_file = fopen(path, "a");
    return log_file ? 0 : -1;
}

void log_msg(const char *level, const char *fmt, ...) {
    char when[32];
    struct tm local;
    time_t now = time(NULL);

    /* localtime_r usa la variable local de cada hilo (localtime no es seguro con hilos). */
    localtime_r(&now, &local);
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &local);

    pthread_mutex_lock(&log_lock);   /* varios hilos escriben a la vez */
    va_list args;

    printf("[%s] [%s] ", when, level);
    va_start(args, fmt); vprintf(fmt, args); va_end(args);
    printf("\n");
    fflush(stdout);

    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", when, level);
        va_start(args, fmt); vfprintf(log_file, fmt, args); va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_lock);
}

void logger_close(void) {
    if (log_file) fclose(log_file);
}
