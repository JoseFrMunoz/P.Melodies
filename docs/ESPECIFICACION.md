# P.Melodies — Especificación

> Especificación del producto y plan de desarrollo por fases. Las reglas de trabajo (antes sección 1)
> están en [`CLAUDE.md`](../CLAUDE.md). Las rutas a `docs/design/` son relativas a la raíz del repositorio.

---

## 0. Contexto

Vas a construir **P.Melodies**, un plugin de audio VST3 para FL Studio en Windows. Reúne en una sola ventana cuatro módulos de efectos reordenables:

1. **EQ** paramétrico, inspirado en el funcionamiento de FabFilter Pro-Q (la versión 4: sus modos de carácter son los de §4).
2. **Compresor**, inspirado en FabFilter Pro-C 2.
3. **Saturación multibanda**, inspirada en FabFilter Saturn 2.
4. **Distorsión** de banda completa, inspirada en Distructor de FL Studio, con Fruity Fast Dist, Fruity WaveShaper y Saturn 2 como apoyo (decidido el 2026-11-07; P.P. puede cambiarla si no le convence).

Los manuales de todas las referencias están en [`docs/referencias/README.md`](referencias/README.md).

**Quién lo pide:** P.P., desarrollador (Grado Superior DAM) que programa en Java y Python, **no en C++**, y produce música en FL Studio. El proyecto irá a su GitHub como pieza de portafolio, así que el código, los commits y la documentación deben ser de calidad profesional.

**Cómo se compila:** P.P. **no va a instalar Visual Studio**. Toda la compilación de Windows se hace en **GitHub Actions**, y P.P. descarga el `.vst3` desde los artefactos del workflow para probarlo en FL Studio. Puedes compilar y pasar tests en tu propio entorno (Linux) para iterar rápido, pero la referencia de "funciona" es el build de Windows en CI.

**Referencias visuales:** están en `docs/design/`:
- `png/*.png` — capturas de cada pantalla. Míralas antes de escribir cualquier interfaz.
- `html/*.html` — las mismas pantallas en HTML estático. Ahí están los valores exactos de colores, tamaños, espaciados y tipografías.
- Pantallas: `01-vista-general-eq`, `02-compresor`, `03-saturacion-multibanda`, `03b-saturacion-menu-estilos-abierto`, `04-distorsion`, `05-presets-y-ajustes`, `06-componentes-e-interacciones`, `07-plantillas-de-aspecto`.

**Funcionalidad e interacciones de las referencias:** obligatorias. **Colores:** la paleta de marca «Obsidiana y oro» está decidida (§12). **El resto del estilo visual:** provisional, P.P. puede cambiarlo más adelante. Por eso todos los colores, fuentes y medidas están centralizados en un único sistema de tema y se cambian sin tocar los componentes.

---

## 2. Producto y tecnología

| Aspecto | Decisión |
|---|---|
| Nombre | P.Melodies |
| Formato v1 | VST3, Windows x64. macOS queda para más adelante, pero no cierres la puerta: código multiplataforma. |
| Framework | **JUCE 8** con CMake (fija la versión con `FetchContent` o CPM). |
| Lenguaje | C++20 |
| Licencia | **AGPLv3**, la licencia gratuita de JUCE 8 para código abierto. Añade `LICENSE` y cabeceras. |
| Canales | Mono→mono y estéreo→estéreo, más un bus de **sidechain** estéreo opcional que FL Studio detecte. |
| Frecuencias de muestreo | 44,1 a 192 kHz. Debe funcionar igual en todas. |
| Tamaños de buffer | Cualquiera, incluidos 1 muestra y tamaños impares o variables entre llamadas. |
| Idioma | Identificadores y comentarios en inglés. Interfaz en español. README en español e inglés. |

---

## 3. Arquitectura

### 3.1 Núcleo
- `AudioProcessorValueTreeState` para todos los parámetros. Todos son **automatizables** desde FL Studio, con nombres legibles y unidades. El orden de la cadena no es un parámetro (§12, 2026-11-11).
- Interfaz común `Module` con `prepare(spec)`, `reset()`, `process(block, sidechain)`, `getLatencySamples()` y `setBypassed(bool)`. Los cuatro módulos la implementan.
- **Cadena reordenable:** el orden se guarda en el estado del plugin como una lista de módulos (`eq,compressor,saturation,distortion`), así se guarda con el proyecto y los presets. No es automatizable (decisión del 2026-11-11, §12). El diseño debe poder ampliarse a más módulos en el futuro sin romper presets ni proyectos guardados: un estado sin un módulo nuevo lo añade al final. Orden por defecto: EQ → Compresor → Saturación → Distorsión.
- **Cambio de orden sin clics:** al reordenar, haz un fundido cruzado de 20 ms entre la cadena antigua y la nueva, procesando ambas durante el fundido.
- **Latencia:** reporta a FL Studio una latencia **constante**, la máxima posible según los ajustes que la generan (lookahead y oversampling), y compensa internamente con retardos para que no cambie al mover parámetros ni al reordenar. La ruta *dry* del mix global debe ir alineada.
- **Oversampling:** usa `juce::dsp::Oversampling`. Cada módulo que lo necesite tiene su selector. Ajuste global de calidad: un factor *al reproducir* y otro *al exportar* (detectado con `isNonRealtime()`), que se aplica como mínimo.
- **Tiempo real:** nada de reservas de memoria, locks, I/O ni logs en el hilo de audio. Comunicación audio→interfaz (medidores, analizador, curvas) con atómicos o FIFOs lock-free.
- **Suavizado** de todos los parámetros continuos (`SmoothedValue` o equivalente) para que mover knobs o automatizar no produzca chasquidos ni *zipper noise*.
- Protección contra denormales y comprobaciones de NaN/Inf en modo debug.

