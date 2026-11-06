# Arquitectura de P.Melodies

Estado: **Fase 0** (base común). Los cuatro módulos existen con su latencia y su oversampling definitivos, pero todavía no procesan el audio. Su DSP llega en las fases 1 a 4.

## Organización del código

```
src/
  PluginProcessor.*      AudioProcessor de JUCE: buses, parámetros, estado guardado
  PluginEditor.*         Ventana del plugin
  core/                  Motor de audio independiente de JUCE AudioProcessor
    Module.h               Interfaz común de los módulos
    ModuleBase.*           Latencia constante, bypass sin clics, oversampling
    ModuleChain.*          Cadena reordenable con fundido cruzado
    ChainOrder.*           Orden de la cadena ↔ índice de permutación (0–23)
    AudioEngine.*          Recorrido completo de la señal (sección global incluida)
    Parameters.*           IDs, rangos y lectura sin bloqueos de los parámetros
  dsp/                   Piezas DSP reutilizables (retardo, clipper, medidores…)
  modules/               Implementación de cada módulo (en la fase 0, marcadores)
  ui/                    Interfaz: tema, componentes y paneles
tests/                   Tests Catch2 (se ejecutan en local y en CI)
```

`AudioEngine` no depende de `juce::AudioProcessor`, así que los tests lo usan directamente con módulos de prueba. El procesador solo traduce los parámetros a una instantánea (`EngineParameters`) en cada bloque.

## Recorrido de la señal

```
entrada ─► ganancia IN ─► medidor IN ─┬─► cadena (4 módulos) ─────────────── procesada ─┐
                                      └─► retardo (latencia de la cadena) ── seca ──────┴─► mix
mix ─► ganancia OUT ─► comparar a igual volumen ─► clipper de seguridad ─► bypass del host ─► medidor OUT ─► salida
```

- La señal **original** (antes de la ganancia de entrada, retrasada por la latencia) alimenta la comparación a igual volumen y el bypass del host.
- Todas las ganancias se suavizan durante 20 ms. Todos los interruptores (comparar, clipper, bypass) hacen un fundido de 20 ms.

## Latencia

La latencia que se comunica a FL Studio es **constante**, y la máxima posible (decisión §12 de la especificación):

1. Cada módulo declara `latencia = latencia propia máxima (p. ej. lookahead) + latencia del oversampling a 16x`.
2. Su ruta procesada se rellena con un retardo hasta esa latencia, con el factor de oversampling que esté usando.
3. Su ruta seca (para el bypass) se retrasa exactamente esa latencia.
4. La cadena suma las latencias de los módulos, que no dependen del orden. La ruta seca del mix global se retrasa esa suma.

La latencia de cada factor de oversampling se lleva a un número **entero** de muestras con un pequeño retardo a la frecuencia interna más alta. Así todas las alineaciones son retardos enteros exactos, y el test de impulso lo comprueba muestra a muestra.

En la fase 0, el EQ no tiene latencia (v1, sin oversampling) y los otros tres módulos reservan la del oversampling a 16x. El total es de unos 4,2 ms a 48 kHz. Con el lookahead del compresor (fase 2) se sumarán 20 ms.

## Módulos: estados del procesado

`ModuleBase` decide en cada bloque si la ruta procesada debe sonar:

- **Apagado o neutro** (los ajustes actuales no cambian nada): la ruta procesada no se calcula y la salida es la entrada retrasada, idéntica bit a bit. Así un preset neutro no altera el audio y no gasta CPU.
- **Encender:** se reinician los filtros, se procesa en silencio durante un *pre-roll* que los rellena y después hay un fundido de entrada de 10 ms.
- **Apagar:** fundido de salida de 10 ms hasta la ruta seca alineada.
- **Cambiar el factor de oversampling:** fundido de salida, cambio, *pre-roll* y fundido de entrada, todo sin clics.

## Reordenado sin clics

`ModuleChain` tiene dos juegos completos de módulos. Al cambiar el orden:

1. El juego de reserva copia el estado del activo (retardos, envolventes, filtros propios), sin reservar memoria.
2. Procesa el nuevo orden en silencio durante un *pre-roll* (2 × latencia + 64 muestras) que rellena los filtros de oversampling.
3. La salida hace un fundido cruzado lineal de 20 ms del orden antiguo al nuevo. Las dos señales están alineadas y correlacionadas, así que un fundido de ganancia constante no cambia el nivel.

Las peticiones que llegan durante el *pre-roll* reinician el cambio con el orden más reciente. Las que llegan durante el fundido esperan a que termine.

## Hilos

- **Audio:** `processBlock` → `AudioEngine::process`. Sin reservas de memoria, locks, I/O ni logs; hay un test que cuenta las reservas. Los bloques más grandes que el tamaño preparado se dividen internamente.
- **Interfaz:** lee los medidores con atómicos en un temporizador a 30 Hz. Los cambios de parámetros van por los adjuntos de JUCE (`ParameterAttachment`), que agrupan cada arrastre en un gesto.
- **Audio → interfaz con más datos** (muestras para el analizador, historiales de nivel de las pantallas de los módulos): `dsp::LockFreeFifo`, una cola de un productor y un consumidor sin bloqueos ni reservas de memoria. Si está llena, el audio escribe lo que cabe y sigue: nunca espera a la interfaz.
- **Estado:** XML con número de versión (`stateVersion`). `migrateState()` completa los parámetros que falten con su valor por defecto y es el punto donde irán las migraciones futuras.

## Interfaz

- `ui::Theme` contiene todos los colores, fuentes y medidas. Ningún componente tiene colores fijos: los lee del tema a través de `ui::LookAndFeel`, así que el tema se puede cambiar en caliente (las cuatro plantillas llegan en la fase 5).
- Fuentes embebidas: Inter (interfaz), JetBrains Mono (valores) y Space Grotesk (títulos). Sus licencias están en `licenses/`.

## Tests y CI

- `tests/` usa Catch2. Los tests de interfaz llaman a las mismas funciones que los gestos del ratón, así que se ejecutan sin pantalla.
- `tests/Measurement.h` reúne las herramientas de medida para los tests de los módulos: respuesta en frecuencia exacta a partir de la respuesta al impulso, ajuste de seno, armónicos y aliasing, tiempos de respuesta y detector de clics. Cada herramienta se comprueba con señales de resultado conocido (`MeasurementTests.cpp`); los métodos están en `docs/dsp/README.md`.
- La etiqueta `[.snapshot]` genera capturas PNG si se define `PM_SNAPSHOT_DIR`.
- La etiqueta `[.cpu]` mide el uso de CPU de una instancia (48 kHz, bloques de 512): el plugin con los ajustes por defecto y cuatro módulos activos con oversampling a 2x, 8x y 16x. Por ahora solo informa; en la fase 6 se compara con el presupuesto.
- GitHub Actions:
  - **Windows:** build Release con Ninja y MSVC, tests, uso de CPU (tabla en el resumen de la ejecución), pluginval al nivel 10 y el `.vst3` como artefacto. Además genera el instalador (`installer/PMelodies.iss`, Inno Setup) y lo prueba en el propio runner con `installer/test-installer.ps1`: instalación silenciosa, ficheros idénticos al build, reinstalación encima y desinstalación limpia.
  - **Linux:** build y tests.
  - **Caché de compilación:** los dos jobs guardan ccache entre ejecuciones, así que cada push solo recompila lo que ha cambiado (JUCE y Catch2 salen de la caché). Linux guarda también los paquetes `.deb`. Un push nuevo a la misma rama cancela la ejecución anterior.
  - Cada tag `v*` publica una release.
