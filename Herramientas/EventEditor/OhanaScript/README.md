# OhanaScript integrado en EventEditor

Los archivos de `include/` y `source/` son una copia del compilador OhanaScript del proyecto OHANA4P. `source/main.cpp` se deja fuera porque EventEditor llama directamente al lexer, parser, compilador y serializador. El serializador conserva la versión 2 del bytecode, permite escribirlo en memoria y expone los índices de su string pool.

## Exportación `.fest`

El archivo no contiene JSON. La única firma del contenedor es `FEST`; la región StartEvent no lleva firma propia. Todos los enteros son little-endian.

| Campo | Tamaño | Contenido |
| --- | ---: | --- |
| Firma | 4 bytes | `FEST` |
| Versión del contenedor | 4 bytes | `2` |
| Tamaño de la región StartEvent | 4 bytes | Contador y registros StartEvent |
| Tamaño del bytecode | 4 bytes | Número de bytes del payload OhanaScript |
| Región StartEvent | Variable | Empieza inmediatamente después de la cabecera de 16 bytes |
| Bytecode | Variable | Bytecode OhanaScript versión `2` |

La región StartEvent empieza con un contador de filas (`u32`). Cada fila ocupa 12 bytes:

| Campo | Tamaño | Contenido |
| --- | ---: | --- |
| Índice de string | 4 bytes | Índice base cero en el string pool del bytecode |
| Entero 1 | 4 bytes | Valor entero en complemento a dos |
| Entero 2 | 4 bytes | Valor entero en complemento a dos |

Cada string de StartEvent se agrega al string pool OhanaScript. El serializador deduplica los strings repetidos y devuelve sus índices para que la región StartEvent los referencie. Así, la región no almacena copias UTF-8 de los strings.

El bytecode empieza al terminar la región StartEvent. No se añade manifiesto JSON ni otra firma al archivo. El bytecode conserva su formato OhanaScript existente; su primer byte es la versión del bytecode, no una firma. La cantidad de tracks y el índice de track de cada evento se incluyen como enteros en las globales compiladas. Los eventos `Animation` exportan también `MdlID`, `AnmID` y `argument` como enteros. Los nombres visibles y tipos (`Animation`, `Script`, `Camera`) de los eventos no se exportan. Los scripts de timeline se compilan como funciones `evento` cuando corresponde.

Cada evento `Script` referencia un archivo externo de código OhanaScript. En el diálogo del evento, `Create...` crea un archivo `.ohs` en la ubicación elegida y `Open...` adjunta uno existente; EventEditor no edita el código dentro del diálogo. El `.events.json` guarda la ruta relativa al proyecto cuando es posible, y el exportador lee el archivo fuente para compilarlo. Si el código no declara un `evento` ni una `funcion`, se trata como el cuerpo de un evento y se envuelve automáticamente en una función con nombre estable para esa fila de timeline. Los proyectos antiguos que todavía contienen el campo `script` integrado siguen siendo legibles.

## Menú de consola del mapa

Al cargar el `.fest`, el motor ejecuta `evento main` una vez. Los scripts pueden crear y consultar un menú de depuración con estas funciones nativas:

| Función | Uso |
| --- | --- |
| `menu_crear(titulo)` | Crea el menú y elimina sus opciones anteriores. |
| `menu_opcion(texto)` | Añade una opción habilitada. |
| `menu_habilitar(indice, habilitada)` | Activa o desactiva una opción por índice base cero. |
| `menu_mostrar()` | Muestra el menú. |
| `menu_cerrar()` | Cierra y oculta el menú. |
| `menu_seleccion()` | Devuelve el índice seleccionado o `-1`. |
| `menu_esperar()` | Suspende el script hasta que se confirme una opción y devuelve su índice. |
| `mapa_cargar(id)` | Solicita cargar el mapa de `map.dat` con ese ID cuando termina el paso actual del script. |
| `compilacion_fecha()` | Devuelve la fecha de compilación como número `AAAAMMDD`. |
| `compilacion_hora()` | Devuelve la hora de compilación como número `HHMMSS`. |
| `compilacion_autor()` | Devuelve el usuario configurado en el entorno de compilación. |
| `compilacion_modo()` | Devuelve `0` para Debug y `1` para Release. |
| `compilacion_info()` | Devuelve una línea con modo, fecha, hora y autor para mostrar con `mostrar()`. |

Cada mapa usa su mismo ID para encontrar su archivo dentro de `mapinfo.dat`. Dentro de ese archivo, el script FEST inicial siempre está en el ID 0. Así, el mapa 1 busca su propio MapInfo ID 1 y ejecuta su script ID 0 con la escena activa.

En modo Debug, las flechas arriba/abajo cambian la selección, Enter la confirma y Escape cierra el menú. Ejemplo:

```text
evento main:
    menu_crear("FCSEDAVID4NX - MAP SELECT")
    mostrar(compilacion_info())
    menu_opcion("0: ActionTest (mapa 0)")
    menu_opcion("1: ActionTest (mapa 1)")
    menu_mostrar()

    variable opcion = menu_esperar()
    menu_cerrar()
    si opcion == 0:
        mapa_cargar(0)
    si opcion == 1:
        mapa_cargar(1)
```

`menu_esperar()` deja suspendido el mismo `evento main`; el motor lo reanuda en sus pasos fijos mientras sigue dibujando y procesando entrada.

Para compilar desde consola sin abrir la interfaz, pasa la ruta del proyecto como argumento:

```text
EventEditor.exe "C:\ruta\nivel.events.json" ["C:\destino\nivel.fest"]
```

El segundo argumento es opcional y puede ser una ruta `.fest` o una carpeta de destino. Si se omite, el `.fest` se crea junto al JSON usando el nombre completo sin la extensión `.json` (por ejemplo, `nivel.events.fest`). Sin argumentos, el programa abre el editor normalmente.