### 3.2 Sección global (pie de ventana)
- **Ganancia de entrada y salida:** ±24 dB, con medidores de pico y RMS, retención de pico e indicador de clip que se resetea con un clic.
- **Mix global:** 0–100 %, dry/wet de todo el plugin, con el dry compensado en latencia.
- **Comparar a igual volumen:** al activarlo, el bypass se escucha con la misma sonoridad que el procesado (iguala por RMS integrado durante unos segundos), para comparar sin que lo más fuerte parezca mejor.
- **Clipper de seguridad:** activado por defecto, techo de −0,3 dBFS, clipping suave. Se puede desactivar. Protege de picos, sobre todo del feedback de la saturación.
- **Calidad:** botón que abre los ajustes de oversampling global.
- **Indicador de latencia** en ms.

---

## 4. Módulo 1 — EQ (referencia: `01-vista-general-eq.png`)

**Comportamiento a emular:** EQ paramétrico de fase mínima, con edición directa sobre la curva.

- **Bandas:** hasta 8.
- **Tipos:** campana, shelf grave, shelf agudo, corte de graves, corte de agudos, notch, pasa-banda y tilt shelf.
- **Rangos:**
  - Frecuencia de 10 Hz a 30 kHz, limitada por debajo de Nyquist.
  - Ganancia ±30 dB.
  - Q de 0,025 a 40.
  - Pendiente en cortes de graves y agudos: 6, 12, 18, 24, 36, 48, 72 y 96 dB/oct. En los demás tipos el knob de pendiente aparece desactivado.
- **Calidad del filtro:** respuesta fiel a la analógica también cerca de Nyquist, con filtros que corrijan el "encogimiento" de la curva en agudos. Usa filtros estables con automatización rápida (por ejemplo SVF/TPT) y documenta la elección en `docs/dsp/eq.md`.
- **Latencia cero** en v1.
- **Carácter:**
  - *Clean:* transparente.
  - *Subtle:* saturación suave tipo transformador.
  - *Warm:* saturación más marcada tipo válvula, con armónicos pares.
  - Se aplica tras el filtrado y es muy sutil a niveles normales.
- **Analizador de espectro:** pre, post u off. FFT con ventana, promediado suave y pendiente de visualización de 4,5 dB/oct. Escala de la curva de ±6, ±12 o ±30 dB (§12, revisión del 2026-11-11).
- **Interacción** (ver pantalla 01 y 06):
  - Doble clic en zona vacía crea una banda. El tipo depende de la posición: cerca de los extremos crea shelf o corte.
  - Arrastrar un punto mueve frecuencia y ganancia.
  - La rueda cambia la Q; en los cortes, la pendiente (un paso por muesca), y Ctrl+rueda, la Q. Shift: pasos finos de Q.
  - Alt+clic hace bypass de la banda.
  - Al seleccionar una banda se muestra su panel: tipo, frecuencia, ganancia, Q, pendiente, *Solo* (escucha solo esa banda) y *Bypass*, con la papelera para eliminarla.
  - **Mini panel** junto al punto seleccionado (revisión de P.P., §12): bypass, tipo (despliega las 8 formas), solo mientras se mantiene pulsado (arrastrándolo a los lados se mueve la frecuencia y, con Ctrl, en vertical la Q; dos líneas marcan el rango que se oye y el resto se atenúa), eliminar, y frecuencia con su nota, ganancia (o pendiente) y Q. Sustituye a la etiqueta flotante. Va encima del punto, o debajo si no cabe, y lo sigue. Se cierra con un clic en el fondo o con Esc.
  - Se dibuja la contribución de la banda seleccionada (relleno) y la curva total.

---

## 5. Módulo 2 — Compresor (referencia: `02-compresor.png`)

**Comportamiento a emular:** compresor con estilos que dependen del programa y una visualización clara de qué hace.

- **Estilos:** 8 algoritmos propios. Documenta el diseño de cada uno en `docs/dsp/compresor.md`.
  - *Clean:* limpio y preciso, con poca dependencia del programa.
  - *Classic:* respuesta más "analógica", con release dependiente del programa.
  - *Opto:* lento, knee muy suave y release que depende de la historia de la reducción.
  - *Vocal:* pensado para voz, se adapta para mantenerla delante y estable.
  - *Mastering:* suave y transparente, para mezclas completas.
  - *Bus:* "pegamento" para grupos y buses.
  - *Punch:* preserva transientes y da pegada.
  - *Pumping:* bombeo exagerado tipo EDM.
- **Parámetros:**

| Parámetro | Rango |
|---|---|
| Threshold | −60 a 0 dB |
| Ratio | 1:1 a 100:1, más ∞:1 |
| Knee | 0 a 72 dB |
| Range | 0 a 60 dB, limita la reducción máxima |
| Attack | 0,005 a 250 ms |
| Release | 10 a 2500 ms, con interruptor de auto-release |
| Lookahead | 0 a 20 ms |
| Hold | 0 a 500 ms |
| Makeup | −12 a +36 dB, con interruptor de auto-gain |
| Mix | 0 a 100 %, para compresión paralela |

