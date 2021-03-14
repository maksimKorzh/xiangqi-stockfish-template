/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2019 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

/// When compiling with provided Makefile (e.g. for Linux and OSX), configuration
/// is done automatically. To get started type 'make help'.
///
/// When Makefile is not used (e.g. with Microsoft Visual Studio) some switches
/// need to be set manually:
///
/// -DNDEBUG      | Disable debugging mode. Always use this for release.
///
/// -DNO_PREFETCH | Disable use of prefetch asm-instruction. You may need this to
///               | run on some very old machines.
///
/// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
///               | only in 64-bit mode and requires hardware with popcnt support.
///
/// -DUSE_PEXT    | Add runtime support for use of pext asm-instruction. Works
///               | only in 64-bit mode and requires hardware with pext support.

#include <cassert>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>

#if defined(_MSC_VER)
// Disable some silly and noisy warning from MSVC compiler
#pragma warning(disable: 4127) // Conditional expression is constant
#pragma warning(disable: 4146) // Unary minus operator applied to unsigned type
#pragma warning(disable: 4800) // Forcing value to bool 'true' or "false'
#endif

/// Predefined macros hell:
///
/// __GNUC__           Compiler is gcc, Clang or Intel on Linux
/// __INTEL_COMPILER   Compiler is Intel
/// _MSC_VER           Compiler is MSVC or Intel on Windows
/// _WIN32             Building on Windows (any)
/// _WIN64             Building on Windows 64 bit

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#  include <intrin.h> // Microsoft header for _BitScanForward64()
#  define IS_64BIT
#endif

#if defined(USE_POPCNT) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if !defined(NO_PREFETCH) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <xmmintrin.h> // Intel and Microsoft header for _mm_prefetch()
#endif

#if defined(USE_PEXT)
#  include <immintrin.h> // Header for _pext_u64() intrinsic
#  define pext(b, m) _pext_u64(b, m)
#else
#  define pext(b, m) 0
#endif

#ifdef USE_POPCNT
constexpr bool HasPopCnt = true;
#else
constexpr bool HasPopCnt = false;
#endif

#ifdef USE_PEXT
constexpr bool HasPext = true;
#else
constexpr bool HasPext = false;
#endif

#ifdef IS_64BIT
constexpr bool Is64Bit = true;
#else
constexpr bool Is64Bit = false;
#endif

typedef uint64_t Key;
typedef uint64_t Bitboard;

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

/// A move needs 16 bits to be stored
///
/// bit  0- 5: destination square (from 0 to 63)
/// bit  6-11: origin square (from 0 to 63)
/// bit 12-13: promotion piece type - 2 (from KNIGHT-2 to QUEEN-2)
/// bit 14-15: special move flag: promotion (1), en passant (2), castling (3)
/// NOTE: EN-PASSANT bit is set only when a pawn can be captured
///
/// Special cases are MOVE_NONE and MOVE_NULL. We can sneak these in because in
/// any normal move destination square is always different from origin square
/// while MOVE_NONE and MOVE_NULL have the same origin and destination square.

enum Move : int {
  MOVE_NONE,
  MOVE_NULL = 65
};

enum MoveType {
  NORMAL,
  PROMOTION = 1 << 14,
  ENPASSANT = 2 << 14,
  CASTLING  = 3 << 14
};

enum Color {
  WHITE, BLACK, COLOR_NB = 2
};

enum CastlingSide {
  KING_SIDE, QUEEN_SIDE, CASTLING_SIDE_NB = 2
};

enum CastlingRight {
  NO_CASTLING,
  WHITE_OO,
  WHITE_OOO = WHITE_OO << 1,
  BLACK_OO  = WHITE_OO << 2,
  BLACK_OOO = WHITE_OO << 3,

  WHITE_CASTLING = WHITE_OO | WHITE_OOO,
  BLACK_CASTLING = BLACK_OO | BLACK_OOO,
  ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

  CASTLING_RIGHT_NB = 16
};

enum Phase {
  PHASE_ENDGAME,
  PHASE_MIDGAME = 128,
  MG = 0, EG = 1, PHASE_NB = 2
};

enum ScaleFactor {
  SCALE_FACTOR_DRAW    = 0,
  SCALE_FACTOR_NORMAL  = 64,
  SCALE_FACTOR_MAX     = 128,
  SCALE_FACTOR_NONE    = 255
};

enum Bound {
  BOUND_NONE,
  BOUND_UPPER,
  BOUND_LOWER,
  BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

enum Value : int {
  VALUE_ZERO      = 0,
  VALUE_DRAW      = 0,
  VALUE_KNOWN_WIN = 10000,
  VALUE_MATE      = 32000,
  VALUE_INFINITE  = 32001,
  VALUE_NONE      = 32002,

