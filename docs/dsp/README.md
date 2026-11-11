# Notas de DSP y referencias

Cada módulo documenta aquí sus decisiones de DSP y las fuentes públicas en las que se basa: libros, artículos y documentación. No se copia código de ningún proyecto (regla 8).

## Fase 0: base común

### Oversampling (`src/dsp/OversamplingEngine.*`)

- **Filtros:** `juce::dsp::Oversampling` con etapas de 2x en cascada y filtros FIR *half-band* de rizado constante (equiripple), en calidad máxima.
- **Por qué FIR y no IIR:** un FIR simétrico tiene fase lineal, así que su retardo es puro y conocido: (N − 1)/2 muestras. Eso permite alinear exactamente la ruta procesada con la seca (mix global y bypass). Un IIR polifásico tiene menos latencia, pero su retardo de grupo depende de la frecuencia y la señal seca no se podría alinear.
- **Latencia entera:** la latencia de cada factor se redondea hacia arriba a un número entero de muestras de la frecuencia base. Para ello se añade un retardo entero a la frecuencia interna más alta, sin interpolar.
- **Medido en los tests:**
  - El rizado de ganancia en la banda de paso es menor de 0,05 dB por módulo a 16x.
  - Tras quitar esa ganancia, el residuo es menor de 10⁻⁴: el retardo es exacto a la muestra.
- **Todos los factores preparados de antemano:** de 1x a 16x se preparan en `prepareToPlay`, así que cambiar de factor no reserva memoria.

Referencias:
- P. P. Vaidyanathan, *Multirate Systems and Filter Banks*, Prentice Hall, 1993 (filtros *half-band* y diezmado).
- A. V. Oppenheim, R. W. Schafer, *Discrete-Time Signal Processing*, 3.ª ed., Pearson, 2010 (FIR de fase lineal y retardo de grupo).
- Documentación de JUCE: `juce::dsp::Oversampling`.

### Compensación de latencia

Todas las rutas que deben sumarse o cruzarse se retrasan con líneas de retardo enteras (`src/dsp/DelayLine.h`). La latencia declarada al host es la máxima posible y no cambia con los parámetros. Así se evita que FL Studio tenga que recalcular su compensación (PDC) mientras suena.

### Fundidos cruzados

- Los cambios de orden, el bypass y los interruptores usan fundidos **lineales** (de ganancia constante): las dos señales que se cruzan están alineadas y muy correlacionadas, y en ese caso la suma lineal mantiene el nivel.
- El fundido de potencia constante solo es adecuado para señales no correlacionadas.

Referencia:
- U. Zölzer (ed.), *DAFX: Digital Audio Effects*, 2.ª ed., Wiley, 2011.

### Clipper de seguridad (`src/dsp/SafetyClipper.h`)

- **Curva:** identidad exacta por debajo de *techo − 3 dB*. Por encima, `k + (c − k)·tanh((|x| − k)/(c − k))`, donde *k* es el inicio de la rodilla y *c* el techo.
- **Propiedades:**
  - Es continua en valor y en pendiente en la rodilla.
  - Es monótona.
  - Nunca supera el techo, porque la salida se recorta además con `min`.
- **Sin oversampling** a propósito: no añade latencia. Es una red de seguridad que solo debería actuar en picos ocasionales. Límite conocido: no controla los picos entre muestras (*inter-sample peaks*).

Referencias:
- U. Zölzer (ed.), *DAFX*, cap. 4 (procesado no lineal).
- J. Pakarinen, D. T. Yeh, "A Review of Digital Techniques for Modeling Vacuum-Tube Guitar Amplifiers", *Computer Music Journal* 33(2), 2009 (saturación con `tanh`).

### Comparar a igual volumen (`src/dsp/LoudnessMatcher.h`)

- **Medida:** la potencia media de la señal procesada y de la original se integra con un filtro de un polo de constante de tiempo de 3 s, en doble precisión.
- **Ganancia:** se aplica a la original √(P_procesada / P_original), limitada a ±24 dB y suavizada en 200 ms. Con silencio se mantiene la última ganancia válida.
- **RMS simple:** se usa RMS sin ponderar, como pide la especificación. Una mejora posible sería ponderar con la curva K de ITU-R BS.1770.

Referencia:
- ITU-R BS.1770-4, *Algorithms to measure audio programme loudness and true-peak audio level*, 2015.

