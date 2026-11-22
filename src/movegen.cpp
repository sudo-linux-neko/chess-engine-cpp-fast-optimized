// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#include "movegen.h"
#include "board.h"
#include "bitboard.h"
#include <cassert>

// ============================================================================
// HYPERNOVA CHESS ENGINE - Move Generation
// ============================================================================
//
// Complete move generation using bitboard techniques.
// Generates all legal moves efficiently.
//
// ============================================================================

// ============================================================================
// MVV-LVA TABLE
// ============================================================================

const int MVV_LVA[PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    {},                              // NO_PIECE
    {0,  105, 205, 305, 405, 505, 0},  // PAWN victim
    {0,  104, 204, 304, 404, 504, 0},  // KNIGHT victim
    {0,  103, 203, 303, 403, 503, 0},  // BISHOP victim
    {0,  102, 202, 302, 402, 502, 0},  // ROOK victim
    {0,  101, 201, 301, 401, 501, 0},  // QUEEN victim
    {0,  100, 200, 300, 400, 500, 0}   // KING victim
};

// ============================================================================
// HELPER: Generate pseudo-legal moves for a position
// ============================================================================

static void generate_pawn_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Color them = static_cast<Color>(us == WHITE ? BLACK : WHITE);
    Bitboard ourPawns = pos.pieces(us, PAWN);
    Bitboard ourPieces = pos.pieces(us);
    Bitboard theirPieces = pos.pieces(them);
    Bitboard allPieces = pos.occupied_bb();
    Bitboard empty = ~allPieces;
    Bitboard epTarget = pos.ep_square() <= 63 ? square_bb(pos.ep_square()) : 0;

    int push = us == WHITE ? 8 : -8;
    int captureL = us == WHITE ? 7 : -9;
    int captureR = us == WHITE ? 9 : -7;
    int rank7 = us == WHITE ? 6 : 1;
    int rank2 = us == WHITE ? 1 : 6;

    // Single pushes
    if (!captures_only) {
        Bitboard singlePush = (us == WHITE ? shift_north(ourPawns) : shift_south(ourPawns)) & empty;
        while (singlePush) {
            Square to = pop_lsb(singlePush);
            Square from = static_cast<Square>(to - push);
            PieceType pt = PAWN;

            // Promotion
            if (rank_of(to) == (us == WHITE ? 7 : 0)) {
                for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN}) {
                    moves.add(pack_move(from, to, PROMOTION, promo, make_piece(us, PAWN)));
                }
            } else {
                moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN)));
            }
        }

        // Double pushes
        Bitboard pawnsOnRank2 = ourPawns & (us == WHITE ? RANK_2_BB : RANK_7_BB);
        Bitboard doublePush = (us == WHITE ? shift_north(shift_north(pawnsOnRank2)) : shift_south(shift_south(pawnsOnRank2))) & empty;
        // Also need the intermediate square to be empty
        Bitboard intermediate = (us == WHITE ? shift_north(pawnsOnRank2) : shift_south(pawnsOnRank2)) & empty;
        doublePush &= (us == WHITE ? shift_south(intermediate) : shift_north(intermediate));
        while (doublePush) {
            Square to = pop_lsb(doublePush);
            Square from = static_cast<Square>(to - 2 * push);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN)));
        }
    }

    // Captures
    Bitboard captL = (us == WHITE ? shift_nw(ourPawns) : shift_sw(ourPawns)) & (theirPieces | epTarget);
    while (captL) {
        Square to = pop_lsb(captL);
        Square from = static_cast<Square>(to - captureL);
        bool isEp = to == pos.ep_square();
        Piece captured = isEp ? make_piece(them, PAWN) : pos.piece_on(to);

        if (rank_of(to) == (us == WHITE ? 7 : 0)) {
            for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN}) {
                MoveType mt = isEp ? EN_PASSANT : PROMOTION;
                moves.add(pack_move(from, to, mt, promo, make_piece(us, PAWN), captured));
            }
        } else {
            MoveType mt = isEp ? EN_PASSANT : NORMAL;
            moves.add(pack_move(from, to, mt, NO_PIECE_TYPE, make_piece(us, PAWN), captured));
        }
    }

    Bitboard captR = (us == WHITE ? shift_ne(ourPawns) : shift_se(ourPawns)) & (theirPieces | epTarget);
    while (captR) {
        Square to = pop_lsb(captR);
        Square from = static_cast<Square>(to - captureR);
        bool isEp = to == pos.ep_square();
        Piece captured = isEp ? make_piece(them, PAWN) : pos.piece_on(to);

        if (rank_of(to) == (us == WHITE ? 7 : 0)) {
            for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN}) {
                MoveType mt = isEp ? EN_PASSANT : PROMOTION;
                moves.add(pack_move(from, to, mt, promo, make_piece(us, PAWN), captured));
            }
        } else {
            MoveType mt = isEp ? EN_PASSANT : NORMAL;
            moves.add(pack_move(from, to, mt, NO_PIECE_TYPE, make_piece(us, PAWN), captured));
        }
    }
}

