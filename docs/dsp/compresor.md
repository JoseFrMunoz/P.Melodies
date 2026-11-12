# Compresor: diseño del DSP

Código: `src/dsp/comp/CompressorCurve.*` (curva estática y estilos) y `src/dsp/comp/CompressorDynamics.*` (detector y tiempos). Tests: `tests/CompDspTests.cpp`.

La especificación (§5) pide un compresor con 8 estilos que dependan del programa. Las respuestas de P.P. a las preguntas de la fase 2 (§12) fijan cómo: todos los estilos comparten threshold, ratio, knee y range, y se diferencian en **cómo detectan el nivel** y en su **comportamiento temporal**. Ningún estilo añade saturación propia (es cosa del módulo 3).

En todo el documento, la **ganancia** es el cambio que aplica el compresor en dB (≤ 0, sin el makeup): nivel de salida = nivel de entrada + ganancia.

## 1. Cadena del detector

```
señal del detector ─ nivel (picos en 5 ms, o RMS) ─ curva estática ─ attack / hold / release ─ ganancia
```

- **Estéreo enlazado al 100 %** (§12): manda el canal más fuerte, y los dos canales reciben la misma ganancia, así que la imagen estéreo no se mueve.
- **Nivel por picos:** el máximo de |x| en los últimos 5 ms, con una cola monótona (Lemire 2006): coste constante por muestra y sin reservas de memoria. Así un tono estable de más de 100 Hz da una ganancia estable, en lugar de seguir cada media onda. A cambio, el release empieza 5 ms tarde: es un pequeño hold fijo.
- **Nivel RMS** (Opto, Mastering y Bus): media cuadrática con un filtro de un polo (10 o 25 ms). Se lee 3 dB más alto, para que un seno dé el mismo nivel con los dos detectores y el threshold signifique lo mismo en todos los estilos.

## 2. Curva estática

Es la rodilla suave de Giannoulis, Massberg y Reiss (2012), con interpolación cuadrática dentro del knee. Con *T* el threshold, *W* el knee, *R* el ratio y *s* = 1/R − 1 (−1 para ∞:1):

| Nivel *x* | Ganancia |
|---|---|
| 2(x − T) < −W | 0 |
| \|2(x − T)\| ≤ W | s·(x − T + W/2)² / (2W) |
| 2(x − T) > W | s·(x − T) |

- Después se limita con **Range**: la ganancia nunca baja de −range.
- **Ratio:** de 1:1 a 100:1, más ∞:1 (internamente, cualquier ratio de 1000 o más).
- **Opto** suma 12 dB al knee del knob: «knee extra suave».
- **Vocal** sustituye ratio y knee por su curva automática (§12, pregunta 3), y sus knobs aparecen atenuados:
  - knee fijo de 10 dB;
  - 1,5:1 al pasar el threshold;
  - el ratio local sube 0,04 por cada dB por encima del knee, hasta 20:1.
  - Ejemplo: 4:1 unos 10 dB por encima del final del knee. Cuanto más se pasa del umbral, más comprime.
- **Comprobado en los tests:**
  - la fórmula, para 7 ratios, 6 knees, 4 thresholds y 2 ranges, con un error menor de 10⁻⁹ dB;
  - que la pendiente es continua en los bordes del knee;
  - que la salida nunca baja cuando sube la entrada.

## 3. Attack, release y hold

La ganancia en dB pasa por un detector de picos «con ramas suaves» (Giannoulis et al., 2012, §3.3): un filtro de un polo que usa el coeficiente de attack cuando hace falta más reducción y el de release cuando hace falta menos. Con α = e^(−1/(τ·fs)):

- **Clean:** un escalón alcanza el 63 % de su cambio de ganancia en el tiempo de attack, y recupera el 63 % en el tiempo de release (más los 5 ms de la ventana de picos). Medido en los tests a 44,1, 48 y 96 kHz, con attacks de 0,1 a 100 ms y releases de 10 a 1000 ms: error ≤ 2 % o 1,5 muestras.
- **Hold:** tras el último momento en que la ganancia bajaba, la mantiene el tiempo de Hold antes de empezar el release.
- **Cambios de ajustes:** threshold, knee, ratio (como 1/R − 1) y range se deslizan en 20 ms. Automatizarlos no produce saltos: con el attack al mínimo, un cambio de −10 a −30 dB de threshold mueve la ganancia como máximo 0,023 dB por muestra.

