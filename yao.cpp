// =========================================================================
// YAO : Yet Another Othello
// Copyright (c) 2025 Dekrit Gampamole. All Rights Reserved.
// =========================================================================

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

using uint64 = unsigned long long;

// =========================================================================
// Bagian 1: CORE (Kontrak Data & Aturan Permainan Murni)
// - Representasi papan menggunakan bitboard (uint64).
// - Fungsi-fungsi aturan bersifat murni (tidak mengubah state input).
// =========================================================================

/**
 * @brief Enum untuk merepresentasikan pemain (hitam selalu mulai).
 */
enum class Player {
    Black, // Hitam (mulai)
    White  // Putih
};

/**
 * @brief Fungsi utilitas untuk membalik pemain.
 * @param p Pemain saat ini.
 * @return Pemain berikutnya.
 */
Player switch_player(Player p) {
    return (p == Player::Black) ? Player::White : Player::Black;
}

/**
 * @brief Struct untuk menyimpan status permainan saat ini.
 * * Kontrak data yang bersih, tidak membocorkan detail implementasi bitboard
 * kecuali pada fungsi-fungsi aturan inti.
 */
struct GameState {
    uint64 black_discs = 0;
    uint64 white_discs = 0;
    Player current_player = Player::Black;
    int pass_count = 0; // Menghitung pass berturut-turut
    std::string last_move_coord = "START";

    // History disimpan di GameController untuk menghemat memori pada GameState tunggal,
    // namun state ini cukup untuk mesin aturan dan AI.

    /**
     * @brief Menginisialisasi papan ke posisi awal Othello.
     */
    GameState() {
        // Posisi awal:
        // D4 (Hitam): Index 27
        // E5 (Hitam): Index 36
        // E4 (Putih): Index 28
        // D5 (Putih): Index 35
        black_discs = (1ULL << 27) | (1ULL << 36);
        white_discs = (1ULL << 28) | (1ULL << 35);
    }
};

/**
 * @brief Konversi koordinat manusia (A1-H8) ke indeks 0-63.
 * * Peta: A1=0, H1=7, A8=56, H8=63.
 * @param coord String koordinat (mis. "D3", "a1").
 * @return Indeks 0-63, atau -1 jika input tidak valid.
 */
int coord_to_index(const std::string& coord) {
    if (coord.length() != 2) return -1;

    char col_char = std::toupper(coord[0]);
    char row_char = coord[1];

    if (col_char < 'A' || col_char > 'H' || row_char < '1' || row_char > '8') {
        return -1;
    }

    int col = col_char - 'A'; // 0-7
    int row = row_char - '1'; // 0-7

    // A1 (row 0, col 0) -> 0
    // H8 (row 7, col 7) -> 63
    return row * 8 + col;
}

/**
 * @brief Konversi indeks 0-63 ke koordinat manusia (A1-H8).
 * * @param index Indeks 0-63.
 * @return String koordinat (mis. "D3").
 */
std::string index_to_coord(int index) {
    if (index < 0 || index > 63) return "XX";
    char col_char = 'A' + (index % 8);
    char row_char = '1' + (index / 8);
    return std::string() + col_char + row_char;
}

namespace Core {

    // Arah-arah untuk Bitboard: Diwakili oleh perubahan indeks 0-63
    // {Delta_Row, Delta_Col}
    const int DIRECTIONS[8] = {-9, -8, -7, -1, 1, 7, 8, 9};
    
    // Masker untuk mencegah 'wrap-around' (deret melompat dari H ke A atau sebaliknya)
    const uint64 MASK_A = 0xFEFEFEFEFEFEFEFEULL; // Mencegah wrap-around dari kolom A (digunakan untuk pergeseran ke kanan)
    const uint64 MASK_H = 0x7F7F7F7F7F7F7F7FULL; // Mencegah wrap-around dari kolom H (digunakan untuk pergeseran ke kiri)

