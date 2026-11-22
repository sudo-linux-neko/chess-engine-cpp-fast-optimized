// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#ifndef HYPERNOVA_BOARD_H
#define HYPERNOVA_BOARD_H

/**
 * ============================================================================
 * HYPERNOVA CHESS ENGINE - Board State
 * ============================================================================
 *
 * Compact board representation with incremental Zobrist hashing,
 * piece lists for fast NNUE feature extraction, and full FEN support.
 *
 * Uses piece lists (array-based) instead of iterating bitboards for
 * NNUE feature updates, which is significantly faster.
 *
 * Memory: 256 bytes - fits in 4 cache lines
 *
 * License: MIT
 * Author: HyperNova Team
 * ============================================================================
 */

#include "types.h"
#include "bitboard.h"
#include <array>
#include <string>
#include <cstring>

// ============================================================================
// ZOBRIST KEYS
// ============================================================================

/// Zobrist keys for incremental hashing
struct ZobristKeys {
    std::array<std::array<Bitboard, 64>, PIECE_NB> pieces;  // piece + square
    std::array<Bitboard, 16> castling;                       // castling rights
    std::array<Bitboard, 64> enpassant;                      // en passant file
    Bitboard side;                                           // side to move

    void init() noexcept;
};

extern ZobristKeys Zobrist;

// ============================================================================
// BOARD STATE
// ============================================================================

/// Compact board state (64 bytes - fits in one cache line ideally)
struct alignas(CACHE_LINE_SIZE) BoardState {
    Bitboard byType[PIECE_TYPE_NB];  // Bitboards by piece type (0=empty, 1-6)
    Bitboard byColor[COLOR_NB];      // Bitboards by color
    Bitboard occupied;               // All occupied squares
    Bitboard checkers;               // Squares giving check
    Bitboard pinned;                 // Pinned pieces (aligned with king)
    int pieceCount[PIECE_NB];        // Piece counts
    Piece board[64];                 // Piece on each square (fast indexed)
    Square kingSquare[COLOR_NB];     // King positions
    Bitboard blockers[COLOR_NB];     // Pieces blocking king from checks
    Bitboard pinners[COLOR_NB];      // Pieces pinning enemy pieces
};

// ============================================================================
// GAME STATE
// ============================================================================

/// Key type for transposition table (Zobrist hash)
using Key = Bitboard;

/// Material key (for NNUE feature reuse)
using MaterialKey = Bitboard;

/// A single position state (for undo/history)
struct alignas(16) StateInfo {
    Bitboard checkers;          // Checkers bitboard
    Bitboard pinned;            // Pinned pieces  
    Bitboard blockers[COLOR_NB]; // Blockers for pin detection
    Bitboard pinners[COLOR_NB];  // Pinners for pin detection
    Square epSquare;            // En passant square
    CastlingRights castlingRights; // Castle permissions
    int rule50;                 // Half-move clock for 50-move rule
    int gamePly;               // Game ply
    Key key;                    // Zobrist key
    Key materialKey;            // Material key
    Move lastMove;              // Last move played
    Piece captured;             // Captured piece
    NNUEAccumulator accumulator; // NNUE accumulator state

    void clear() noexcept {
        std::memset(this, 0, sizeof(*this));
        epSquare = static_cast<Square>(64); // No ep
    }
};

// ============================================================================
// PIECE LIST (for NNUE feature updates)
// ============================================================================

/// Maximum number of pieces on the board
constexpr int MAX_PIECES_PER_SIDE = 16;

/// Piece list entry for fast iteration
struct PieceEntry {
    Piece pc;          // Piece type
    Square sq;         // Current square
};

/// Piece list (compact, for NNUE incremental updates)
struct PieceList {
    int count[PIECE_NB];                          // Count of each piece type
    std::array<PieceEntry, 32> list;              // List of all pieces
    int index[64];                                // Index in list for each square (-1 if empty)

    void init() noexcept {
        std::memset(count, 0, sizeof(count));
        std::memset(index, -1, sizeof(index));
    }

    void add_piece(Piece pc, Square sq) noexcept {
        int idx = count[pc]++;
        list[idx] = {pc, sq};
        index[sq] = idx;
    }

    void remove_piece(Square sq) noexcept {
        int idx = index[sq];
        if (idx < 0) return;
        Piece pc = list[idx].pc;
        int last = --count[pc];
        if (idx < last) {
            list[idx] = list[last];
            index[list[idx].sq] = idx;
        }
        index[sq] = -1;
    }

    void move_piece(Square from, Square to) noexcept {
        int idx = index[from];
        if (idx < 0) return;
        list[idx].sq = to;
        index[to] = idx;
        index[from] = -1;
    }

    template<typename F>
    void for_each(F&& f) const noexcept {
        for (int pc = W_PAWN; pc <= B_KING; ++pc) {
            for (int i = 0; i < count[pc]; ++i) {
                f(list[i]);
            }
        }
    }
};

