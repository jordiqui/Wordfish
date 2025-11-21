# Análisis de los últimos parches basados en Stockfish

## Contexto de evolución reciente
- La rama integra un buscador MCTS con controles de profundidad, simulaciones y factor de exploración (`MCTS Rollout Depth`, `MCTS Simulations`, `MCTS Explore`). La lógica principal vive en `MCTS::analyze`, que ajusta el presupuesto de iteraciones y tiempo antes de lanzar playouts y usa un coeficiente de exploración derivado de `MCTS Explore`.【F:src/mcts.cpp†L65-L120】【F:src/mcts.cpp†L190-L350】
- `run_monte_carlo` en `search.cpp` conecta ese análisis con la tubería UCI, asigna las estadísticas a `rootMoves` y emite las líneas MultiPV resultantes. Actualmente no alimenta el sistema de experiencia con los resultados de Monte Carlo ni reutiliza la lógica de gestión de tiempo del buscador alfa-beta.【F:src/search.cpp†L306-L428】
- La capa de experiencia ahora preserva opciones no especificadas y mantiene la tabla aprendida; las mutaciones pasan por `Experience::update_settings`, que sólo persiste cuando se alteran los parámetros relevantes.【F:src/experience.cpp†L515-L579】

## Recomendaciones concretas (enfocadas en no generar regresiones)
1. **Alinear la gestión de `movetime` de MCTS con el buscador clásico.**
   - Hoy el presupuesto de tiempo para MCTS no descuenta `Move Overhead` cuando se usa `movetime`, lo que puede dejar menos margen de seguridad que el buscador alfa-beta.【F:src/mcts.cpp†L104-L118】
   - Propongo aplicar el mismo descuento que en la rama `time/incs`, es decir, restar `overhead` también en la rama `movetime` antes de fijar `endTime`. Esto mantiene la simetría con el gestor de tiempos existente y reduce el riesgo de apuros de reloj en controles fijos.

2. **Registrar resultados de MCTS en la experiencia para mantener la coherencia del libro aprendido.**
   - El flujo alfa-beta llama a `Experience::on_search_complete`, pero el modo Monte Carlo termina sin volcar la mejor línea y la profundidad buscada al almacén de experiencia.【F:src/search.cpp†L306-L428】【F:src/experience.cpp†L587-L660】
   - Tras ordenar `rootMoves`, invocar `Experience::on_search_complete(rootPos, rootMoves, rootMoves.front().score, rootMoves.front().averageScore, Depth(depthHint), limits);` conservaría los datos del modo MCTS sin modificar la selección de movimiento ni el formato UCI.

3. **Respetar señales de parada dentro de los playouts para evitar sobrepasar presupuestos.**
   - `MCTS::analyze` verifica `stopRequested` y el presupuesto antes de cada selección/expansión, pero los playouts (`rollout`) no observan la señal una vez iniciados, de modo que un playout largo puede superar el tiempo fijado.【F:src/mcts.cpp†L190-L350】
   - Pasar `stopRequested` (o un `Budget` reducido) a `rollout` y abortar si se dispara permite finalizar el ciclo principal sin consumir iteraciones adicionales, manteniendo NPS y tiempos más estables.

Estas intervenciones reutilizan estructuras existentes y no alteran la interfaz UCI, por lo que ofrecen mejoras graduales con bajo riesgo de regresión de rendimiento.
