#!/usr/bin/env bash
set -euo pipefail

# This script resets the local "main" branch so that it matches "work" exactly.
# It removes any tracked files from "main" that could conflict with "work".

if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "Este script debe ejecutarse dentro de un repositorio Git." >&2
  exit 1
fi

if [[ -n "$(git status --porcelain)" ]]; then
  echo "El árbol de trabajo debe estar limpio antes de continuar." >&2
  echo "Por favor, commitea o descarta los cambios pendientes." >&2
  exit 1
fi

if ! git show-ref --quiet refs/heads/work; then
  echo "La rama 'work' no existe en este repositorio." >&2
  exit 1
fi

current_branch=$(git rev-parse --abbrev-ref HEAD)

cleanup() {
  # Intenta volver a la rama original para no interrumpir el flujo del usuario.
  git checkout "$current_branch" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# Crea "main" si no existe y cambia a ella.
if git show-ref --quiet refs/heads/main; then
  git checkout main >/dev/null 2>&1
else
  git checkout -b main work >/dev/null 2>&1
fi

# Alinea "main" con "work", eliminando cualquier contenido previo.
git reset --hard work >/dev/null 2>&1
git clean -fd >/dev/null 2>&1

echo "La rama 'main' ahora es idéntica a 'work'."

trap - EXIT
git checkout "$current_branch" >/dev/null 2>&1
