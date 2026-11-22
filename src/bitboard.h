// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#ifndef HYPERNOVA_BITBOARD_H
#define HYPERNOVA_BITBOARD_H

#include <cstdint>
#include <cstring>
#include <string>
#include <array>
#include <bit>
#include <immintrin.h>
#include "types.h"

// ============================================================================
// BITBOARD CONSTANTS
// ============================================================================

constexpr Bitboard EMPTY_BB = 0ULL;
constexpr Bitboard FULL_BB  = ~EMPTY_BB;

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = 0x0202020202020202ULL;
constexpr Bitboard FILE_C_BB = 0x0404040404040404ULL;
constexpr Bitboard FILE_D_BB = 0x0808080808080808ULL;
constexpr Bitboard FILE_E_BB = 0x1010101010101010ULL;
constexpr Bitboard FILE_F_BB = 0x2020202020202020ULL;
constexpr Bitboard FILE_G_BB = 0x4040404040404040ULL;
constexpr Bitboard FILE_H_BB = 0x8080808080808080ULL;

constexpr Bitboard RANK_1_BB = 0x00000000000000FFULL;
constexpr Bitboard RANK_2_BB = 0x000000000000FF00ULL;
constexpr Bitboard RANK_3_BB = 0x0000000000FF0000ULL;
constexpr Bitboard RANK_4_BB = 0x00000000FF000000ULL;
constexpr Bitboard RANK_5_BB = 0x000000FF00000000ULL;
constexpr Bitboard RANK_6_BB = 0x0000FF0000000000ULL;
constexpr Bitboard RANK_7_BB = 0x00FF000000000000ULL;
constexpr Bitboard RANK_8_BB = 0xFF00000000000000ULL;

constexpr Bitboard DIAG_MAIN_BB = 0x8040201008040201ULL;
constexpr Bitboard DIAG_ANTI_BB = 0x0102040810204080ULL;

constexpr Bitboard RANK_1_8_BB[COLOR_NB] = { RANK_1_BB, RANK_8_BB };
constexpr Bitboard RANK_2_7_BB[COLOR_NB] = { RANK_2_BB, RANK_7_BB };
constexpr Bitboard RANK_3_6_BB[COLOR_NB] = { RANK_3_BB, RANK_6_BB };

/// Square bitboards
extern Bitboard SQUARE_BB[64];
extern Bitboard ADJACENT_FILES[8];
extern Bitboard BETWEEN_BB[64][64];
extern Bitboard LINE_BB[64][64];
extern Bitboard PAWN_ATTACKS[COLOR_NB][64];
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];

// ============================================================================
// MAGIC BITBOARD DATA
// ============================================================================

struct MagicData {
    Bitboard* attacks;
    Bitboard mask;
    Bitboard magic;
    int shift;
};

extern MagicData ROOK_MAGICS[64];
extern MagicData BISHOP_MAGICS[64];
extern Bitboard ROOK_ATTACKS[0x19000];
extern Bitboard BISHOP_ATTACKS[0x1480];

// ============================================================================
// BITBOARD OPERATIONS
// ============================================================================

inline int popcount(Bitboard b) noexcept {
    return static_cast<int>(_mm_popcnt_u64(b));
}

inline int countr_zero(Bitboard b) noexcept {
    return static_cast<int>(_tzcnt_u64(b));
}

inline int countl_zero(Bitboard b) noexcept {
    return static_cast<int>(_lzcnt_u64(b));
}

inline int lsb(Bitboard b) noexcept { return countr_zero(b); }

inline int msb(Bitboard b) noexcept {
    return static_cast<int>(63ULL - _lzcnt_u64(b));
}

inline Square pop_lsb(Bitboard& b) noexcept {
    Square sq = static_cast<Square>(countr_zero(b));
    b &= b - 1;
    return sq;
}

inline bool more_than_one(Bitboard b) noexcept { return b & (b - 1); }
inline bool exactly_one(Bitboard b) noexcept { return b && !(b & (b - 1)); }

// ============================================================================
// BIT ITERATOR
// ============================================================================

