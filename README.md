# Pong - Multiplayer Online Game
Internet: Arquitectura y Protocolos - EAFIT

## Introducción

## Desarrollo
### Arquitectura
### Protocolo (MyAppGameProtocol)
### Servidor
#### Lógica del juego

El servidor mantiene el estado de cada partida y calcula los movimientos, las colisiones, el marcador y el ganador. El cliente deberá enviar acciones de movimiento; no determina las posiciones ni los puntos.

La implementación se organiza en los siguientes módulos:

| Módulo | Responsabilidad |
|---|---|
| `game.h` / `game.c` | Estado de una partida, movimiento de paletas y pelota, colisiones, puntuación, victoria y abandono. |
| `players.h` / `players.c` | Registro, validación básica, consulta y eliminación de perfiles. |
| `lobby.h` / `lobby.c` | Espera, emparejamiento, cancelación, cierre de partidas y limpieza por desconexión. |
| `engine.h` / `engine.c` | Acceso sincronizado a jugadores, lobby y partidas mediante un mutex. |
| `match_loop.h` / `match_loop.c` | Creación, ejecución y parada del hilo que actualiza cada partida. |

#### Reglas y coordenadas

La cancha utiliza coordenadas lógicas con origen `(0, 0)` en la esquina superior izquierda. El eje `x` aumenta hacia la derecha y el eje `y` hacia abajo.

| Parámetro | Valor |
|---|---|
| Ancho y alto de la cancha | 800 × 600 unidades |
| Ancho y alto de cada paleta | 20 × 100 unidades |
| Separación respecto al borde lateral | 30 unidades |
| Posición horizontal de las paletas | Izquierda: 30; derecha: 750 |
| Radio de la pelota | 10 unidades |
| Velocidad de las paletas | 300 unidades por segundo |
| Velocidad inicial de la pelota | Horizontal: 300; vertical: 150 unidades por segundo |
| Puntaje de victoria | 5 puntos |

La posición de una paleta corresponde a su esquina superior izquierda. Su posición vertical se limita al intervalo `[0, 500]`. La posición de la pelota corresponde a su centro.

La pelota rebota contra los límites superior e inferior y contra las caras interiores de las paletas. La detección contra paletas utiliza una aproximación rectangular del espacio ocupado por la pelota; no calcula una colisión circular exacta en las esquinas.

Cuando la pelota sale completamente por un extremo lateral, el jugador contrario obtiene un punto. Si nadie ha ganado, la pelota vuelve al centro y se dirige hacia el jugador que recibió el punto. Las paletas conservan sus posiciones.

Al alcanzar cinco puntos, la partida termina, se guarda el identificador del ganador y se detiene la pelota. Si un jugador abandona durante una partida activa, gana su rival y se conserva el marcador alcanzado.

#### Registro y lobby

Cada perfil se asocia al identificador de conexión asignado por el servidor. El registro admite un nickname de hasta 32 bytes y un correo de hasta 128 bytes, sin contar el terminador de cadena de C.

Se rechazan identificadores no positivos, campos vacíos, textos demasiado largos, espacios y caracteres de control. El correo se valida de forma básica: debe contener una sola arroba y texto a ambos lados. Esta validación no verifica que la cuenta exista.

Una conexión no puede registrar dos perfiles. Actualmente se permiten nicknames y correos repetidos entre conexiones diferentes.

El registro admite hasta 64 jugadores y el lobby dispone de 32 espacios para partidas. Los estados de un jugador son:

- `PLAYER_IDLE`: registrado y disponible.
- `PLAYER_WAITING`: esperando un rival.
- `PLAYER_PLAYING`: asociado a una partida.

El primer jugador que entra al lobby espera. Cuando llega otro disponible, se crea una partida: el primero controla la paleta izquierda y el segundo la derecha.

Un jugador no puede entrar nuevamente a la cola mientras espera o juega. Puede cancelar la espera y conservar su perfil. Al cerrar una partida finalizada, se conserva una copia del resultado, se libera su espacio y sus jugadores quedan disponibles.

