// YAO: Yet Another Othello
// Copyright (C) 2025 Dekrit Gampamole
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

using uint64 = unsigned long long;

// =========================================================================
// Part 1: CORE (Data Contracts & Pure Game Rules)
// - Board representation uses bitboards (uint64).
// - Rule functions are pure (do not modify input state).
// =========================================================================

static const std::string COORDS[64] = {
    "A1","B1","C1","D1","E1","F1","G1","H1",
    "A2","B2","C2","D2","E2","F2","G2","H2",
    "A3","B3","C3","D3","E3","F3","G3","H3",
    "A4","B4","C4","D4","E4","F4","G4","H4",
    "A5","B5","C5","D5","E5","F5","G5","H5",
    "A6","B6","C6","D6","E6","F6","G6","H6",
    "A7","B7","C7","D7","E7","F7","G7","H7",
    "A8","B8","C8","D8","E8","F8","G8","H8"
};

/**
 * @brief Enum to represent the player (black always starts).
 */
enum class Player {
    Black, // Black (starts)
    White  // White
};

/**
 * @brief Utility function to switch players.
 * @param p The current player.
 * @return The next player.
 */
Player switch_player(Player p) {
    return (p == Player::Black) ? Player::White : Player::Black;
}

/**
 * @brief Struct to store the current game state.
 * * A clean data contract that doesn't leak bitboard implementation details
 * except in the core rule functions.
 */
struct GameState {
    uint64 black_discs = 0;
    uint64 white_discs = 0;
    Player current_player = Player::Black;
    int pass_count = 0; // Counts consecutive passes
    std::string last_move_coord = "START";

    // History is stored in the GameController to save memory on a single GameState,
    // but this state is sufficient for the rule engine and AI.

    /**
     * @brief Initializes the board to the starting Othello position.
     */
    GameState() {
        // Initial position:
        // D4 (Black): Index 27
        // E5 (Black): Index 36
        // E4 (White): Index 28
        // D5 (White): Index 35
        black_discs = (1ULL << 27) | (1ULL << 36);
        white_discs = (1ULL << 28) | (1ULL << 35);
    }
};

/**
 * @brief Converts human-readable coordinates (A1-H8) to an index from 0-63.
 * * Map: A1=0, H1=7, A8=56, H8=63.
 * @param coord Coordinate string (e.g., "D3", "a1").
 * @return Index 0-63, or -1 if the input is invalid.
 */
int coord_to_index(const std::string& coord) {
    std::string upper_coord = coord;
    std::transform(upper_coord.begin(), upper_coord.end(), upper_coord.begin(), ::toupper);

    for (int i = 0; i < 64; ++i) {
        if (COORDS[i] == upper_coord) {
            return i;
        }
    }
    return -1; // Not found
}

/**
 * @brief Converts an index from 0-63 to human-readable coordinates (A1-H8).
 * * @param index Index 0-63.
 * @return Coordinate string (e.g., "D3").
 */
std::string index_to_coord(int index) {
    if (index < 0 || index > 63) return "XX";
    return COORDS[index];
}

namespace Core {

    // Directions for the Bitboard: Represented by changes in the 0-63 index
    // {Delta_Row, Delta_Col}
    const int DIRECTIONS[8] = {-9, -8, -7, -1, 1, 7, 8, 9};
    
    // Masks to prevent 'wrap-around' (a sequence jumping from H to A or vice-versa)
    const uint64 MASK_A = 0xFEFEFEFEFEFEFEFEULL; // Prevents wrap-around from column A (used for right shifts)
    const uint64 MASK_H = 0x7F7F7F7F7F7F7F7FULL; // Prevents wrap-around from column H (used for left shifts)