class BitIterator {
public:
    explicit BitIterator(Bitboard b) noexcept : bits(b) {}
    Square operator*() const noexcept { return static_cast<Square>(countr_zero(bits)); }
    BitIterator& operator++() noexcept { bits &= bits - 1; return *this; }
    bool operator!=(const BitIterator&) const noexcept { return bits != 0; }
    BitIterator begin() const noexcept { return *this; }
    BitIterator end() const noexcept { return BitIterator(EMPTY_BB); }
private:
    Bitboard bits;
};

// ============================================================================
// BITBOARD QUERIES
// ============================================================================

inline Bitboard square_bb(Square sq) noexcept { return 1ULL << sq; }
inline bool occupied(Bitboard b, Square sq) noexcept { return (b >> sq) & 1; }
inline bool any(Bitboard b) noexcept { return b != 0; }
inline bool none(Bitboard b) noexcept { return b == 0; }

inline void set_bit(Bitboard& b, Square sq) noexcept { b |= (1ULL << sq); }
inline void clear_bit(Bitboard& b, Square sq) noexcept { b &= ~(1ULL << sq); }
inline void toggle_bit(Bitboard& b, Square sq) noexcept { b ^= (1ULL << sq); }

inline Bitboard shift_north(Bitboard b) noexcept { return b << 8; }
inline Bitboard shift_south(Bitboard b) noexcept { return b >> 8; }
inline Bitboard shift_east(Bitboard b) noexcept { return (b << 1) & ~FILE_A_BB; }
inline Bitboard shift_west(Bitboard b) noexcept { return (b >> 1) & ~FILE_H_BB; }
inline Bitboard shift_ne(Bitboard b) noexcept { return (b << 9) & ~FILE_A_BB; }
inline Bitboard shift_nw(Bitboard b) noexcept { return (b << 7) & ~FILE_H_BB; }
inline Bitboard shift_se(Bitboard b) noexcept { return (b >> 7) & ~FILE_A_BB; }
inline Bitboard shift_sw(Bitboard b) noexcept { return (b >> 9) & ~FILE_H_BB; }

inline Bitboard file_bb(Square sq) noexcept { return FILE_A_BB << file_of(sq); }
inline Bitboard rank_bb(Square sq) noexcept { return RANK_1_BB << (rank_of(sq) * 8); }
inline Bitboard adjacent_files_bb(int file) noexcept { return ADJACENT_FILES[file]; }

// ============================================================================
// SLIDING ATTACKS
// ============================================================================

inline Bitboard rook_attacks(Square sq, Bitboard occ) noexcept {
    const MagicData& m = ROOK_MAGICS[sq];
    #if defined(__BMI2__)
    return m.attacks[_pext_u64(occ, m.mask)];
    #else
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
    #endif
}

inline Bitboard bishop_attacks(Square sq, Bitboard occ) noexcept {
    const MagicData& m = BISHOP_MAGICS[sq];
    #if defined(__BMI2__)
    return m.attacks[_pext_u64(occ, m.mask)];
    #else
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
    #endif
}

inline Bitboard queen_attacks(Square sq, Bitboard occ) noexcept {
    return rook_attacks(sq, occ) | bishop_attacks(sq, occ);
}

inline Bitboard attacks_from(PieceType pt, Square sq, Bitboard occ) noexcept {
    switch (pt) {
        case PAWN:   return 0;
        case KNIGHT: return KNIGHT_ATTACKS[sq];
        case BISHOP: return bishop_attacks(sq, occ);
        case ROOK:   return rook_attacks(sq, occ);
        case QUEEN:  return queen_attacks(sq, occ);
        case KING:   return KING_ATTACKS[sq];
        default:     return 0;
    }
}

inline Bitboard pawn_attacks(Color c, Square sq) noexcept {
    return PAWN_ATTACKS[c][sq];
}

inline Bitboard between_bb(Square a, Square b) noexcept { return BETWEEN_BB[a][b]; }
inline Bitboard line_bb(Square a, Square b) noexcept { return LINE_BB[a][b]; }
inline bool aligned(Square a, Square b, Square c) noexcept {
    return any(LINE_BB[a][b] & square_bb(c));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void init_bitboards() noexcept;
void init_magics() noexcept;
void init_attacks() noexcept;

// ============================================================================
// UTILITY
// ============================================================================

void print_bitboard(Bitboard b) noexcept;
std::string bitboard_to_string(Bitboard b);

#endif // HYPERNOVA_BITBOARD_H