    /**
     * @brief Menghitung bidak yang ter-flip dalam satu arah.
     * * @param move_mask Bitmask untuk posisi langkah (1 bit aktif).
     * @param own_board Bitboard pemain yang bergerak.
     * @param opp_board Bitboard lawan.
     * @param direction Arah pergerakan (dari array DIRECTIONS).
     * @param mask Masker batas wrap-around (MASK_A atau MASK_H).
     * @return Bitmask dari bidak yang ter-flip.
     */
    uint64 get_flips_in_direction(uint64 move_mask, uint64 own_board, uint64 opp_board, int direction, uint64 mask) {
        uint64 flipped = 0;
        uint64 current = move_mask;

        // Geser ke arah yang ditentukan
        if (direction < 0) {
            current = (current & mask) >> (-direction);
        } else {
            current = (current & mask) << direction;
        }

        // 1. Lewati disk lawan
        while (current != 0 && (current & opp_board) != 0) {
            flipped |= current; // Tambahkan ke daftar flip
            
            if (direction < 0) {
                current = (current & mask) >> (-direction);
            } else {
                current = (current & mask) << direction;
            }
        }

        // 2. Pastikan diakhiri oleh disk sendiri
        if (current != 0 && (current & own_board) != 0) {
            return flipped;
        }

        // Jika tidak diakhiri oleh disk sendiri atau mencapai tepi, tidak ada flip
        return 0;
    }

