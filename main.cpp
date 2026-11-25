// 8-Puzzle mini: tablero + reglas + heuristica + 3 algoritmos + tabla.
// Un solo archivo, sin dependencias externas. C++17.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Board = std::array<int, 9>;

const Board GOAL = {{1, 2, 3, 4, 5, 6, 7, 8, 0}};
const int INF = 1000000;

enum class Move { Up, Down, Left, Right, None };

const char* moveName(Move m) {
    switch (m) {
        case Move::Up: return "Up";
        case Move::Down: return "Down";
        case Move::Left: return "Left";
        case Move::Right: return "Right";
        default: return "None";
    }
}

Move opposite(Move m) {
    switch (m) {
        case Move::Up: return Move::Down;
        case Move::Down: return Move::Up;
        case Move::Left: return Move::Right;
        case Move::Right: return Move::Left;
        default: return Move::None;
    }
}

int distance1D(int a, int b) { return a > b ? a - b : b - a; }

bool isGoal(const Board& b) { return b == GOAL; }

int blankIndex(const Board& b) {
    for (int i = 0; i < 9; ++i) {
        if (b[i] == 0) return i;
    }
    return -1;
}

// mueve el espacio vacio; devuelve false si el movimiento sale del tablero
bool applyMove(const Board& b, Move m, Board& out) {
    const int blank = blankIndex(b);
    const int row = blank / 3;
    const int col = blank % 3;
    int target = -1;

    switch (m) {
        case Move::Up:
            if (row == 0) return false;
            target = blank - 3;
            break;
        case Move::Down:
            if (row == 2) return false;
            target = blank + 3;
            break;
        case Move::Left:
            if (col == 0) return false;
            target = blank - 1;
            break;
        case Move::Right:
            if (col == 2) return false;
            target = blank + 1;
            break;
        default:
            return false;
    }

    out = b;
    std::swap(out[blank], out[target]);
    return true;
}

// callback devuelve false para cortar la iteracion
template <class F>
bool forEachMove(const Board& b, F f) {
    const Move moves[4] = {Move::Up, Move::Down, Move::Left, Move::Right};
    for (Move m : moves) {
        Board next;
        if (applyMove(b, m, next)) {
            if (!f(next, m)) return false;
        }
    }
    return true;
}

bool solvable(const Board& b) {
    int inversions = 0;
    for (int i = 0; i < 9; ++i) {
        if (b[i] == 0) continue;
        for (int j = i + 1; j < 9; ++j) {
            if (b[j] != 0 && b[i] > b[j]) ++inversions;
        }
    }
    return inversions % 2 == 0;
}

int manhattan(const Board& b) {
    int total = 0;
    for (int i = 0; i < 9; ++i) {
        const int tile = b[i];
        if (tile == 0) continue;
        const int goal = tile - 1;
        total += distance1D(i / 3, goal / 3) + distance1D(i % 3, goal % 3);
    }
    return total;
}

struct BoardHash {
    std::size_t operator()(const Board& b) const noexcept {
        std::size_t code = 0;
        std::size_t multiplier = 1;
        for (int i = 0; i < 9; ++i) {
            code += static_cast<std::size_t>(b[i]) * multiplier;
            multiplier *= 9;
        }
        return code;
    }
};

struct Metrics {
    std::size_t expanded = 0;
    std::size_t generated = 0;
    std::size_t maxFrontier = 0;
    double ms = 0.0;

    void note(std::size_t n) {
        if (n > maxFrontier) maxFrontier = n;
    }
};

struct SearchResult {
    bool solved = false;
    int cost = 0;
    std::vector<Move> path;
    Metrics metrics;
};

struct Node {
    Board state;
    int parent;  // indice en nodes, -1 para la raiz
    Move move;
    int g;
    int h;
};

int fOf(const Node& n) { return n.g + n.h; }

void buildPath(const std::vector<Node>& nodes, int index, std::vector<Move>& path) {
    while (nodes[index].parent != -1) {
        path.push_back(nodes[index].move);
        index = nodes[index].parent;
    }
    std::reverse(path.begin(), path.end());
}