static void generate_knight_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Bitboard ourKnights = pos.pieces(us, KNIGHT);
    Bitboard ourPieces = pos.pieces(us);
    Bitboard target = captures_only ? pos.pieces(static_cast<Color>(us ^ 1)) : ~ourPieces;

    while (ourKnights) {
        Square from = pop_lsb(ourKnights);
        Bitboard attacks = KNIGHT_ATTACKS[from] & target;
        while (attacks) {
            Square to = pop_lsb(attacks);
            Piece captured = pos.piece_on(to);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, KNIGHT), captured));
        }
    }
}

static void generate_bishop_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Bitboard ourBishops = pos.pieces(us, BISHOP);
    Bitboard ourPieces = pos.pieces(us);
    Bitboard target = captures_only ? pos.pieces(static_cast<Color>(us ^ 1)) : ~ourPieces;
    Bitboard occ = pos.occupied_bb();

    while (ourBishops) {
        Square from = pop_lsb(ourBishops);
        Bitboard attacks = bishop_attacks(from, occ) & target;
        while (attacks) {
            Square to = pop_lsb(attacks);
            Piece captured = pos.piece_on(to);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, BISHOP), captured));
        }
    }
}

static void generate_rook_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Bitboard ourRooks = pos.pieces(us, ROOK);
    Bitboard ourPieces = pos.pieces(us);
    Bitboard target = captures_only ? pos.pieces(static_cast<Color>(us ^ 1)) : ~ourPieces;
    Bitboard occ = pos.occupied_bb();

    while (ourRooks) {
        Square from = pop_lsb(ourRooks);
        Bitboard attacks = rook_attacks(from, occ) & target;
        while (attacks) {
            Square to = pop_lsb(attacks);
            Piece captured = pos.piece_on(to);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, ROOK), captured));
        }
    }
}

static void generate_queen_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Bitboard ourQueens = pos.pieces(us, QUEEN);
    Bitboard ourPieces = pos.pieces(us);
    Bitboard target = captures_only ? pos.pieces(static_cast<Color>(us ^ 1)) : ~ourPieces;
    Bitboard occ = pos.occupied_bb();

    while (ourQueens) {
        Square from = pop_lsb(ourQueens);
        Bitboard attacks = queen_attacks(from, occ) & target;
        while (attacks) {
            Square to = pop_lsb(attacks);
            Piece captured = pos.piece_on(to);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, QUEEN), captured));
        }
    }
}

