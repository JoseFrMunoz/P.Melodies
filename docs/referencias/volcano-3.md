# Referencia: FabFilter Volcano 3

Investigación pedida por P.P. el 2026-11-07 para una posible ampliación del EQ. **Pendiente: no se implementa nada hasta que P.P. diga qué partes quiere y en qué fase** (ver §13 de la especificación).

Regla 8: se puede emular el *comportamiento*, nunca el código, el nombre, los nombres de sus estilos, los logos ni la interfaz.

## Qué es

Volcano 3 (noviembre de 2021) es un **filtro creativo** con modulación, no un EQ de mezcla como el de la fase 1. Se usa sobre todo para barridos, resonancias y movimiento en sintetizadores, baterías y efectos.

## Funciones

Fuente principal: el **manual oficial** (`ffvolcano3-manual.pdf`, leído el 2026-11-07). Lo que sale solo de reseñas se indica.

### Filtros

- **Hasta 4 filtros** a la vez, editables arrastrando en la pantalla (con analizador de espectro y teclado de piano opcional).
- **Formas:** pasa-bajos, pasa-altos, pasa-banda, campana, shelf grave, shelf agudo, notch y pasa-todo.
- **Pendientes:** 6, 12, 24 o 48 dB/oct.
  - En campanas y shelves, la pendiente hace el filtro más ancho o más estrecho.
  - Pasa-banda y notch no admiten 6 dB/oct.
- **Controles por filtro:**
  - **Frecuencia**, con un anillo de **panorama**: desplaza la frecuencia central hacia abajo en un canal y hacia arriba en el otro, para efectos estéreo.
  - **Pico (*Peak*):** en pasa-bajos, pasa-altos y pasa-banda es la **resonancia**; con poca da tonos más cálidos y en lo más alto casi todos los estilos **autooscilan**. En campana y shelves controla **a la vez la ganancia y la resonancia**, y puede ser negativo.
  - **Drive por filtro** (nuevo en la versión 3): ajusta la **saturación interna** de ese filtro.
  - **Estilo** (ver abajo), **retardo** de salida de hasta 50 ms (para peine, Haas, chorus y flanger combinados con filtros en paralelo), **nivel y panorama** de salida, y botón de activar/anular.
- **Drive global:** fija cuánto se excita la entrada de los filtros, de limpio a sucio. Además, subiendo la ganancia de entrada y bajando la de salida (±36 dB cada una) se sobrecargan los filtros.
- **Estilos:** 11, que deciden el carácter de la saturación y de la resonancia. Incluso las campanas y los shelves son **muy no lineales** en todos ellos salvo el lineal. Resumen de lo que dice el manual:
  1. el filtro original de su sintetizador FabFilter One;
  2. suave y cremoso;
  3. con mucha sobrecarga y carácter propio, para guitarras distorsionadas;
  4. distorsión moderada con un silbido limpio;
  5. distorsión moderada y jugosa, con bastante autooscilación en graves;
  6. extremo, para ideas salvajes;
  7. suave y limpio, de uso general;
  8. válvula: más cálido, con buena sobrecarga, para sintetizadores;
  9. metálico: más áspero y afilado;
  10. una versión más suave del de válvula;
  11. **lineal**: sin drive ni recorte.

  Sus nombres no se pueden usar (regla 8).

### Enrutado y estéreo

- **Serie o paralelo:** el número de esquemas depende de cuántos filtros haya y del modo.
- **Estéreo, L/R o M/S:** en L/R y M/S, la mitad de los filtros va a un canal (izquierdo o *mid*) y la otra mitad al otro (derecho o *side*); los filtros se crean y borran por parejas y el panorama por filtro se desactiva.
- **Mezcla** seca/procesada global.

### Modulación

- **Fuentes:**
  - **XLFO:** LFO por pasos, añadiendo o quitando pasos libremente, con deslizamiento por paso, sincronía y disparo por notas MIDI (modo *Retrigger* o *Legato*).
  - **Generador de envolvente** ADSR con curvas ajustables. Se dispara con la entrada principal, con el sidechain externo o con notas MIDI.
  - **Seguidor de envolvente** del nivel o de los transitorios (modo *Transient*: solo responde a golpes cortos).
  - **Fuentes MIDI** y hasta **6 controladores XY o deslizadores**.
- **Destinos:** casi todos los parámetros de filtro (drive, frecuencia, pico, panorama, retardo, nivel y panorama de salida), además de la entrada, la salida y la mezcla.
- **Asignación arrastrando** desde la fuente hasta el destino, con visualización en vivo de fuentes y destinos.

### Otros

