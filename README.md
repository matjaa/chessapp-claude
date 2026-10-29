# Chess App (C++17 + SFML 3)

A desktop chess game for Windows where you play against one of three bots
(Easy / Medium / Hard).

## Folder layout

```
ChessApp/
  CMakeLists.txt     build script (downloads SFML 3 automatically)
  assets/            font (DejaVu Sans) and piece images (Cburnett)
  src/main.cpp       entry point
  src/gui/           SFML window, drawing, input
    Application        window, game, mouse/keyboard input, side panel
    BoardView          board, coordinates, highlights, pieces, flipping
    Button             side-panel buttons
    PromotionPicker    queen/rook/bishop/knight choice
    PieceTextures      loads the piece images
    Theme.hpp          layout sizes and colours
    Assets             finds the assets folder
  src/core/          chess rules (library chess_core, no SFML)
    Types.hpp          colours, pieces, squares, Move, MoveList
    Position.hpp/.cpp  board, FEN, legal move generation, attacks, hashing
    Game.hpp/.cpp      game state: moves played, checkmate/stalemate, draws, resign
    Notation.hpp/.cpp  moves in standard algebraic notation (Nf3, exd5, O-O, e8=Q+)
    Perft.hpp/.cpp     perft move-generation counter
  src/engine/        bots (library chess_engine, no SFML)
    Evaluation         material, piece-square tables, bishop pair
    Search             alpha-beta with iterative deepening and quiescence
    Bot                the Easy / Medium / Hard settings
  tests/             core_tests: rule tests + perft
                     engine_tests: mates, tactics, bot-vs-bot games
```

## Requirements (Windows)

- **Visual Studio 2022** with the workload *Desktop development with C++*
  (this includes the MSVC compiler, CMake and Ninja), **or**
- **VS Code** with the extensions *C/C++* and *CMake Tools*, plus the
  Visual Studio 2022 Build Tools (same workload) for the compiler.

No manual SFML install is needed. The first configure downloads and builds
SFML, which takes a few minutes; later builds are fast.

## Build and run

### Visual Studio 2022
1. *File → Open → Folder…* and pick the `ChessApp` folder.
2. Wait until the Output window says *CMake generation finished*.
3. In the toolbar, choose the configuration (**x64-Release** is best for
   playing; add it via *Manage Configurations…* if only x64-Debug exists).
4. Choose **ChessApp.exe** as the startup item and press **F5** (or Ctrl+F5).

### VS Code
1. *File → Open Folder…* and pick the `ChessApp` folder.
2. When CMake Tools asks for a kit, choose
   *Visual Studio Community 2022 Release - amd64*.
3. Click **Build** in the status bar, then the ▶ (Launch) button.

### Command line (Developer PowerShell for VS 2022)
```
cmake -S . -B build
cmake --build build --config Release
.\build\bin\Release\ChessApp.exe
```

The executable ends up in `build/.../bin`, with the `assets` folder copied
next to it automatically.

## Tests

`core_tests` checks the rules (castling, en passant, promotion, checkmate,
stalemate, all automatic draws, resigning, move notation) and runs perft on six standard
positions, including the start position and Kiwipete. It is built together
with the game. From the VS Code terminal (cmd):

```
cmake --build build --config Release && build\bin\Release\core_tests.exe
```

It ends with `ALL TESTS PASSED` (exit code 0). In a Debug build, add
`--quick` to skip the slowest perft depths.

`engine_tests` checks the bots: every level plays a mate in one, Hard finds
a mate in two, Medium and Hard win a free queen and avoid a poisoned pawn,
and a few bot-vs-bot games are played (all moves must be legal, Medium must
beat Easy). `--quick` skips the Hard game.

```
build\bin\Release\engine_tests.exe
```

## The bots

| Level  | Looks ahead                          | Randomness                    |
|--------|--------------------------------------|-------------------------------|
| Easy   | 1 move, does not see recaptures      | up to ±1.5 pawns per move     |
| Medium | 2 moves + all captures after that    | up to ±0.3 pawns per move     |
| Hard   | 4 moves + all captures after that    | none (equal moves at random)  |

All levels use the same evaluation (material and piece placement), extend
the search when in check and know about repetition and fifty-move draws.
The bot thinks on a background thread and waits at least half a second
before moving.

## Playing

Click a piece to select it, then click one of the marked squares to move.
Right-click or Esc cancels a selection.

The panel on the right has two views.

**Between games** (at start-up and after a game ends) it shows the choices:

- **Play as White / Black**: your pieces are put at the bottom. As Black, the
  bot moves first.
- **Bot level Easy / Medium / Hard**.
- **Start game** / **New game**: starts a game with these choices. As White
  you can also just make your first move.

After a game, the finished game's moves stay visible below the choices.

**During a game** the choices are hidden. The panel shows whose move it is,
which side and bot you are playing, and the moves so far in standard
algebraic notation (`1. e4 e5 2. Nf3 Nc6 ...`, with `+` for check and `#` for
mate; the newest move is bold). When the list is longer than the panel, the
newest moves are shown. At the bottom:

- **Flip board**: turns the board around (also available between games).
- **Resign**: gives up the game. Click it twice (the second click within
  three seconds) so it cannot happen by accident.

## Credits

- Font: DejaVu Sans (free licence, see `assets/fonts/DejaVu-LICENSE.txt`).
- Pieces: "Cburnett" set by Colin M.L. Burnett (see `assets/pieces/LICENSE.txt`).

## License

The source code is licensed under the GNU General Public License v3.0
(see `LICENSE`). The piece images and the font keep their own licences,
listed under Credits.
