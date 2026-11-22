// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#ifndef HYPERNOVA_MOVEGEN_H
#define HYPERNOVA_MOVEGEN_H

/**
 * ============================================================================
 * HYPERNOVA CHESS ENGINE - Move Generation
 * ============================================================================
 *
 * High-performance move generator using bitboard techniques.
 * Generates legal moves only (no pseudo-legal).
 *
 * Optimizations:
 *   - Stage-based generation (tactical first for QSearch)
 *   - Inline move picking with SEE
 *   - Bulk generation with pre-sized arrays
 *   - Branchless check evasion generation
 *
 * Stage 0: TT move (hash move)
 * Stage 1: Captures (MVV-LVA ordered + SEE filtered)
 * Stage 2: Good quiet moves (history ordered)
 * Stage 3: Bad captures (SEE < 0)
 * Stage 4: Killer moves
 * Stage 5: Counter moves
 * Stage 6: Rest of quiet moves
 *
 * License: MIT
 * Author: HyperNova Team
 * ============================================================================
 */

#include "types.h"
#include "board.h"

// ============================================================================
// MOVE LIST
// ============================================================================

/// A move + score pair for ordering
struct ScoredMove {
    Move move;
    int score;

    bool operator<(const ScoredMove& other) const noexcept {
        return score < other.score;
    }
};

/// Fixed-capacity move list (avoids heap allocation)
struct MoveList {
    std::array<ScoredMove, MAX_MOVES> moves;
    int size{0};

    void add(Move m) noexcept {
        moves[size].move = m;
        moves[size].score = 0;
        ++size;
    }

    void add(Move m, int score) noexcept {
        moves[size].move = m;
        moves[size].score = score;
        ++size;
    }

    ScoredMove& operator[](int i) noexcept { return moves[i]; }
    const ScoredMove& operator[](int i) const noexcept { return moves[i]; }

    void clear() noexcept { size = 0; }
    bool empty() const noexcept { return size == 0; }

    // Sort by score descending (simple insertion sort for small lists)
    void sort() noexcept {
        for (int i = 1; i < size; ++i) {
            ScoredMove key = moves[i];
            int j = i - 1;
            while (j >= 0 && moves[j].score < key.score) {
                moves[j + 1] = moves[j];
                --j;
            }
            moves[j + 1] = key;
        }
    }

    // Partial sort: only sort first N elements, pick best at index
    void sort(int n) noexcept {
        if (n >= size) { sort(); return; }
        // Selection sort first n
        for (int i = 0; i < n; ++i) {
            int best = i;
            for (int j = i + 1; j < size; ++j) {
                if (moves[j].score > moves[best].score)
                    best = j;
            }
            if (best != i) std::swap(moves[i], moves[best]);
        }
    }

    auto begin() noexcept { return moves.begin(); }
    auto end() noexcept { return moves.begin() + size; }
    auto begin() const noexcept { return moves.begin(); }
    auto end() const noexcept { return moves.begin() + size; }
};

// ============================================================================
// EXTENDED MOVE PICKING (Stage-based)
// ============================================================================

/// Stage-based move picker for search
class MovePicker {
public:
    MovePicker() noexcept = default;

    /// Initialize for root/bestmove search
    void init(const Position& pos, Move ttMove, Depth depth,
              const ButterflyHistory* mainHistory,
              const ContinuationHistory* contHistory,
              const CaptureHistory* capHistory,
              Move countermove) noexcept;

    /// Initialize for quiescence search (captures only)
    void init_qsearch(const Position& pos, Move ttMove) noexcept;

    /// Initialize for probcut
    void init_probcut(const Position& pos, Move ttMove, Value threshold) noexcept;

    /// Get next move (returns NONE_MOVE when exhausted)
    Move next_move() noexcept;

    /// Get the score of the last returned move
    int last_score() const noexcept { return lastScore; }

    /// Current stage
    int stage() const noexcept { return curStage; }

private:
    /// Generate all moves for current stage
    void generate_stage() noexcept;

