# YAO: Yet Another Othello

This is a simple implementation of the Othello game played in the terminal. You will play as the Blue pieces against a simple AI that plays as the Yellow pieces.

## Features

- **Text-Based Interface**: Play directly in your terminal.
- **Player vs. AI**: You (Blue) against the AI (Yellow).
- **Smart AI**: The AI uses the Minimax algorithm with Alpha-Beta Pruning optimization to determine the best move.
- **Legal Move Display**: Valid moves will be marked with a dot (`·`) on the board.
- **In-Game Commands**:
    - `<coordinates>`: To place a piece (e.g., `D3`).
    - U `UNDO`: To undo your last move and the AI's move.
    - ? `HINT`: To ask the AI for a move suggestion.
    - P `PASS`: To pass your turn if you have no legal moves.
    - Q `QUIT`: To exit the game.

## How to Compile

Make sure you have a C++ compiler (like `g++`). Use the following command to compile the code:

```bash
g++ yao.cpp -o othello -std=c++17 -Wall
```

This command will generate an executable file named `othello`.

## How to Run

After successful compilation, run the game with the following command:

```bash
./othello
```

## How to Play

1. Run the game.
2. You are the **Blue** player.
3. On your turn, enter the coordinates (e.g., `F5`).
4. The AI will automatically take its turn after you.
5. The game ends when the entire board is filled or when neither player can make a move.