// ---------------------------------------------------------------------------
// GBFS: prioridad por menor h
// ---------------------------------------------------------------------------
SearchResult gbfs(const Board& start) {
    SearchResult r;
    r.metrics.generated = 1;

    if (isGoal(start)) {
        r.solved = true;
        return r;
    }
    if (!solvable(start)) return r;

    std::vector<Node> nodes;
    nodes.push_back(Node{start, -1, Move::None, 0, manhattan(start)});

    auto cmp = [&](int a, int b) {
        if (nodes[a].h != nodes[b].h) return nodes[a].h > nodes[b].h;
        return nodes[a].g > nodes[b].g;
    };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> frontier(cmp);
    frontier.push(0);
    r.metrics.note(frontier.size());

    std::unordered_set<Board, BoardHash> closed;

    while (!frontier.empty()) {
        const int cur = frontier.top();
        frontier.pop();

        if (!closed.insert(nodes[cur].state).second) continue;
        ++r.metrics.expanded;

        if (isGoal(nodes[cur].state)) {
            r.solved = true;
            r.cost = nodes[cur].g;
            buildPath(nodes, cur, r.path);
            break;
        }

        // Copia importante: nodes.push_back() puede reubicar el vector
        // e invalidar la referencia a nodes[cur].state mientras iteramos.
        const Board current = nodes[cur].state;
        const int parentG = nodes[cur].g;
        forEachMove(current, [&](const Board& next, Move mv) {
            if (closed.find(next) != closed.end()) return true;
            ++r.metrics.generated;
            nodes.push_back(Node{next, cur, mv, parentG + 1, manhattan(next)});
            frontier.push(static_cast<int>(nodes.size()) - 1);
            return true;
        });
        r.metrics.note(frontier.size());
    }
    return r;
}

// ---------------------------------------------------------------------------
// A*: prioridad por menor f = g + h, con lazy deletion
// ---------------------------------------------------------------------------
SearchResult aStar(const Board& start) {
    SearchResult r;
    r.metrics.generated = 1;

    if (isGoal(start)) {
        r.solved = true;
        return r;
    }
    if (!solvable(start)) return r;

    std::vector<Node> nodes;
    nodes.push_back(Node{start, -1, Move::None, 0, manhattan(start)});

    auto cmp = [&](int a, int b) {
        if (fOf(nodes[a]) != fOf(nodes[b])) return fOf(nodes[a]) > fOf(nodes[b]);
        return nodes[a].h > nodes[b].h;
    };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> frontier(cmp);
    frontier.push(0);
    r.metrics.note(frontier.size());

    std::unordered_map<Board, int, BoardHash> bestG;
    bestG[start] = 0;

    while (!frontier.empty()) {
        const int cur = frontier.top();
        frontier.pop();

        const auto it = bestG.find(nodes[cur].state);
        if (it == bestG.end() || it->second != nodes[cur].g) continue;
        ++r.metrics.expanded;

        if (isGoal(nodes[cur].state)) {
            r.solved = true;
            r.cost = nodes[cur].g;
            buildPath(nodes, cur, r.path);
            break;
        }

        // Misma precaucion que en GBFS: copiar antes de seguir insertando nodos.
        const Board current = nodes[cur].state;
        const int parentG = nodes[cur].g;
        forEachMove(current, [&](const Board& next, Move mv) {
            const int tentative = parentG + 1;
            const auto known = bestG.find(next);
            if (known != bestG.end() && known->second <= tentative) return true;

            bestG[next] = tentative;
            ++r.metrics.generated;
            nodes.push_back(Node{next, cur, mv, tentative, manhattan(next)});
            frontier.push(static_cast<int>(nodes.size()) - 1);
            return true;
        });
        r.metrics.note(frontier.size());
    }
    return r;
}

