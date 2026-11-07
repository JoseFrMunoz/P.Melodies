# Referencia: FabFilter Volcano 3

Investigación pedida por P.P. el 2026-11-07 para una posible ampliación del EQ. **Pendiente: no se implementa nada hasta que P.P. diga qué partes quiere y en qué fase** (ver §13 de la especificación).

Regla 8: se puede emular el *comportamiento*, nunca el código, el nombre, los nombres de sus estilos, los logos ni la interfaz.

## Qué es

Volcano 3 (noviembre de 2021) es un **filtro creativo** con modulación, no un EQ de mezcla como el de la fase 1. Se usa sobre todo para barridos, resonancias y movimiento en sintetizadores, baterías y efectos.

## Funciones

Fuentes: la ayuda oficial de FabFilter, la nota de prensa y varias reseñas (lista al final). La web de FabFilter no se pudo abrir desde esta sesión, así que lo de la ayuda oficial viene de los extractos que dan los buscadores.

### Filtros

- **Hasta 4 filtros** a la vez.
- **Formas:** pasa-bajos, pasa-altos, pasa-banda, campana, shelf grave, shelf agudo, notch y pasa-todo (*all-pass*).
- **Pendientes:** de 6 a 48 dB/oct (las reseñas citan 12, 24 y 48; la ayuda oficial añade 6).
- **Frecuencia, pico (resonancia) y ganancia** por filtro, editables arrastrando en la pantalla.
- **Estilos de filtro:** 11 estilos que deciden cómo satura el filtro y cómo se comporta su resonancia. Van de uno completamente lineal, sin saturación, a otros muy agresivos. Con el pico alto, algunos **autooscilan** como los filtros analógicos clásicos, y hay un botón que silencia esa autooscilación cuando no hay señal de entrada. Incluso las campanas y los shelves son no lineales en los estilos con carácter.
- **Drive por filtro:** cada filtro tiene su propio circuito de saturación ajustable.
- **Panorama por filtro:** una reseña lo usa como un «tilt EQ estéreo»: un realce a 1 kHz en un canal y una octava más abajo en el otro.

### Enrutado y estéreo

- **Serie, paralelo o mixto:** varios esquemas de conexión entre los 4 filtros.
- **Estéreo, L/R o M/S:** en L/R y M/S, la mitad de los filtros va a un canal (izquierdo o *mid*) y la otra mitad al otro (derecho o *side*).

### Modulación

- **Fuentes**, sin límite de número:
  - **LFO por pasos** (hasta 16 pasos, con formas por paso).
  - **Generador de envolvente**, disparado por el audio o por MIDI.
  - **Seguidor de envolvente** de la entrada o del sidechain, con un modo *transitorio* que solo reacciona a golpes cortos (por ejemplo, la batería).
  - **Fuentes MIDI** (rueda de modulación, controladores CC, notas).
  - **Controladores XY.**
- **Asignación arrastrando:** se arrastra un «cable» desde la fuente hasta el parámetro de destino (frecuencia, pico, drive…), con una cantidad por conexión.
- **Visualización en vivo** de las fuentes y de cómo se mueven los parámetros modulados.

### Otros

- **Modo de alta calidad** con oversampling 4x (en los estilos no lineales reduce el aliasing).
- Deshacer, rehacer y A/B; presets.

## Qué encajaría en el EQ de P.Melodies

| Idea | Qué aportaría | Choque con lo ya decidido |
|---|---|---|
| **Estilos por banda** (filtro lineal o con carácter y resonancia no lineal) | El EQ pasaría de mezcla a herramienta creativa. Hoy el carácter (*Clean, Subtle, Warm*) es global y va después del filtrado. | Un filtro no lineal genera aliasing: necesita oversampling, y el EQ es de **latencia cero en la v1** (§4). |
| **Drive por banda** | Saturación localizada en una zona de frecuencias. | Mismo problema de oversampling y latencia. Se solapa con la saturación multibanda (fase 3). |
| **Autooscilación con silencio automático** | Sonidos tipo sintetizador con Q muy alta. | Hay que protegerla (nivel, clipper de seguridad). Hoy la Q del EQ es de 0,025 a 40 con filtros estables. |
| **Forma pasa-todo** | Ajustes de fase y efectos tipo *phaser* al modularla. | Ninguno grave: es una forma más. |
| **Modulación de los parámetros de banda** (LFO, envolvente, seguidor, MIDI) | Barridos y movimiento automáticos sin dibujar automatización en FL Studio. | **§10 deja fuera de la v1** la modulación y el EQ dinámico (un seguidor de envolvente sobre la ganancia es un EQ dinámico). |
| **Fuentes MIDI** | Controlar el EQ con el teclado o la rueda de modulación. | El plugin hoy no recibe MIDI (`NEEDS_MIDI_INPUT FALSE`). Cambiarlo afecta a cómo lo ve FL Studio. |
| **Enrutado serie/paralelo y L/R o M/S por banda** | Más combinaciones de bandas. | **§10 deja fuera de la v1** el procesado Mid/Side. |
| **Panorama por banda** | Efectos de ensanche e inclinación estéreo. | Necesita procesar cada canal por separado. |

## Cómo dejarlo preparado sin implementarlo

Aunque se deje para más adelante, la fase 1 puede diseñarse para no cerrar la puerta (§10: «diseña el código para que estas mejoras se puedan añadir sin reescribir lo existente»):

- **Bandas como objetos independientes**, con su propia ruta de proceso. Ahí se podrían añadir más tarde un estilo, un drive o un panorama por banda.
- **Parámetros de banda con suavizado** preparados para recibir una desviación de modulación además del valor del usuario.
- **La cola sin bloqueos de la fase 0** (`dsp::LockFreeFifo`) ya sirve para enviar a la interfaz los valores modulados y dibujarlos en vivo.

## Decisiones pendientes para P.P.

1. **Qué partes interesan:** estilos, drive, autooscilación, modulación, MIDI, enrutado o panorama.
2. **Cuándo:** como ampliación del EQ en la fase 1, como fase propia tras la 1.0.0 o en la v2 junto con lo que ya está en §10.
3. **Si se acepta latencia en el EQ** cuando se use un estilo no lineal (oversampling), o si esos estilos se quedan sin oversampling.

## Fuentes

- FabFilter, ayuda en línea de Volcano 3: [controles de los filtros](https://fabfilter.com/help/volcano/using/filtercontrols), [seguidor de envolvente](https://www.fabfilter.com/help/volcano/using/ef) y [ayuda general](https://fabfilter.com/help/volcano).
- Nota de prensa: [Sonicstate, 2021-11-19](https://sonicstate.com/amped/2021/11/19/fabfilter-releases-fabfilter-volcano-3/) y [Recording Magazine](https://www.recordingmag.com/news/fabfilter-releases-fabfilter-volcano-3/).
- Reseñas: [MusicTech](https://musictech.com/reviews/plug-ins/fabfilter-volcano-3-review/), [Mix (Barry Rudolph)](https://www.mixonline.com/technology/reviews/plug-ins/fabfilter-volcano-3-plug-in-a-real-world-review), [Producer Sphere](https://producersphere.com/fabfilter-volcano-3-review), [MusicRadar](https://www.musicradar.com/news/fabfilter-volcano-3-plugin).