- **Sidechain:**
  - Fuente interna o externa: el bus de sidechain de FL Studio, para el típico bombeo con el kick.
  - EQ del sidechain con pasa-altos (20 Hz–2 kHz, por defecto 80 Hz a 24 dB/oct), pasa-bajos (1–20 kHz) y una banda de campana libre.
  - Botón *Escuchar* para oír la señal que dispara el compresor.
- **Oversampling:** off, 2x o 4x.
- **Visualización:**
  - Gráfico que se desplaza en el tiempo con el nivel de entrada, el de salida y la línea de reducción de ganancia, más la línea del threshold.
  - Curva de transferencia con el knee y un punto que marca el nivel actual.

---

## 6. Módulo 3 — Saturación multibanda (referencia: `03-…png` y `03b-…png`)

**Comportamiento a emular:** saturación por bandas con estilos analógicos y controles independientes por banda.

- **Bandas:** de 1 a 6. Botón *+ Banda*. Crossovers arrastrables en la pantalla.
- **Pendiente de los crossovers:** 6, 12, 24 o 48 dB/oct. La suma de bandas sin procesar debe reconstruir la señal: respuesta plana en magnitud, con compensación de fase entre bandas. Añade un test que lo verifique.
- **Por banda:**

| Control | Detalle |
|---|---|
| Estilo | 16 estilos en 6 categorías: Cinta (limpia, cálida, saturada), Válvula (limpia, cálida, saturada), Transformador (suave, grueso), Suave (sutil, suave, fuerte), Clipper (suave, duro) y Amp (limpio, crunch). Desplegable en rejilla, como en `03b`. |
| Drive | 0 a 24 dB. También se ajusta arrastrando arriba o abajo dentro de la banda. |
| Mix | 0 a 100 % |
| Feedback | 0 a 100 %, con su frecuencia de resonancia (20 Hz–20 kHz). Debe ser estable: limita internamente para que no se dispare. |
| Dynamics | −100 a +100. En negativo, el drive baja con el nivel y los transientes salen más limpios. En positivo, sube con el nivel. |
| Tono | Graves, medios, agudos y presencia, ±12 dB cada uno, aplicado a la señal procesada. |
| Nivel | ±24 dB |
| Solo y bypass | por banda |

- **Oversampling** seleccionable para evitar aliasing.
- **Pantalla:** espectro tenue de fondo, cada banda con su nombre, estilo y una barra de drive, la línea de nivel por banda y los crossovers con su frecuencia.

---

## 7. Módulo 4 — Distorsión (referencia: `04-distorsion.png`)

**Comportamiento a emular:** distorsión de banda completa con varios algoritmos, tomando como referencia Distructor de FL Studio (y, para modos que no tenga, Fruity Fast Dist, Fruity WaveShaper y los estilos de efectos de Saturn 2).

- **Modos:** Hard clip, Foldback, Rectificador, Fuzz, Bitcrusher y Downsample, mostrados como tarjetas con su curva.
- **Parámetros:**

| Parámetro | Rango |
|---|---|
| Pre HP | 20 Hz a 2 kHz |
| Pre LP | 1 a 20 kHz |
| Drive | 0 a 48 dB |
| Tono | ±12 dB, inclinación oscuro↔brillante |
| Mix | 0 a 100 % |
| Salida | ±24 dB |
| Bits | 1 a 16, solo en Bitcrusher |
| Frecuencia | 100 Hz a 48 kHz, solo en Downsample |

- *Bits* y *Frecuencia* se muestran desactivados en los demás modos.
- **Oversampling:** off, 2x, 4x u 8x. En Bitcrusher y Downsample el aliasing forma parte del efecto: documenta cómo se trata.
- **Pantallas:** curva de transferencia del modo actual y forma de onda de entrada frente a salida.

---

## 8. Interfaz, presets y ajustes

### 8.1 Ventana y navegación (pantallas 01 y 06)
- Tamaño base **1000×640 px**, escalable al 75, 100, 125 y 150 %. El tamaño se recuerda.
- **Cabecera:** logo y nombre, preset anterior y siguiente, desplegable de presets, botón de **guardar preset**, A/B con copiar A→B, deshacer, rehacer, tema y ajustes.
- **Panel izquierdo "Cadena de señal":**
  - Lista vertical de los 4 módulos en el orden real, entre las marcas IN y OUT.
  - Cada fila lleva asa de arrastre, color del módulo, nombre, un resumen de estado y el botón de encendido (bypass).
  - **Reordenado en vivo** (§12, 2026-11-11): al arrastrar desde el asa, los demás módulos se apartan deslizándose en cuanto el arrastrado pasa la mitad de su hueco, y un hueco punteado marca dónde quedará. Al soltar, en cualquier punto, el módulo se desliza a ese hueco; no hace falta apuntar. El orden llega al plugin al soltar, como un solo gesto (y un solo paso de deshacer en la fase 5). Los cambios de orden desde fuera (cargar un proyecto con la ventana abierta y, en la fase 5, presets y deshacer) también se animan.
  - El clic abre el módulo en el panel derecho.
  - Estados: normal, seleccionado, apagado y arrastrando (pantalla 06).
- **Panel derecho:** solo el módulo seleccionado, a tamaño completo.
- **Pie:** la sección global (3.2).

