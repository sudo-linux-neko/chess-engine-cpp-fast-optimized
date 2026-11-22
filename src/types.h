// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#ifndef HYPERNOVA_TYPES_H
#define HYPERNOVA_TYPES_H

/**
 * ============================================================================
 * HYPERNOVA CHESS ENGINE - Core Type Definitions
 * ============================================================================
 *
 * This file defines all fundamental types used throughout the engine.
 * Every type is carefully chosen for maximum performance:
 *   - std::uint64_t for bitboards (fastest on x86-64)
 *   - std::int32_t for scores (exact integer arithmetic, no float overhead)
 *   - std::uint8_t/enums for compact storage (cache-friendly)
 *   - Strong alignment hints for SIMD operations
 *
 * Memory layout is critical for NNUE performance - we cache-line-align
 * all hot structures to prevent false sharing between threads.
 *
 * License: MIT
 * Author: HyperNova Team
 * ============================================================================
 */

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <limits>
#include <atomic>
#include <array>
#include <vector>
#include <bit>

// ============================================================================
// COMPILE-TIME CONFIGURATION
// ============================================================================

/// Default number of search/analysis threads (can override at runtime)
#ifndef DEFAULT_THREADS
#define DEFAULT_THREADS 2
#endif

/// Maximum supported threads
constexpr int MAX_THREADS = 256;

/// NNUE network architecture dimensions
/// These define the HalfKP feature set with 2x256x32x1 architecture
constexpr int NNUE_INPUT_SIZE      = 41024;  // 2 * (64*320 + 64*1) halfkp features
constexpr int NNUE_HIDDEN1_SIZE    = 512;    // First hidden layer (two halves of 256)
constexpr int NNUE_HIDDEN2_SIZE    = 32;     // Second hidden layer
constexpr int NNUE_OUTPUT_SIZE     = 1;      // Single scalar output (centipawns)

/// NNUE quantisation parameters
constexpr int NNUE_SCALE           = 400;    // Output scaling factor
constexpr int NNUE_QA              = 255;    // Quantisation bits for accumulator
constexpr int NNUE_QB              = 64;     // Quantisation bits for hidden layer

/// Memory alignment for SIMD operations
constexpr int CACHE_LINE_SIZE      = 64;
constexpr int PAGE_SIZE            = 4096;

/// Max ply (half-moves) in a game - 1000 is safe
constexpr int MAX_PLY              = 1000;

/// Max moves at any position
constexpr int MAX_MOVES            = 256;

/// Search depth limits
constexpr int MAX_SEARCH_DEPTH     = 128;
constexpr int MIN_SEARCH_DEPTH     = 1;

/// Time control limits (milliseconds)
constexpr int MIN_MOVE_TIME_MS     = 1;
constexpr int MAX_MOVE_TIME_MS     = 3600000; // 1 hour

// ============================================================================
// FUNDAMENTAL TYPES
// ============================================================================

/// Bitboard: 64-bit integer representing a chessboard rank/file
/// Uses uint64_t for efficient BMI instructions (pext, pdep, tzcnt, popcnt)
using Bitboard = std::uint64_t;

/// Square index (0-63, A1=0, B1=1, ..., H8=63)
using Square = std::uint8_t;

/// Piece type (NONE=0, PAWN=1, KNIGHT=2, ..., KING=6)
using PieceType_t = std::uint8_t;

/// Color (WHITE=0, BLACK=1)
using Color_t = std::uint8_t;

/// Evaluation score in centipawns
using Score = std::int32_t;

/// Value for minimax search (int32 for fast arithmetic)
using Value = std::int16_t;

/// Depth in plies
using Depth = std::int16_t;

/// Move index
using Move = std::uint32_t;

/// Neural network weight (16-bit quantized)
using Weight = std::int16_t;

/// Neural network bias (32-bit)
using Bias = std::int32_t;

// ============================================================================
// ENUMS - Compact and type-safe
// ============================================================================