// ---------------------------------------------------------------------------
// IDA*: DFS por umbrales crecientes
// ---------------------------------------------------------------------------
SearchResult idaStar(const Board& start) {
    SearchResult r;

    if (isGoal(start)) {
        r.solved = true;
        r.metrics.generated = 1;
        return r;
    }
    if (!solvable(start)) return r;

    int bound = manhattan(start);

    while (bound < INF) {
        ++r.metrics.generated;  // la raiz se cuenta en cada iteracion

        std::unordered_map<Board, int, BoardHash> seen;
        seen[start] = 0;
        std::vector<Move> path;
        int nextBound = INF;
        int solution = INF;

        std::function<int(const Board&, int, int)> dfs =
            [&](const Board& s, int g, int limit) -> int {
            const int f = g + manhattan(s);
            ++r.metrics.expanded;

            if (f > limit) {
                if (f < nextBound) nextBound = f;
                return INF;
            }
            if (isGoal(s)) return g;

            forEachMove(s, [&](const Board& next, Move mv) {
                const int ng = g + 1;
                const auto known = seen.find(next);
                if (known != seen.end() && known->second <= ng) return true;

                seen[next] = ng;
                ++r.metrics.generated;
                path.push_back(mv);
                r.metrics.note(path.size());

                const int outcome = dfs(next, ng, limit);
                if (outcome != INF) {
                    solution = outcome;
                    return false;  // encontrado: no hacemos pop, queremos el camino
                }
                path.pop_back();
                return true;
            });
            return solution;
        };

        const int outcome = dfs(start, 0, bound);
        if (outcome != INF) {
            r.solved = true;
            r.cost = outcome;
            r.path = path;
            break;
        }
        if (nextBound >= INF) break;
        bound = nextBound;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Benchmark minimo
// ---------------------------------------------------------------------------
struct Instance {
    std::string name;
    Board board;
};

Board scramble(unsigned seed, int steps) {
    Board s = GOAL;
    Move last = Move::None;
    std::mt19937 rng(seed);

    for (int i = 0; i < steps; ++i) {
        std::vector<Move> legal;
        forEachMove(s, [&](const Board&, Move mv) {
            if (mv != opposite(last)) legal.push_back(mv);
            return true;
        });
        std::uniform_int_distribution<std::size_t> dist(0, legal.size() - 1);
        const Move chosen = legal[dist(rng)];
        Board next;
        applyMove(s, chosen, next);
        s = next;
        last = chosen;
    }
    return s;
}

std::vector<Instance> buildInstances() {
    std::vector<Instance> v;
    v.push_back(Instance{"meta_0", GOAL});

    const int steps[] = {2, 6, 12, 20};
    for (int i = 0; i < 4; ++i) {
        v.push_back(Instance{
            "semilla_" + std::to_string(100 + i) + "_pasos_" + std::to_string(steps[i]),
            scramble(100u + static_cast<unsigned>(i), steps[i])});
    }

    v.push_back(Instance{"sin_solucion", Board{{2, 1, 3, 4, 5, 6, 7, 8, 0}}});
    return v;
}

int main() {
    const int reps = 5;
    const std::vector<std::pair<std::string, std::function<SearchResult(const Board&)>>> algorithms = {
        {"GBFS", gbfs}, {"A*", aStar}, {"IDA*", idaStar}};

    std::cout << "Repeticiones por caso: " << reps << "\n\n";
    std::cout << std::left << std::setw(6) << "Algo"
              << std::setw(24) << "Instancia"
              << std::right << std::setw(7) << "Coste"
              << std::setw(10) << "Expand."
              << std::setw(10) << "Gener."
              << std::setw(10) << "Frontera"
              << std::setw(11) << "ms" << '\n';
    std::cout << std::string(68, '-') << '\n';

    for (const Instance& instance : buildInstances()) {
        for (const auto& algorithm : algorithms) {
            SearchResult sample;
            double totalMs = 0.0;

            for (int rep = 0; rep < reps; ++rep) {
                const auto t0 = std::chrono::steady_clock::now();
                const SearchResult r = algorithm.second(instance.board);
                const auto t1 = std::chrono::steady_clock::now();

                Metrics m = r.metrics;
                m.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                totalMs += m.ms;
                if (rep == 0) {
                    sample = r;
                    sample.metrics = m;
                }
            }

            const std::string cost = sample.solved ? std::to_string(sample.cost) : "---";
            std::cout << std::left << std::setw(6) << algorithm.first
                      << std::setw(24) << instance.name
                      << std::right << std::setw(7) << cost
                      << std::setw(10) << sample.metrics.expanded
                      << std::setw(10) << sample.metrics.generated
                      << std::setw(10) << sample.metrics.maxFrontier
                      << std::setw(11) << std::fixed << std::setprecision(3)
                      << totalMs / reps << '\n';
        }
    }
    return 0;
}
