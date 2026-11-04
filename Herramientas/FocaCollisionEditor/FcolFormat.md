# Formato FCOL, versión 3

Todos los enteros y flotantes son little-endian. Cada campo comienza en un offset múltiplo de 4. Las cadenas guardan su longitud como `uint32`, seguida de bytes UTF-8 y entre cero y tres bytes `00` para alinear el siguiente campo a 4 bytes. La longitud no incluye ese relleno.

## Cabecera

| Campo | Tamaño | Valor |
|---|---:|---|
| Firma | 4 bytes | ASCII `FCOL` |
| Versión | `uint32` | `3` |
| Cantidad de colisiones | `uint32` | Número de registros |

La cabecera no guarda nombre ni ruta de modelo de referencia.

## Registro de colisión

| Campo | Tamaño | Valor |
|---|---:|---|
| ID | `uint32` | Identificador editable y único de la colisión |
| Forma | `uint32` | `0` = AABB, `1` = OBB |
| Tipo | `uint32` | `0` = sólida, `1` = trigger |
| Centro | 3 `float32` | X, Y, Z en coordenadas del modelo |
| Tamaño | 3 `float32` | Extensiones completas X, Y, Z; deben ser positivas |
| Rotación | 3 `float32` | Euler X, Y, Z en grados; AABB se mantiene alineada a los ejes |

Los registros se escriben en orden, sin separadores adicionales. El archivo termina inmediatamente después del último registro y su tamaño total siempre es múltiplo de 4 bytes.

El editor también lee archivos de versiones 1 y 2. La versión 1 guardaba nombres de colisión en lugar de IDs; al leerla, asigna a cada registro su índice como ID. La versión 2 ya guardaba IDs, pero incluía el nombre del modelo en la cabecera. Al guardar, el editor escribe versión 3 y omite cualquier referencia al modelo.