### Orden de la cadena como un único parámetro (`src/core/ChainOrder.*`)

Las 24 permutaciones de 4 módulos se numeran con el código de Lehmer (sistema de numeración factorial). El índice 0 es el orden por defecto (EQ → Compresor → Saturación → Distorsión). Los índices son estables, porque se guardan en proyectos y presets.

Referencia:
- D. E. Knuth, *The Art of Computer Programming*, vol. 2, §3.3.2 (sistema factorial), Addison-Wesley.

### Medidores

- **Pico:** máximo absoluto por bloque, entregado a la interfaz con un máximo atómico.
- **RMS:** media cuadrática con un integrador de un polo de 300 ms.
- **En pantalla:** el pico cae a 24 dB/s, la retención dura 1,5 s y el indicador de clip se activa a 0 dBFS hasta que se pulsa.

### Denormales y valores no finitos

- **Denormales:** `juce::ScopedNoDenormals` activa *flush-to-zero* en el hilo de audio.
- **NaN/Inf:** en debug, `debug::checkFinite` detiene la ejecución si aparece uno a la salida de los módulos o del motor. Los tests lo comprueban con parámetros y señales extremos.

## Fase 1: EQ

Diseño de los filtros (prototipos analógicos, ajuste de magnitud hasta Nyquist y proceso con SVF TPT), precisión medida y referencias: [`eq.md`](eq.md).

## Herramientas de medida de los tests (`tests/Measurement.h`)

Las usarán los tests de las fases 1 a 4. Todo el cálculo es en doble precisión, mucho más exacto que las señales `float` que miden, y cada herramienta se comprueba con señales de resultado conocido en `tests/MeasurementTests.cpp`.

- **Respuesta en frecuencia:** se mide la respuesta al impulso de una ruta lineal y se evalúa su transformada exactamente en cualquier frecuencia, sin limitarse a las bandas de una FFT. Sirve para comparar el EQ y los crossovers con la respuesta analítica.
- **Ajuste de seno:** mínimos cuadrados con seno, coseno y continua a una frecuencia conocida (el ajuste de tres parámetros de IEEE 1057). Da ganancia y fase con cualquier longitud de ventana, también en rutas no lineales.
- **Armónicos y aliasing:** el tono de prueba se coloca en una banda exacta de la FFT (muestreo coherente, IEEE 1241). Así los armónicos y sus alias caen en bandas exactas y no hace falta ventana. Con una banda impar y una FFT de tamaño potencia de dos, ningún alias de los primeros pliegues coincide con un armónico, de modo que se separan exactamente:
  - **THD:** potencia de los armónicos por debajo de Nyquist respecto a la fundamental.
  - **Resto:** todo lo demás (aliasing y ruido) respecto a la fundamental.
  - **Ejemplo medido:** `tanh(4x)` sobre un seno de 7 kHz a 48 kHz deja el aliasing en −28 dB sin oversampling y en −123 dB a 8x. La THD (el 3.er armónico, a 21 kHz) no cambia: el oversampling quita el aliasing, no la distorsión.
- **Tiempos de respuesta:** traza de ganancia muestra a muestra (salida / entrada, con la latencia compensada) y tiempo hasta recorrer una fracción del salto, por ejemplo el 63,2 % para la constante de tiempo de un filtro de un polo.
- **Clics:** pico de la señal tras un pasa-altos Butterworth de 4.º orden a 8 kHz.
  - Un tono de prueba grave (100–200 Hz) casi desaparece; un salto brusco, no.
  - Medido a 48 kHz: un salto de tamaño *d* da unos 0,4 *d*; un tono limpio de 150 Hz a −6 dBFS, unos 5·10⁻⁸; un fundido lineal de 20 ms sobre él, unos 6·10⁻⁵.

Referencias:
- IEEE Std 1057-2017, *Standard for Digitizing Waveform Recorders* (ajuste de seno de tres parámetros).
- IEEE Std 1241-2010, *Standard for Terminology and Test Methods for Analog-to-Digital Converters* (muestreo coherente, THD).
- J. W. Cooley, J. W. Tukey, "An Algorithm for the Machine Calculation of Complex Fourier Series", *Mathematics of Computation* 19(90), 1965 (FFT radix 2).
- R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (pasa-altos del detector de clics).