    /// Score a capture using MVV-LVA
    int score_capture(Move m) const noexcept;

    /// Score a quiet move using history
    int score_quiet(Move m) const noexcept;

    /// Static Exchange Evaluation
    bool see(Move m, Value threshold = 0) const noexcept;

    enum Stages : int {
        STAGE_TT = 0,
        STAGE_INIT_CAPTURES = 1,
        STAGE_GOOD_CAPTURES = 2,
        STAGE_KILLER1 = 3,
        STAGE_KILLER2 = 4,
        STAGE_COUNTER = 5,
        STAGE_QUIET = 6,
        STAGE_BAD_CAPTURES = 7,
        STAGE_QS_CHECKS = 8,
        STAGE_QS_CAPTURES = 9,
        STAGE_EVASIONS = 10,
        STAGE_PROBCUT = 11
    };

    const Position* pos{nullptr};
    Move ttMove{0};
    Move killer1{0}, killer2{0};
    Move counter{0};
    Depth depth{0};
    MoveList curMoves;
    int curIdx{0};
    int curStage{STAGE_TT};
    int lastScore{0};
    Value probcutThreshold{0};
    bool skipQuiets{false};
    bool skipEarly{false};
    const ButterflyHistory* mainHistory{nullptr};
    const ContinuationHistory* contHistory{nullptr};
    const CaptureHistory* capHistory{nullptr};
};

// ============================================================================
// MOVE GENERATION FUNCTIONS
// ============================================================================

/// Generate all legal moves in a position
/// @param pos The position
/// @param moves Output move list
void gen_legal_moves(const Position& pos, MoveList& moves) noexcept;

/// Generate only captures (for quiescence search)
/// @param pos The position
/// @param moves Output move list
void gen_captures(const Position& pos, MoveList& moves) noexcept;

/// Generate only non-captures
/// @param pos The position
/// @param moves Output move list
void gen_quiets(const Position& pos, MoveList& moves) noexcept;

/// Generate check evasions
/// @param pos The position
/// @param moves Output move list
void gen_evasions(const Position& pos, MoveList& moves) noexcept;

/// Generate only moves that give check (for QSearch verification)
/// @param pos The position
/// @param moves Output move list
void gen_checks(const Position& pos, MoveList& moves) noexcept;

// ============================================================================
// PERFT (Performance Test)
// ============================================================================

/// Run a perft test from a position
/// @param pos The position
/// @param depth Depth to search
/// @return Number of leaf nodes
std::uint64_t perft(Position& pos, int depth) noexcept;

/// Run a divide test (perft for root moves)
void divide(Position& pos, int depth) noexcept;

// ============================================================================
// STATIC EXCHANGE EVALUATION
// ============================================================================

/// Compute Static Exchange Evaluation for a move
/// @param pos The position
/// @param m The move
/// @return SEE score in centipawns
Value see(const Position& pos, Move m) noexcept;

/// Check if SEE for a move is above a threshold
bool see_above(const Position& pos, Move m, Value threshold) noexcept;

// ============================================================================
// CONSTANTS
// ============================================================================

/// Score bonuses for move ordering (from Stockfish)
constexpr int MAX_HISTORY = 16384;
constexpr int HISTORY_MULTIPLIER = 324;
constexpr int GOOD_CAPTURE_BONUS = 0;
constexpr int BAD_CAPTURE_PENALTY = -1;
constexpr int KILLER_BONUS = 8000;
constexpr int COUNTER_BONUS = 7000;
constexpr int TT_MOVE_SCORE = 20000;
constexpr int PROMOTION_BONUS = 6000;
constexpr int CAPTURE_BONUS_BASE = 5000;

/// MVV-LVA scores [victim][attacker]
extern const int MVV_LVA[PIECE_TYPE_NB][PIECE_TYPE_NB];

/// Move existence sentinel
constexpr Move NONE_MOVE = 0;

#endif // HYPERNOVA_MOVEGEN_H