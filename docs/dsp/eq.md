# EQ: diseño de los filtros

Código: `src/dsp/eq/EqFilterDesign.*` (diseño) y `src/dsp/eq/SvfCascade.h` (proceso). Tests: `tests/EqDesignTests.cpp`.

La especificación (§4) pide un EQ de **latencia cero** cuya curva sea **fiel a la analógica también cerca de Nyquist**, sin el «encogimiento» de los agudos, y que aguante automatizaciones rápidas. El diseño tiene tres pasos:

1. Cada banda parte de un **prototipo analógico**: la curva que tendría un EQ analógico ideal.
2. Cada sección del prototipo se convierte en una **sección digital** cuya magnitud se parece lo más posible a la analógica hasta Nyquist.
3. Cada sección digital se ejecuta como un **filtro de variables de estado TPT** (SVF), estable aunque sus coeficientes cambien muestra a muestra.

## 1. Prototipos analógicos

En las fórmulas, *s* está normalizada a la frecuencia de la banda (s = j es la frecuencia de la banda), *G* es la ganancia en dB y A = 10^(G/40).

| Tipo | H(s) | Notas |
|---|---|---|
| Campana | (s² + s·A/Q + 1) / (s² + s/(A·Q) + 1) | Simétrica: un corte es el inverso exacto del realce. |
| Shelf grave | A·(s² + (√A/Qs)·s + A) / (A·s² + (√A/Qs)·s + 1) | G en DC, G/2 en su frecuencia, 0 dB en agudos. |
| Shelf agudo | A·(A·s² + (√A/Qs)·s + 1) / (s² + (√A/Qs)·s + A) | Lo mismo, al revés. |
| Tilt shelf | 10^(−G/20) × shelf agudo de 2G | −G en graves, +G en agudos y 0 dB en su frecuencia (§12). |
| Notch | (s² + 1) / (s² + s/Q + 1) | Cero exacto en su frecuencia. |
| Pasa-banda | (s/Q) / (s² + s/Q + 1) | 0 dB en su frecuencia. |
| Corte de graves / agudos | Butterworth de orden n = pendiente / 6 | 6 a 96 dB/oct: secciones de 2.º orden más una de 1.º si n es impar. |

**Convenio de la Q** (como en Pro-Q, Q = 1 es la forma por defecto de cada tipo):
- Campana, notch y pasa-banda: la Q es la del prototipo.
- Shelves y tilt: Qs = Q / √2. Con Q = 1 la transición es suave, sin rebote; con más Q aparece un rebote resonante a cada lado.
- Cortes: Q = 1 es la respuesta máximamente plana (Butterworth, −3 dB en la frecuencia de corte). La Q multiplica la de la sección que ya es más resonante. Un corte de 6 dB/oct no tiene Q.

## 2. De analógico a digital

Una sección digital de 2.º orden tiene cinco coeficientes. Su respuesta se repite y se pliega en Nyquist, así que no puede seguir a la analógica hasta arriba del todo: la transformación bilineal clásica (Robert Bristow-Johnson) la obliga a valer lo mismo que en DC justo en Nyquist, y por eso las campanas agudas se estrechan y caen. Para evitarlo, el diseño **ajusta la magnitud** (no la fase) a la del prototipo, con estas herramientas:

- **Base phi (Vicanek 2016):** el cuadrado de la magnitud de un polinomio de 2.º orden en z⁻¹ es lineal en tres coeficientes, |N|² = P0·cos²(w/2) + P1·sin²(w/2) + P2·sin²(w). Igualar |H|² en una frecuencia da una ecuación lineal.
- **Ajuste de tres puntos (Vicanek):** polos por **invarianza al impulso** (z = e^(sT)) y numerador de fase mínima con la magnitud exacta en DC, en la frecuencia de la banda y en Nyquist.
- **Ajuste de cinco puntos:** polos y ceros a la vez. La magnitud es exacta en DC, en Nyquist, en la frecuencia de la banda y en los dos bordes de su pico (las frecuencias a media ganancia en dB). Son tres ecuaciones lineales en (Q0, P2, Q2), que luego se factorizan en un biquad estable y de fase mínima.
- **Mínimos cuadrados ponderados:** el mismo planteamiento con diez frecuencias repartidas hasta casi Nyquist, más la de la banda con mucho peso. El error se divide por la magnitud esperada, de modo que se minimiza el error relativo. Hace dos pasadas al estilo Steiglitz-McBride, empezando por los polos de la invarianza al impulso.
- **Inversión:** una campana o un shelf que cortan mucho no tienen solución de tres puntos con esos polos, pero su realce simétrico sí. Como el corte analógico es exactamente el inverso del realce, se diseña el realce y se invierte: los ceros pasan a ser polos.
- **Casos con forma fija:**
  - Pasa-altos: doble cero en DC, para que la pendiente sea exacta en la banda eliminada.
  - Notch: par de ceros exactamente sobre la circunferencia unidad, en su frecuencia, para que la muesca sea completa.
  - Secciones de 1.º orden: un polo por invarianza al impulso, exactas en DC y en Nyquist.

