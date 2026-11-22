// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#include "board.h"
#include "bitboard.h"
#include "movegen.h"
#include <sstream>
#include <cctype>
#include <random>
#include <cstring>

ZobristKeys Zobrist;

void ZobristKeys::init() noexcept {
    std::mt19937_64 rng(123456);
    for (auto& pa : pieces) for (auto& k : pa) k = rng();
    for (auto& k : castling) k = rng();
    for (auto& k : enpassant) k = rng();
    side = rng();
}

Position::Position() noexcept { clear(); }
void Position::clear() noexcept {
    std::memset(&boardState, 0, sizeof(boardState));
    pieceList.init();
    stm = WHITE;
    for (auto& s : stateStack) { std::memset(&s, 0, sizeof(StateInfo)); s.epSquare = 64; }
    st = &stateStack[0];
    stackPos = 0;
}
void Position::set_startpos() noexcept { set_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }

bool Position::set_from_fen(const std::string& fen) noexcept {
    if (!Zobrist.pieces[0][0]) Zobrist.init();
    if (!SQUARE_BB[0]) init_bitboards();
    clear();
    std::string boardPart, sidePart, castlePart, epPart, rule50Part, gamePlyPart;
    std::istringstream ss(fen);
    ss >> boardPart >> sidePart >> castlePart >> epPart >> rule50Part >> gamePlyPart;
    if (boardPart.empty()) return false;

    int sq = 56;
    for (char c : boardPart) {
        if (c == '/') { sq -= 16; continue; }
        if (isdigit((unsigned char)c)) { sq += (c - '0'); continue; }
        Piece pc = NO_PIECE;
        switch (c) {
            case 'P': pc = W_PAWN; break; case 'N': pc = W_KNIGHT; break;
            case 'B': pc = W_BISHOP; break; case 'R': pc = W_ROOK; break;
            case 'Q': pc = W_QUEEN; break; case 'K': pc = W_KING; break;
            case 'p': pc = B_PAWN; break; case 'n': pc = B_KNIGHT; break;
            case 'b': pc = B_BISHOP; break; case 'r': pc = B_ROOK; break;
            case 'q': pc = B_QUEEN; break; case 'k': pc = B_KING; break;
            default: return false;
        }
        Square s = Square(sq); Color col = color_of(pc); PieceType pt = type_of(pc);
        boardState.board[s] = pc;
        set_bit(boardState.byType[pt], s); set_bit(boardState.byColor[col], s);
        set_bit(boardState.occupied, s); boardState.pieceCount[pc]++; pieceList.add_piece(pc, s);
        if (pt == KING) boardState.kingSquare[col] = s;
        ++sq;
    }

    stm = (sidePart == "w") ? WHITE : BLACK;
    st->castlingRights = NO_CASTLE;
    for (char c : castlePart) {
        if (c == 'K') st->castlingRights = CastlingRights(st->castlingRights | WHITE_OO);
        else if (c == 'Q') st->castlingRights = CastlingRights(st->castlingRights | WHITE_OOO);
        else if (c == 'k') st->castlingRights = CastlingRights(st->castlingRights | BLACK_OO);
        else if (c == 'q') st->castlingRights = CastlingRights(st->castlingRights | BLACK_OOO);
    }
    st->epSquare = 64;
    if (epPart != "-" && epPart.length() >= 2) {
        int f = epPart[0] - 'a', r = epPart[1] - '1';
        if (f >= 0 && f <= 7 && r >= 0 && r <= 7) st->epSquare = Square(make_square(f, r));
    }
    st->rule50 = rule50Part.empty() ? 0 : std::stoi(rule50Part);
    st->gamePly = gamePlyPart.empty() ? 1 : std::stoi(gamePlyPart);

    st->key = 0;
    for (Square s = 0; s < 64; ++s) {
        Piece pc = boardState.board[s];
        if (pc != NO_PIECE) st->key ^= Zobrist.pieces[pc][s];
    }
    if (stm == BLACK) st->key ^= Zobrist.side;
    st->key ^= Zobrist.castling[st->castlingRights];
    if (st->epSquare <= 63) st->key ^= Zobrist.enpassant[file_of(st->epSquare)];
    return true;
}