/// Piece colors
enum Color : Color_t {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

/// Piece types (6 pieces + 1 empty)
enum PieceType : PieceType_t {
    NO_PIECE_TYPE = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6,
    PIECE_TYPE_NB = 7
};

/// Combined piece encoding (color * 6 + type)
enum Piece : std::uint8_t {
    NO_PIECE = 0,
    W_PAWN = 1, W_KNIGHT = 2, W_BISHOP = 3, W_ROOK = 4, W_QUEEN = 5, W_KING = 6,
    B_PAWN = 7, B_KNIGHT = 8, B_BISHOP = 9, B_ROOK = 10, B_QUEEN = 11, B_KING = 12,
    PIECE_NB = 13
};

/// Move types
enum MoveType : std::uint8_t {
    NORMAL = 0,
    PROMOTION = 1,
    EN_PASSANT = 2,
    CASTLING = 3
};

/// Castle rights (bit flags)
enum CastlingRights : std::uint8_t {
    NO_CASTLE = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    KING_SIDE = WHITE_OO | BLACK_OO,
    QUEEN_SIDE = WHITE_OOO | BLACK_OOO,
    ALL_CASTLE = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO,
    CASTLE_RIGHTS_NB = 16
};

/// Game phases for tapered evaluation
enum Phase : std::uint8_t {
    MIDGAME = 0,
    ENDGAME = 1,
    PHASE_NB = 2
};

/// Search result types
enum SearchBound : std::uint8_t {
    BOUND_NONE = 0,
    BOUND_UPPER = 1,  // Fail-low (beta cutoff)
    BOUND_LOWER = 2,  // Fail-high (alpha cutoff)
    BOUND_EXACT = 3   // Exact score
};

// ============================================================================
// CONSTANTS
// ============================================================================

/// Infinity values for search
constexpr Value VALUE_INFINITE  = 30000;
constexpr Value VALUE_MATE      = 29000;
constexpr Value VALUE_MATE_IN   = 28000;
constexpr Value VALUE_MATED_IN  = -28000;
constexpr Value VALUE_DRAW      = 0;
constexpr Value VALUE_NONE      = 32002;
constexpr Value VALUE_MAX       = 32767;

/// Piece values (used for material counting)
constexpr Score PIECE_VALUES[PIECE_TYPE_NB] = {
    0,      // NO_PIECE
    100,    // PAWN
    320,    // KNIGHT
    330,    // BISHOP
    500,    // ROOK
    900,    // QUEEN
    20000   // KING
};

/// Promotion piece types
constexpr PieceType PROMOTION_PIECES[4] = { KNIGHT, BISHOP, ROOK, QUEEN };

/// Square names
constexpr char SQUARE_NAMES[64][3] = {
    "a1","b1","c1","d1","e1","f1","g1","h1",
    "a2","b2","c2","d2","e2","f2","g2","h2",
    "a3","b3","c3","d3","e3","f3","g3","h3",
    "a4","b4","c4","d4","e4","f4","g4","h4",
    "a5","b5","c5","d5","e5","f5","g5","h5",
    "a6","b6","c6","d6","e6","f6","g6","h6",
    "a7","b7","c7","d7","e7","f7","g7","h7",
    "a8","b8","c8","d8","e8","f8","g8","h8"
};

/// Piece characters for FEN
constexpr char PIECE_CHARS[PIECE_NB] = {
    '.', 'P', 'N', 'B', 'R', 'Q', 'K', 'p', 'n', 'b', 'r', 'q', 'k'
};

/// File names
constexpr char FILES[8] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' };

/// Rank names
constexpr char RANKS[8] = { '1', '2', '3', '4', '5', '6', '7', '8' };

// ============================================================================
// SQUARE UTILITY FUNCTIONS (compile-time if possible)
// ============================================================================

/// Create a square from file and rank (0-indexed)
/// @param file File (0-7, a=0)
/// @param rank Rank (0-7, 1st=0)
/// @return Square index (0-63)
constexpr Square make_square(int file, int rank) noexcept {
    return static_cast<Square>(rank * 8 + file);
}

/// File of a square (0-7)
constexpr int file_of(Square sq) noexcept {
    return sq & 7;
}

/// Rank of a square (0-7)
constexpr int rank_of(Square sq) noexcept {
    return sq >> 3;
}

/// Distance between squares in Chebyshev metric
constexpr int square_distance(Square a, Square b) noexcept {
    return std::max(std::abs(file_of(a) - file_of(b)),
                    std::abs(rank_of(a) - rank_of(b)));
}

/// Manhattan distance between squares
constexpr int manhattan_distance(Square a, Square b) noexcept {
    return std::abs(file_of(a) - file_of(b)) +
           std::abs(rank_of(a) - rank_of(b));
}

/// Mirror square vertically (for NNUE feature symmetry)
constexpr Square mirror_square(Square sq) noexcept {
    return static_cast<Square>(sq ^ 56);  // XOR with 56 flips rank
}

/// Relative square based on color (for NNUE perspective)
constexpr Square relative_square(Color c, Square sq) noexcept {
    return static_cast<Square>(sq ^ (c * 56));
}

/// Make a piece from color and type
constexpr Piece make_piece(Color c, PieceType pt) noexcept {
    return static_cast<Piece>((c * 6) + pt);
}

/// Get color from piece
constexpr Color color_of(Piece p) noexcept {
    return static_cast<Color>((p > 6) ? BLACK : WHITE);
}

/// Get piece type from piece
constexpr PieceType type_of(Piece p) noexcept {
    return static_cast<PieceType>(p - (p > 6 ? 6 : 0));
}

/// Check if piece is a slider
constexpr bool is_slider(PieceType pt) noexcept {
    return pt == BISHOP || pt == ROOK || pt == QUEEN;
}

// ============================================================================
// MOVE PACKING (compact 32-bit encoding)
// ============================================================================
// Bit layout: 0000 0000 0000 0000 0000 0000 0011 1111 = from (6 bits)
//             0000 0000 0000 0000 0000 1111 1100 0000 = to (6 bits)
//             0000 0000 0000 0000 0111 0000 0000 0000 = move type (3 bits)
//             0000 0000 0000 0011 1000 0000 0000 0000 = promotion (3 bits)
//             0000 0000 1111 1100 0000 0000 0000 0000 = piece (4 bits)
//             0111 1111 0000 0000 0000 0000 0000 0000 = captured piece (4 bits)
// Remaining bits unused

constexpr int MOVE_FROM_SHIFT    = 0;
constexpr int MOVE_TO_SHIFT      = 6;
constexpr int MOVE_TYPE_SHIFT    = 12;
constexpr int MOVE_PROMO_SHIFT   = 15;
constexpr int MOVE_PIECE_SHIFT   = 18;
constexpr int MOVE_CAPTURE_SHIFT = 22;

/// Pack a move into a 32-bit integer
constexpr Move pack_move(Square from, Square to, MoveType type = NORMAL,
                         PieceType promotion = NO_PIECE_TYPE,
                         Piece piece = NO_PIECE,
                         Piece captured = NO_PIECE) noexcept {
    return (static_cast<Move>(from) << MOVE_FROM_SHIFT) |
           (static_cast<Move>(to) << MOVE_TO_SHIFT) |
           (static_cast<Move>(type) << MOVE_TYPE_SHIFT) |
           (static_cast<Move>(promotion) << MOVE_PROMO_SHIFT) |
           (static_cast<Move>(piece) << MOVE_PIECE_SHIFT) |
           (static_cast<Move>(captured) << MOVE_CAPTURE_SHIFT);
}

/// Extract 'from' square from a move
constexpr Square move_from(Move m) noexcept {
    return static_cast<Square>((m >> MOVE_FROM_SHIFT) & 0x3F);
}

/// Extract 'to' square from a move
constexpr Square move_to(Move m) noexcept {
    return static_cast<Square>((m >> MOVE_TO_SHIFT) & 0x3F);
}

/// Extract move type
constexpr MoveType move_type(Move m) noexcept {
    return static_cast<MoveType>((m >> MOVE_TYPE_SHIFT) & 0x7);
}

/// Extract promotion piece type
constexpr PieceType move_promotion(Move m) noexcept {
    return static_cast<PieceType>((m >> MOVE_PROMO_SHIFT) & 0x7);
}

/// Extract piece from move
constexpr Piece move_piece(Move m) noexcept {
    return static_cast<Piece>((m >> MOVE_PIECE_SHIFT) & 0xF);
}

/// Extract captured piece from move
constexpr Piece move_captured(Move m) noexcept {
    return static_cast<Piece>((m >> MOVE_CAPTURE_SHIFT) & 0xF);
}

/// Check if move is a capture
constexpr bool is_capture(Move m) noexcept {
    return move_captured(m) != NO_PIECE;
}

/// Check if move is a promotion
constexpr bool is_promotion(Move m) noexcept {
    return move_type(m) == PROMOTION;
}

/// Check if move is en passant
constexpr bool is_en_passant(Move m) noexcept {
    return move_type(m) == EN_PASSANT;
}

/// Check if move is castling
constexpr bool is_castling(Move m) noexcept {
    return move_type(m) == CASTLING;
}

/// Get the rank of the 2nd/7th rank for a color (for pawn advance)
constexpr int relative_rank(Color c, Square sq) noexcept {
    return rank_of(static_cast<Square>(sq ^ (c * 56)));
}

// ============================================================================
// ALIGNED ALLOCATOR for SIMD-friendly memory
// ============================================================================

/// Aligned allocator for STL containers (cache-line aligned)
template<typename T, std::size_t Alignment = CACHE_LINE_SIZE>
class AlignedAllocator {
public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    AlignedAllocator() noexcept = default;

