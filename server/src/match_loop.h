#ifndef MATCH_LOOP_H
#define MATCH_LOOP_H

#include <stdatomic.h>
#include "engine.h"

typedef struct {
    engine_t *engine;
    int match_id;
    pthread_t thread;
    atomic_int stop_requested;
    int started;
} match_loop_t;

/* Inicializar la variable con {0} antes del primer uso. */
int match_loop_start(
    match_loop_t *loop,
    engine_t *engine,
    int match_id
);

/* Solicitar la parada y esperar a que el hilo termine. */
int match_loop_stop(match_loop_t *loop);

#endif