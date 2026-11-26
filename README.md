# 8-Puzzle — versión simple

Un único archivo `main.cpp` que resuelve **el mismo tablero 3×3** con tres estrategias distintas y muestra una tabla para compararlas.

No hay carpetas, ni CMake, ni dependencias externas. Es la versión reducida del proyecto: toda la lógica está en un solo sitio para que sea fácil de leer y de explicar.

---

## 1. ¿Qué hace exactamente?

Toma un tablero del 8-Puzzle:

```text
1 2 3
4 5 6
7 8 0
```

donde `0` es el espacio vacío. Un movimiento consiste en intercambiar el `0` con una ficha vecina (arriba, abajo, izquierda o derecha). El objetivo es llegar al tablero ordenado mostrado arriba.

El programa prueba tres formas distintas de buscar ese objetivo:

| Algoritmo | Idea en una frase | ¿Da el mínimo de movimientos? |
|---|---|---|
| **GBFS** | "Siempre sigo el estado que parece más cercano a la meta". | No |
| **A*** | "Sigo el estado con menor coste ya recorrido + lo que falta". | Sí |
| **IDA*** | "Pruebo con un límite de pasos, y si no llego, lo subo poco a poco". | Sí |

Luego imprime, para cada caso, el coste de la solución, cuántos nodos se expandieron, cuántos se generaron y cuál fue el tamaño máximo de la frontera.

---

## 2. Cómo compilar y ejecutar

### Windows con GCC / MinGW

```powershell
g++ -std=c++17 -O2 main.cpp -o puzzle.exe
.\puzzle.exe
```

### WSL / Linux / macOS

```bash
g++ -std=c++17 -O2 main.cpp -o puzzle
./puzzle
```

No necesitas instalar nada más: solo un compilador con soporte C++17. También funciona con compiladores un poco más antiguos si aceptan `-std=c++1z`, porque el código evita características nuevas como `std::optional` o `inline` variables.

---

## 3. El problema

### Representación

El tablero es un `std::array<int, 9>`:

```cpp
using Board = std::array<int, 9>;
```

El índice lineal es `fila * 3 + columna`. Por ejemplo, el índice `4` es el centro y el `8` es la esquina inferior derecha.

Se usa un `array` y no un `vector` porque el tamaño siempre es 9: no se reserva memoria dinámica al copiar estados, y la copia es muy barata.

### Convención de movimientos

`Move` indica **hacia dónde se mueve el espacio vacío**, no hacia dónde se mueve la ficha:

| Movimiento | Nueva posición del `0` | Condición |
|---|---|---|
| `Up` | `blank - 3` | fila > 0 |
| `Down` | `blank + 3` | fila < 2 |
| `Left` | `blank - 1` | columna > 0 |
| `Right` | `blank + 1` | columna < 2 |

La legalidad se comprueba con **fila y columna**, no solo con `blank ± 1`. Si estás en la columna 2 y sumas 1, saltas a la fila siguiente: sería un movimiento inválido que el tablero real no permite.

### Estados sin solución

Antes de buscar se comprueba la paridad de inversiones. Como el tablero tiene ancho impar (3), un estado es soluble si el número de inversiones de las fichas `1..8` es par. Así evitamos que un algoritmo explore infinitamente un caso imposible.

---

## 4. La heurística

Se usa la **distancia de Manhattan**:

```text
h(s) = Σ |fila_actual - fila_meta| + |columna_actual - columna_meta|
```

se suma sobre las fichas `1..8`; el `0` no cuenta.

Es **admisible** porque cada ficha necesita al menos esa distancia para llegar a su sitio, y es **consistente** porque un solo movimiento puede cambiar `h` como máximo en 1. Eso es lo que permite que A* e IDA* sean óptimos.

---

## 5. Cómo está organizado `main.cpp`

Aunque es un solo archivo, está dividido en bloques bien delimitados:

| Bloque | Para qué sirve |
|---|---|
| Tipos y constantes | `Board`, `GOAL`, `Move`, `INF` |
| Reglas del tablero | `applyMove`, `forEachMove`, `solvable` |
| Heurística | `manhattan` |
| Hash del tablero | `BoardHash` (base 9) |
| Estructuras comunes | `Metrics`, `SearchResult`, `Node` |
| **GBFS** | búsqueda voraz por menor `h` |
| **A*** | búsqueda por menor `f = g + h` con *lazy deletion* |
| **IDA*** | DFS por umbrales crecientes |
| Benchmark | generación de instancias, repeticiones y tabla final |
| `main` | ejecuta todo y imprime los resultados |

---

## 6. Los tres algoritmos, paso a paso

### GBFS — Greedy Best-First Search

1. Parte del estado inicial con `h` calculado.
2. Mantiene una cola de prioridad ordenada por **menor `h`**.
3. Saca el mejor, lo marca como expandido y genera sus vecinos.
4. Un `unordered_set` evita expandir dos veces el mismo estado.