static void generate_king_moves(const Position& pos, MoveList& moves, bool captures_only) noexcept {
    Color us = pos.side_to_move();
    Square from = pos.king_square(us);
    Bitboard target = captures_only ? pos.pieces(static_cast<Color>(us ^ 1)) : ~pos.pieces(us);
    Bitboard attacks = KING_ATTACKS[from] & target;

    while (attacks) {
        Square to = pop_lsb(attacks);
        Piece captured = pos.piece_on(to);
        moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, KING), captured));
    }

    // Castling
    if (!captures_only && !pos.in_check()) {
        const auto& st = pos.state();
        if (us == WHITE) {
            if (st.castlingRights & WHITE_OO) {
                // Check squares between king and rook are empty
                if (none(pos.occupied_bb() & 0x60ULL) && // f1, g1
                    !pos.attacked_by(BLACK, static_cast<Square>(4)) &&
                    !pos.attacked_by(BLACK, static_cast<Square>(5)) &&
                    !pos.attacked_by(BLACK, static_cast<Square>(6))) {
                    moves.add(pack_move(static_cast<Square>(4), static_cast<Square>(6), CASTLING, NO_PIECE_TYPE, make_piece(WHITE, KING)));
                }
            }
            if (st.castlingRights & WHITE_OOO) {
                if (none(pos.occupied_bb() & 0x0EULL) && // b1, c1, d1
                    !pos.attacked_by(BLACK, static_cast<Square>(2)) &&
                    !pos.attacked_by(BLACK, static_cast<Square>(3)) &&
                    !pos.attacked_by(BLACK, static_cast<Square>(4))) {
                    moves.add(pack_move(static_cast<Square>(4), static_cast<Square>(2), CASTLING, NO_PIECE_TYPE, make_piece(WHITE, KING)));
                }
            }
        } else {
            if (st.castlingRights & BLACK_OO) {
                if (none(pos.occupied_bb() & 0x6000000000000000ULL) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(60)) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(61)) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(62))) {
                    moves.add(pack_move(static_cast<Square>(60), static_cast<Square>(62), CASTLING, NO_PIECE_TYPE, make_piece(BLACK, KING)));
                }
            }
            if (st.castlingRights & BLACK_OOO) {
                if (none(pos.occupied_bb() & 0x0E00000000000000ULL) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(58)) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(59)) &&
                    !pos.attacked_by(WHITE, static_cast<Square>(60))) {
                    moves.add(pack_move(static_cast<Square>(60), static_cast<Square>(58), CASTLING, NO_PIECE_TYPE, make_piece(BLACK, KING)));
                }
            }
        }
    }
}

// ============================================================================
// MAIN GENERATION FUNCTIONS
// ============================================================================

void gen_legal_moves(const Position& pos, MoveList& moves) noexcept {
    moves.clear();

    if (pos.in_check()) {
        gen_evasions(pos, moves);
        return;
    }

    // Generate all pseudo-legal moves
    generate_pawn_moves(pos, moves, false);
    generate_knight_moves(pos, moves, false);
    generate_bishop_moves(pos, moves, false);
    generate_rook_moves(pos, moves, false);
    generate_queen_moves(pos, moves, false);
    generate_king_moves(pos, moves, false);
}

void gen_captures(const Position& pos, MoveList& moves) noexcept {
    moves.clear();

    if (pos.in_check()) {
        gen_evasions(pos, moves);
        return;
    }

    generate_pawn_moves(pos, moves, true);
    generate_knight_moves(pos, moves, true);
    generate_bishop_moves(pos, moves, true);
    generate_rook_moves(pos, moves, true);
    generate_queen_moves(pos, moves, true);
    generate_king_moves(pos, moves, true);
}