Una desconexión retira al jugador de la cola o resuelve su partida, según su estado, y elimina su perfil.

#### Entradas y actualización de partidas

Las acciones admitidas son `MOVE_UP`, `MOVE_STOP` y `MOVE_DOWN`. El motor guarda la última dirección de cada jugador. Repetir un mensaje de movimiento no produce desplazamientos adicionales por sí mismo.

El ciclo aplica las direcciones según el tiempo de simulación. El cliente deberá enviar `MOVE_STOP` al soltar la tecla. El tiempo transcurrido lo calcula el servidor.

Cada partida puede tener un hilo de actualización. Se rechaza iniciar un segundo ciclo activo para la misma partida.

El hilo espera aproximadamente 16 milisegundos entre actualizaciones y mide el intervalo con `CLOCK_MONOTONIC`. Este intervalo no garantiza una frecuencia exacta, porque depende de la planificación del sistema operativo.

Si el intervalo supera 0,25 segundos, el avance de esa actualización se limita a ese valor. La física divide el tiempo recibido en pasos de hasta 0,005 segundos para reducir errores de detección de colisiones.

#### Sincronización y ciclo de vida

El motor utiliza un mutex compartido para coordinar el registro, el emparejamiento, las entradas, las consultas, las actualizaciones y la limpieza de desconexiones.

Las funciones de consulta entregan copias del estado. Los consumidores no deben conservar punteros a espacios internos de partidas ni acceder directamente a los datos compartidos sin respetar la sincronización.

Las esperas del ciclo se realizan fuera del bloqueo. La integración de red deberá realizar también los envíos fuera del mutex del motor.

Los hilos identifican las partidas mediante su ID. Si una partida se elimina y su espacio se reutiliza con otro ID, el hilo anterior no actualiza la nueva partida.

Para el cierre normal se recoge el hilo mediante `pthread_join` y después se libera la partida finalizada, conservando una copia de su resultado. En una desconexión, el motor puede retirar la partida bajo el mutex; el hilo asociado debe recogerse igualmente.

El motor y las variables de control de hilos deben permanecer válidos mientras los hilos los utilicen. El motor se destruye únicamente después de recoger todos sus hilos.

#### Compilación y pruebas

Desde la raíz del repositorio, en un entorno Linux con GCC y Make:

```bash
make -C server
```

El comando compila y enlaza los módulos del servidor. Para construir y ejecutar las pruebas:

```bash
make -C server test
```

| Prueba | Comportamientos comprobados |
|---|---|
| `test_game` | Movimientos, límites, rebotes, puntuación, victoria y abandono. |
| `test_players` | Registro, consulta, rechazo de datos inválidos, duplicidad por conexión y capacidad. |
| `test_lobby` | Emparejamiento, cancelación de espera, cierre, reutilización de espacios y desconexiones. |
| `test_engine` | Registro y emparejamiento desde varios hilos; consultas, entradas y limpieza mediante el motor. |
| `test_match_loop` | Movimiento automático, rechazo de ciclos duplicados, parada, desconexión, reutilización de espacios y cierre por puntuación. |

Las pruebas verifican escenarios específicos; no constituyen una garantía de ausencia de errores en todas las interacciones concurrentes.

Para eliminar los archivos generados de compilación y pruebas:

```bash
make -C server clean
```

#### Estado de integración

Los módulos de lógica y red están reunidos y se compilan en el mismo ejecutable. Las cinco pruebas anteriores pasan.

En esta versión, el servidor de red todavía responde mediante eco. Está pendiente conectar los tipos y campos del protocolo con el registro, el lobby, las entradas, el arranque de ciclos y el envío de estados y resultados.

También están pendientes las pruebas completas con el cliente y la validación del despliegue en AWS.
### Cliente
### Despliegue en AWS

## Conclusiones

## Referencias
