# CLAUDE.md — P.Melodies

Reglas de trabajo del proyecto, copiadas literalmente de la sección 1 de la especificación original.
La especificación completa (producto, arquitectura, módulos, fases y decisiones aprobadas) está en
[`docs/ESPECIFICACION.md`](docs/ESPECIFICACION.md). Léela antes de trabajar en cualquier fase.

## 1. Reglas de trabajo (obligatorias en todo el proyecto)

1. **Una fase cada vez.** El desarrollo está dividido en fases (sección 9). No empieces una fase hasta que P.P. haya dado su **visto bueno explícito** a la anterior. Un "vale" a otra cosa no cuenta: pregunta si tienes dudas.
2. **Cierre de cada fase.** Al terminarla:
   - Todos los tests automáticos en verde, en local y en CI (Windows y Linux).
   - pluginval sin errores en el build de Windows.
   - Un `.vst3` descargable del workflow.
   - Un mensaje a P.P. con: qué se ha hecho, cómo descargar el build, una **lista concreta de pruebas manuales en FL Studio** (pasos y resultado esperado), los límites conocidos y una explicación breve (3–6 líneas) de los conceptos de C++ o DSP nuevos de esa fase.
   - **Detente y espera.** No adelantes trabajo de la fase siguiente.
3. **Si P.P. encuentra un fallo:** corrígelo, añade un test que lo reproduzca, vuelve a pasar **todos** los tests (también los de fases anteriores) y entrega de nuevo. Un fallo en un módulo puede repetirse en otros: revisa si el mismo patrón existe en el código compartido.
4. **Regresión cero.** Ninguna fase puede romper algo ya aprobado. La batería de tests crece con cada fase y se ejecuta entera siempre.
5. **Nada sin probar.** No digas que algo funciona si no lo has comprobado con un test o con el build. Si algo no se puede probar automáticamente, dilo y añádelo a la lista manual.
6. **Pregunta antes de decidir** algo que cambie el comportamiento audible, la interfaz o el alcance. Para detalles técnicos internos, decide tú y explica la decisión en el cierre de fase.
7. **Commits pequeños y descriptivos**, en inglés, siguiendo Conventional Commits (`feat(eq): …`, `fix(comp): …`, `test: …`). Una rama por fase (`phase/0-foundation`, `phase/1-eq`…), fusionada a `main` solo tras el visto bueno.
8. **Código propio.** Emula el *comportamiento* de los plugins de referencia, nunca su código, nombre, logos ni interfaz. No copies código de Vital (es GPL) ni de ningún otro proyecto: puedes inspirarte en ideas documentadas públicamente (artículos, papers, libros de DSP) y debes citarlas en `docs/dsp/`.
9. **Persistencia de las reglas.** En la fase 0, copia esta sección 1 a un `CLAUDE.md` en la raíz del repo y el resto del documento a `docs/ESPECIFICACION.md`, para que las reglas sigan vigentes en futuras sesiones.

---

## Referencia rápida (no forma parte de las reglas originales)

- **Estado de las fases:** consulta `docs/ESPECIFICACION.md` §12 (decisiones aprobadas) y el historial de ramas `phase/*`.
- **Configurar y compilar (Linux):**
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`
- **Tests:** `ctest --test-dir build --output-on-failure`
- **Dependencias Linux:** ver el job `linux` de `.github/workflows/build.yml`.
- **Referencia de "funciona":** el build de Windows en GitHub Actions (tests + pluginval al nivel 10).
- **Hilo de audio:** sin reservas de memoria, locks, I/O ni logs. Todo lo que se reserva, se reserva en `prepareToPlay`.