**Ventaja:** suele llegar rápido.  
**Problema:** ignora cuánto coste lleva acumulado, así que puede quedarse en un camino largo. En el caso de 20 pasos devuelve **42**.

### A*

1. Ordena la cola por **menor `f = g + h`**.
2. Si dos nodos empatan en `f`, prefiere el de menor `h`.
3. Un `unordered_map` guarda el mejor `g` visto para cada estado (`bestG`).
4. Como `priority_queue` no sabe actualizar prioridades (*decrease-key*), cuando aparece un mejor camino se inserta un nodo nuevo. El viejo se descarta al sacarlo comparando su `g` con `bestG`: eso es *lazy deletion*.

**Resultado:** con Manhattan es óptimo. En el caso de 20 pasos devuelve **20**.

### IDA*

1. Empieza con un umbral igual a `h(inicial)`.
2. Hace DFS podando cualquier rama donde `f > umbral`.
3. Si no encontró solución, el umbral pasa a ser el menor `f` que se pasó de límite.
4. Repite hasta encontrar la meta.

Cada iteración crea su propia tabla de transposición (`seen`), que se destruye al terminar: no queda información sucia de un umbral a otro. La "frontera" en IDA* es la pila de movimientos, así que su tamaño es la profundidad de la búsqueda.

**Resultado:** también óptimo, y usa mucha menos memoria que A*, pero reexpande más nodos.

---

## 7. Estructuras de datos y por qué

| Estructura | Dónde se usa | Por qué |
|---|---|---|
| `std::array<int, 9>` | tablero | Tamaño fijo, copia barata, sin heap |
| `std::vector<Node>` | nodos de GBFS/A* | Guarda los nodos por índice; el padre es un `int`, no un puntero |
| `std::priority_queue<int>` | frontera de GBFS/A* | `O(log n)` por inserción/extracción y almacenamiento contiguo |
| `std::unordered_set<Board, BoardHash>` | cerrados de GBFS | Consulta media `O(1)` de estados ya vistos |
| `std::unordered_map<Board, int, BoardHash>` | `bestG` y `seen` | Guardar el mejor coste conocido por estado |
| `std::vector<Move>` | camino y pila de IDA* | Solo 9 movimientos como máximo en profundidad |
| `std::function` | DFS recursivo de IDA* | Permite que la función se llame a sí misma capturando el estado local |

El hash del tablero es en base 9:

```text
code = Σ ficha[i] · 9^i
```

Como hay 9 posiciones y 9 dígitos posibles (`0..8`), hay como máximo `9^9 = 387.420.489` códigos, que caben en `size_t`. Para este problema **no hay colisiones**.

---

## 8. Resultados obtenidos

Repeticiones: 5 por caso. Compilado con GCC 6.3 usando `-O2`.

| Instancia | Óptimo | GBFS (coste / expandidos) | A* (coste / expandidos) | IDA* (coste / expandidos / pila) |
|---|---:|---:|---:|---:|
| `meta_0` | 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| `semilla_100_pasos_2` | 2 | 2 / 3 | 2 / 3 | 2 / 5 / 2 |
| `semilla_101_pasos_6` | 6 | 6 / 7 | 6 / 7 | 6 / 11 / 6 |
| `semilla_102_pasos_12` | 12 | 12 / 29 | 12 / 30 | 12 / 72 / 12 |
| `semilla_103_pasos_20` | 20 | **42 / 308** | **20 / 85** | **20 / 138 / 20** |
| `sin_solucion` | — | rechazado / 0 | rechazado / 0 | rechazado / 0 |

### Cómo leer la tabla

- En los casos fáciles los tres algoritmos empatan.
- En el caso difícil, **GBFS se sale**: devuelve 42 pasos donde el óptimo es 20.
- **A*** expands menos nodos que GBFS en ese caso (85 frente a 308) porque su prioridad es más informada.
- **IDA*** expande más que A* (138 frente a 85), pero su memoria crece con la profundidad (20) en lugar de con los 142 nodos generados de A*.

Los tiempos en milisegundos se imprimen en la tabla del programa, pero pueden variar entre ejecuciones; los números de nodos son deterministas.

---

## 9. Detalles que importan (y que se corrigieron)

1. **`Move` es la dirección del hueco**, no de la ficha. Fijar la convención evita confundir el camino al reconstruirlo.
2. **Control de fila/columna** en `applyMove`: evita el bug de "cruzar" de columna con `blank ± 1`.
3. **Hash perfecto en base 9**: más barato y sin colisiones para este dominio.
4. **Solubilidad antes de buscar**: los estados imposibles se rechazan con 0 expansiones.
5. **Copia del tablero antes de insertar nodos**: en GBFS y A* se hace `const Board current = nodes[cur].state;` antes de llamar a `forEachMove`. Si no, un `push_back` puede reubicar el `vector` e invalidar la referencia que se está iterando, generando tableros corruptos y soluciones no óptimas.