    /**
     * @brief Calculates the discs flipped in a single direction.
     * * @param move_mask Bitmask for the move position (1 active bit).
     * @param own_board The moving player's bitboard.
     * @param opp_board The opponent's bitboard.
     * @param direction The direction of movement (from the DIRECTIONS array).
     * @param mask The wrap-around boundary mask (MASK_A or MASK_H).
     * @return Bitmask of the flipped discs.
     */
    uint64 get_flips_in_direction(uint64 move_mask, uint64 own_board, uint64 opp_board, int direction, uint64 mask) {
        uint64 flipped = 0;
        uint64 current = move_mask;

        // Shift in the specified direction
        if (direction < 0) {
            current = (current & mask) >> (-direction);
        } else {
            current = (current & mask) << direction;
        }

        // 1. Traverse opponent's discs
        while (current != 0 && (current & opp_board) != 0) {
            flipped |= current; // Add to the flip list
            
            if (direction < 0) {
                current = (current & mask) >> (-direction);
            } else {
                current = (current & mask) << direction;
            }
        }

        // 2. Ensure it ends with one's own disc
        if (current != 0 && (current & own_board) != 0) {
            return flipped;
        }

        // If it doesn't end with one's own disc or reaches the edge, no flip
        return 0;
    }

    /**
     * @brief Calculates all legal moves (pure move generator).
     * * @param state The current game state.
     * @return A bitmask (uint64) where active bits indicate legal move positions.
     */
    uint64 generate_legal_moves(const GameState& state) {
        uint64 own_board = (state.current_player == Player::Black) ? state.black_discs : state.white_discs;
        uint64 opp_board = (state.current_player == Player::Black) ? state.white_discs : state.black_discs;
        
        // Empty board (where discs can be placed)
        uint64 empty_board = ~(own_board | opp_board);
        uint64 legal_moves = 0;

        // Iterate through all empty cells
        for (int i = 0; i < 64; ++i) {
            uint64 move_mask = (1ULL << i);

            if (move_mask & empty_board) {
                uint64 total_flips = 0;

                // Check in all eight directions
                for (int d = 0; d < 8; ++d) {
                    uint64 mask = 0xFFFFFFFFFFFFFFFFULL; // Default mask (columns B-G)
                    
                    // Determine the wrap-around mask
                    if (DIRECTIONS[d] % 8 != 0) { // Check horizontal/diagonal directions
                        if (DIRECTIONS[d] > 0) { // Left shift (E, NE, SE), use MASK_H
                             mask = MASK_H; 
                        } else { // Right shift (W, NW, SW), use MASK_A
                             mask = MASK_A; 
                        }
                    }

                    total_flips |= get_flips_in_direction(move_mask, own_board, opp_board, DIRECTIONS[d], mask);
                }

                if (total_flips != 0) {
                    legal_moves |= move_mask;
                }
            }
        }
        return legal_moves;
    }

    /**
     * @brief Calculates all discs flipped for a specific move.
     * * @param state The current game state.
     * @param move_index The 0-63 index of the valid move.
     * @return Bitmask of the flipped discs.
     */
    uint64 get_flips_for_move(const GameState& state, int move_index) {
        uint64 move_mask = (1ULL << move_index);
        uint64 own_board = (state.current_player == Player::Black) ? state.black_discs : state.white_discs;
        uint64 opp_board = (state.current_player == Player::Black) ? state.white_discs : state.black_discs;
        uint64 total_flips = 0;

        // Same as the move generator, but counts total flips
        for (int d = 0; d < 8; ++d) {
            uint64 mask = 0xFFFFFFFFFFFFFFFFULL; 
            
            if (DIRECTIONS[d] % 8 != 0) { 
                if (DIRECTIONS[d] > 0) { // Left shift (E, NE, SE), use MASK_H
                     mask = MASK_H; 
                } else { // Right shift (W, NW, SW), use MASK_A
                     mask = MASK_A; 
                }
            }
            total_flips |= get_flips_in_direction(move_mask, own_board, opp_board, DIRECTIONS[d], mask);
        }
        return total_flips;
    }

