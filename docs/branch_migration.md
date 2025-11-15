# Migración a la rama principal

Este repositorio ha sido preparado para que el trabajo realizado en la rama `work` pase a formar parte de la rama principal `main`.

## Contexto

- Toda la estructura del proyecto se ha verificado y trasladado tal cual se encontraba en `work`.
- Esta documentación sirve como comprobante del traspaso y referencia para futuros mantenimientos.

## Pasos realizados

1. Se creó la rama `main` a partir del último commit estable de `work`.
2. Se conservó el historial existente para que cualquier seguimiento sea transparente.
3. Se dejó constancia de la migración mediante este documento.

## Cómo garantizar que no haya conflictos con `main`

Para evitar que el historial anterior de `main` introduzca conflictos al
fusionar los trabajos de `work`, se puede alinear directamente el contenido de
ambas ramas. El repositorio incluye ahora un script que automatiza el proceso:

```bash
./scripts/reset_main_to_work.sh
```

El script:

1. Verifica que el árbol de trabajo esté limpio.
2. Crea la rama `main` (si aún no existe) o se cambia a ella.
3. Restablece `main` para que sea idéntica a `work`, eliminando cualquier
   archivo rastreado que pudiese generar conflictos.
4. Devuelve al usuario a la rama original en la que estaba trabajando.

Si se prefiere realizar los pasos manualmente, se puede ejecutar la siguiente
secuencia de comandos:

```bash
git checkout main        # o git checkout -b main work si aún no existe
git reset --hard work    # descarta cualquier cambio previo en main
git clean -fd            # elimina archivos/directorios rastreados obsoletos
git checkout work        # vuelve a la rama de trabajo
```

## Próximos pasos sugeridos

- Ejecutar el script anterior antes de publicar `main` como rama predeterminada.
- Establecer `main` como la rama por defecto en el repositorio remoto.
- Eliminar la rama `work` cuando ya no sea necesaria.
- Continuar con el desarrollo habitual tomando `main` como base.