    /**
     * @brief Menghitung semua langkah legal (generator langkah murni).
     * * @param state Status permainan saat ini.
     * @return Bitmask (uint64) di mana bit aktif menunjukkan posisi langkah legal.
     */
    uint64 generate_legal_moves(const GameState& state) {
        uint64 own_board = (state.current_player == Player::Black) ? state.black_discs : state.white_discs;
        uint64 opp_board = (state.current_player == Player::Black) ? state.white_discs : state.black_discs;
        
        // Papan kosong (tempat bidak bisa diletakkan)
        uint64 empty_board = ~(own_board | opp_board);
        uint64 legal_moves = 0;

        // Iterasi melalui semua sel kosong
        for (int i = 0; i < 64; ++i) {
            uint64 move_mask = (1ULL << i);

            if (move_mask & empty_board) {
                uint64 total_flips = 0;

                // Periksa di kedelapan arah
                for (int d = 0; d < 8; ++d) {
                    uint64 mask = 0xFFFFFFFFFFFFFFFFULL; // Default mask (kolom B-G)
                    
                    // Tentukan masker wrap-around
                    if (DIRECTIONS[d] % 8 != 0) { // Cek arah horizontal/diagonal
                        if (DIRECTIONS[d] > 0) { // Pergeseran ke kiri (E, NE, SE), pakai MASK_H
                             mask = MASK_H; 
                        } else { // Pergeseran ke kanan (W, NW, SW), pakai MASK_A
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
     * @brief Menghitung semua bidak yang ter-flip untuk langkah tertentu.
     * * @param state Status permainan saat ini.
     * @param move_index Indeks 0-63 dari langkah yang sah.
     * @return Bitmask dari bidak yang ter-flip.
     */
    uint64 get_flips_for_move(const GameState& state, int move_index) {
        uint64 move_mask = (1ULL << move_index);
        uint64 own_board = (state.current_player == Player::Black) ? state.black_discs : state.white_discs;
        uint64 opp_board = (state.current_player == Player::Black) ? state.white_discs : state.black_discs;
        uint64 total_flips = 0;

        // Sama seperti generator langkah, tapi hitung total flip
        for (int d = 0; d < 8; ++d) {
            uint64 mask = 0xFFFFFFFFFFFFFFFFULL; 
            
            if (DIRECTIONS[d] % 8 != 0) { 
                if (DIRECTIONS[d] > 0) { // Pergeseran ke kiri (E, NE, SE), pakai MASK_H
                     mask = MASK_H; 
                } else { // Pergeseran ke kanan (W, NW, SW), pakai MASK_A
                     mask = MASK_A; 
                }
            }
            total_flips |= get_flips_in_direction(move_mask, own_board, opp_board, DIRECTIONS[d], mask);
        }
        return total_flips;
    }

    /**
     * @brief Menerapkan langkah yang sah dan mengembalikan status baru (fungsi pure).
     * * @param state Status permainan saat ini.
     * @param move_index Indeks 0-63 langkah yang sah (diasumsikan sudah divalidasi).
     * @return GameState baru setelah langkah diterapkan.
     */
    GameState apply_move(const GameState& state, int move_index) {
        GameState next_state = state;
        uint64 move_mask = (1ULL << move_index);
        
        // 1. Tentukan papan dan flip
        uint64 flips = get_flips_for_move(state, move_index);
        next_state.last_move_coord = index_to_coord(move_index);

        if (state.current_player == Player::Black) {
            // Pasang bidak baru
            next_state.black_discs |= move_mask;
            // Flip bidak lawan
            next_state.black_discs |= flips;
            next_state.white_discs &= ~flips;
        } else {
            // Pasang bidak baru
            next_state.white_discs |= move_mask;
            // Flip bidak lawan
            next_state.white_discs |= flips;
            next_state.black_discs &= ~flips;
        }

        // 2. Giliran dan pass
        next_state.current_player = switch_player(state.current_player);
        next_state.pass_count = 0; // Reset pass count setelah langkah sah

        return next_state;
    }

    /**
     * @brief Menerapkan langkah 'Pass' (fungsi pure).
     * * @param state Status permainan saat ini.
     * @return GameState baru setelah Pass.
     */
    GameState apply_pass(const GameState& state) {
        GameState next_state = state;
        next_state.current_player = switch_player(state.current_player);
        next_state.pass_count = state.pass_count + 1;
        next_state.last_move_coord = "PASS";
        return next_state;
    }

    /**
     * @brief Menghitung bidak yang terisi di bitboard (popcount/popcnt).
     * @param board Bitboard.
     * @return Jumlah bit yang aktif (disks).
     */
    int count_discs(uint64 board) {
        // Implementasi popcount standar C++20 atau fallback (manual)
        // Di sini menggunakan loop manual karena keterbatasan C++ non-modern, tapi efisien di hardware modern.
        int count = 0;
        while (board > 0) {
            board &= (board - 1);
            count++;
        }
        return count;
    }

    /**
     * @brief Memeriksa apakah permainan sudah berakhir.
     * * @param state Status permainan saat ini.
     * @param black_legal Jumlah langkah legal Hitam.
     * @param white_legal Jumlah langkah legal Putih.
     * @return True jika permainan terminal.
     */
    bool is_terminal(const GameState& state, uint64 black_legal, uint64 white_legal) {
        // Kondisi 1: Papan penuh
        int total_discs = count_discs(state.black_discs) + count_discs(state.white_discs);
        if (total_discs == 64) {
            return true;
        }

        // Kondisi 2: Dua pemain tidak bisa bergerak (dua pass berturut-turut)
        if (state.pass_count >= 2) {
            return true;
        }

        // Kondisi 3: Salah satu pemain kehabisan bidak (tidak standar Othello, tapi bisa)
        if (Core::count_discs(state.black_discs) == 0 || Core::count_discs(state.white_discs) == 0) {
            return true;
        }

        // Tambahan: Jika papan tidak penuh, dan kedua pemain tidak punya langkah
        // (Ini sudah tercakup oleh pass_count, tapi sebagai safety check):
        if (count_discs(black_legal) == 0 && count_discs(white_legal) == 0 && total_discs < 64) {
             return true;
        }

        return false;
    }
} // namespace Core


// =========================================================================
// Bagian 2: ENGINE (AI, Evaluasi, Search)
// =========================================================================

namespace Engine {

    // Nilai posisi berdasarkan heuristik standar Othello.
    // Prioritaskan Sudut (0, 7, 56, 63) dan Hindari X-Squares (1, 6, 8, 15, 48, 55, 57, 62)
    const int POSITION_WEIGHTS[64] = {
        // A1 A2 A3 A4 A5 A6 A7 A8
        200, -20, 10,  5,  5, 10, -20, 200, // Baris 1/8 (Pojok: 200, X-Square: -20)
        -20, -30, -5, -5, -5, -5, -30, -20, // Baris 2/7
        10,  -5,  2,  2,  2,  2,  -5,  10, // Baris 3/6
        5,  -5,  2,  1,  1,  2,  -5,   5, // Baris 4/5
        5,  -5,  2,  1,  1,  2,  -5,   5, 
        10,  -5,  2,  2,  2,  2,  -5,  10,
        -20, -30, -5, -5, -5, -5, -30, -20,
        200, -20, 10,  5,  5, 10, -20, 200
    };

    /**
     * @brief Menghitung nilai heuristik untuk GameState (Evaluasi).
     * * @param state Status permainan.
     * @param player Pemain yang dievaluasi (maksimalkan).
     * @return Nilai integer heuristik.
     */
    int evaluate(const GameState& state, Player ai_player) {
        int ai_score = 0;
        int opp_score = 0;

        // 1. Mobilitas (Jumlah langkah legal)
        uint64 ai_legal = Core::generate_legal_moves(state);
        GameState next_state = state;
        next_state.current_player = switch_player(state.current_player);
        uint64 opp_legal = Core::generate_legal_moves(next_state);

        int ai_mobility = Core::count_discs(ai_legal);
        int opp_mobility = Core::count_discs(opp_legal);
        
        // Bobot Mobilitas (mis. 5x)
        ai_score += ai_mobility * 5;
        opp_score += opp_mobility * 5;

        // 2. Stabilitas Posisi (Bobot posisi)
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

        // 3. Selisih Bidak (Dianggap penting di akhir permainan)
        int black_discs = Core::count_discs(state.black_discs);
        int white_discs = Core::count_discs(state.white_discs);
        int disc_diff = (ai_player == Player::Black) ? (black_discs - white_discs) : (white_discs - black_discs);
        
        int total_discs = black_discs + white_discs;
        
        // Bobot selisih bidak berdasarkan fase permainan (mis. 0x di awal, 5x di akhir)
        double phase_weight = (total_discs <= 20) ? 0.5 : (total_discs <= 40 ? 2.0 : 5.0); 
        ai_score += (int)(disc_diff * phase_weight);
        
        // Final Score: AI_Score - Opponent_Score
        return ai_score - opp_score;
    }

    /**
     * @brief Implementasi algoritma Minimax dengan Alpha-Beta Pruning.
     * * @param state Status permainan saat ini.
     * @param depth Kedalaman pencarian yang tersisa.
     * @param alpha Nilai alpha (maksimum)
     * @param beta Nilai beta (minimum)
     * @param maximizing_player True jika pemain AI (memaksimalkan), False jika lawan (meminimalkan).
     * @param ai_player Pemain yang menjalankan AI (digunakan untuk evaluasi final).
     * @return Nilai heuristik terbaik yang ditemukan.
     */
    int minimax_ab(GameState state, int depth, int alpha, int beta, bool maximizing_player, Player ai_player) {
        
        uint64 legal_moves_mask = Core::generate_legal_moves(state);

        // Kasus Terminal: Kedalaman 0 atau Game Over
        GameState opponent_state = state;
        opponent_state.current_player = switch_player(state.current_player);
        uint64 opponent_legal_moves = Core::generate_legal_moves(opponent_state);
        if (depth == 0 || Core::is_terminal(state, legal_moves_mask, opponent_legal_moves)) {
            return evaluate(state, ai_player);
        }

        // ----------------------------------------------------
        // Kasus Pass
        if (Core::count_discs(legal_moves_mask) == 0) {
            GameState next_state = Core::apply_pass(state);
            return minimax_ab(next_state, depth, alpha, beta, !maximizing_player, ai_player);
        }
        // ----------------------------------------------------

        if (maximizing_player) { // Pemain AI
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
        } else { // Pemain Lawan
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
     * @brief Menemukan langkah terbaik untuk AI (fungsi utama AI).
     * * @param state Status permainan saat ini.
     * @param depth Kedalaman pencarian.
     * @return Indeks langkah 0-63 terbaik, atau -1 jika pass.
     */
    int find_best_move(const GameState& state, int depth) {
        uint64 legal_moves_mask = Core::generate_legal_moves(state);
        
        if (Core::count_discs(legal_moves_mask) == 0) {
            return -1; // Pass
        }

        int best_move_index = -2; // Default invalid index
        int best_eval = std::numeric_limits<int>::min();

        // Cari langkah terbaik di level root
        for (int i = 0; i < 64; ++i) {
            if (legal_moves_mask & (1ULL << i)) {
                // Terapkan langkah
                GameState next_state = Core::apply_move(state, i);
                
                // Panggil Minimax pada level berikutnya (minimizer)
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
// Bagian 3: UI & APP (GameController & Main Loop)
// =========================================================================

namespace UI {

    /**
     * @brief Menampilkan papan permainan, skor, dan giliran.
     */
    void print_board(const GameState& state, uint64 legal_moves = 0ULL) {
        std::cout << "\n  A B C D E F G H\n";
        std::cout << " +-----------------\n";
        
        for (int row = 0; row < 8; ++row) {
            std::cout << row + 1 << "|";
            for (int col = 0; col < 8; ++col) {
                int index = row * 8 + col;
                uint64 mask = (1ULL << index);
                
                if (state.black_discs & mask) {
                    std::cout << " \u25cf"; // Hitam
                } else if (state.white_discs & mask) {
                    std::cout << " \u25cb"; // Putih
                } else if (legal_moves & mask) {
                    std::cout << " \u00b7"; // Titik untuk langkah legal
                } else {
                    std::cout << "  "; // Kosong
                }
            }
            std::cout << " |\n";
        }
        std::cout << " +-----------------\n";

        int black_score = Core::count_discs(state.black_discs);
        int white_score = Core::count_discs(state.white_discs);
        
        std::cout << "Skor: \u25cf Hitam: " << black_score 
                  << " | \u25cb Putih: " << white_score << "\n";
        
        std::string player_name = (state.current_player == Player::Black) ? "\u25cf Hitam (Anda)" : "\u25cb Putih (AI)";
        std::cout << "Giliran: " << player_name << "\n";
        std::cout << "Langkah Terakhir: " << state.last_move_coord << "\n";
    }

    /**
     * @brief Struct untuk menyimpan hasil parsing perintah.
     */
    struct Command {
        enum Type { INVALID, MOVE, UNDO, HINT, QUIT, PASS };
        Type type = INVALID;
        int move_index = -1; // Hanya digunakan jika type == MOVE
        std::string error_message;
    };

    /**
     * @brief Parser perintah UI yang tegas.
     */
    Command parse_command(const std::string& input, uint64 legal_moves) {
        Command cmd;
        std::string token;
        std::stringstream ss(input);
        ss >> token;

        // Normalisasi dan validasi
        std::transform(token.begin(), token.end(), token.begin(), ::tolower);

        if (token == "quit" || token == "exit") {
            cmd.type = Command::QUIT;
        } else if (token == "undo") {
            cmd.type = Command::UNDO;
        } else if (token == "hint") {
            cmd.type = Command::HINT;
        } else if (token == "pass") {
            if (Core::count_discs(legal_moves) == 0) {
                 cmd.type = Command::PASS;
            } else {
                 cmd.type = Command::INVALID;
                 cmd.error_message = "Tidak bisa PASS: Anda masih punya langkah legal!";
            }
        } else if (token == "move") {
            std::string coord;
            if (ss >> coord) {
                int index = coord_to_index(coord);
                if (index != -1) {
                    uint64 mask = (1ULL << index);
                    if (legal_moves & mask) {
                        cmd.type = Command::MOVE;
                        cmd.move_index = index;
                    } else {
                        cmd.type = Command::INVALID;
                        cmd.error_message = "Langkah " + coord + " tidak legal. Coba sel yang ditandai (\u00b7).";
                    }
                } else {
                    cmd.type = Command::INVALID;
                    cmd.error_message = "Koordinat " + coord + " tidak valid. Gunakan format A1-H8 (mis. D3).";
                }
            } else {
                cmd.type = Command::INVALID;
                cmd.error_message = "Perintah MOVE membutuhkan koordinat (mis. MOVE D3).";
            }
        } else {
            cmd.type = Command::INVALID;
            cmd.error_message = "Perintah tidak dikenal. Coba MOVE D3, UNDO, HINT, PASS, atau QUIT.";
        }
        return cmd;
    }
} // namespace UI


/**
 * @brief Kelas pengontrol yang menengahi antara UI, Core, dan Engine.
 */
class GameController {
private:
    std::vector<GameState> history_;
    const int AI_DEPTH = 5; // Kedalaman pencarian AI (bisa disesuaikan)

public:
    GameController() {
        // Inisialisasi state awal
        history_.push_back(GameState());
    }

    const GameState& get_current_state() const {
        return history_.back();
    }

    /**
     * @brief Menangani langkah pengguna (validasi dan mutasi state).
     * @param move_index Indeks langkah 0-63.
     */
    void handle_move(int move_index) {
        const GameState& current_state = get_current_state();
        GameState next_state = Core::apply_move(current_state, move_index);
        history_.push_back(next_state);
    }
    
    /**
     * @brief Menangani pass.
     */
    void handle_pass() {
        const GameState& current_state = get_current_state();
        GameState next_state = Core::apply_pass(current_state);
        history_.push_back(next_state);
    }

    /**
     * @brief Menangani undo.
     */
    bool handle_undo() {
        if (history_.size() > 1) {
            history_.pop_back();
            // Jika AI bermain (Hitam vs Putih), undo 2 kali agar kembali ke giliran pemain.
            if (get_current_state().current_player == Player::White && history_.size() > 1) {
                 history_.pop_back();
            }
            return true;
        }
        return false;
    }

    /**
     * @brief Menghasilkan langkah terbaik dari AI.
     * @return Indeks langkah 0-63, atau -1 jika pass.
     */
    int get_ai_move() const {
        return Engine::find_best_move(get_current_state(), AI_DEPTH);
    }

    /**
     * @brief Memeriksa status akhir permainan.
     * @param legal_moves Masker langkah legal pemain saat ini.
     * @return Status akhir string, atau string kosong jika permainan berlanjut.
     */
    std::string check_game_end(uint64 legal_moves) const {
        const GameState& state = get_current_state();
        
        // Cek langkah legal untuk kedua pemain
        GameState next_player_state = state;
        next_player_state.current_player = switch_player(state.current_player);
        uint64 next_player_legal = Core::generate_legal_moves(next_player_state);

        if (Core::is_terminal(state, legal_moves, next_player_legal)) {
            int black_score = Core::count_discs(state.black_discs);
            int white_score = Core::count_discs(state.white_discs);

            if (black_score > white_score) {
                return "\n=== PERMAINAN BERAKHIR: HITAM (\u25cf) MENANG! (" + std::to_string(black_score) + " - " + std::to_string(white_score) + ") ===\n";
            } else if (white_score > black_score) {
                return "\n=== PERMAINAN BERAKHIR: PUTIH (\u25cb) MENANG! (" + std::to_string(white_score) + " - " + std::to_string(black_score) + ") ===\n";
            } else {
                return "\n=== PERMAINAN BERAKHIR: IMBANG! (" + std::to_string(black_score) + " - " + std::to_string(white_score) + ") ===\n";
            }
        }
        return "";
    }
};

/**
 * @brief Fungsi utama aplikasi.
 */
int main() {
    std::cout << "======================================\n";
//    std::cout << " __   __ _    ___   \n";
//    std::cout << " \ \ / // \  / _ \  \n";
//    std::cout << "  \ V // _ \| | | | \n";
//    std::cout << "   | |/ ___ \ |_| | \n";
//    std::cout << "   |_/_/   \_\___/  \n";
    std::cout << "YET ANOTHER OTHELLO \n";
    std::cout << "======================================\n";
    std::cout << "Anda (\u25cf Hitam) vs AI (\u25cb Putih, Kedalaman " << 5 << ")\n";
    std::cout << "Perintah: MOVE D3, UNDO, PASS, HINT, QUIT\n";

    GameController controller;
    bool running = true;
    std::string input;
    
    // Setel I/O untuk UTF-8 (untuk simbol bidak)
    std::cout.imbue(std::locale("en_US.UTF-8")); 

    while (running) {
        const GameState& current_state = controller.get_current_state();
        uint64 human_legal_moves = Core::generate_legal_moves(current_state);
        
        // 1. Cek Status Akhir
        std::string game_status = controller.check_game_end(human_legal_moves);
        if (!game_status.empty()) {
            UI::print_board(current_state);
            std::cout << game_status;
            running = false;
            break;
        }

        // 2. Giliran Pemain (Hitam)
        if (current_state.current_player == Player::Black) {
            UI::print_board(current_state, human_legal_moves);

            if (Core::count_discs(human_legal_moves) == 0) {
                std::cout << "\n(\u25cf Hitam tidak punya langkah! Otomatis PASS.)\n";
                controller.handle_pass();
                continue;
            }

            std::cout << "\n\u25cf Giliran Hitam > ";
            std::getline(std::cin, input);

            UI::Command cmd = UI::parse_command(input, human_legal_moves);

            switch (cmd.type) {
                case UI::Command::MOVE:
                    controller.handle_move(cmd.move_index);
                    break;
                case UI::Command::UNDO:
                    if (!controller.handle_undo()) {
                        std::cout << ">> Error: Tidak ada langkah untuk di-undo lagi (hanya tersisa state awal).\n";
                    } else {
                        std::cout << ">> UNDO Berhasil. Kembali ke giliran Hitam.\n";
                    }
                    break;
                case UI::Command::HINT: {
                    std::cout << ">> Mencari petunjuk AI...\n";
                    int hint_index = controller.get_ai_move();
                    if (hint_index != -1) {
                         std::cout << ">> Petunjuk: " << index_to_coord(hint_index) << "\n";
                    } else {
                         std::cout << ">> Petunjuk: PASS.\n";
                    }
                    break;
                }
                case UI::Command::PASS:
                    controller.handle_pass();
                    std::cout << ">> Hitam memilih PASS.\n";
                    break;
                case UI::Command::QUIT:
                    running = false;
                    break;
                case UI::Command::INVALID:
                    std::cout << ">> Error: " << cmd.error_message << "\n";
                    break;
            }

        } else {
            // 3. Giliran AI (Putih)
            UI::print_board(current_state);
            std::cout << "\n\u25cb Giliran Putih (AI). Memikirkan langkah...\n";

            int ai_move = controller.get_ai_move();
            
            if (ai_move == -1) {
                std::cout << ">> AI memilih PASS. (\u25cb Putih tidak punya langkah).\n";
                controller.handle_pass();
            } else {
                std::cout << ">> AI bergerak ke: " << index_to_coord(ai_move) << "\n";
                controller.handle_move(ai_move);
            }
            // Tambahkan jeda visual
            #ifdef _WIN32
                Sleep(1000);
            #else
                // Fallback untuk sistem non-Windows
                struct timespec ts = { 1, 0 };
                nanosleep(&ts, NULL);
            #endif
        }
    }

    std::cout << "\nTerima kasih sudah bermain!\n";
    return 0;
}