  VALUE_MATE_IN_MAX_PLY  =  VALUE_MATE - 2 * MAX_PLY,
  VALUE_MATED_IN_MAX_PLY = -VALUE_MATE + 2 * MAX_PLY,

  PawnValueMg   = 128,   PawnValueEg   = 213,
  KnightValueMg = 782,   KnightValueEg = 865,
  BishopValueMg = 830,   BishopValueEg = 918,
  RookValueMg   = 1289,  RookValueEg   = 1378,
  QueenValueMg  = 2529,  QueenValueEg  = 2687,

  MidgameLimit  = 15258, EndgameLimit  = 3915
};

enum PieceType {
  NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
  ALL_PIECES = 0,
  PIECE_TYPE_NB = 8
};

enum Piece {
  NO_PIECE,
  W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
  B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  PIECE_NB = 16
};

extern Value PieceValue[PHASE_NB][PIECE_NB];

enum Depth : int {

  ONE_PLY = 1,

  DEPTH_ZERO          =  0 * ONE_PLY,
  DEPTH_QS_CHECKS     =  0 * ONE_PLY,
  DEPTH_QS_NO_CHECKS  = -1 * ONE_PLY,
  DEPTH_QS_RECAPTURES = -5 * ONE_PLY,

  DEPTH_NONE   = -6 * ONE_PLY,
  DEPTH_OFFSET = DEPTH_NONE,
  DEPTH_MAX    = MAX_PLY * ONE_PLY
};

static_assert(!(ONE_PLY & (ONE_PLY - 1)), "ONE_PLY is not a power of 2");

enum Square : int {
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
  SQ_NONE,

  SQUARE_NB = 64
};

enum Direction : int {
  NORTH =  8,
  EAST  =  1,
  SOUTH = -NORTH,
  WEST  = -EAST,

  NORTH_EAST = NORTH + EAST,
  SOUTH_EAST = SOUTH + EAST,
  SOUTH_WEST = SOUTH + WEST,
  NORTH_WEST = NORTH + WEST
};

enum File : int {
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB
};

enum Rank : int {
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB
};


/// Score enum stores a middlegame and an endgame value in a single integer (enum).
/// The least significant 16 bits are used to store the middlegame value and the
/// upper 16 bits are used to store the endgame value. We have to take care to
/// avoid left-shifting a signed int to avoid undefined behavior.
enum Score : int { SCORE_ZERO };

constexpr Score make_score(int mg, int eg) {
  return Score((int)((unsigned int)eg << 16) + mg);
}

/// Extracting the signed lower and upper 16 bits is not so trivial because
/// according to the standard a simple cast to short is implementation defined
/// and so is a right shift of a signed integer.
inline Value eg_value(Score s) {
  union { uint16_t u; int16_t s; } eg = { uint16_t(unsigned(s + 0x8000) >> 16) };
  return Value(eg.s);
}

inline Value mg_value(Score s) {
  union { uint16_t u; int16_t s; } mg = { uint16_t(unsigned(s)) };
  return Value(mg.s);
}

#define ENABLE_BASE_OPERATORS_ON(T)                                \
constexpr T operator+(T d1, T d2) { return T(int(d1) + int(d2)); } \
constexpr T operator-(T d1, T d2) { return T(int(d1) - int(d2)); } \
constexpr T operator-(T d) { return T(-int(d)); }                  \
inline T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }         \
inline T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }

#define ENABLE_INCR_OPERATORS_ON(T)                                \
inline T& operator++(T& d) { return d = T(int(d) + 1); }           \
inline T& operator--(T& d) { return d = T(int(d) - 1); }

#define ENABLE_FULL_OPERATORS_ON(T)                                \
ENABLE_BASE_OPERATORS_ON(T)                                        \
constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
constexpr int operator/(T d1, T d2) { return int(d1) / int(d2); }  \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); }    \
inline T& operator/=(T& d, int i) { return d = T(int(d) / i); }

ENABLE_FULL_OPERATORS_ON(Value)
ENABLE_FULL_OPERATORS_ON(Depth)
ENABLE_FULL_OPERATORS_ON(Direction)

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(Color)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

ENABLE_BASE_OPERATORS_ON(Score)

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_INCR_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

/// Additional operators to add integers to a Value
constexpr Value operator+(Value v, int i) { return Value(int(v) + i); }
constexpr Value operator-(Value v, int i) { return Value(int(v) - i); }
inline Value& operator+=(Value& v, int i) { return v = v + i; }
inline Value& operator-=(Value& v, int i) { return v = v - i; }