### 8.2 Componentes (pantalla 06)
- **Knob:**
  - Arrastrar arriba o abajo ajusta el valor; con Ctrl, el ajuste es fino.
  - **Doble clic vuelve al valor por defecto.**
  - Alt+clic permite escribir el valor; la rueda ajusta en pasos pequeños.
  - Al arrastrar aparece un tooltip con el valor.
  - Estados: normal, hover, arrastrando y escribiendo.
- **Tooltips de ayuda** en cada control, tras 0,6 s de hover, desactivables en ajustes.
- **Atajos** (solo con la ventana del plugin enfocada): Ctrl+Z deshacer, Ctrl+Y rehacer, A/B alterna la comparación, teclas 1–4 abren los módulos y Esc cierra menús.
- **Prohibido Ctrl+S o cualquier atajo para guardar presets:** entraría en conflicto con el guardado del proyecto de FL Studio. Los presets **solo** se guardan con el botón de guardar.

### 8.3 Presets (pantalla 05)
- Navegador con buscador, categorías (Todos, Favoritos, Init, Voz, 808, Melodía, Bus de batería y Míos), favoritos con estrella y doble clic para cargar.
- Botones *Guardar como…*, *Importar* y *Abrir carpeta*.
- Los presets de fábrica van embebidos en el plugin y **no se pueden sobrescribir**. Los del usuario se guardan en `Documentos/P.Melodies/Presets/` en XML legible, con extensión `.pmpreset` y número de versión, para migrarlos en el futuro.
- Un preset guarda todos los parámetros, incluido el orden de la cadena.
- **Presets de fábrica:** un *Init* neutro, que no altera el audio, y al menos 3 ejemplos en cada categoría: Voz, 808, Melodía y Bus de batería.
- **A/B:** dos estados completos independientes. **Deshacer y rehacer:** con `UndoManager`, agrupando cada gesto de arrastre en un solo paso.

### 8.4 Ajustes (pantalla 05)
- Tamaño de ventana.
- Tema.
- Calidad: oversampling al reproducir (off, 2x o 4x) y al exportar (2x, 4x, 8x o 16x).
- Clipper de seguridad y su techo.
- Tooltips.
- Versión, licencia y enlace al repositorio.

### 8.5 Temas (pantalla 07)
- Cuatro plantillas: **Oscuro** (por defecto), **Claro**, **Neón** y **Vintage**. Los valores de cada una están en `07-plantillas-de-aspecto.html`.
- Todo color sale de un objeto de tema con tokens: fondo, panel, elevado, borde, texto, texto secundario, acento y los colores de cada módulo. El tema se cambia en caliente.
- **Fuentes:** Inter (interfaz), JetBrains Mono (valores) y Space Grotesk (títulos), embebidas como BinaryData, con sus licencias OFL en `licenses/`.
- **Paleta del tema Oscuro: «Obsidiana y oro»**, la paleta de marca de P.P. (§12). Base casi negra con un matiz frío (`#06070A` a `#262932`), textos cálidos (`#EDE9E0`, `#D2CEC4`, `#9B978E`) y **oro viejo `#D4AF37` como firma** (acento).
- **Colores de módulo:** EQ oro `#D4AF37` (el mismo que la firma), Compresor acero `#8FA6BF`, Saturación cobre `#C8743C` y Distorsión granate `#AD374E`.

---

## 9. Fases de desarrollo

**Cómo empieza cada fase de módulo (1 a 4)** (acuerdos de §12):

1. Visto bueno de P.P. a la fase anterior y rama nueva `phase/N-…`.
2. Leer la sección del módulo en esta especificación, sus pantallas de `docs/design/` y los **manuales de sus plugins de referencia** (`docs/referencias/README.md`), solo para emular su comportamiento (regla 8).
3. **Preguntar a P.P.** las dudas de ese módulo, con una recomendación en cada una, y esperar sus respuestas antes de programar. Las preguntas de un módulo no se adelantan a fases anteriores.

**Cómo termina:** con el cierre de la regla 2 (también el instalador `.exe` como artefacto) y **se detiene a esperar el visto bueno**.

Lo que no está en la fase correspondiente no se hace: las peticiones de §13 esperan a que P.P. les asigne una fase.

### Fase 0 — Base común
**Estado:** visto bueno de P.P. el 2026-11-11 tras probarla en FL Studio. De esa prueba salieron dos cambios, ya hechos, que P.P. comprobará durante la fase 1:
- **Mix global «no funciona»:** funciona, pero en la fase 0 ningún módulo procesa, así que la señal procesada y la original son la misma y cualquier mezcla suena igual. Un test nuevo sigue todo el camino (arrastrar el knob con el ratón → parámetro → mezcla en el audio). Se oirá en cuanto el EQ cambie el sonido.
- **Reordenado en vivo y fluido** en el panel de la cadena (§8.1).

- **Orden de la cadena para más de 4 módulos** (2026-11-11, opción A de P.P.): el texto de §3.1 y §10 que llegó desde `main` pide que la cadena admita más módulos sin romper presets ni proyectos. El orden deja de ser un parámetro de 24 valores y pasa a ser una lista de módulos en el estado (§12). Los proyectos guardados con el parámetro antiguo se leen con su orden.

