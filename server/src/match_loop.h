#ifndef MATCH_LOOP_H
#define MATCH_LOOP_H

#include <stdatomic.h>
#include "engine.h"

/* Qué pasó en la partida en este paso. */
typedef enum {
    MATCH_EVENT_TICK,   /* la bola avanzó, no hubo punto */
    MATCH_EVENT_POINT,  /* alguien anotó y la partida sigue */
    MATCH_EVENT_OVER    /* la partida terminó por puntos */
} match_event_t;

/*
 * Función que el ciclo llama después de cada paso de la partida.
 * tick cuenta los pasos desde que empezó (~60 por segundo).
 * El servidor la usa para enviar STATE y GAME_OVER a los jugadores.
 */
typedef void (*match_event_fn)(const game_t *game, match_event_t event,
                               unsigned long tick);

typedef struct {
    engine_t *engine;
    int match_id;
    pthread_t thread;
    atomic_int stop_requested;
    int started;

    /* Solo los usa match_loop_run_detached. */
    match_event_fn on_event;
    int self_owned;
} match_loop_t;

/* Inicializar la variable con {0} antes del primer uso. */
int match_loop_start(
    match_loop_t *loop,
    engine_t *engine,
    int match_id
);

/* Solicitar la parada y esperar a que el hilo termine. */
int match_loop_stop(match_loop_t *loop);

/*
 * Arrancar el ciclo de una partida que se maneja sola:
 * llama a on_event en cada paso, al terminar libera la partida
 * en el motor y se borra de memoria. Lo usa el servidor.
 */
int match_loop_run_detached(
    engine_t *engine,
    int match_id,
    match_event_fn on_event
);

#endif