// ============================================================================
// POSITION CLASS
// ============================================================================

/// Main position class with full chess rules
class Position {
public:
    Position() noexcept;
    ~Position() noexcept = default;

    // Copy/Move
    Position(const Position&) = default;
    Position& operator=(const Position&) = default;

    // ======================================================================
    // SETUP
    // ======================================================================

    /// Set position from FEN string
    bool set_from_fen(const std::string& fen) noexcept;

    /// Set starting position
    void set_startpos() noexcept;

    /// Clear the board
    void clear() noexcept;

    // ======================================================================
    // QUERIES
    // ======================================================================

    /// Get piece on square
    Piece piece_on(Square sq) const noexcept { return boardState.board[sq]; }

    /// Get bitboard for piece type
    Bitboard pieces(PieceType pt) const noexcept { return boardState.byType[pt]; }

    /// Get bitboard for color
    Bitboard pieces(Color c) const noexcept { return boardState.byColor[c]; }

    /// Get bitboard for piece type and color
    Bitboard pieces(Color c, PieceType pt) const noexcept {
        return boardState.byColor[c] & boardState.byType[pt];
    }

    /// Get all occupied squares
    Bitboard occupied_bb() const noexcept { return boardState.occupied; }

    /// Get empty squares
    Bitboard empty_bb() const noexcept { return ~boardState.occupied; }

    /// Current side to move
    Color side_to_move() const noexcept { return stm; }

    /// En passant square
    Square ep_square() const noexcept { return st->epSquare; }

    /// Castling rights
    CastlingRights castling_rights() const noexcept { return st->castlingRights; }

    /// Half-move clock
    int rule50() const noexcept { return st->rule50; }

    /// Game ply
    int game_ply() const noexcept { return st->gamePly; }

    /// Current hash key
    Key key() const noexcept { return st->key; }

    /// Material key
    MaterialKey material_key() const noexcept { return st->materialKey; }

    /// King square for color
    Square king_square(Color c) const noexcept { return boardState.kingSquare[c]; }

    /// Check if side to move is in check
    bool in_check() const noexcept { return any(st->checkers); }

    /// Check if side to move has checkers
    Bitboard checkers() const noexcept { return st->checkers; }

    /// Get pinned pieces for side
    Bitboard pinned_pieces(Color c) const noexcept {
        return st->blockers[c];
    }

    /// Get number of pieces of a given type
    int piece_count(Piece pc) const noexcept { return boardState.pieceCount[pc]; }

    /// Get piece list (for NNUE)
    const PieceList& piece_list() const noexcept { return pieceList; }

    /// Get current state info
    const StateInfo& state() const noexcept { return *st; }

    /// Get NNUE accumulator
    const NNUEAccumulator& accumulator() const noexcept { return st->accumulator; }

    /// Get material key for both sides
    Bitboard material_key_for(Color c) const noexcept;

    // ======================================================================
    // MOVE EXECUTION
    // ======================================================================

    /// Make a move on the board
    void do_move(Move m, StateInfo& newSt) noexcept;

    /// Undo a move
    void undo_move(Move m) noexcept;

    /// Check if a move is legal (doesn't leave king in check)
    bool legal(Move m) const noexcept;

    /// Check if a square is attacked by a color
    bool attacked_by(Color attacker, Square sq) const noexcept;

    /// Get attackers to a square
    Bitboard attackers_to(Square sq, Bitboard occupied) const noexcept;

    /// Check if the position is a draw
    bool is_draw() const noexcept;

    /// Check if the game is over
    int is_game_over() const noexcept;  // Returns winner or 0 for draw

    /// Get the material imbalance (for NNUE)
    int material_imbalance() const noexcept;

    // ======================================================================
    // FEN (Forsyth-Edwards Notation)
    // ======================================================================

    /// Get FEN string of current position
    std::string fen() const noexcept;

    /// Print board to string
    std::string to_string() const noexcept;

    // ======================================================================
    // INTERNAL
    // ======================================================================

    /// Move piece on board without state update
    void move_piece_on_board(Square from, Square to) noexcept;

    /// Update checkers/pinners/blockers
    void update_checkers() noexcept;

    /// Recalculate all state from board
    void recalc() noexcept;

private:
    // Core board state (cache-line aligned)
    BoardState boardState;

    // Side to move
    Color stm;

    // Current state info (points into history)
    StateInfo* st;

    // State stack (for undo)
    StateInfo stateStack[MAX_PLY];
    int stackPos;

    // Piece list for NNUE
    PieceList pieceList;

    // Hash key for null move (avoids stale TT entries)
    Key nullMoveKey;

    friend class MoveGen;
    class NNUEFeaturesFriend;
    friend class NNUEFeaturesFriend;
};

/// Check if a FEN string represents a valid position
bool is_valid_fen(const std::string& fen);

#endif // HYPERNOVA_BOARD_H