Todo lo que comparten los módulos. Si falla aquí, fallaría en los cuatro.
- Repo, CMake, JUCE fijado, `LICENSE` AGPLv3, `CLAUDE.md`, `docs/ESPECIFICACION.md`, README inicial y `.gitignore`.
- **CI en GitHub Actions:**
  - Job Windows (`windows-latest`): build Release VST3, tests, pluginval con el nivel de exigencia máximo y subida del `.vst3` como artefacto.
  - Job Linux: build y tests.
  - Cada tag `v*` crea una GitHub Release con el `.vst3` en un zip.
- Procesador con APVTS, bus de sidechain, 4 módulos *passthrough*, cadena reordenable con fundido, gestión de latencia, oversampling, suavizado y sección global completa (3.2).
- Sistema de tema y componentes base: knob, interruptor, selector segmentado, desplegable, medidor y tooltip.
- Ventana con cabecera, panel de cadena con arrastre y encendido, y pie. Los paneles de módulo quedan vacíos con su título.
- Guardar y restaurar el estado en el proyecto de FL Studio.
- **Añadido después, a petición de P.P. (§12):**
  - **Instalador `.exe`** (Inno Setup, `installer/`): generado en el CI, probado allí (instalar, reinstalar, desinstalar) y adjunto a cada release.
  - **CI rápido:** caché de compilación (ccache) en Windows y Linux, Windows con Ninja y caché de paquetes de Linux. Un push tarda unos 2–2,5 min (antes, de 9 a 15).
  - **Herramientas de medida para los tests de los módulos** (`tests/Measurement.h`): respuesta en frecuencia, ajuste de seno, armónicos y aliasing, tiempos de respuesta y clics.
  - **Medida de CPU** (`Tests "[.cpu]"`), con la tabla en el resumen de cada ejecución del CI. Por ahora solo informa.
  - **Cola sin bloqueos audio → interfaz** (`dsp::LockFreeFifo`) para el analizador y las pantallas de los módulos.
- **Tests:**
  - Passthrough nulo: con todo neutro, la salida es idéntica a la entrada, salvo la latencia declarada.
  - La latencia declarada coincide con la medida por impulso.
  - El reordenado no produce discontinuidades.
  - Ida y vuelta del estado guardado.
  - Todas las frecuencias de muestreo y tamaños de buffer (incluidos 1, impares y variables).
  - Mono y estéreo.
  - Sin NaN/Inf con parámetros extremos.
  - El clipper respeta el techo.
  - Además: las herramientas de medida comprobadas con señales de resultado conocido, y la cola sin bloqueos (orden, llena/vacía, sin reservas de memoria, dos hilos).

### Fase 1 — EQ
Sección 4 completa, DSP e interfaz.
- **Estado:** hecha y en verde en el CI (2026-11-11). Pendiente de las pruebas de P.P. en FL Studio y de su visto bueno. P.P. aprobó todas las recomendaciones de las preguntas del módulo (§12, filas «EQ: …»).
- **Cómo quedó** (detalle del DSP en `docs/dsp/eq.md`):
  - Filtros ajustados a la curva analógica hasta Nyquist: en bandas típicas, menos de 0,3 dB hasta fs/4 en campanas, shelves y tilt.
  - Proceso con SVF TPT; en los deslizamientos, coeficientes en rampa muestra a muestra, sin *zipper*.
  - Cambios de forma con calentamiento y fundido cruzado; encender, apagar o borrar una banda, con fundido de 20 ms.
  - Carácter Subtle y Warm por residuo, con pre- y de-énfasis.
  - Interfaz con la gráfica, el editor de banda y el analizador de la pantalla 01.
  - CPU (Xeon de 2,1 GHz): el EQ con 8 bandas, dos cortes de 96 dB/oct y Warm usa el 0,8 % de un núcleo; con todas las frecuencias automatizadas, el 4,5 %.
- **Referencias:** manual de Pro-Q 4 (sus modos de carácter son los de §4) y, como apoyo, el de Pro-Q 3.
- **Diseño preparado para la ampliación pendiente de §13** (tipo Volcano), sin implementarla:
  - Cada banda es un objeto independiente con su propia ruta de proceso.
  - Los parámetros de banda se suavizan de forma que más adelante puedan recibir una modulación.
- El analizador de espectro recibe las muestras por la cola sin bloqueos de la fase 0.
- **Pruebas manuales pendientes de la fase 0** (van en la lista de cierre de la fase 1):
  - Con una banda del EQ realzada, el Mix global pasa del sonido original (0 %) al procesado (100 %).
  - El reordenado en vivo del panel de la cadena es fluido y sin clics.
- **Tests:**
  - Respuesta de cada tipo de filtro frente a la respuesta analítica, con tolerancia, en varias frecuencias de muestreo.
  - Barrido de automatización de frecuencia y ganancia sin clics.
  - Estabilidad con Q extremas y frecuencias cerca de Nyquist.
  - Solo y bypass de banda.
  - Carácter Clean idéntico a la entrada cuando la ganancia es 0.
  - Se miden con las herramientas de la fase 0: respuesta en frecuencia exacta para la comparación analítica y detector de clics para los barridos.

