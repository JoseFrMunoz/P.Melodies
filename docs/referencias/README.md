# Plugins de referencia y sus manuales

Manuales oficiales de los plugins cuyo **comportamiento** emula P.Melodies (§0 de la especificación). Se leen **al empezar la fase del módulo correspondiente**, junto con la especificación, y las preguntas que salgan se hacen a P.P. antes de programar.

- **Regla 8:** se emula el comportamiento, nunca el código, los nombres (de estilos, modos, etc.), los logos ni la interfaz.
- **Los PDF no se guardan en el repositorio:** son material con derechos de FabFilter. Se enlazan; quien trabaje en una fase los descarga.
- Los enlaces se comprobaron el 2026-11-07. La columna *SHA-256* (16 primeros caracteres) identifica la edición leída; si FabFilter publica otra, el resumen cambiará.

| Módulo y fase | Referencia (§0) | Manual | Páginas | SHA-256 | Notas |
|---|---|---|---|---|---|
| EQ (fase 1) | FabFilter Pro-Q | [Pro-Q 4](https://www.fabfilter.com/downloads/pdf/help/ffproq4-manual.pdf) | 64 | `51ee76a3aa00bb3d` | Versión que encaja con la especificación: sus modos de carácter son los de §4 (limpio, sutil, cálido). Incluye la forma pasa-todo y la interacción entre ganancia y Q. |
| EQ (fase 1) | FabFilter Pro-Q | [Pro-Q 3](https://www.fabfilter.com/downloads/pdf/help/ffproq3-manual.pdf) | 50 | `a0b126e7f892e51c` | Versión anterior, como apoyo (tilt shelf, pendientes, interacción en la curva). |
| Compresor (fase 2) | FabFilter Pro-C 2 | [Pro-C 2](https://www.fabfilter.com/downloads/pdf/help/ffproc2-manual.pdf) | 37 | `fedae45788d81ef6` | Referencia principal: sus 8 estilos son los de §5. |
| Compresor (fase 2) | — | [Pro-C 3](https://www.fabfilter.com/downloads/pdf/help/ffproc3-manual.pdf) | 47 | `ae0fb5d89fe2f42e` | Versión nueva: 6 estilos más (también compresión ascendente), modos de carácter y un EQ de sidechain de hasta 6 bandas. **Fuera de la especificación:** solo como contexto. |
| Saturación multibanda (fase 3) | FabFilter Saturn 2 | [Saturn 2](https://www.fabfilter.com/downloads/pdf/help/ffsaturn2-manual.pdf) | 48 | `59b2de152bc797dd` | Referencia principal de §6 (bandas, drive, feedback, dynamics, tono). |
| Distorsión (fase 4) | Distructor de FL Studio | [Distructor](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Distructor.htm) (web) | — | — | **Principal** (decidido el 2026-11-07). Incluida en todas las ediciones de FL Studio: P.P. puede comparar de oído. Varios algoritmos: plegado de onda, reducción de bits y de frecuencia de muestreo, recorte suave, moldeado de onda; además, filtros. |
| Distorsión (fase 4) | Apoyo | [Fruity Fast Dist](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Fruity%20Fast%20Dist.htm) (web) | — | — | Distorsión sencilla y ligera (preamplificación, umbral, dos tipos, mezcla, salida): carácter de referencia para el recorte duro. |
| Distorsión (fase 4) | Apoyo | [Fruity WaveShaper](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Fruity%20WaveShaper.htm) (web) | — | — | Curva de transferencia editable: referencia para la pantalla de curva de §7. |
| Distorsión (fase 4) | Apoyo | [Saturn 2](https://www.fabfilter.com/downloads/pdf/help/ffsaturn2-manual.pdf) | — | — | Sus estilos de efectos cubren lo que Distructor no parece tener: rectificado (y *foldback* y destrucción). |
| Pendiente del EQ (§13) | FabFilter Volcano 3 | [Volcano 3](https://www.fabfilter.com/downloads/pdf/help/ffvolcano3-manual.pdf) | 47 | `865073f73004ffeb` | Resumen e ideas en [`volcano-3.md`](volcano-3.md). |

**Sin manual buscado:** Vital, citado en §10 solo por su vista de módulos apilados, que es de la v2.

**Acceso:** fabfilter.com está permitido en el entorno *P.Melodies* de Claude Code (en el entorno por defecto no lo estaba). Los manuales de FL Studio son páginas de image-line.com: hay que permitir ese dominio antes de leerlos en la fase 4.