    /**
     * @brief Applies a valid move and returns a new state (pure function).
     * * @param state The current game state.
     * @param move_index The 0-63 index of the valid move (assumed to be validated).
     * @return New GameState after the move is applied.
     */
    GameState apply_move(const GameState& state, int move_index) {
        GameState next_state = state;
        uint64 move_mask = (1ULL << move_index);
        
        // 1. Determine the board and flips
        uint64 flips = get_flips_for_move(state, move_index);
        next_state.last_move_coord = index_to_coord(move_index);

        if (state.current_player == Player::Black) {
            // Place a new disc
            next_state.black_discs |= move_mask;
            // Flip opponent's discs
            next_state.black_discs |= flips;
            next_state.white_discs &= ~flips;
        } else {
            // Place a new disc
            next_state.white_discs |= move_mask;
            // Flip opponent's discs
            next_state.white_discs |= flips;
            next_state.black_discs &= ~flips;
        }

        // 2. Turn and pass
        next_state.current_player = switch_player(state.current_player);
        next_state.pass_count = 0; // Reset pass count after a valid move

        return next_state;
    }

    /**
     * @brief Applies a 'Pass' move (pure function).
     * * @param state The current game state.
     * @return New GameState after the Pass.
     */
    GameState apply_pass(const GameState& state) {
        GameState next_state = state;
        next_state.current_player = switch_player(state.current_player);
        next_state.pass_count = state.pass_count + 1;
        next_state.last_move_coord = "PASS";
        return next_state;
    }

    /**
     * @brief Counts the discs on a bitboard (popcount/popcnt).
     * @param board The bitboard.
     * @return The number of active bits (discs).
     */
    int count_discs(uint64 board) {
        // Standard C++20 popcount implementation or fallback (manual)
        // Using a manual loop here due to non-modern C++ limitations, but it's efficient on modern hardware.
        int count = 0;
        while (board > 0) {
            board &= (board - 1);
            count++;
        }
        return count;
    }

    /**
     * @brief Checks if the game is over.
     * * @param state The current game state.
     * @param black_legal The number of legal moves for Black.
     * @param white_legal The number of legal moves for White.
     * @return True if the game is in a terminal state.
     */
    bool is_terminal(const GameState& state, uint64 black_legal, uint64 white_legal) {
        // Condition 1: The board is full
        int total_discs = count_discs(state.black_discs) + count_discs(state.white_discs);
        if (total_discs == 64) {
            return true;
        }

        // Condition 2: Neither player can move (two consecutive passes)
        if (state.pass_count >= 2) {
            return true;
        }

        // Condition 3: One player has no discs (not standard Othello, but possible)
        if (Core::count_discs(state.black_discs) == 0 || Core::count_discs(state.white_discs) == 0) {
            return true;
        }

        // Additional: If the board is not full, and both players have no moves
        // (This is already covered by pass_count, but as a safety check):
        if (count_discs(black_legal) == 0 && count_discs(white_legal) == 0 && total_discs < 64) {
             return true;
        }

        return false;
    }
} // namespace Core


// =========================================================================
// Part 2: ENGINE (AI, Evaluation, Search)
// =========================================================================

namespace Engine {

    // Positional values based on standard Othello heuristics.
    // Prioritize Corners (0, 7, 56, 63) and avoid X-Squares (1, 6, 8, 15, 48, 55, 57, 62)
    const int POSITION_WEIGHTS[64] = {
        // A1 A2 A3 A4 A5 A6 A7 A8
        200, -20, 10,  5,  5, 10, -20, 200, // Row 1/8 (Corner: 200, X-Square: -20)
        -20, -30, -5, -5, -5, -5, -30, -20, // Row 2/7
        10,  -5,  2,  2,  2,  2,  -5,  10, // Row 3/6
        5,  -5,  2,  1,  1,  2,  -5,   5, // Row 4/5
        5,  -5,  2,  1,  1,  2,  -5,   5, 
        10,  -5,  2,  2,  2,  2,  -5,  10,
        -20, -30, -5, -5, -5, -5, -30, -20,
        200, -20, 10,  5,  5, 10, -20, 200
    };