void gen_evasions(const Position& pos, MoveList& moves) noexcept {
    moves.clear();
    Color us = pos.side_to_move();
    Square kingSq = pos.king_square(us);
    Bitboard checkers = pos.checkers();
    Bitboard occ = pos.occupied_bb();

    // King moves
    Bitboard kingTarget = KING_ATTACKS[kingSq] & ~pos.pieces(us);
    while (kingTarget) {
        Square to = pop_lsb(kingTarget);
        if (!pos.attacked_by(static_cast<Color>(us ^ 1), to)) {
            Piece captured = pos.piece_on(to);
            moves.add(pack_move(kingSq, to, NORMAL, NO_PIECE_TYPE, make_piece(us, KING), captured));
        }
    }

    // Single check: can block or capture
    if (popcount(checkers) == 1) {
        Square checkerSq = static_cast<Square>(countr_zero(checkers));
        Bitboard target;

        // Capture the checker
        // Pawn captures that also capture checker
        Bitboard pawns = pos.pieces(us, PAWN);
        Bitboard pawnCaptures = (us == WHITE ? shift_ne(pawns) | shift_nw(pawns) : shift_se(pawns) | shift_sw(pawns)) & checkers;
        while (pawnCaptures) {
            Square to = pop_lsb(pawnCaptures);
            Square from = static_cast<Square>(to + (us == WHITE ? -8 : 8));
            if (file_of(from) == file_of(to) - (us == WHITE ? 1 : 1)) from = static_cast<Square>(to - (us == WHITE ? 7 : 9));
            else from = static_cast<Square>(to - (us == WHITE ? 9 : 7));
            Piece captured = pos.piece_on(to);
            if (rank_of(to) == (us == WHITE ? 7 : 0)) {
                for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                    moves.add(pack_move(from, to, PROMOTION, promo, make_piece(us, PAWN), captured));
            } else {
                moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN), captured));
            }
        }

        // Other piece captures
        target = pos.pieces(us) & ~pos.pieces(us, PAWN) & ~pos.pieces(us, KING);
        while (target) {
            Square from = pop_lsb(target);
            Piece pc = pos.piece_on(from);
            PieceType pt = type_of(pc);
            Bitboard attacks = attacks_from(pt, from, occ) & checkers;
            while (attacks) {
                Square to = pop_lsb(attacks);
                moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, pc, pos.piece_on(to)));
            }
        }

        // Blocks
        Bitboard between = between_bb(kingSq, checkerSq);
        if (any(between)) {
            // Pawns can push to block
            Bitboard pawnPushes = us == WHITE ? shift_north(pos.pieces(us, PAWN)) : shift_south(pos.pieces(us, PAWN));
            pawnPushes &= ~occ;
            Bitboard singlePushes = pawnPushes & between;
            while (singlePushes) {
                Square to = pop_lsb(singlePushes);
                Square from = static_cast<Square>(to - (us == WHITE ? 8 : -8));
                if (rank_of(to) == (us == WHITE ? 7 : 0)) {
                    for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                        moves.add(pack_move(from, to, PROMOTION, promo, make_piece(us, PAWN)));
                } else {
                    moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN)));
                }
            }

            // Double pushes that land on between
            Bitboard doublePush = pawnPushes & ~occ;
            doublePush = (us == WHITE ? shift_north(doublePush) : shift_south(doublePush)) & ~occ & between;
            while (doublePush) {
                Square to = pop_lsb(doublePush);
                Square from = static_cast<Square>(to - (us == WHITE ? 16 : -16));
                moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN)));
            }

            // Other pieces block
            Bitboard blockers = pos.pieces(us) & ~pos.pieces(us, PAWN) & ~pos.pieces(us, KING);
            while (blockers) {
                Square from = pop_lsb(blockers);
                Piece pc = pos.piece_on(from);
                PieceType pt = type_of(pc);
                Bitboard atk = attacks_from(pt, from, occ) & between;
                while (atk) {
                    Square to = pop_lsb(atk);
                    moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, pc));
                }
            }
        }
    }
}

