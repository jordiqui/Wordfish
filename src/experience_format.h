#pragma once
#include <cstdint>
#include <array>

#pragma pack(push, 1)

struct ExpHeaderV2 {
    // EXACTO: 32 bytes. Cadena + NUL + padding a 32.
    char signature[32]; // "SugaR Experience version 2\0" + zeros
};

struct ExpIndexRoot {
    // Bytes inmediatamente después de la cabecera.
    // Campos deducidos por comparación con Revolution.
    // No uses tipos dependientes de plataforma (nada de time_t).
    uint32_t magic;          // 0x44707223 (LE)  // 'Dp r#' en hex al revés
    uint64_t salt_or_uuid;   // semilla aleatoria/uuid (puede ser rand64)
    uint16_t record_size;    // 0x0011  (17 bytes)  -- ver nota abajo
    uint16_t key_size;       // 0x0002  (2 bytes)   -- ver nota abajo
    uint64_t reserved0;      // pon a 0
};

struct ExpDummyEntry {
    // Entrada mínima para que el visor no vea "archivo vacío".
    // Adáptalo a tu layout real si usas otra clave.
    uint64_t zobrist;  // clave posición
    uint16_t move;     // movimiento codificado (por ejemplo 16 bits from|to)
    int16_t  score;    // eval acumulada
    uint8_t  depth;    // profundidad media
    uint8_t  count;    // ocurrencias
    // ajusta si tu formato guarda más campos; mantén pack(1)
};

#pragma pack(pop)

static_assert(sizeof(ExpHeaderV2) == 32, "Header must be 32 bytes");

// Nota sobre record_size y key_size: En Revolution el bloque que sigue a la cabecera contiene estas parejas como 0x0011 y 0x0002 (vistas en LE como bytes 11 00 y 02 00). Si tu layout final difiere, actualiza ambos para reflejar el tamaño real de tus registros y de la clave.