- **Alta calidad (*High Quality*):** oversampling de los algoritmos internos para reducir el aliasing de las no linealidades, a cambio de más CPU. El manual no da el factor (una reseña habla de 4x) ni menciona la latencia.
- **Silencio automático de la autooscilación (*Auto Mute Self-Osc*):** silencia la autooscilación cuando la entrada de los filtros queda en silencio; si no, con el pico al máximo, los filtros seguirían sonando tras parar el audio.
- Bypass global sin clics, deshacer, rehacer, A/B y presets.

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
| **Panorama por banda** (la frecuencia central baja en un canal y sube en el otro) | Efectos de ensanche e inclinación estéreo. | Necesita procesar cada canal por separado. |
| **Ganancia y resonancia en un solo control** (al realzar más, la campana se estrecha y resuena más) | Realces más «analógicos» y agresivos. | En §4 la Q es independiente de la ganancia: sería un modo opcional por banda. Pro-Q 4 también tiene una interacción ganancia-Q opcional, más suave. |
| **Retardo por banda** (hasta 50 ms, con bandas en paralelo) | Peine, Haas, chorus y flanger. | Lejos de un EQ: encajaría mejor en un módulo de efectos aparte. |

## Saturar la frecuencia realzada

Investigación añadida el 2026-11-07, a petición de P.P.

### Qué hace Volcano 3

Según el manual:

- **La saturación está dentro del filtro:** cada filtro tiene su **drive** propio, que ajusta su «saturación interna», y los filtros son no lineales incluso en campana y shelf (salvo el estilo lineal).
- **En campana y shelf, el pico controla a la vez la ganancia y la resonancia.** El realce sale de la resonancia del propio filtro no lineal.
- **El estilo decide cómo satura:** de suave y cremoso a metálico y áspero.
- **El oversampling (*High Quality*) existe** precisamente para el aliasing que generan esas no linealidades.

Deducción (FabFilter no publica la técnica interna): si el realce es la resonancia de un filtro que satura por dentro, lo que más se distorsiona es justo **la frecuencia realzada**, y más cuanto más fuerte llega o más alto está el pico. El resto del espectro pasa casi limpio. Es el comportamiento de un filtro analógico llevado al límite: se parece a la opción B de abajo.

**Ejemplo real (reseña de Mix):** una campana a 770 Hz con un estilo agresivo y drive +2 dB, sobre un bajo, para darle mordida a esa zona.

### Cómo se podría hacer en P.Melodies

**A. Saturar solo la parte que suma la campana (o el shelf).** Es la opción recomendada.

- **La idea:** un filtro de campana de 2.º orden se puede escribir exactamente como *la entrada más una componente pasa-banda escalada por la ganancia* (descomposición de Regalia y Mitra). Esa componente es justo «lo que añade el realce».
- **El procesado:** se satura esa componente y se suma a la entrada limpia: `salida = entrada + S(realce)`.
- **Propiedades:**
  - Con el drive a 0, `S` no actúa y el resultado es bit a bit el del EQ lineal: se puede comprobar con un test.
  - A volumen bajo, la curva es la misma que sin saturación.
  - Solo se distorsiona el contenido de la banda realzada, y más cuanto más fuerte llega: el comportamiento de Volcano.
  - Los armónicos nuevos (2f, 3f…) salen de la frecuencia realzada.
- **Shelves:** se descomponen igual, en entrada más componente de realce.
- **Cortes:** saturar una banda que atenúa no tiene un sentido claro. Propuesta: solo en realces (decisión de P.P.).
- **Curvas de saturación:** se pueden reutilizar las de la saturación multibanda (fase 3) cuando existan. Por eso tiene sentido hacerlo después de la fase 3.

**B. Saturación dentro del filtro resonante**, para pasa-bajos y pasa-altos con pico y, al estilo de Volcano, también para campanas y shelves cuyo realce sale de la resonancia:

- **Cómo:** se mete la curva de saturación en el lazo de resonancia (y en los integradores) de un filtro de estado variable (SVF) o tipo *ladder*. Es lo que hacen los filtros analógicos de sintetizador.
- **Efecto:**
  - El pico se comprime y se ensucia al subir el nivel, y la autooscilación queda acotada sola.
  - La curva cambia con el nivel: más analógico, pero menos predecible que la opción A.
- **Coste:**
  - Es más complejo: el lazo sin retardo con una no linealidad exige resolver una ecuación en cada muestra (Newton-Raphson o aproximaciones).
  - Usa más CPU.
  - Es más difícil de probar con exactitud que la opción A.

**Aliasing (en ambas opciones).** Los armónicos nuevos se pliegan por debajo de Nyquist (aliasing) si la banda está alta. Hay dos caminos, y es una decisión que cambia la latencia:

1. **Oversampling** (ya existe en la fase 0):
   - Es lo más limpio.
   - Añade latencia, y por la decisión de §12 la latencia declarada debe ser siempre la máxima. El EQ dejaría de ser de latencia cero incluso con la saturación apagada.