/// Additional operators to add a Direction to a Square
constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

/// Only declared but not defined. We don't want to multiply two scores due to
/// a very high risk of overflow. So user should explicitly convert to integer.
Score operator*(Score, Score) = delete;

/// Division of a Score must be handled separately for each term
inline Score operator/(Score s, int i) {
  return make_score(mg_value(s) / i, eg_value(s) / i);
}

/// Multiplication of a Score by an integer. We check for overflow in debug mode.
inline Score operator*(Score s, int i) {

  Score result = Score(int(s) * i);

  assert(eg_value(result) == (i * eg_value(s)));
  assert(mg_value(result) == (i * mg_value(s)));
  assert((i == 0) || (result / i) == s);

  return result;
}

constexpr Color operator~(Color c) {
  return Color(c ^ BLACK); // Toggle color
}

constexpr Square operator~(Square s) {
  return Square(s ^ SQ_A8); // Vertical flip SQ_A1 -> SQ_A8
}

constexpr File operator~(File f) {
  return File(f ^ FILE_H); // Horizontal flip FILE_A -> FILE_H
}

constexpr Piece operator~(Piece pc) {
  return Piece(pc ^ 8); // Swap color of piece B_KNIGHT -> W_KNIGHT
}

constexpr CastlingRight operator|(Color c, CastlingSide s) {
  return CastlingRight(WHITE_OO << ((s == QUEEN_SIDE) + 2 * c));
}

constexpr Value mate_in(int ply) {
  return VALUE_MATE - ply;
}

constexpr Value mated_in(int ply) {
  return -VALUE_MATE + ply;
}

constexpr Square make_square(File f, Rank r) {
  return Square((r << 3) + f);
}

constexpr Piece make_piece(Color c, PieceType pt) {
  return Piece((c << 3) + pt);
}

constexpr PieceType type_of(Piece pc) {
  return PieceType(pc & 7);
}

inline Color color_of(Piece pc) {
  assert(pc != NO_PIECE);
  return Color(pc >> 3);
}

constexpr bool is_ok(Square s) {
  return s >= SQ_A1 && s <= SQ_H8;
}

constexpr File file_of(Square s) {
  return File(s & 7);
}

constexpr Rank rank_of(Square s) {
  return Rank(s >> 3);
}

constexpr Square relative_square(Color c, Square s) {
  return Square(s ^ (c * 56));
}

constexpr Rank relative_rank(Color c, Rank r) {
  return Rank(r ^ (c * 7));
}

constexpr Rank relative_rank(Color c, Square s) {
  return relative_rank(c, rank_of(s));
}

constexpr Direction pawn_push(Color c) {
  return c == WHITE ? NORTH : SOUTH;
}

constexpr Square from_sq(Move m) {
  return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
  return Square(m & 0x3F);
}

constexpr int from_to(Move m) {
 return m & 0xFFF;
}

constexpr MoveType type_of(Move m) {
  return MoveType(m & (3 << 14));
}

constexpr PieceType promotion_type(Move m) {
  return PieceType(((m >> 12) & 3) + KNIGHT);
}

constexpr Move make_move(Square from, Square to) {
  return Move((from << 6) + to);
}

template<MoveType T>
constexpr Move make(Square from, Square to, PieceType pt = KNIGHT) {
  return Move(T + ((pt - KNIGHT) << 12) + (from << 6) + to);
}

constexpr bool is_ok(Move m) {
  return from_sq(m) != to_sq(m); // Catch MOVE_NULL and MOVE_NONE
}


/*****************************\
 =============================
 
         XIANGQI TYPES
        
 =============================
\*****************************/

// starting position
#define XQ_START_FEN "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1";

// sides to move
enum XQColor: int { XQ_RED, XQ_BLACK, XQ_NO_COLOR };

// piece encoding
enum XQPiece: int {
  XQ_EMPTY,
  
  XQ_RED_PAWN,
  XQ_RED_ADVISOR,
  XQ_RED_BISHOP,
  XQ_RED_KNIGHT,
  XQ_RED_CANNON,
  XQ_RED_ROOK,
  XQ_RED_KING,
  
  XQ_BLACK_PAWN,
  XQ_BLACK_ADVISOR,
  XQ_BLACK_BISHOP,
  XQ_BLACK_KNIGHT,
  XQ_BLACK_CANNON,
  XQ_BLACK_ROOK,
  XQ_BLACK_KING,
  
