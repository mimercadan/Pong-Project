#define _POSIX_C_SOURCE 200809L

#include "match_loop.h"

#include <errno.h>
#include <stddef.h>
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

    if (clock_gettime(CLOCK_MONOTONIC, &previous) != 0) {
        release_worker(loop);
        return NULL;
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

        /* Después conectaremos aquí la comunicación del estado. */
    }

    release_worker(loop);
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