## 4. Estilos

| Estilo | Detector | Realimentación β | Release en dos etapas | Otros |
|---|---|---|---|---|
| Clean | Picos | 0 | — | Attack y release exactos |
| Classic | Picos | 0,5 | 50 % de la reducción, se carga en 300 ms, release × 4 | |
| Opto | RMS 10 ms | 0 | 50 %, 1 s, × 10 | Knee + 12 dB |
| Vocal | Picos | 0 | 50 %, 200 ms, × 4 | Curva automática |
| Mastering | RMS 25 ms | 0 | 50 %, 1 s, × 5 | Ganancia suavizada 2 ms |
| Bus | RMS 10 ms | 0,25 | 60 %, 250 ms, × 5 | |
| Punch | Picos | 0 | — | Attack en S, release × 0,6 |
| Pumping | Picos | 0 | — | Release lento al principio y rápido al final |

### Realimentación (Classic y Bus)

Los compresores antiguos detectan su propia salida. Aquí el detector ve *u* = x + β·g: la entrada más una parte β de la ganancia.

- **La curva no cambia.** Una realimentación pura deja el ratio efectivo en 1/(2 − 1/R), así que no pasaría de 2:1. Para evitarlo, la ganancia en función de *u* se elige de modo que el estado estable siga siendo exactamente la curva de los knobs: c(u) = G(x(u)). Como u(x) = x + β·G(x) es monótona (u' ≥ 1 − β > 0), se invierte por regiones:
  - en el knee, con una ecuación de 2.º grado cuyo discriminante es (1 + β·s)² ≥ 0;
  - por encima, con x = T + (u − T)/(1 + β·s);
  - en el límite del range, con x = u + β·range.
  
  Los tests comprueban la ida y vuelta con un error menor de 10⁻⁷ dB.
- **Sin retardo de una muestra:** el bucle se resuelve con un paso de Newton alrededor del estado actual (la idea de los filtros *zero-delay feedback*, Zavalishin):
  g = [α·g₁ + (1 − α)(c − c'·β·g₁)] / (1 − (1 − α)·β·c')
  - Como c' ≤ 0, el denominador es ≥ 1 y el bucle es estable con cualquier attack, incluso de 5 µs.
  - En el estado estable da exactamente g = c.
- **Efecto audible:** la constante de tiempo efectiva se divide por (1 + β·|c'|), así que cuanto más comprime, más rápido reacciona. Con ratio 10:1, Classic llega al 63 % en menos del 70 % del tiempo de Clean.

### Release en dos etapas

Es la memoria de Classic, Opto, Vocal, Mastering y Bus, y también el auto-release. Junto a la etapa principal hay una etapa lenta que:
- se carga con su propio attack, lento (de 200 ms a 1 s);
- se descarga con release × K.

La ganancia es el mínimo de la principal y de *w* × la lenta, donde *w* es la parte de la reducción que retiene. En compresión estable manda la principal, así que la curva estática no cambia. Por eso:
- **Tras un golpe corto**, la etapa lenta apenas se ha cargado y el release es el del knob.
- **Tras una compresión larga**, *w* de la reducción vuelve mucho más despacio. Es el «release que depende del programa» de los compresores ópticos (la célula T4 del LA-2A, con un release rápido para la primera mitad y lento para el resto) y de los automáticos de bus.
- **En los tests**, el 90 % de la recuperación tarda más del doble tras 3 s de compresión que tras 30 ms, en los cinco estilos. En Clean no cambia.

### Auto-release

Añade a cualquier estilo el release en dos etapas: al menos el 60 % de la reducción, cargada en 400 ms y con release × 5. El knob Release escala los dos tiempos (§12, pregunta 6).

### Punch

El attack tiene dos etapas en serie, cada una 2,15 veces más rápida que el knob. Así el 63 % se sigue alcanzando en el tiempo de attack, pero el arranque tiene forma de S: el principio del transitorio pasa casi intacto.
- A una décima del attack, Punch reduce menos del 30 % de lo que reduce Clean.
- El release se acorta a 0,6 × el del knob: recupera rápido.

### Pumping

La constante de tiempo del release depende de cuánto falta por recuperar: τ = release × (0,3 + 1,4·q), donde *q* es la reducción actual dividida entre la más profunda desde que empezó el último attack.
- Al principio va a 1,7 × el release, despacio; al final, a 0,3 ×, deprisa.
- Así se forma el bombeo tipo EDM, que se marca más con Hold.
- **En los tests:** con 0,3 releases transcurridos, recupera menos que Clean, pero llega al 95 % antes.

### Mastering

Además del RMS de 25 ms y la memoria, suaviza la ganancia final con 2 ms. Los cambios de ganancia más suaves producen menos distorsión.

## 5. Auto-gain

Se calcula a partir de los ajustes, sin medir la señal (§12, pregunta 5): los niveles a medio camino entre el threshold y 0 dBFS conservan su nivel.

auto-gain = −G(T/2)

- Con threshold −20 dB y ratio 4:1 son +7,5 dB.
- Respeta el knee, el range y la curva del estilo.
- El knob Makeup se suma encima.

## 6. Referencias

- D. Giannoulis, M. Massberg, J. D. Reiss, "Digital Dynamic Range Compressor Design — A Tutorial and Analysis", *Journal of the Audio Engineering Society* 60(6), 2012 (rodilla suave, detector con ramas suaves, realimentación frente a prealimentación).
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2.ª ed., Wiley, 2011, cap. 4 (procesadores de dinámica, detectores de pico y RMS).
- J. D. Reiss, A. McPherson, *Audio Effects: Theory, Implementation and Application*, CRC Press, 2014, cap. 6 (compresión, release dependiente del programa).
- D. Lemire, "Streaming Maximum-Minimum Filter Using No More than Three Comparisons per Element", *Nordic Journal of Computing* 13(4), 2006 (máximo en ventana deslizante).
- V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1, Native Instruments, 2018 (resolución de bucles de realimentación sin retardo).
- Descripción pública del comportamiento de la célula óptica T4 (release en dos etapas) en la documentación del Teletronix LA-2A.
- Manual de FabFilter Pro-C 2, solo como referencia de comportamiento (regla 8): nombres de los estilos y qué se espera de cada uno.

## 7. Descripción para el usuario

Texto para el futuro botón de información del módulo (§13, 2026-11-12).

- **Estilos.** Todos usan los mismos threshold, ratio, knee y range; cambian cómo escuchan la señal y cómo reaccionan:
  - *Clean:* preciso y limpio; los tiempos son los de los knobs.
  - *Classic:* como los compresores antiguos, escucha en parte su propia salida. Cuanto más comprime, más rápido reacciona, y tras una compresión larga vuelve más despacio.
  - *Opto:* lento y muy suave, con una rodilla más ancha. Tras una compresión larga, la mitad de la reducción tarda mucho en volver.
  - *Vocal:* pone el ratio y el knee solo, con más ratio cuanto más se pasa la voz del umbral, para mantenerla delante y estable.
  - *Mastering:* escucha el nivel medio y cambia la ganancia muy suavemente, para mezclas completas.
  - *Bus:* nivel medio, algo de realimentación y release en dos etapas: el «pegamento» de grupos y buses.
  - *Punch:* deja pasar el principio de cada golpe y recupera rápido, para dar pegada.
  - *Pumping:* el release vuelve despacio al principio y rápido al final, con un bombeo marcado; más aún con Hold.
- **Auto-gain:** sube la salida lo que se calcula a partir del threshold, el ratio y el knee, para que los niveles a medio camino entre el threshold y 0 dBFS suenen igual. No sube y baja con la música. El Makeup se suma encima.
- **Auto-release:** el release se alarga tras compresiones largas y se mantiene corto tras golpes sueltos. El knob Release escala el efecto.
- **Estéreo:** los dos canales reciben siempre la misma reducción, así que la imagen no se mueve.