void gen_checks(const Position& pos, MoveList& moves) noexcept {
    moves.clear();
    Color us = pos.side_to_move();
    Square ourKing = pos.king_square(us);
    Square theirKing = pos.king_square(static_cast<Color>(us ^ 1));
    Bitboard occ = pos.occupied_bb();
    Bitboard target = ~pos.pieces(us);

    // Pawns that give check
    Bitboard pawns = pos.pieces(us, PAWN);
    Bitboard pawnChecks = (us == WHITE ? shift_ne(pawns) | shift_nw(pawns) : shift_se(pawns) | shift_sw(pawns));
    pawnChecks &= pos.pieces(static_cast<Color>(us ^ 1));
    while (pawnChecks) {
        Square to = pop_lsb(pawnChecks);
        Square from = static_cast<Square>(to + (us == WHITE ? -8 : 8));
        // Determine actual from
        int diff = static_cast<int>(to) - static_cast<int>(from);
        if (diff == (us == WHITE ? 7 : -7)) from = static_cast<Square>(to - (us == WHITE ? 7 : -7));
        else from = static_cast<Square>(to - (us == WHITE ? 9 : -9));
        Piece captured = pos.piece_on(to);
        if (rank_of(to) == (us == WHITE ? 7 : 0)) {
            for (PieceType promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                moves.add(pack_move(from, to, PROMOTION, promo, make_piece(us, PAWN), captured));
        } else {
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN), captured));
        }
    }

    // Discovered checks via pawn push
    Bitboard pawnPushes = (us == WHITE ? shift_north(pawns) : shift_south(pawns)) & ~occ;
    while (pawnPushes) {
        Square to = pop_lsb(pawnPushes);
        Square from = static_cast<Square>(to - (us == WHITE ? 8 : -8));
        // Check if this unmasks a discovered check
        StateInfo st;
        Move m = pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, PAWN));
        Position tmp = pos;
        tmp.do_move(m, st);
        if (tmp.in_check()) {
            moves.add(m);
        }
    }

    // Knight checks
    Bitboard knights = pos.pieces(us, KNIGHT);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard atk = KNIGHT_ATTACKS[from] & pos.pieces(static_cast<Color>(us ^ 1));
        while (atk) {
            Square to = pop_lsb(atk);
            moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, make_piece(us, KNIGHT), pos.piece_on(to)));
        }
    }

    // Slider checks (bishop, rook, queen)
    Bitboard bishops = pos.pieces(us, BISHOP);
    Bitboard rooks = pos.pieces(us, ROOK);
    Bitboard queens = pos.pieces(us, QUEEN);

    Bitboard sliders = bishops | rooks | queens;
    while (sliders) {
        Square from = pop_lsb(sliders);
        Piece pc = pos.piece_on(from);
        PieceType pt = type_of(pc);
        Bitboard atk;

        if (pt == BISHOP) atk = bishop_attacks(from, occ);
        else if (pt == ROOK) atk = rook_attacks(from, occ);
        else atk = queen_attacks(from, occ);

        atk &= target;
        while (atk) {
            Square to = pop_lsb(atk);
            // Check if it gives check by checking if king is in the line
            Bitboard atkWithKing = (pt == BISHOP) ? bishop_attacks(from, occ & ~square_bb(to)) :
                                   (pt == ROOK) ? rook_attacks(from, occ & ~square_bb(to)) :
                                   queen_attacks(from, occ & ~square_bb(to));
            if (atkWithKing & square_bb(theirKing)) {
                moves.add(pack_move(from, to, NORMAL, NO_PIECE_TYPE, pc, pos.piece_on(to)));
            }
        }
    }
}

void gen_quiets(const Position& pos, MoveList& moves) noexcept {
    moves.clear();
    if (pos.in_check()) return;
    generate_pawn_moves(pos, moves, false);
    generate_knight_moves(pos, moves, false);
    generate_bishop_moves(pos, moves, false);
    generate_rook_moves(pos, moves, false);
    generate_queen_moves(pos, moves, false);
    generate_king_moves(pos, moves, false);
}

// ============================================================================
// PERFT
// ============================================================================

std::uint64_t perft(Position& pos, int depth) noexcept {
    if (depth == 0) return 1;

    MoveList moves;
    gen_legal_moves(pos, moves);

    if (depth == 1) return static_cast<std::uint64_t>(moves.size);

    std::uint64_t nodes = 0;
    for (int i = 0; i < moves.size; ++i) {
        StateInfo st;
        pos.do_move(moves[i].move, st);
        nodes += perft(pos, depth - 1);
        pos.undo_move(moves[i].move);
    }
    return nodes;
}

