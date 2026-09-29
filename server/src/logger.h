#ifndef LOGGER_H
#define LOGGER_H

int  logger_init(const char *path);                 /* abre el archivo de log */
void log_msg(const char *level, const char *fmt, ...); /* consola + archivo */
void logger_close(void);

#endif