    /**
     * @brief Calculates the heuristic value for a GameState (Evaluation).
     * * @param state The game state.
     * @param player The player being evaluated (maximize).
     * @return Integer heuristic value.
     */
    int evaluate(const GameState& state, Player ai_player) {
        int ai_score = 0;
        int opp_score = 0;

        // 1. Mobility (Number of legal moves)
        uint64 ai_legal = Core::generate_legal_moves(state);
        GameState next_state = state;
        next_state.current_player = switch_player(state.current_player);
        uint64 opp_legal = Core::generate_legal_moves(next_state);

        int ai_mobility = Core::count_discs(ai_legal);
        int opp_mobility = Core::count_discs(opp_legal);
        
        // Mobility Weight (e.g., 5x)
        ai_score += ai_mobility * 5;
        opp_score += opp_mobility * 5;

        // 2. Positional Stability (Position Weights)
        for (int i = 0; i < 64; ++i) {
            uint64 mask = (1ULL << i);
            if (state.black_discs & mask) {
                if (ai_player == Player::Black) {
                    ai_score += POSITION_WEIGHTS[i];
                } else {
                    opp_score += POSITION_WEIGHTS[i];
                }
            } else if (state.white_discs & mask) {
                if (ai_player == Player::White) {
                    ai_score += POSITION_WEIGHTS[i];
                } else {
                    opp_score += POSITION_WEIGHTS[i];
                }
            }
        }

        // 3. Disc Difference (Considered important at the end of the game)
        int black_discs = Core::count_discs(state.black_discs);
        int white_discs = Core::count_discs(state.white_discs);
        int disc_diff = (ai_player == Player::Black) ? (black_discs - white_discs) : (white_discs - black_discs);
        
        int total_discs = black_discs + white_discs;
        
        // Disc difference weight based on the game phase (e.g., 0.5x at the beginning, 5x at the end)
        double phase_weight = (total_discs <= 20) ? 0.5 : (total_discs <= 40 ? 2.0 : 5.0); 
        ai_score += (int)(disc_diff * phase_weight);
        
        // Final Score: AI_Score - Opponent_Score
        return ai_score - opp_score;
    }

    /**
     * @brief Implementation of the Minimax algorithm with Alpha-Beta Pruning.
     * * @param state The current game state.
     * @param depth The remaining search depth.
     * @param alpha The alpha value (maximum).
     * @param beta The beta value (minimum).
     * @param maximizing_player True if it's the AI's turn (maximizing), False if it's the opponent's (minimizing).
     * @param ai_player The player running the AI (used for the final evaluation).
     * @return The best heuristic value found.
     */
    int minimax_ab(GameState state, int depth, int alpha, int beta, bool maximizing_player, Player ai_player) {
        
        uint64 legal_moves_mask = Core::generate_legal_moves(state);

        // Terminal Case: Depth 0 or Game Over
        GameState opponent_state = state;
        opponent_state.current_player = switch_player(state.current_player);
        uint64 opponent_legal_moves = Core::generate_legal_moves(opponent_state);
        if (depth == 0 || Core::is_terminal(state, legal_moves_mask, opponent_legal_moves)) {
            return evaluate(state, ai_player);
        }

        // ----------------------------------------------------
        // Pass Case
        if (Core::count_discs(legal_moves_mask) == 0) {
            GameState next_state = Core::apply_pass(state);
            return minimax_ab(next_state, depth, alpha, beta, !maximizing_player, ai_player);
        }
        // ----------------------------------------------------

        if (maximizing_player) { // AI Player
            int max_eval = std::numeric_limits<int>::min();

            for (int i = 0; i < 64; ++i) {
                if (legal_moves_mask & (1ULL << i)) {
                    GameState next_state = Core::apply_move(state, i);
                    int eval = minimax_ab(next_state, depth - 1, alpha, beta, false, ai_player);
                    max_eval = std::max(max_eval, eval);
                    alpha = std::max(alpha, max_eval);
                    if (beta <= alpha) {
                        break; // Pruning
                    }
                }
            }
            return max_eval;
        } else { // Opponent Player
            int min_eval = std::numeric_limits<int>::max();

            for (int i = 0; i < 64; ++i) {
                if (legal_moves_mask & (1ULL << i)) {
                    GameState next_state = Core::apply_move(state, i);
                    int eval = minimax_ab(next_state, depth - 1, alpha, beta, true, ai_player);
                    min_eval = std::min(min_eval, eval);
                    beta = std::min(beta, min_eval);
                    if (beta <= alpha) {
                        break; // Pruning
                    }
                }
            }
            return min_eval;
        }
    }