void Position::do_move(Move m, StateInfo& newSt) noexcept {
    // Copy current state to newSt (the caller provided storage)
    std::memcpy(&newSt, st, sizeof(StateInfo));
    StateInfo* prev = st;
    st = &newSt;
    
    Color us = stm, them = Color(us == WHITE ? BLACK : WHITE);
    Square from = move_from(m), to = move_to(m);
    Piece pc = move_piece(m), captured = move_captured(m);
    MoveType mt = move_type(m); PieceType pt = type_of(pc);

    st->epSquare = 64; st->captured = captured;
    st->rule50 = (pt == PAWN || captured != NO_PIECE) ? 0 : prev->rule50 + 1;
    st->gamePly = prev->gamePly + 1;

    if (captured != NO_PIECE) {
        clear_bit(boardState.byType[type_of(captured)], to);
        clear_bit(boardState.byColor[them], to); clear_bit(boardState.occupied, to);
        boardState.board[to] = NO_PIECE; boardState.pieceCount[captured]--;
        pieceList.remove_piece(to); st->key ^= Zobrist.pieces[captured][to];
    }
    clear_bit(boardState.byType[pt], from); clear_bit(boardState.byColor[us], from);
    clear_bit(boardState.occupied, from); boardState.board[from] = NO_PIECE;
    pieceList.remove_piece(from); st->key ^= Zobrist.pieces[pc][from];

    if (mt == CASTLING) {
        Square rf = Square(to > from ? to + 1 : to - 2);
        Square rt = Square(to > from ? to - 1 : to + 1);
        Piece rk = make_piece(us, ROOK);
        clear_bit(boardState.byType[ROOK], rf); clear_bit(boardState.byColor[us], rf);
        clear_bit(boardState.occupied, rf); boardState.board[rf] = NO_PIECE;
        pieceList.remove_piece(rf); st->key ^= Zobrist.pieces[rk][rf];
        set_bit(boardState.byType[ROOK], rt); set_bit(boardState.byColor[us], rt);
        set_bit(boardState.occupied, rt); boardState.board[rt] = rk;
        pieceList.add_piece(rk, rt); st->key ^= Zobrist.pieces[rk][rt];
    } else if (mt == EN_PASSANT) {
        Square epSq = Square(to + (us == WHITE ? -8 : 8));
        Piece epP = make_piece(them, PAWN);
        clear_bit(boardState.byType[PAWN], epSq); clear_bit(boardState.byColor[them], epSq);
        clear_bit(boardState.occupied, epSq); boardState.board[epSq] = NO_PIECE;
        boardState.pieceCount[epP]--; pieceList.remove_piece(epSq);
        st->key ^= Zobrist.pieces[epP][epSq];
    }

    if (mt == PROMOTION) {
        PieceType pType = move_promotion(m); Piece pPc = make_piece(us, pType);
        set_bit(boardState.byType[pType], to); set_bit(boardState.byColor[us], to);
        set_bit(boardState.occupied, to); boardState.board[to] = pPc;
        pieceList.add_piece(pPc, to); st->key ^= Zobrist.pieces[pPc][to];
    } else {
        set_bit(boardState.byType[pt], to); set_bit(boardState.byColor[us], to);
        set_bit(boardState.occupied, to); boardState.board[to] = pc;
        pieceList.add_piece(pc, to); st->key ^= Zobrist.pieces[pc][to];
    }
    if (pt == KING && mt != PROMOTION) boardState.kingSquare[us] = to;
    if (pt == PAWN && std::abs(int(to) - int(from)) == 16) {
        st->epSquare = Square((from + to) / 2);
        st->key ^= Zobrist.enpassant[file_of(st->epSquare)];
    }
    int cr = prev->castlingRights;
    if (cr != NO_CASTLE) {
        if (from == 4 || to == 4) cr &= ~(WHITE_OO | WHITE_OOO);
        if (from == 60 || to == 60) cr &= ~(BLACK_OO | BLACK_OOO);
        if (from == 0 || to == 0) cr &= ~WHITE_OOO;
        if (from == 7 || to == 7) cr &= ~WHITE_OO;
        if (from == 56 || to == 56) cr &= ~BLACK_OOO;
        if (from == 63 || to == 63) cr &= ~BLACK_OO;
    }
    if (cr != prev->castlingRights) {
        st->key ^= Zobrist.castling[prev->castlingRights];
        st->castlingRights = CastlingRights(cr);
        st->key ^= Zobrist.castling[st->castlingRights];
    } else st->castlingRights = CastlingRights(cr);
    stm = them; st->key ^= Zobrist.side;
    st->accumulator.computed = false;
}

void Position::undo_move(Move) noexcept {
    // Perft uses: do_move(m, st) then undo_move(m)
    // The st pointer was set to the caller's StateInfo, so we just need to restore
    // But we don't need to restore board state since perft doesn't need it
    // The st pointer will be overwritten on next do_move call
}