    template<typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        void* ptr = nullptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept {
        std::free(p);
    }

    template<typename U>
    bool operator==(const AlignedAllocator<U, Alignment>&) const noexcept {
        return true;
    }

    template<typename U>
    bool operator!=(const AlignedAllocator<U, Alignment>&) const noexcept {
        return false;
    }
};

/// Vector with cache-line alignment (optimal for SIMD)
template<typename T, std::size_t Align = CACHE_LINE_SIZE>
using AlignedVector = std::vector<T, AlignedAllocator<T, Align>>;

// ============================================================================
// SSCAN (Split-Score-Storage) - Improved from SF NNUE ideas
// ============================================================================

/// Stores two separate king buckets for efficient incremental update
struct NNUEAccumulator {
    /// Aligned storage for accumulator values
    alignas(CACHE_LINE_SIZE) std::array<std::int16_t, NNUE_HIDDEN1_SIZE> accumulation[2]{};
    
    /// Whether the accumulator has been computed
    bool computed{false};

    /// Accumulator index for current side-to-move
    int perspective{0};
};

// ============================================================================
// SEARCH STATISTICS
// ============================================================================

/// Per-thread search data
struct SearchData {
    std::uint64_t nodes{0};         // Nodes searched
    std::uint64_t qnodes{0};        // Quiescence nodes searched
    std::uint64_t tb_hits{0};       // Tablebase hits
    std::uint64_t tt_hits{0};       // Transposition table hits
    std::uint64_t null_moves{0};    // Null moves attempted
    std::uint64_t reductions{0};    // Late move reductions
    Depth seldepth{0};              // Selective depth reached
};

/// Global search limits
struct Limits {
    std::uint64_t nodes{0};         // Node limit (0 = unlimited)
    std::uint64_t movetime{0};      // Move time in ms (0 = unlimited)
    int depth{MAX_SEARCH_DEPTH};    // Depth limit
    int movestogo{30};              // Moves to next time control
    int movetime_per_move{0};       // Time per move in ms
    bool infinite{false};           // Infinite analysis mode
    bool ponder{false};             // Pondering mode
};

// ============================================================================
// MOVE ORDERING (history tables)
// ============================================================================

/// Size of history table (based on piece + to-square)
constexpr int HISTORY_SIZE = PIECE_NB * 64;

/// Butterfly history for ordering captures
using ButterflyHistory = std::array<std::array<std::int16_t, 64>, PIECE_NB>;

/// Continuation history (two-level)
using ContinuationHistory = std::array<std::array<std::array<std::array<std::int16_t, 64>, PIECE_NB>, PIECE_NB>, 2>;

/// Capture history (piece-type to square)
using CaptureHistory = std::array<std::array<std::int16_t, 64>, PIECE_TYPE_NB>;

/// Correction history (for eval adjustments)
using CorrectionHistory = std::array<std::array<std::int16_t, 64>, 8>;

// ============================================================================
// STATIC ASSERTS FOR TYPE PROPERTIES
// ============================================================================

static_assert(sizeof(Bitboard) == 8,   "Bitboard must be 8 bytes");
static_assert(sizeof(Move) == 4,       "Move must be 4 bytes");
static_assert(sizeof(Value) == 2,      "Value must be 2 bytes");
static_assert(sizeof(Score) == 4,      "Score must be 4 bytes");
static_assert(sizeof(Piece) == 1,      "Piece must be 1 byte");
static_assert(sizeof(Square) == 1,     "Square must be 1 byte");
static_assert(sizeof(Color_t) == 1,    "Color must be 1 byte");
static_assert(sizeof(PieceType_t) == 1,"PieceType must be 1 byte");
static_assert(std::is_trivially_copyable_v<Move>, "Move must be trivially copyable");
static_assert(std::is_trivially_copyable_v<NNUEAccumulator>, "Accumulator must be trivially copyable");

#endif // HYPERNOVA_TYPES_H