    /**
     * @brief Finds the best move for the AI (main AI function).
     * * @param state The current game state.
     * @param depth The search depth.
     * @return The index of the best move (0-63), or -1 for a pass.
     */
    int find_best_move(const GameState& state, int depth) {
        uint64 legal_moves_mask = Core::generate_legal_moves(state);
        
        if (Core::count_discs(legal_moves_mask) == 0) {
            return -1; // Pass
        }

        int best_move_index = -2; // Default invalid index
        int best_eval = std::numeric_limits<int>::min();

        // Find the best move at the root level
        for (int i = 0; i < 64; ++i) {
            if (legal_moves_mask & (1ULL << i)) {
                // Apply the move
                GameState next_state = Core::apply_move(state, i);
                
                // Call Minimax on the next level (minimizer)
                int current_eval = minimax_ab(next_state, depth - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, state.current_player);

                if (current_eval > best_eval) {
                    best_eval = current_eval;
                    best_move_index = i;
                }
            }
        }
        
        return best_move_index;
    }

} // namespace Engine


// =========================================================================
// Part 3: UI & APP (GameController & Main Loop)
// =========================================================================

namespace UI {

    /**
     * @brief Displays the game board, score, and turn with a larger UI.
     */
    void print_board(const GameState& state, uint64 legal_moves = 0ULL) {
        // Column header row
        std::cout << "\n   ";
        for (int i = 0; i < 8; ++i) {
            std::cout << "  " << (char)('A' + i) << "   ";
        }
        std::cout << "\n";

        for (int row = 0; row < 8; ++row) {
            // Top border row
            std::cout << "  +";
            for (int col = 0; col < 8; ++col) {
                std::cout << "-----+";
            }
            std::cout << "\n";

            // Cell content rows (3 lines)
            for (int line = 0; line < 3; ++line) {
                // Row label (only in the middle)
                if (line == 1) {
                    std::cout << row + 1 << " |";
                } else {
                    std::cout << "  |";
                }

                for (int col = 0; col < 8; ++col) {
                    int index = row * 8 + col;
                    uint64 mask = (1ULL << index);
                    char piece_char = ' ';
                    bool is_legal = (legal_moves & mask);

                    if (state.black_discs & mask) {
                        piece_char = '#';
                    } else if (state.white_discs & mask) {
                        piece_char = 'O';
                    }

                    if (line == 1) {
                        if (piece_char == '#') {
                            std::cout << " ### |";
                        } else if (piece_char == 'O') {
                            std::cout << " | | |";
                        } else if (is_legal) {
                            std::cout << "  .  |";
                        } else {
                            std::cout << "     |";
                        }
                    } else if (line == 0 || line == 2) {
                         if (piece_char == '#') {
                            std::cout << " ### |";
                        } else if (piece_char == 'O') {
                            std::cout << " +-+ |";
                        } else {
                            std::cout << "     |";
                        }
                    }
                }
                std::cout << "\n";
            }
        }

        // Bottom border row
        std::cout << "  +";
        for (int col = 0; col < 8; ++col) {
            std::cout << "-----+";
        }
        std::cout << "\n";


        int black_score = Core::count_discs(state.black_discs);
        int white_score = Core::count_discs(state.white_discs);
        
        std::cout << "\nScore: # Black: " << black_score 
                  << " | O White: " << white_score << "\n";
        
        std::string player_name = (state.current_player == Player::Black) ? "# Black (You)" : "O White (AI)";
        std::cout << "Turn: " << player_name << "\n";
        std::cout << "Last Move: " << state.last_move_coord << "\n";
    }

