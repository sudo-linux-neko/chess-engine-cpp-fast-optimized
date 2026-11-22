// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vaibhav Agarwal

#include "bitboard.h"
Bitboard SQUARE_BB[64];
Bitboard ADJACENT_FILES[8];
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[COLOR_NB][64];
Bitboard BETWEEN_BB[64][64];
Bitboard LINE_BB[64][64];
Bitboard ROOK_ATTACKS[0x19000];
Bitboard BISHOP_ATTACKS[0x1480];
MagicData ROOK_MAGICS[64];
MagicData BISHOP_MAGICS[64];

static Bitboard rook_raw(Square sq, Bitboard b) noexcept {
    Bitboard a = 0; int r = rank_of(sq), f = file_of(sq);
    for (int t = r+1; t<=7; ++t) { Square s=Square(make_square(f,t)); set_bit(a,s); if(b>>s&1)break; }
    for (int t = r-1; t>=0; --t) { Square s=Square(make_square(f,t)); set_bit(a,s); if(b>>s&1)break; }
    for (int t = f+1; t<=7; ++t) { Square s=Square(make_square(t,r)); set_bit(a,s); if(b>>s&1)break; }
    for (int t = f-1; t>=0; --t) { Square s=Square(make_square(t,r)); set_bit(a,s); if(b>>s&1)break; }
    return a;
}
static Bitboard bishop_raw(Square sq, Bitboard b) noexcept {
    Bitboard a=0; int r=rank_of(sq), f=file_of(sq);
    for(int t=1; r+t<=7&&f+t<=7; ++t){Square s=Square(make_square(f+t,r+t));set_bit(a,s);if(b>>s&1)break;}
    for(int t=1; r+t<=7&&f-t>=0; ++t){Square s=Square(make_square(f-t,r+t));set_bit(a,s);if(b>>s&1)break;}
    for(int t=1; r-t>=0&&f+t<=7; ++t){Square s=Square(make_square(f+t,r-t));set_bit(a,s);if(b>>s&1)break;}
    for(int t=1; r-t>=0&&f-t>=0; ++t){Square s=Square(make_square(f-t,r-t));set_bit(a,s);if(b>>s&1)break;}
    return a;
}

void init_attacks() noexcept {
    for(int i=0;i<64;++i) SQUARE_BB[i]=1ULL<<i;
    ADJACENT_FILES[0]=0x0202020202020202ULL; ADJACENT_FILES[1]=0x0505050505050505ULL;
    ADJACENT_FILES[2]=0x0A0A0A0A0A0A0A0AULL; ADJACENT_FILES[3]=0x1414141414141414ULL;
    ADJACENT_FILES[4]=0x2828282828282828ULL; ADJACENT_FILES[5]=0x5050505050505050ULL;
    ADJACENT_FILES[6]=0xA0A0A0A0A0A0A0A0ULL; ADJACENT_FILES[7]=0x4040404040404040ULL;

    for(Square sq=0;sq<64;++sq){
        int f=file_of(sq),r=rank_of(sq);
        Bitboard w=0,b=0,knight=0,king=0;
        if(r<7){if(f>0)set_bit(w,Square(sq+7));if(f<7)set_bit(w,Square(sq+9));}
        if(r>0){if(f>0)set_bit(b,Square(sq-9));if(f<7)set_bit(b,Square(sq-7));}
        PAWN_ATTACKS[0][sq]=w; PAWN_ATTACKS[1][sq]=b;
        auto add=[&](int df,int dr){int nf=f+df,nr=r+dr;if(nf>=0&&nf<=7&&nr>=0&&nr<=7)set_bit(knight,Square(make_square(nf,nr)));};
        add(1,2);add(-1,2);add(2,1);add(-2,1);add(2,-1);add(-2,-1);add(1,-2);add(-1,-2);
        KNIGHT_ATTACKS[sq]=knight;
        for(int df=-1;df<=1;++df)for(int dr=-1;dr<=1;++dr){if(!df&&!dr)continue;int nf=f+df,nr=r+dr;if(nf>=0&&nf<=7&&nr>=0&&nr<=7)set_bit(king,Square(make_square(nf,nr)));}
        KING_ATTACKS[sq]=king;
    }

    for(Square a=0;a<64;++a)for(Square b=0;b<64;++b){
        Bitboard occ=square_bb(a)|square_bb(b), between=0;
        if(rook_raw(a,occ)&square_bb(b)) between=rook_raw(a,occ)&rook_raw(b,occ);
        else if(bishop_raw(a,occ)&square_bb(b)) between=bishop_raw(a,occ)&bishop_raw(b,occ);
        BETWEEN_BB[a][b]=between; LINE_BB[a][b]=between|square_bb(a)|square_bb(b);
    }

    int ro=0, bo=0;
    for(Square sq=0;sq<64;++sq){
        Bitboard mask=0; int r=rank_of(sq),f=file_of(sq);
        for(int t=r+1;t<=6;++t)set_bit(mask,Square(make_square(f,t)));
        for(int t=r-1;t>=1;--t)set_bit(mask,Square(make_square(f,t)));
        for(int t=f+1;t<=6;++t)set_bit(mask,Square(make_square(t,r)));
        for(int t=f-1;t>=1;--t)set_bit(mask,Square(make_square(t,r)));
        int bits=popcount(mask),size=1<<bits;
        ROOK_MAGICS[sq].mask=mask; ROOK_MAGICS[sq].shift=64-bits; ROOK_MAGICS[sq].magic=0; ROOK_MAGICS[sq].attacks=&ROOK_ATTACKS[ro]; ro+=size;
        for(int i=0;i<size;++i){Bitboard occ=0,m=mask;int idx=i;while(m){Square s=pop_lsb(m);if(idx&1)occ|=square_bb(s);idx>>=1;}ROOK_MAGICS[sq].attacks[i]=rook_raw(sq,occ);}
    }
    for(Square sq=0;sq<64;++sq){
        Bitboard mask=0; int r=rank_of(sq),f=file_of(sq);
        for(int t=1;r+t<=6&&f+t<=6;++t)set_bit(mask,Square(make_square(f+t,r+t)));
        for(int t=1;r+t<=6&&f-t>=1;++t)set_bit(mask,Square(make_square(f-t,r+t)));
        for(int t=1;r-t>=1&&f+t<=6;++t)set_bit(mask,Square(make_square(f+t,r-t)));
        for(int t=1;r-t>=1&&f-t>=1;++t)set_bit(mask,Square(make_square(f-t,r-t)));
        int bits=popcount(mask),size=1<<bits;
        BISHOP_MAGICS[sq].mask=mask; BISHOP_MAGICS[sq].shift=64-bits; BISHOP_MAGICS[sq].magic=0; BISHOP_MAGICS[sq].attacks=&BISHOP_ATTACKS[bo]; bo+=size;
        for(int i=0;i<size;++i){Bitboard occ=0,m=mask;int idx=i;while(m){Square s=pop_lsb(m);if(idx&1)occ|=square_bb(s);idx>>=1;}BISHOP_MAGICS[sq].attacks[i]=bishop_raw(sq,occ);}
    }
}
void init_magics() noexcept {}
void init_bitboards() noexcept { init_attacks(); }