**Elección:** cada sección prueba los métodos que le corresponden y se queda con el que menos se desvía en dB del prototipo, medido en 12 frecuencias de control: de dos octavas por debajo de la banda hasta casi Nyquist, más la de la banda y sus bordes. Por debajo de −40 dB cuenta como −40 dB, porque en una banda eliminada solo importa la forma.

**Coste:** la elección completa cuesta unos 6 µs por campana y unos 45 µs por corte de 96 dB/oct. Se hace solo cuando cambia el destino de un parámetro. Mientras el parámetro se desliza hacia ese destino, se reutiliza el método elegido para cada sección (`DesignChoices`): 0,9 µs por campana y 7 µs por corte de 96 dB/oct.

## 3. Proceso: SVF con mezcla de salidas

Cada sección digital se ejecuta como un **SVF TPT** con la forma de Simper (2013), que integra con la regla trapezoidal. Su estado son las dos «corrientes» de los integradores, que no saltan cuando cambian los coeficientes. Por eso se pueden poner coeficientes nuevos cada pocas muestras mientras un parámetro se desliza, sin clics ni inestabilidad.

- **Cualquier biquad estable** se puede escribir así:
  - g y k se obtienen de A = 1 + a1 + a2, B = 1 − a1 + a2 y C = 1 − a2: g = √(A/B) y k = 2C/(B·g).
  - La salida mezcla las tres del SVF: grave × ganancia en DC + agudo × ganancia en Nyquist + banda × un tercer coeficiente, que se deduce de b0.
- **Estabilidad:** el filtro es estable si g > 0 y k > 0. Un test lo comprueba en todas las combinaciones extremas.
- **Precisión:** el diseño se calcula en doble precisión y el proceso en `float`. A, B y C se calculan sin cancelaciones (con `expm1`), así que una banda de 10 Hz a 192 kHz sigue siendo exacta.

## Precisión medida (tests)

Diferencia con el prototipo analógico, con bandas hasta fs/8 (6 kHz a 48 kHz) y las Q habituales de cada tipo. Es la misma a 44,1, 48, 96 y 192 kHz:

| Tipo | Hasta fs/4 | De fs/4 a 0,45·fs |
|---|---|---|
| Campana (Q 0,3–10, ±18 dB) | ≤ 0,25 dB | ≤ 0,65 dB |
| Shelves y tilt (Q 0,5–2) | ≤ 0,30 dB | ≤ 0,48 dB |
| Corte de graves (6–96 dB/oct, Q 0,5–2) | ≤ 0,27 dB | ≤ 0,33 dB |
| Corte de agudos | ≤ 0,67 dB | ≤ 1,6 dB (banda eliminada, 30–40 dB por debajo) |
| Pasa-banda | ≤ 0,56 dB | ≤ 0,93 dB |
| Notch | cero exacto en su frecuencia (< −170 dB); error lineal ≤ 0,03 |

- **Ganancias exactas:** la campana da exactamente su ganancia en su frecuencia y los shelves en DC.
- **Frente al bilineal:** una campana de +12 dB a 10 kHz, Q 1, a 44,1 kHz se desvía menos de 0,6 dB de la analógica entre 14 y 20 kHz. La bilineal clásica se desvía más de 2 dB (test «near Nyquist the curve does not cramp»).
- **Bandas muy cerca de Nyquist:** la frecuencia se limita a 0,475·fs (§4). Allí ningún filtro de 2.º orden sigue del todo a la analógica.

## Referencias

- M. Vicanek, *Matched Second Order Digital Filters*, 2016 (base phi, ajuste de tres puntos con polos por invarianza al impulso, inversión del realce).
- A. Simper, *Linear Trapezoidal Integrated State Variable Filter* y *Solving the continuous SVF equations using trapezoidal integration and equivalent currents*, Cytomic, 2013 (SVF TPT y mezcla de salidas).
- V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1, 2018 (integración trapezoidal, TPT).
- R. Bristow-Johnson, *Cookbook formulae for audio EQ biquad filter coefficients* (prototipos analógicos de campana y shelves, y diseño bilineal de comparación).
- S. J. Orfanidis, "Digital Parametric Equalizer Design With Prescribed Nyquist-Frequency Gain", *J. Audio Eng. Soc.* 45(6), 1997 (el problema del encogimiento cerca de Nyquist).
- K. Steiglitz, L. E. McBride, "A technique for the identification of linear systems", *IEEE Trans. Automatic Control* 10(4), 1965 (reponderación iterativa del error de ecuación).
- S. Butterworth, "On the Theory of Filter Amplifiers", *Wireless Engineer* 7, 1930 (Q de las secciones de los cortes).
