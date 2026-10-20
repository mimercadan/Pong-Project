#define _POSIX_C_SOURCE 200809L

#include "match_loop.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

/* Buscar una partida. Se llama con el mutex adquirido. */
static match_slot_t *find_match_locked(
    engine_t *engine,
    int match_id
) {
    for (int i = 0; i < MATCH_CAPACITY; i++) {
        match_slot_t *slot = &engine->lobby.matches[i];

        if (slot->active && slot->game.id == match_id) {
            return slot;
        }
    }

    return NULL;
}

/* Liberar la marca solo si sigue siendo la misma partida. */
static void release_worker(match_loop_t *loop) {
    pthread_mutex_lock(&loop->engine->lock);

    match_slot_t *slot = find_match_locked(
        loop->engine, loop->match_id
    );

    if (slot != NULL) {
        slot->worker_running = 0;
    }

    pthread_mutex_unlock(&loop->engine->lock);
}

static void *run_match(void *argument) {
    match_loop_t *loop = argument;
    struct timespec previous;

    /* Una partida independiente no tiene a nadie esperándola con join. */
    if (loop->self_owned) {
        pthread_detach(pthread_self());
    }

    /* Si no se puede leer el reloj, no se entra al ciclo. */
    if (clock_gettime(CLOCK_MONOTONIC, &previous) != 0) {
        atomic_store(&loop->stop_requested, 1);
    }

    while (!atomic_load(&loop->stop_requested)) {
        /* Esperar aproximadamente 16 milisegundos sin retener el mutex. */
        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 16000000L
        };

        int sleep_result;

        do {
            sleep_result = nanosleep(&delay, &delay);
        } while (sleep_result == -1 && errno == EINTR &&
                 !atomic_load(&loop->stop_requested));

        if (atomic_load(&loop->stop_requested)) {
            break;
        }

        if (sleep_result == -1) {
            break;
        }

        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            break;
        }

        /* Convertir la diferencia de tiempo a segundos. */
        double elapsed =
            (double)(now.tv_sec - previous.tv_sec) +
            (double)(now.tv_nsec - previous.tv_nsec) / 1000000000.0;

        previous = now;

        if (elapsed <= 0.0) {
            continue;
        }

        /* Limitar el avance si el sistema estuvo detenido mucho tiempo. */
        if (elapsed > 0.25) {
            elapsed = 0.25;
        }

        game_t snapshot;

        if (engine_step_match(
            loop->engine,
            loop->match_id,
            (float)elapsed,
            &snapshot
        ) != 0) {
            break;
        }

        if (snapshot.status == GAME_FINISHED) {
            break;
        }

        /* Avisar al servidor para que envíe el estado a los jugadores. */
        if (loop->on_event != NULL) {
            loop->on_event(&snapshot, 0);
        }
    }

    release_worker(loop);

    /*
     * Si la partida terminó por puntos, liberarla y avisar el resultado.
     * Si un jugador se desconectó, engine_disconnect ya la liberó;
     * entonces engine_finish_match falla y no se avisa dos veces.
     */
    if (loop->on_event != NULL) {
        game_t result;

        if (engine_finish_match(loop->engine, loop->match_id, &result) == 0) {
            loop->on_event(&result, 1);
        }
    }

    /* Una partida independiente se borra a sí misma al terminar. */
    if (loop->self_owned) {
        free(loop);
    }

    return NULL;
}

int match_loop_start(
    match_loop_t *loop,
    engine_t *engine,
    int match_id
) {
    if (loop == NULL || engine == NULL ||
        match_id <= 0 || loop->started) {
        return -1;
    }

    pthread_mutex_lock(&engine->lock);

    match_slot_t *slot = find_match_locked(engine, match_id);

    if (slot == NULL ||
        slot->game.status != GAME_PLAYING ||
        slot->worker_running) {
        pthread_mutex_unlock(&engine->lock);
        return -1;
    }

    slot->worker_running = 1;

    loop->engine = engine;
    loop->match_id = match_id;
    atomic_init(&loop->stop_requested, 0);

    int result = pthread_create(
        &loop->thread, NULL, run_match, loop
    );

    if (result != 0) {
        slot->worker_running = 0;
        pthread_mutex_unlock(&engine->lock);
        return -1;
    }

    loop->started = 1;

    pthread_mutex_unlock(&engine->lock);
    return 0;
}

int match_loop_stop(match_loop_t *loop) {
    if (loop == NULL || !loop->started) {
        return -1;
    }

    atomic_store(&loop->stop_requested, 1);

    if (pthread_join(loop->thread, NULL) != 0) {
        return -1;
    }

    loop->started = 0;
    return 0;
}

int match_loop_run_detached(
    engine_t *engine,
    int match_id,
    match_event_fn on_event
) {
    match_loop_t *loop = calloc(1, sizeof *loop);

    if (loop == NULL) {
        return -1;
    }

    loop->on_event = on_event;
    loop->self_owned = 1;

    if (match_loop_start(loop, engine, match_id) != 0) {
        free(loop);
        return -1;
    }

    /* Desde aquí no se toca loop: el hilo lo libera al terminar. */
    return 0;
}
