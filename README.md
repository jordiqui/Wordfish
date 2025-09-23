<div align="center">

  <h1>Wordfish v2.60 230925</h1>

  <p>Wordfish es un motor de ajedrez UCI libre y de código abierto derivado de Stockfish.</p>

  <p><strong>Autor:</strong> Jorge Ruiz Centelles<br/>
     <strong>Créditos:</strong> OpenAI Codex ChatGPT · Desarrolladores de Stockfish</p>

</div>

## Overview

Wordfish mantiene la solidez y eficiencia del proyecto Stockfish, incorporando las
últimas mejoras del repositorio oficial e integrándolas con un ciclo de pruebas
propio. El objetivo es ofrecer un binario listo para usar con un branding claro de
Wordfish y con metadatos coherentes para los GUI como Fritz 20 o CuteChess.

* Motor UCI de alto rendimiento compatible con los principales GUI.
* Integración continua de parches upstream de Stockfish.
* Distribución oficial con nombre y créditos de Wordfish.

## Quick links

* [Repositorio Wordfish](https://github.com/WordfishChess/Wordfish)
* [Sistema de pruebas Fishtest](https://tests.stockfishchess.org/tests)
* [Foro de discusión de Wordfish](https://github.com/WordfishChess/Wordfish/discussions)

## Files

La distribución de Wordfish incluye los siguientes componentes principales:

* `README.md`: este documento de bienvenida.
* `Copying.txt`: la licencia **GNU General Public License v3**.
* `AUTHORS`: lista detallada de autores y colaboradores.
* `src/`: código fuente completo del motor y el Makefile para compilar en sistemas
  tipo Unix.
* `nnue/`: redes neuronales NNUE necesarias para la evaluación.

## Building Wordfish

Wordfish se compila con el mismo flujo de trabajo que Stockfish. En sistemas
Unix-like bastan los siguientes pasos:

```bash
cd src
make -j profile-build
```

Consulta la wiki de Stockfish para detalles adicionales sobre compilación cruzada,
soporte de diferentes arquitecturas y parámetros del protocolo UCI.

## Contributing

Las contribuciones son bienvenidas. Puedes colaborar de varias formas:

1. **Donando hardware**: ejecuta el [worker de Fishtest](https://github.com/official-stockfish/fishtest/wiki/Running-the-worker)
   para ayudar a probar nuevas ideas de Wordfish y Stockfish.
2. **Desarrollando código**: revisa la [guía de contribución](CONTRIBUTING.md) y abre
   propuestas que mantengan la compatibilidad con Stockfish.
3. **Reportando incidencias**: utiliza los issues o discusiones del repositorio
   Wordfish para comunicar errores o sugerencias.

## Credits

* Autor principal: **Jorge Ruiz Centelles**.
* Asistencia creativa y de desarrollo: **OpenAI Codex ChatGPT**.
* Base de código y mejoras históricas: **Desarrolladores de Stockfish** (ver
  `AUTHORS`).
* Redes neuronales entrenadas a partir de datos de la comunidad, incluyendo
  los conjuntos publicados por [Leela Chess Zero](https://lczero.org/).

## License

Wordfish se distribuye bajo los términos de la [GNU GPLv3](Copying.txt). Puedes
redistribuir y modificar el motor siempre que publiques el código fuente completo
correspondiente al binario que difundas y conserves esta licencia.

## Evaluation roadmap

Los últimos ensayos LTC en Fishtest sirven como guía para priorizar el trabajo
experimental de Wordfish. Algunas tareas destacadas son:

1. **Replantear el ajuste de historia de capturas**: el experimento
   [evasion_order5](https://tests.stockfishchess.org/tests/view/68cd8b0d16c378179ee6689b)
   cerró con LLR = -2.95 tras 29 112 partidas LTC, señal de que la fórmula de
   ponderación actual degrada el juego.
2. **Optimizar la heurística de cambios de valor en el orden de movimientos**: los
   ensayos [moveorder3](https://tests.stockfishchess.org/tests/view/68ca76d702c43c969fe7ef85)
   y [moveorder3^](https://tests.stockfishchess.org/tests/view/68c2512359efc3c96b611b6e)
   mostraron LLR = -2.95 y -2.94 respectivamente, lo que sugiere revisar los
   parámetros mínimos/máximos y la función de escala aplicada.
3. **Investigar la prueba _next-tt-move_**: el test
   [next-tt-move](https://tests.stockfishchess.org/tests/view/68cb021d02c43c969fe7f028)
   fue rechazado con LLR = -2.94 tras 88 062 partidas; conviene estudiar logs y
   partidas para decidir si merece un nuevo intento con parámetros distintos.
4. **Seguir de cerca la serie _preQsExt_**: varias ejecuciones (por ejemplo
   [preQsExtD5](https://tests.stockfishchess.org/tests/view/68ccc07716c378179ee667cd))
   siguen activas o cercanas al corte (LLR ≈ 1.18 con 56 634 partidas); podrían
   beneficiarse de tests adicionales o combinaciones con otras extensiones.
5. **Apoyar los ajustes de búsqueda en curso**: proyectos como
   [tune_search4](https://tests.stockfishchess.org/tests/view/68c941f202c43c969fe7ee64)
   continúan acumulando datos (812 partidas de 200 000 planificadas); es recomendable
   aportar hardware y validar la estabilidad para acelerar la convergencia.

Mantener un seguimiento activo de estas tareas permitirá que Wordfish adopte las
mejoras que superen las pruebas y descarte con rapidez las ideas regresivas.