### Fase 2 — Compresor
Sección 5 completa.
- **Referencias:** manual de Pro-C 2 (sus 8 estilos son los de §5). El de Pro-C 3 solo como contexto: lo que añade queda fuera de la especificación.
- El gráfico de niveles recibe sus datos por la cola sin bloqueos de la fase 0.
- **Tests:**
  - Curva estática (entrada/salida) para cada combinación de ratio y knee.
  - Tiempos de attack y release medidos con señales escalón (traza de ganancia y tiempo de respuesta de la fase 0).
  - Range limita la reducción.
  - El lookahead suma la latencia correcta.
  - Sidechain externo y filtros del sidechain.
  - Auto-gain.
  - Mix 0 % idéntico a la entrada.

### Fase 3 — Saturación multibanda
Sección 6 completa.
- **Referencia:** manual de Saturn 2.
- Las curvas de saturación de los estilos se escriben como piezas reutilizables: la ampliación pendiente del EQ (§13, saturar la frecuencia realzada) podría usarlas.
- **Tests:**
  - Reconstrucción plana de los crossovers con 1 a 6 bandas y todas las pendientes.
  - Drive 0 y mix 0 % idénticos a la entrada.
  - El feedback al 100 % no se dispara.
  - El aliasing con oversampling queda por debajo de un umbral definido (análisis de armónicos de la fase 0).
  - Solo y bypass por banda.

### Fase 4 — Distorsión
Sección 7 completa.
- **Referencias:** Distructor de FL Studio (principal), con Fruity Fast Dist, Fruity WaveShaper y Saturn 2 como apoyo. Sus manuales son páginas de image-line.com, que hay que permitir en el entorno de red antes de leerlas.
- La pantalla de forma de onda (entrada frente a salida) recibe sus datos por la cola sin bloqueos de la fase 0.
- **Tests:**
  - Curva de transferencia de cada modo.
  - Mix 0 % idéntico a la entrada.
  - Bitcrusher con 16 bits casi transparente.
  - Downsample a la frecuencia indicada.
  - Sin NaN/Inf en ningún modo.

### Fase 5 — Presets, A/B, deshacer, ajustes y temas
Secciones 8.2 a 8.5: atajos, presets de fábrica y de usuario, A/B, deshacer y rehacer, ajustes, escalado de ventana, 4 temas y tooltips.
- Se activan los controles de la cabecera que la fase 0 dejó atenuados (presets, A/B, deshacer/rehacer y tema).
- Las plantillas Claro, Neón y Vintage conservan sus colores de antes de la paleta de marca. Al empezar la fase, P.P. decide si se adaptan a la marca (por ejemplo, con la firma oro) o cambian.
- **Tests:**
  - Ida y vuelta de presets.
  - Los presets de fábrica son de solo lectura.
  - A/B independientes.
  - Un gesto de arrastre equivale a un paso de deshacer.
  - Cambiar de tema no altera el audio.

### Fase 6 — Pulido y versión 1.0.0
- Revisión de CPU con un presupuesto objetivo documentado: la medida de CPU de la fase 0 pasa de informar a comprobar ese presupuesto en el CI.
- Revisión de accesibilidad: contraste y tamaños.
- README final con capturas, instalación (el instalador `.exe` y, como alternativa, copiar el `.vst3` a `C:\Program Files\Common Files\VST3`; después, reescanear en FL Studio), uso y licencias.
- `CHANGELOG.md` y tag `v1.0.0`: la release lleva el `.vst3` en un zip y el instalador.

### Después de la v1
- Peticiones de §13 a las que P.P. asigne una fase.
- Lo que §10 deja para la v2.

---

## 10. Fuera de alcance de la v1 (no implementar)

Quedan para la v2:
- EQ dinámico.
- Fases lineal y natural.
- Procesado Mid/Side.
- EQ match.
- Modulación en la saturación (LFO y seguidor de envolvente).
- Vista de módulos apilados con scroll, como la de Vital.
- macOS y otros formatos.

Diseña el código para que estas mejoras se puedan añadir sin reescribir lo existente.

En [`docs/PENDIENTES.md`](PENDIENTES.md) hay más ideas de módulos para el futuro (cristalizador, delay y reverb). **No los implementes:** son solo una lista de pendientes. Pero ten en cuenta que la cadena tendrá que admitir más de 4 módulos, así que no fijes el número de módulos en la arquitectura ni en el parámetro que guarda el orden.

---

## 11. Cómo empezar

Inicio original del proyecto (ya hecho):

1. Lee este documento completo y mira todas las imágenes de `docs/design/png/`.
2. Responde a P.P. con un resumen de 10 líneas de lo que vas a construir, el plan detallado de la **Fase 0** y las dudas que tengas.
3. Espera su confirmación y empieza la Fase 0.

**Al retomar el trabajo en una sesión nueva:**

1. Lee `CLAUDE.md`, este documento (sobre todo §9, §12 y §13) y `docs/arquitectura.md`.
2. Comprueba en qué fase está el proyecto (§9 y ramas `phase/*`) y si P.P. ha dado su visto bueno a la última.
3. Para empezar una fase de módulo, sigue los pasos del principio de §9.

---

## 12. Decisiones aprobadas

Decisiones tomadas con P.P. Tienen el mismo valor que el resto de la especificación. Las que tienen fecha se tomaron durante la fase 0.