void divide(Position& pos, int depth) noexcept {
    MoveList moves;
    gen_legal_moves(pos, moves);
    std::uint64_t total = 0;

    for (int i = 0; i < moves.size; ++i) {
        StateInfo st;
        pos.do_move(moves[i].move, st);
        std::uint64_t count = perft(pos, depth - 1);
        pos.undo_move(moves[i].move);
        total += count;

        // Print move and count
        Square from = move_from(moves[i].move);
        Square to = move_to(moves[i].move);
        char promo = ' ';
        if (is_promotion(moves[i].move)) {
            promo = "nbrq"[move_promotion(moves[i].move) - 1];
        }
        printf("%s%s%c: %llu\n",
               SQUARE_NAMES[from], SQUARE_NAMES[to], promo,
               (unsigned long long)count);
    }
    printf("\nTotal: %llu\n", (unsigned long long)total);
}

// ============================================================================
// STATIC EXCHANGE EVALUATION
// ============================================================================

Value see(const Position& pos, Move m) noexcept {
    Square from = move_from(m);
    Square to = move_to(m);
    Piece captured = move_captured(m);
    Piece piece = move_piece(m);

    if (captured == NO_PIECE && !is_promotion(m)) return 0;

    std::array<Value, 32> values;
    int idx = 0;

    // Value of captured piece
    values[idx] = PIECE_VALUES[type_of(captured)] - PIECE_VALUES[type_of(piece)];
    if (is_promotion(m)) {
        values[idx] += PIECE_VALUES[move_promotion(m)] - PIECE_VALUES[PAWN];
    }
    Bitboard occ = pos.occupied_bb();
    // Remove from and to squares
    clear_bit(occ, from);
    if (captured != NO_PIECE) clear_bit(occ, to);
    else if (is_en_passant(m)) clear_bit(occ, static_cast<Square>(to - 8)); // Remove en passant victim

    Color us = color_of(piece);
    Color them = static_cast<Color>(us == WHITE ? BLACK : WHITE);
    Bitboard attackers = pos.attackers_to(to, occ);
    Square kingSq = pos.king_square(them);
    Square ourKingSq = pos.king_square(us);
    bool kingAttacker = false;

    while (true) {
        us = static_cast<Color>(us ^ 1);
        attackers &= occ;
        Bitboard ourAttackers = attackers & pos.pieces(us);

        // If we have no attackers, break
        if (none(ourAttackers)) break;

        // Find least valuable attacker
        int capturedType = 0;
        Square captureFrom = 0;
        for (PieceType pt = PAWN; pt <= KING; pt = static_cast<PieceType>(pt + 1)) {
            Bitboard att = ourAttackers & pos.pieces(us, pt);
            if (att) {
                captureFrom = static_cast<Square>(countr_zero(att));
                capturedType = pt;
                break;
            }
        }

        if (capturedType == KING) {
            // King can only recapture if it doesn't move into check
            if (pos.attackers_to(to, occ) & pos.pieces(static_cast<Color>(us ^ 1))) {
                break;
            }
        }

        // Remove the capturing piece and add its value
        values[++idx] = PIECE_VALUES[capturedType];
        clear_bit(occ, captureFrom);
        clear_bit(occ, to);
        attackers = pos.attackers_to(to, occ);

        // Swap the captured type
        captured = make_piece(us, static_cast<PieceType>(capturedType));
    }

    // Minimax the SEE values
    while (--idx >= 0) {
        if (values[idx] < -values[idx + 1])
            values[idx] = -values[idx + 1];
    }

    return values[0];
}

bool see_above(const Position& pos, Move m, Value threshold) noexcept {
    return see(pos, m) > threshold;
}

// ============================================================================
// MOVE PICKER METHODS
// ============================================================================

void MovePicker::init(const Position& p, Move tt, Depth d,
                       const ButterflyHistory* mh,
                       const ContinuationHistory* ch,
                       const CaptureHistory* caph,
                       Move countermove) noexcept {
    pos = &p;
    ttMove = tt;
    depth = d;
    mainHistory = mh;
    contHistory = ch;
    capHistory = caph;
    counter = countermove;
    curIdx = 0;
    curStage = STAGE_TT;
    skipQuiets = false;
    skipEarly = false;
}

void MovePicker::init_qsearch(const Position& p, Move tt) noexcept {
    pos = &p;
    ttMove = tt;
    curIdx = 0;
    curStage = STAGE_QS_CAPTURES;
}