    /**
     * @brief Struct to store the result of command parsing.
     */
    struct Command {
        enum Type { INVALID, MOVE, UNDO, HINT, QUIT, PASS };
        Type type = INVALID;
        int move_index = -1; // Only used if type == MOVE
        std::string error_message;
    };

    /**
     * @brief A strict UI command parser.
     */
    Command parse_command(const std::string& input, uint64 legal_moves) {
        Command cmd;
        std::string upper_input = input;
        std::transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

        // Trim whitespace
        upper_input.erase(upper_input.find_last_not_of(" \n\r\t")+1);
        upper_input.erase(0, upper_input.find_first_not_of(" \n\r\t"));

        if (upper_input == "Q") {
            cmd.type = Command::QUIT;
        } else if (upper_input == "U") {
            cmd.type = Command::UNDO;
        } else if (upper_input == "?") {
            cmd.type = Command::HINT;
        } else if (upper_input == "P") {
            if (Core::count_discs(legal_moves) == 0) {
                 cmd.type = Command::PASS;
            } else {
                 cmd.type = Command::INVALID;
                 cmd.error_message = "Cannot PASS: You still have legal moves!";
            }
        } else {
            int index = coord_to_index(upper_input);
            if (index != -1) {
                uint64 mask = (1ULL << index);
                if (legal_moves & mask) {
                    cmd.type = Command::MOVE;
                    cmd.move_index = index;
                } else {
                    cmd.type = Command::INVALID;
                    cmd.error_message = "Move " + upper_input + " is not legal. Try a cell marked with (.).";
                }
            } else {
                cmd.type = Command::INVALID;
                cmd.error_message = "Unknown command. Try A1-H8, U, P, ?, or Q.";
            }
        }
        return cmd;
    }
} // namespace UI


/**
 * @brief The controller class that mediates between the UI, Core, and Engine.
 */
class GameController {
private:
    std::vector<GameState> history_;
    const int AI_DEPTH = 5; // AI search depth (can be adjusted)

public:
    GameController() {
        // Initialize the starting state
        history_.push_back(GameState());
    }

    const GameState& get_current_state() const {
        return history_.back();
    }

    /**
     * @brief Handles the user's move (validation and state mutation).
     * @param move_index The 0-63 index of the move.
     */
    void handle_move(int move_index) {
        const GameState& current_state = get_current_state();
        GameState next_state = Core::apply_move(current_state, move_index);
        history_.push_back(next_state);
    }
    
    /**
     * @brief Handles a pass.
     */
    void handle_pass() {
        const GameState& current_state = get_current_state();
        GameState next_state = Core::apply_pass(current_state);
        history_.push_back(next_state);
    }