| Tema | Decisión |
|---|---|
| Ramas | Una rama por fase (`phase/0-foundation`, `phase/1-eq`…). P.P. la fusiona a `main` con un PR tras dar el visto bueno. |
| Fabricante y copyright | Fabricante `JoseFrMunoz` (visible en FL Studio). Cabeceras: `Copyright (C) 2026 JoseFrMunoz`. Los códigos internos del plugin (fabricante y plugin) se fijan en la Fase 0 y no se cambian nunca. |
| Latencia | Se declara siempre la máxima posible (lookahead máximo + oversampling máximo), aunque el lookahead esté a 0 y el oversampling apagado. Con el compresor (Fase 2) serán unos 20 ms más unos pocos ms de oversampling. |
| Comparar a igual volumen | Interruptor. Activado: se escucha la señal original, alineada en latencia, con la ganancia que iguala su RMS al del procesado (RMS integrado durante unos 3 s, medido siempre en segundo plano). Entrada y salida con fundido de 20 ms. Ganancia de igualación limitada a ±24 dB. |
| Clipper de seguridad | Idéntico a la entrada por debajo de *techo − 3 dB*; entre ese punto y el techo, curva suave que nunca supera el techo. Techo ajustable de −6 a 0 dBFS (por defecto −0,3). Sin oversampling, para no añadir latencia. |
| Qué se guarda dónde | Tamaño de ventana, tema y tooltips: preferencias del usuario, comunes a todas las instancias. Calidad (reproducir/exportar), clipper y techo: se guardan con el proyecto, pero cargar un preset no los cambia. Todo lo demás (módulos, orden, entrada, salida, mix) va en el preset. |
| Cabecera en la Fase 0 | Visible completa. Presets, A/B, deshacer/rehacer y tema aparecen atenuados («Disponible más adelante») hasta la Fase 5. Ajustes y el botón Calidad abren el panel de ajustes con las secciones de calidad y clipper. |
| `PROMPT.md` | Sustituido por `CLAUDE.md` + este documento. El original queda en el historial de git. |
| Instalador `.exe` (2026-11-06) | Pasó de pendiente a la fase 0, hecho: `installer/` (Inno Setup), probado en el CI y adjunto a las releases. Sin firma de código: Windows avisa de «editor desconocido». |
| Mejoras de la base común (2026-11-06) | En la fase 0 y sin cambiar el sonido ni la interfaz: CI rápido, herramientas de medida para los tests, medida de CPU y cola sin bloqueos audio → interfaz. Una mejora cada vez, con el CI en verde antes de la siguiente. |
| Preguntas de cada módulo (2026-11-06) | Se hacen al empezar la fase de ese módulo, no antes. Nada se adelanta a su fase. |
| Manuales de referencia (2026-11-07) | Índice en `docs/referencias/README.md`. Se leen al empezar la fase del módulo, solo para emular el comportamiento (regla 8). Los PDF no se suben al repositorio (derechos de terceros): se enlazan. |
| Paleta de marca (2026-11-07) | «Obsidiana y oro» en el tema Oscuro por defecto: base casi negra con matiz frío, textos cálidos, oro viejo `#D4AF37` como firma y módulos en oro, acero, cobre y granate (§8.5). Es la firma visual de P.P.: verla y saber que es suyo. El granate se aclaró de `#A3344A` a `#AD374E` para llegar al contraste mínimo de 3:1 que piden los elementos gráficos; un test comprueba el contraste de todo el tema. |
| Referencia de la distorsión (2026-11-07) | Distructor de FL Studio como principal; Fruity Fast Dist, Fruity WaveShaper y Saturn 2 como apoyo. Se puede cambiar si no convence. |
| Visto bueno de la fase 0 (2026-11-11) | P.P. la probó en FL Studio: «todo bien» salvo el Mix global y el arrastre del panel de la cadena, y pidió seguir con las fases. Los dos cambios se hacen en `phase/0-foundation` y él los comprueba durante la fase 1. La fase 1 sale de esa rama. |
| Orden de la cadena (2026-11-11) | Opción A de P.P.: el orden es una **lista de módulos** guardada en el estado (`chainOrder="eq,compressor,saturation,distortion"`), no un parámetro automatizable. Admite cualquier número de módulos: un módulo desconocido se ignora y uno que falte se añade al final. Llega al audio sin bloqueos (una palabra atómica) y la cadena hace el fundido de siempre. Va en el proyecto y, desde la fase 5, en los presets y en deshacer. Formato del estado: versión 2; los proyectos con el parámetro antiguo (`chainOrder`, 0–23) se convierten al cargarlos. |
| Reordenado en vivo (2026-11-11) | A petición de P.P.: al arrastrar un módulo, los demás se apartan solos y con animación, y al soltar el módulo se desliza a su hueco sin tener que apuntar a una línea. Sustituye a la línea de destino de la pantalla 06. El orden se aplica al soltar, como un solo gesto. |
| EQ: bandas y automatización (2026-11-11, fase 1) | 8 bandas fijas en la lista de parámetros («EQ · Banda 1 · Frecuencia»…), cada una con un parámetro «Activa» que indica si existe. Crear ocupa el primer hueco libre; borrar no renumera las demás, para que la automatización grabada siga moviendo la banda correcta. El *Solo* no se automatiza ni se guarda. El modo del analizador y la escala (±12/±30) se guardan con el proyecto, pero cargar un preset no los cambia. |
| EQ: doble clic (2026-11-11, fase 1) | El tipo depende de la frecuencia: < 30 Hz corte de graves (12 dB/oct); 30–80 Hz shelf grave; 80 Hz–10 kHz campana (en la franja más baja de la gráfica, notch); 10–16 kHz shelf agudo; > 16 kHz corte de agudos (12 dB/oct). Ganancia = altura del clic; Q inicial 1. |
| EQ: carácter sin latencia (2026-11-11, fase 1) | Opción A: saturación suave de orden bajo (casi solo 2.º y 3.er armónico) sin oversampling. El EQ mantiene la latencia cero; un test mide que el aliasing queda muy por debajo de lo audible a niveles normales. |
| EQ: cuánto colorea (2026-11-11, fase 1) | *Subtle* (transformador): armónicos impares, algo más en graves, ≈ 0,1 % con un tono de 1 kHz a −12 dBFS. *Warm* (válvula): sobre todo 2.º armónico, ≈ 0,5 %. Ambos crecen con el nivel. Se afina de oído con P.P. |
| EQ: solo de banda (2026-11-11, fase 1) | Como en Pro-Q: se oye la zona que toca la banda (pasa-banda con su frecuencia y su Q; en shelves, la zona del shelf), y en los cortes lo que se quita. Una sola banda en solo a la vez. |
| EQ: tilt shelf (2026-11-11, fase 1) | Con ganancia +6 dB, los agudos suben 6 dB y los graves bajan 6 dB, girando sobre la frecuencia de la banda. |
| EQ: extras de manejo (2026-11-11, fase 1) | Supr borra la banda seleccionada; Ctrl al arrastrar un punto da ajuste fino; doble clic en un punto permite escribir frecuencia, ganancia y Q; clic derecho en un punto abre un menú (tipo, bypass, borrar); la escala pasa sola a la siguiente (±6 → ±12 → ±30) al llevar una banda más allá; nunca se encoge sola. |
| EQ: funciones de Pro-Q 4 fuera de la especificación (2026-11-11, fase 1) | Ninguna en la v1: interacción ganancia-Q, pendiente en campanas y shelves, pendientes fraccionarias, brickwall, flat tilt, pasa-todo y selección múltiple. Si P.P. quiere alguna, va a §13. (Crear arrastrando sobre la línea entró en la revisión de P.P., fila siguiente.) |
| EQ: revisión de P.P. (2026-11-11, fase 1) | Tras ver la fase 1 y un vídeo de Pro-Q 4 (solo comportamiento, regla 8), P.P. aprobó el boceto y todas las recomendaciones: **(1) Rueda:** en los cortes cambia la pendiente (6 → 96 dB/oct, un paso por muesca) y Ctrl+rueda la Q; en las demás formas, la Q; el ajuste fino de la rueda pasa a Shift. **(2) Mini panel** de la banda al hacer clic en un punto: bypass, tipo (despliega las 8 formas), solo, eliminar y valores (frecuencia con su nota, ganancia, Q o pendiente). Sustituye a la etiqueta flotante; va encima del punto (debajo si no cabe) y lo sigue al arrastrar. Se cierra con un clic en el fondo, Esc o al borrar la banda; al pasar por otro punto solo se ilumina. **(3) Solo arrastrable:** suena mientras se mantienen pulsados los auriculares del mini panel; el arrastre horizontal mueve la frecuencia y con Ctrl cambia la Q; el vertical no hace nada. Al soltar se apaga. El botón «Solo» del editor de abajo sigue fijo. **(4) Crear** también con clic y arrastre sobre la línea, además del doble clic; un clic en el fondo sigue deseleccionando. Al pasar el ratón por la línea, un punto fantasma muestra el icono de la forma que se crearía. **(5) Notas** con la numeración de FL Studio (Do central = C5), y la frecuencia se puede escribir como nota («A5», «C#3+13»). **(6) Colores:** 8 tonos cercanos al oro del EQ, del ámbar al verde oliva claro, uno por banda; la curva total sigue en oro. **(7) Escala ±6 dB** (líneas cada 3 dB) junto a ±12 y ±30; al abrir sigue en ±12. |

---

## 13. Pendientes

Peticiones de P.P. anotadas para más adelante. **No se implementan** hasta que P.P. diga en qué fase van. Las ya asignadas pasan a §12 y §9.

Los **módulos nuevos para después de la v1** (cristalizador, delay y reverb) están en [`docs/PENDIENTES.md`](PENDIENTES.md).

| Fecha | Petición | Notas |
|---|---|---|
| 2026-11-07 | **EQ con funciones de filtro creativo, tomando como referencia FabFilter Volcano 3:** estilos de filtro por banda con resonancia no lineal y autooscilación, drive por banda para **saturar solo la frecuencia realzada** (la zona de la campana o el pico de resonancia, más cuanto más fuerte llega), forma pasa-todo, modulación de las bandas (LFO por pasos, envolvente, seguidor de envolvente, MIDI), enrutado serie/paralelo, L/R o M/S y panorama por banda. | Investigación (con el manual oficial) y encaje con el EQ en [`docs/referencias/volcano-3.md`](referencias/volcano-3.md). **Choca con lo ya decidido:** latencia cero del EQ (§4; los estilos no lineales necesitan oversampling), modulación, EQ dinámico y M/S fuera de la v1 (§10), y el plugin no recibe MIDI. **Recomendación:** no meterlo en la fase 1, sino después de la fase 3 (reutilizaría sus curvas de saturación) o en la v2; la fase 1 se diseña para poder añadirlo (§9). Falta que P.P. elija qué partes y en qué fase. Solo comportamiento: ni código, ni nombres, ni interfaz (regla 8). |