2. **ADAA** (*antiderivative antialiasing*):
   - Reduce el aliasing sin oversampling y casi sin latencia (media muestra en la versión de 1.er orden).
   - Esa media muestra desalinea un poco el realce respecto a la entrada limpia. Habría que compensarla o aceptarla y medirla.

### Cómo se probaría

Con las herramientas de medida de la fase 0 (`tests/Measurement.h`):

- **Drive a 0:** salida idéntica al EQ lineal, bit a bit.
- **Nivel bajo:** la respuesta coincide con la curva lineal con una tolerancia de 0,1 dB.
- **Tono en la frecuencia central:** la THD sube con el drive y con el nivel de entrada.
- **Tono a dos octavas de la banda:** sin distorsión medible (por debajo de −100 dB). Esto confirma que solo satura la zona realzada.
- **Aliasing:** por debajo del umbral que se fije, con la técnica elegida.
- **Valores extremos:** sin NaN, Inf ni inestabilidad con ganancia, Q y drive al máximo.

## Cómo dejarlo preparado sin implementarlo

Aunque se deje para más adelante, la fase 1 puede diseñarse para no cerrar la puerta (§10: «diseña el código para que estas mejoras se puedan añadir sin reescribir lo existente»):

- **Bandas como objetos independientes**, con su propia ruta de proceso. Ahí se podrían añadir más tarde un estilo, un drive o un panorama por banda.
- **Parámetros de banda con suavizado** preparados para recibir una desviación de modulación además del valor del usuario.
- **La cola sin bloqueos de la fase 0** (`dsp::LockFreeFifo`) ya sirve para enviar a la interfaz los valores modulados y dibujarlos en vivo.

## Decisiones pendientes para P.P.

1. **Qué partes interesan:** estilos, drive, autooscilación, modulación, MIDI, enrutado o panorama.
2. **Cuándo:** como ampliación del EQ en la fase 1, como fase propia tras la 1.0.0 o en la v2 junto con lo que ya está en §10.
3. **Si se acepta latencia en el EQ** cuando se use un estilo no lineal (oversampling), o si esos estilos se quedan sin oversampling.
4. **Saturación de la frecuencia realzada:**
   - si se satura solo en realces o también en cortes;
   - si se hace con la opción A (realce de campana y shelves), con la B (resonancia) o con ambas;
   - si el aliasing se trata con oversampling (con latencia) o con ADAA (sin latencia).

## Fuentes

- FabFilter, *Volcano 3 User Manual* ([PDF](https://www.fabfilter.com/downloads/pdf/help/ffvolcano3-manual.pdf)): fuente principal.
- FabFilter, ayuda en línea de Volcano 3: [controles de los filtros](https://fabfilter.com/help/volcano/using/filtercontrols), [seguidor de envolvente](https://www.fabfilter.com/help/volcano/using/ef) y [ayuda general](https://fabfilter.com/help/volcano).
- Nota de prensa: [Sonicstate, 2021-11-19](https://sonicstate.com/amped/2021/11/19/fabfilter-releases-fabfilter-volcano-3/) y [Recording Magazine](https://www.recordingmag.com/news/fabfilter-releases-fabfilter-volcano-3/).
- Reseñas: [MusicTech](https://musictech.com/reviews/plug-ins/fabfilter-volcano-3-review/), [Mix (Barry Rudolph)](https://www.mixonline.com/technology/reviews/plug-ins/fabfilter-volcano-3-plug-in-a-real-world-review) y [su versión en barryrudolph.com](https://barryrudolph.com/mix/fab_filter_volcano3.html), [Producer Sphere](https://producersphere.com/fabfilter-volcano-3-review), [MusicRadar](https://www.musicradar.com/news/fabfilter-volcano-3-plugin), [notas de versión (Toolfarm)](https://www.toolfarm.com/?p=16612).

Técnicas de DSP (literatura pública, para la propuesta propia):

- P. A. Regalia, S. K. Mitra, "Tunable Digital Frequency Response Equalization Filters", *IEEE Transactions on Acoustics, Speech, and Signal Processing* 35(1), 1987: campana como entrada más una componente pasa-banda.
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2.ª ed., Wiley, 2011: filtros de ecualización paramétrica y procesado no lineal.
- V. Zavalishin, *The Art of VA Filter Design*, Native Instruments, 2.ª ed., 2018: filtros SVF y *ladder* sin retardo en el lazo, también no lineales.
- A. Huovilainen, "Non-Linear Digital Implementation of the Moog Ladder Filter", *DAFx-04*, 2004: saturación dentro del lazo de resonancia.
- J. D. Parker, V. Zavalishin, E. Le Bivic, "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution", *DAFx-16*, 2016, y S. Bilbao, F. Esqueda, J. D. Parker, V. Välimäki, "Antiderivative Antialiasing for Memoryless Nonlinearities", *IEEE Signal Processing Letters* 24(7), 2017: ADAA.