    /**
     * @brief Handles an undo.
     */
    bool handle_undo() {
        if (history_.size() > 1) {
            history_.pop_back();
            // If the AI is playing (Black vs. White), undo twice to return to the player's turn.
            if (get_current_state().current_player == Player::White && history_.size() > 1) {
                 history_.pop_back();
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Gets the best move from the AI.
     * @return The 0-63 index of the move, or -1 for a pass.
     */
    int get_ai_move() const {
        return Engine::find_best_move(get_current_state(), AI_DEPTH);
    }

    /**
     * @brief Checks the final game status.
     * @param legal_moves The legal moves mask for the current player.
     * @return A string with the final status, or an empty string if the game continues.
     */
    std::string check_game_end(uint64 legal_moves) const {
        const GameState& state = get_current_state();
        
        // Check legal moves for both players
        GameState next_player_state = state;
        next_player_state.current_player = switch_player(state.current_player);
        uint64 next_player_legal = Core::generate_legal_moves(next_player_state);

        if (Core::is_terminal(state, legal_moves, next_player_legal)) {
            int black_score = Core::count_discs(state.black_discs);
            int white_score = Core::count_discs(state.white_discs);

            if (black_score > white_score) {
                return "\n=== GAME OVER: BLACK (#) WINS! (" + std::to_string(black_score) + " - " + std::to_string(white_score) + ") ===\n";
            } else if (white_score > black_score) {
                return "\n=== GAME OVER: WHITE (O) WINS! (" + std::to_string(white_score) + " - " + std::to_string(black_score) + ") ===\n";
            } else {
                return "\n=== GAME OVER: DRAW! (" + std::to_string(black_score) + " - " + std::to_string(white_score) + ") ===\n";
            }
        }
        return "";
    }
};

/**
 * @brief The main application function.
 */
int main() {
    std::cout << "  __  _____  ____ \n";
    std::cout << "  \\ \\/ / _ |/ __ \\\n";
    std::cout << "   \\  / __ / /_/ /\n";
    std::cout << "   /_/_/ |_\\____/\n";
    std::cout << "=YET-ANOTHER-OTHELLO=\n";
    std::cout << "You (# Black) vs. AI (O White, Depth " << 5 << ")\n";
    std::cout << "Commands: A1-H8 (e.g., D3), U (Undo), P (Pass), ? (Hint), Q (Quit)\n";

    GameController controller;
    bool running = true;
    std::string input;
    
    // Set I/O for UTF-8 (for disc symbols)
    std::cout.imbue(std::locale("en_US.UTF-8")); 

    while (running) {
        const GameState& current_state = controller.get_current_state();
        uint64 human_legal_moves = Core::generate_legal_moves(current_state);
        
        // 1. Check for Game Over
        std::string game_status = controller.check_game_end(human_legal_moves);
        if (!game_status.empty()) {
            UI::print_board(current_state);
            std::cout << game_status;
            running = false;
            break;
        }

        // 2. Player's Turn (Black)
        if (current_state.current_player == Player::Black) {
            UI::print_board(current_state, human_legal_moves);

            if (Core::count_discs(human_legal_moves) == 0) {
                std::cout << "\n(# Black has no moves! Auto PASS.)\n";
                controller.handle_pass();
                continue;
            }

            std::cout << "\n> ";
            std::getline(std::cin, input);

            UI::Command cmd = UI::parse_command(input, human_legal_moves);

            switch (cmd.type) {
                case UI::Command::MOVE:
                    controller.handle_move(cmd.move_index);
                    break;
                case UI::Command::UNDO:
                    if (!controller.handle_undo()) {
                        std::cout << ">> Error: No more moves to undo (only the initial state remains).\n";
                    } else {
                        std::cout << ">> UNDO Successful. Returning to Black's turn.\n";
                    }
                    break;
                case UI::Command::HINT: {
                    std::cout << ">> Finding AI hint...\n";
                    int hint_index = controller.get_ai_move();
                    if (hint_index != -1) {
                         std::cout << ">> Hint: " << index_to_coord(hint_index) << "\n";
                    } else {
                         std::cout << ">> Hint: PASS.\n";
                    }
                    break;
                }
                case UI::Command::PASS:
                    controller.handle_pass();
                    std::cout << ">> Black chose to PASS.\n";
                    break;
                case UI::Command::QUIT:
                    running = false;
                    break;
                case UI::Command::INVALID:
                    std::cout << ">> Error: " << cmd.error_message << "\n";
                    break;
            }

        } else {
            // 3. AI's Turn (White)
            UI::print_board(current_state);
            std::cout << "\nO White's Turn (AI). Thinking...\n";

            int ai_move = controller.get_ai_move();
            
            if (ai_move == -1) {
                std::cout << ">> AI chose to PASS. (O White has no moves).\n";
                controller.handle_pass();
            } else {
                std::cout << ">> AI moves to: " << index_to_coord(ai_move) << "\n";
                controller.handle_move(ai_move);
            }
            // Add a visual pause
            #ifdef _WIN32
                Sleep(1000);
            #else
                // Fallback for non-Windows systems
                struct timespec ts = { 1, 0 };
                nanosleep(&ts, NULL);
            #endif
        }
    }

    std::cout << "\nThanks for playing!\n";
    return 0;
}