void MovePicker::init_probcut(const Position& p, Move tt, Value threshold) noexcept {
    pos = &p;
    ttMove = tt;
    probcutThreshold = threshold;
    curIdx = 0;
    curStage = STAGE_PROBCUT;
}

Move MovePicker::next_move() noexcept {
    while (true) {
        // Try TT move first
        if (curStage == STAGE_TT) {
            curStage = STAGE_GOOD_CAPTURES;
            if (ttMove && ttMove != NONE_MOVE) {
                // Verify the move is legal
                MoveList test;
                gen_legal_moves(*pos, test);
                for (int i = 0; i < test.size; ++i) {
                    if (test[i].move == ttMove) {
                        lastScore = TT_MOVE_SCORE;
                        return ttMove;
                    }
                }
            }
        }

        // Try good captures next
        if (curStage == STAGE_GOOD_CAPTURES) {
            curMoves.clear();
            gen_captures(*pos, curMoves);
            // Filter by SEE and score
            int writeIdx = 0;
            for (int i = 0; i < curMoves.size; ++i) {
                Move m = curMoves[i].move;
                curMoves[i].score = score_capture(m);
            }
            // Sort captures by score
            // We'll just return them in MVV-LVA order
            curStage = STAGE_QUIET;
            if (curMoves.size > 0) {
                // Pick best capture
                int best = 0;
                for (int i = 1; i < curMoves.size; ++i) {
                    if (curMoves[i].score > curMoves[best].score) best = i;
                }
                Move bestMove = curMoves[best].move;
                curMoves[best] = curMoves[curMoves.size - 1];
                curMoves.size--;
                lastScore = TT_MOVE_SCORE - 1;
                if (ttMove != bestMove) return bestMove;
            }
        }

        // Try quiet moves (non-captures)
        if (curStage == STAGE_QUIET && !skipQuiets) {
            if (curMoves.size == 0) {
                gen_legal_moves(*pos, curMoves);
                // Remove TT move and captures
                int writeIdx = 0;
                for (int i = 0; i < curMoves.size; ++i) {
                    Move curMove = curMoves[i].move;
                    if (curMove == ttMove || is_capture(curMove)) continue;
                    curMoves[writeIdx++] = curMoves[i];
                }
                curMoves.size = writeIdx;
                // Score them
                for (int i = 0; i < curMoves.size; ++i) {
                    curMoves[i].score = score_quiet(curMoves[i].move);
                }
                // Sort by score descending
                curMoves.sort();
                curIdx = 0;
            }
            if (curIdx < curMoves.size) {
                lastScore = curMoves[curIdx].score;
                return curMoves[curIdx++].move;
            }
            curStage = STAGE_BAD_CAPTURES;
        }

        // Bad captures
        if (curStage == STAGE_BAD_CAPTURES) {
            curStage = STAGE_TT; // End
            break;
        }

        break;
    }

    return NONE_MOVE;
}

int MovePicker::score_capture(Move m) const noexcept {
    Piece captured = move_captured(m);
    Piece piece = move_piece(m);
    if (captured == NO_PIECE) return 0;

    int score = MVV_LVA[static_cast<int>(type_of(captured))][static_cast<int>(type_of(piece))];
    score += CAPTURE_BONUS_BASE;
    if (is_promotion(m)) score += PROMOTION_BONUS;

    // Apply SEE adjustment
    if (::see_above(*pos, m, 0)) score -= 4000;

    return score;
}

int MovePicker::score_quiet(Move m) const noexcept {
    int score = 0;

    // History heuristic
    if (mainHistory && capHistory) {
        Piece pc = move_piece(m);
        Square to = move_to(m);
        score += (*mainHistory)[pc][to];
    }

    if (m == counter) score += COUNTER_BONUS;

    return score;
}

void MovePicker::generate_stage() noexcept {
    // Handled inline in next_move
}

bool MovePicker::see(Move m, Value threshold) const noexcept {
    return see_above(*pos, m, threshold);
}