  XQ_OFFBOARD
};

// piece types
enum XQPieceType: int {
  XQ_PAWN,
  XQ_ADVISOR,
  XQ_BISHOP,
  XQ_KNIGHT,
  XQ_CANNON,
  XQ_ROOK,
  XQ_KING
};

// map type to piece
const int XQ_PIECE_TYPE[] = {
  0, 
  XQ_PAWN, XQ_ADVISOR, XQ_BISHOP, XQ_KNIGHT, XQ_CANNON, XQ_ROOK, XQ_KING,
  XQ_PAWN, XQ_ADVISOR, XQ_BISHOP, XQ_KNIGHT, XQ_CANNON, XQ_ROOK, XQ_KING,
};

// map color to piece
const int XQ_PIECE_COLOR[] = {
  XQ_NO_COLOR,
  XQ_RED, XQ_RED, XQ_RED, XQ_RED, XQ_RED, XQ_RED, XQ_RED,
  XQ_BLACK, XQ_BLACK, XQ_BLACK, XQ_BLACK, XQ_BLACK, XQ_BLACK, XQ_BLACK
};

// square encoding
enum XQSquare: int {
  XQ_A9 = 23,  XQ_B9 = 24,  XQ_C9 = 25,  XQ_D9 = 26,  XQ_E9 = 27,  XQ_F9 = 28,  XQ_G9 = 29,  XQ_H9 = 30,  XQ_I9 = 31,
  XQ_A8 = 34,  XQ_B8 = 35,  XQ_C8 = 36,  XQ_D8 = 37,  XQ_E8 = 38,  XQ_F8 = 39,  XQ_G8 = 40,  XQ_H8 = 41,  XQ_I8 = 42,
  XQ_A7 = 45,  XQ_B7 = 46,  XQ_C7 = 47,  XQ_D7 = 48,  XQ_E7 = 49,  XQ_F7 = 50,  XQ_G7 = 51,  XQ_H7 = 52,  XQ_I7 = 53,
  XQ_A6 = 56,  XQ_B6 = 57,  XQ_C6 = 58,  XQ_D6 = 59,  XQ_E6 = 60,  XQ_F6 = 61,  XQ_G6 = 62,  XQ_H6 = 63,  XQ_I6 = 64,
  XQ_A5 = 67,  XQ_B5 = 68,  XQ_C5 = 69,  XQ_D5 = 70,  XQ_E5 = 71,  XQ_F5 = 72,  XQ_G5 = 73,  XQ_H5 = 74,  XQ_I5 = 75,
  XQ_A4 = 78,  XQ_B4 = 79,  XQ_C4 = 80,  XQ_D4 = 81,  XQ_E4 = 82,  XQ_F4 = 83,  XQ_G4 = 84,  XQ_H4 = 85,  XQ_I4 = 86,
  XQ_A3 = 89,  XQ_B3 = 90,  XQ_C3 = 91,  XQ_D3 = 92,  XQ_E3 = 93,  XQ_F3 = 94,  XQ_G3 = 95,  XQ_H3 = 96,  XQ_I3 = 97,
  XQ_A2 = 100, XQ_B2 = 101, XQ_C2 = 102, XQ_D2 = 103, XQ_E2 = 104, XQ_F2 = 105, XQ_G2 = 106, XQ_H2 = 107, XQ_I2 = 108,
  XQ_A1 = 111, XQ_B1 = 112, XQ_C1 = 113, XQ_D1 = 114, XQ_E1 = 115, XQ_F1 = 116, XQ_G1 = 117, XQ_H1 = 118, XQ_I1 = 119,
  XQ_A0 = 122, XQ_B0 = 123, XQ_C0 = 124, XQ_D0 = 125, XQ_E0 = 126, XQ_F0 = 127, XQ_G0 = 128, XQ_H0 = 129, XQ_I0 = 130
};

//
// array to convert board square indices to coordinates
/*const char *XQ_BOARD_SQUARES[] = {
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "a9", "b9", "c9", "d9", "e9", "f9", "g9", "h9", "i9", "xx", 
  "xx", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "i8", "xx", 
  "xx", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "i7", "xx", 
  "xx", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "i6", "xx", 
  "xx", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "i5", "xx", 
  "xx", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "i4", "xx", 
  "xx", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "i3", "xx", 
  "xx", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "i2", "xx", 
  "xx", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "i1", "xx", 
  "xx", "a0", "b0", "c0", "d0", "e0", "f0", "g0", "h0", "i0", "xx", 
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx"
};*/



#endif // #ifndef TYPES_H_INCLUDED