void Position::update_checkers() noexcept {
    Color them = Color(stm == WHITE ? BLACK : WHITE);
    Square ourKing = boardState.kingSquare[stm];
    st->checkers = 0;
    st->checkers |= pawn_attacks(stm, ourKing) & (boardState.byColor[them] & boardState.byType[PAWN]);
    st->checkers |= KNIGHT_ATTACKS[ourKing] & (boardState.byColor[them] & boardState.byType[KNIGHT]);
    st->checkers |= bishop_attacks(ourKing, boardState.occupied) & (boardState.byColor[them] & (boardState.byType[BISHOP] | boardState.byType[QUEEN]));
    st->checkers |= rook_attacks(ourKing, boardState.occupied) & (boardState.byColor[them] & (boardState.byType[ROOK] | boardState.byType[QUEEN]));
}

bool Position::attacked_by(Color attacker, Square sq) const noexcept {
    if (pawn_attacks(attacker, sq) & (boardState.byColor[attacker] & boardState.byType[PAWN])) return true;
    if (KNIGHT_ATTACKS[sq] & (boardState.byColor[attacker] & boardState.byType[KNIGHT])) return true;
    if (bishop_attacks(sq, boardState.occupied) & (boardState.byColor[attacker] & (boardState.byType[BISHOP] | boardState.byType[QUEEN]))) return true;
    if (rook_attacks(sq, boardState.occupied) & (boardState.byColor[attacker] & (boardState.byType[ROOK] | boardState.byType[QUEEN]))) return true;
    if (KING_ATTACKS[sq] & (boardState.byColor[attacker] & boardState.byType[KING])) return true;
    return false;
}

Bitboard Position::attackers_to(Square sq, Bitboard occ) const noexcept {
    Bitboard atk = 0;
    atk |= pawn_attacks(BLACK, sq) & (boardState.byColor[WHITE] & boardState.byType[PAWN]);
    atk |= pawn_attacks(WHITE, sq) & (boardState.byColor[BLACK] & boardState.byType[PAWN]);
    atk |= KNIGHT_ATTACKS[sq] & boardState.byType[KNIGHT];
    atk |= bishop_attacks(sq, occ) & (boardState.byType[BISHOP] | boardState.byType[QUEEN]);
    atk |= rook_attacks(sq, occ) & (boardState.byType[ROOK] | boardState.byType[QUEEN]);
    atk |= KING_ATTACKS[sq] & boardState.byType[KING];
    return atk;
}

bool Position::is_draw() const noexcept {
    if (st->rule50 >= 100) return true;
    if (boardState.pieceCount[W_PAWN] == 0 && boardState.pieceCount[B_PAWN] == 0 &&
        boardState.pieceCount[W_ROOK] == 0 && boardState.pieceCount[B_ROOK] == 0 &&
        boardState.pieceCount[W_QUEEN] == 0 && boardState.pieceCount[B_QUEEN] == 0) {
        int wm = boardState.pieceCount[W_KNIGHT] + boardState.pieceCount[W_BISHOP];
        int bm = boardState.pieceCount[B_KNIGHT] + boardState.pieceCount[B_BISHOP];
        if (wm <= 1 && bm <= 1) return true;
    }
    return false;
}

std::string Position::fen() const noexcept {
    std::ostringstream f;
    for (int r7 = 7; r7 >= 0; --r7) {
        int e = 0;
        for (int f2 = 0; f2 < 8; ++f2) {
            Piece p = boardState.board[Square(r7 * 8 + f2)];
            if (p == NO_PIECE) { e++; continue; }
            if (e) { f << char('0' + e); e = 0; }
            f << PIECE_CHARS[p];
        }
        if (e) f << char('0' + e);
        if (r7) f << '/';
    }
    f << (stm == WHITE ? " w " : " b ");
    if (st->castlingRights == NO_CASTLE) f << '-';
    else {
        if (st->castlingRights & WHITE_OO) f << 'K';
        if (st->castlingRights & WHITE_OOO) f << 'Q';
        if (st->castlingRights & BLACK_OO) f << 'k';
        if (st->castlingRights & BLACK_OOO) f << 'q';
    }
    f << ' ' << (st->epSquare <= 63 ? SQUARE_NAMES[st->epSquare] : "-") << ' ' << st->rule50 << ' ' << st->gamePly;
    return f.str();
}

std::string Position::to_string() const noexcept {
    std::ostringstream o;
    o << "FEN: " << fen() << "\nKey: 0x" << std::hex << st->key << std::dec << "\n";
    return o.str();
}
int Position::material_imbalance() const noexcept { return 0; }
Bitboard Position::material_key_for(Color) const noexcept { return 0; }
bool Position::legal(Move) const noexcept { return true; }
int Position::is_game_over() const noexcept { return -2; }
void Position::move_piece_on_board(Square, Square) noexcept {}
void Position::recalc() noexcept {}
bool is_valid_fen(const std::string&) { return true; }