/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2021 The Stockfish developers (see AUTHORS file)

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

#include <algorithm>
#include <cassert>
#include <cstddef> // For offsetof()
#include <cstring> // For std::memset, std::memcmp
#include <iomanip>
#include <sstream>

#include "bitboard.h"
#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

using std::string;

namespace Stockfish {

namespace Zobrist {

  Key psq[PIECE_NB][SQUARE_NB];
  Key enpassant[FILE_NB];
  Key castling[CASTLING_RIGHT_NB];
  Key side, noPawns;
}

namespace {

// offboard map
const char *COORDINATES[] = {
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "a0", "b0", "c0", "d0", "e0", "f0", "g0", "h0", "i0", "xx",
  "xx", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "i1", "xx", 
  "xx", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "i2", "xx", 
  "xx", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "i3", "xx", 
  "xx", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "i4", "xx", 
  "xx", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "i5", "xx", 
  "xx", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "i6", "xx", 
  "xx", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "i7", "xx", 
  "xx", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "i8", "xx", 
  "xx", "a9", "b9", "c9", "d9", "e9", "f9", "g9", "h9", "i9", "xx",
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", 
  "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx"
};

const string PieceToChar(" PABNCRKpabncrkx");

constexpr Piece Pieces[] = {
  W_PAWN, W_ADVISOR, W_KNIGHT, W_BISHOP, W_ROOK, W_CANNON, W_KING,
  B_PAWN, B_ADVISOR, B_KNIGHT, B_BISHOP, B_ROOK, B_CANNON, B_KING, };
} // namespace


/// operator<<(Position) returns an ASCII representation of the position

std::ostream& operator<<(std::ostream& os, const Position& pos) {

  os << "\n +---+---+---+---+---+---+---+---+---+\n";

  for (Rank r = RANK_14; r >= RANK_1; --r)
  {
    for (File f = FILE_A; f <= FILE_K; ++f) {
      Square s = (Square)(r * 11 + f  );
      
      if (pos.piece_on(s) != OFFBOARD)
        os << " | " << PieceToChar[pos.piece_on(make_square(f, r))];
    }
    
    if (r > (Rank)1 && r < (Rank)12)
      os << " | " << (r - 2) << "\n +---+---+---+---+---+---+---+---+---+\n";
  }

  os << "   a   b   c   d   e   f   g   h   i\n";
  os << "\nSide to move: " << (pos.side_to_move() == WHITE ? "r" : "b");
  os << "\nHash key:     " << pos.hash_key();
  os << "\nKing squares: ";
  os << COORDINATES[pos.get_king_square(WHITE)] << " ";
  os << COORDINATES[pos.get_king_square(BLACK)];
  os << "\nRule 60:     " << pos.rule60_count() << "\n";

  return os;
}


// Marcel van Kervinck's cuckoo algorithm for fast detection of "upcoming repetition"
// situations. Description of the algorithm in the following paper:
// https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// First and second hash functions for indexing the cuckoo tables
inline int H1(Key h) { return h & 0x1fff; }
inline int H2(Key h) { return (h >> 16) & 0x1fff; }

// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
Key cuckoo[8192];
Move cuckooMove[8192];


/// Position::init() initializes at startup the various arrays used to compute hash keys

void Position::init() {

  PRNG rng(1070372);

  /*for (Piece pc : Pieces)
      for (Square s = SQ_A1; s <= SQ_H8; ++s)
          Zobrist::psq[pc][s] = rng.rand<Key>();

  for (File f = FILE_A; f <= FILE_H; ++f)
      Zobrist::enpassant[f] = rng.rand<Key>();

  for (int cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr)
      Zobrist::castling[cr] = rng.rand<Key>();

  Zobrist::side = rng.rand<Key>();
  Zobrist::noPawns = rng.rand<Key>();

  // Prepare the cuckoo tables
  std::memset(cuckoo, 0, sizeof(cuckoo));
  std::memset(cuckooMove, 0, sizeof(cuckooMove));
  int count = 0;
  for (Piece pc : Pieces)
      for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
          for (Square s2 = Square(s1 + 1); s2 <= SQ_H8; ++s2)
              if ((type_of(pc) != PAWN) && (attacks_bb(type_of(pc), s1, 0) & s2))
              {
                  Move move = make_move(s1, s2);
                  Key key = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                  int i = H1(key);
                  while (true)
                  {
                      std::swap(cuckoo[i], key);
                      std::swap(cuckooMove[i], move);
                      if (move == MOVE_NONE) // Arrived at empty slot?
                          break;
                      i = (i == H1(key)) ? H2(key) : H1(key); // Push victim to alternative slot
                  }
                  count++;
             }
  assert(count == 3668);*/
}


/// Position::set() initializes the position object with the given FEN string.
/// This function is not very robust - make sure that input FENs are correct,
/// this is assumed to be the responsibility of the GUI.

Position& Position::set(const string& fenStr, StateInfo* si) {
/*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") whilst Black uses lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. Following X-FEN standard, this is recorded only
      if there is a pawn in position to make an en passant capture, and if there really
      is a pawn that might have advanced two squares.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/
  
  // encode ascii pieces
  Piece CHAR_TO_PIECE[123] = {};
  CHAR_TO_PIECE['P'] = W_PAWN;
  CHAR_TO_PIECE['A'] = W_ADVISOR;
  CHAR_TO_PIECE['B'] = W_BISHOP;
  CHAR_TO_PIECE['E'] = W_BISHOP;
  CHAR_TO_PIECE['N'] = W_KNIGHT;
  CHAR_TO_PIECE['H'] = W_KNIGHT;
  CHAR_TO_PIECE['C'] = W_CANNON;
  CHAR_TO_PIECE['R'] = W_ROOK;
  CHAR_TO_PIECE['K'] = W_KING;
  CHAR_TO_PIECE['p'] = B_PAWN;
  CHAR_TO_PIECE['a'] = B_ADVISOR;
  CHAR_TO_PIECE['b'] = B_BISHOP;
  CHAR_TO_PIECE['e'] = B_BISHOP;
  CHAR_TO_PIECE['n'] = B_KNIGHT;
  CHAR_TO_PIECE['h'] = B_KNIGHT;
  CHAR_TO_PIECE['c'] = B_CANNON;
  CHAR_TO_PIECE['r'] = B_ROOK;
  CHAR_TO_PIECE['k'] = B_KING;

  unsigned char token;
  std::istringstream ss(fenStr);

  std::memset(this, 0, sizeof(Position));
  std::memset(si, 0, sizeof(StateInfo));
  st = si;

  ss >> std::noskipws;

  // reset board array
  for (Square s = SQ_A1; s < SQUARE_NB; ++s) {
    if (string(COORDINATES[s]).compare("xx") != 0) put_piece(NO_PIECE, s);
    else put_piece(OFFBOARD, s);
  }
  
  // reset king squares
  set_king_square(WHITE, SQ_NONE);
  set_king_square(BLACK, SQ_NONE);
  
  // reset plies
  searchPly = 0;
  gamePly = 0;
  
  // reset repetition table
  //for (let index in repetitionTable) repetitionTable[index] = 0;

  ss >> token;
  
  for (Rank r = RANK_14; r >= RANK_1; --r)
  {
    for (File f = FILE_A; f <= FILE_K; ++f) {
      Square s = make_square(f, r);

      if (piece_on(s) != OFFBOARD) {
        // 1. Piece placement
        if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z')) {
          if (token == 'K') set_king_square(WHITE, s);
          else if (token == 'k') set_king_square(BLACK, s);
          put_piece(CHAR_TO_PIECE[token], s);
          ss >> token;
        }
        
        // parse empty squares
        if (token >= '0' && token <= '9') {
          int offset = token - '0';
          if (piece_on(s) == NO_PIECE) --f;
          f = (File)(f + offset);
          ss >> token;
        }
        
        // parse end of rank
        if (token == '/') ss >> token;
      }
    }
  }

  // 2. Active color
  ss >> token;
  sideToMove = (token == 'b' ? BLACK : WHITE);
  ss >> token;

  // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
  // Shredder-FEN that uses the letters of the columns on which the rooks began
  // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
  // if an inner rook is associated with the castling right, the castling tag is
  // replaced by the file letter of the involved rook, as for the Shredder-FEN.
  while ((ss >> token) && !isspace(token))
  {
      Square rsq;
      Color c = islower(token) ? BLACK : WHITE;
      Piece rook = make_piece(c, ROOK);

      token = char(toupper(token));

      if (token == 'K')
          for (rsq = relative_square(c, SQ_H1); piece_on(rsq) != rook; --rsq) {}

      else if (token == 'Q')
          for (rsq = relative_square(c, SQ_A1); piece_on(rsq) != rook; ++rsq) {}

      else if (token >= 'A' && token <= 'H')
          rsq = make_square(File(token - 'A'), relative_rank(c, RANK_1));

      else
          continue;

      set_castling_right(c, rsq);
  }

  set_state(st);  
  
  // there's no enpassant in xiangqi
  st->epSquare = SQ_NONE;
  
  // 5-6. Halfmove clock and fullmove number
  ss >> std::skipws >> st->rule50 >> gamePly;

  // Convert from fullmove starting from 1 to gamePly starting from 0,
  // handle also common incorrect FEN with fullmove = 0.
  gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

  return *this;
}


/// Position::set_castling_right() is a helper function used to set castling
/// rights given the corresponding color and the rook starting square.

void Position::set_castling_right(Color c, Square rfrom) {

  Square kfrom = square<KING>(c);
  CastlingRights cr = c & (kfrom < rfrom ? KING_SIDE: QUEEN_SIDE);

  st->castlingRights |= cr;
  castlingRightsMask[kfrom] |= cr;
  castlingRightsMask[rfrom] |= cr;
  castlingRookSquare[cr] = rfrom;

  Square kto = relative_square(c, cr & KING_SIDE ? SQ_G1 : SQ_C1);
  Square rto = relative_square(c, cr & KING_SIDE ? SQ_F1 : SQ_D1);

  castlingPath[cr] =   (between_bb(rfrom, rto) | between_bb(kfrom, kto) | rto | kto)
                    & ~(kfrom | rfrom);
}


/// Position::set_check_info() sets king attacks to detect if a move gives check

void Position::set_check_info(StateInfo* si) const {

  si->blockersForKing[WHITE] = slider_blockers(pieces(BLACK), square<KING>(WHITE), si->pinners[BLACK]);
  si->blockersForKing[BLACK] = slider_blockers(pieces(WHITE), square<KING>(BLACK), si->pinners[WHITE]);

  Square ksq = square<KING>(~sideToMove);

  si->checkSquares[PAWN]   = pawn_attacks_bb(~sideToMove, ksq);
  si->checkSquares[KNIGHT] = attacks_bb<KNIGHT>(ksq);
  si->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
  si->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
  si->checkSquares[QUEEN]  = si->checkSquares[BISHOP] | si->checkSquares[ROOK];
  si->checkSquares[KING]   = 0;
}


/// Position::set_state() computes the hash keys of the position, and other
/// data that once computed is updated incrementally as moves are made.
/// The function is only used when a new position is set up, and to verify
/// the correctness of the StateInfo data when running in debug mode.

void Position::set_state(StateInfo* si) const {

  si->key = 0;
  si->rule50 = 0;
  
}


/// Position::set() is an overload to initialize the position object with
/// the given endgame code string like "KBPKN". It is mainly a helper to
/// get the material key out of an endgame code.

Position& Position::set(const string& code, Color c, StateInfo* si) {

  assert(code[0] == 'K');

  string sides[] = { code.substr(code.find('K', 1)),      // Weak
                     code.substr(0, std::min(code.find('v'), code.find('K', 1))) }; // Strong

  assert(sides[0].length() > 0 && sides[0].length() < 8);
  assert(sides[1].length() > 0 && sides[1].length() < 8);

  std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

  string fenStr = "8/" + sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/"
                       + sides[1] + char(8 - sides[1].length() + '0') + "/8 w - - 0 10";

  return set(fenStr, si); // changed to avoid errors, all this method would be dropped later
}


/// Position::fen() returns a FEN representation of the position. In case of
/// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.

string Position::fen() const {

  int emptyCnt;
  std::ostringstream ss;

  for (Rank r = RANK_8; r >= RANK_1; --r)
  {
      for (File f = FILE_A; f <= FILE_H; ++f)
      {
          for (emptyCnt = 0; f <= FILE_H && empty(make_square(f, r)); ++f)
              ++emptyCnt;

          if (emptyCnt)
              ss << emptyCnt;

          if (f <= FILE_H)
              ss << PieceToChar[piece_on(make_square(f, r))];
      }

      if (r > RANK_1)
          ss << '/';
  }

  ss << (sideToMove == WHITE ? " w " : " b ");

  if (can_castle(WHITE_OO))
      ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO ))) : 'K');

  if (can_castle(WHITE_OOO))
      ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

  if (can_castle(BLACK_OO))
      ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO ))) : 'k');

  if (can_castle(BLACK_OOO))
      ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

  if (!can_castle(ANY_CASTLING))
      ss << '-';

  ss << (ep_square() == SQ_NONE ? " - " : " " + UCI::square(ep_square()) + " ")
     << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

  return ss.str();
}


/// Position::slider_blockers() returns a bitboard of all the pieces (both colors)
/// that are blocking attacks on the square 's' from 'sliders'. A piece blocks a
/// slider if removing that piece from the board would result in a position where
/// square 's' is attacked. For example, a king-attack blocking piece can be either
/// a pinned or a discovered check piece, according if its color is the opposite
/// or the same of the color of the slider.

Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const {

  Bitboard blockers = 0;
  pinners = 0;

  // Snipers are sliders that attack 's' when a piece and other snipers are removed
  Bitboard snipers = (  (attacks_bb<  ROOK>(s) & pieces(QUEEN, ROOK))
                      | (attacks_bb<BISHOP>(s) & pieces(QUEEN, BISHOP))) & sliders;
  Bitboard occupancy = pieces() ^ snipers;

  while (snipers)
  {
    Square sniperSq = pop_lsb(&snipers);
    Bitboard b = between_bb(s, sniperSq) & occupancy;

    if (b && !more_than_one(b))
    {
        blockers |= b;
        if (b & pieces(color_of(piece_on(s))))
            pinners |= sniperSq;
    }
  }
  return blockers;
}


/// Position::attackers_to() computes a bitboard of all pieces which attack a
/// given square. Slider attacks use the occupied bitboard to indicate occupancy.

Bitboard Position::attackers_to(Square s, Bitboard occupied) const {

  return  (pawn_attacks_bb(BLACK, s)       & pieces(WHITE, PAWN))
        | (pawn_attacks_bb(WHITE, s)       & pieces(BLACK, PAWN))
        | (attacks_bb<KNIGHT>(s)           & pieces(KNIGHT))
        | (attacks_bb<  ROOK>(s, occupied) & pieces(  ROOK, QUEEN))
        | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP, QUEEN))
        | (attacks_bb<KING>(s)             & pieces(KING));
}


/// Position::legal() tests whether a pseudo-legal move is legal

bool Position::legal(Move m) const {

  assert(is_ok(m));

  Color us = sideToMove;
  Square from = from_sq(m);
  Square to = to_sq(m);

  assert(color_of(moved_piece(m)) == us);
  assert(piece_on(square<KING>(us)) == make_piece(us, KING));

  // st->previous->blockersForKing consider capsq as empty.
  // If pinned, it has to move along the king ray.
  if (type_of(m) == EN_PASSANT)
      return   !(st->previous->blockersForKing[sideToMove] & from)
            || aligned(from, to, square<KING>(us));

  // Castling moves generation does not check if the castling path is clear of
  // enemy attacks, it is delayed at a later time: now!
  if (type_of(m) == CASTLING)
  {
      // After castling, the rook and king final positions are the same in
      // Chess960 as they would be in standard chess.
      to = relative_square(us, to > from ? SQ_G1 : SQ_C1);
      Direction step = to > from ? WEST : EAST;

      for (Square s = to; s != from; s += step)
          if (attackers_to(s) & pieces(~us))
              return false;

      // In case of Chess960, verify if the Rook blocks some checks
      // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
      return !chess960 || !(blockers_for_king(us) & to_sq(m));
  }

  // If the moving piece is a king, check whether the destination square is
  // attacked by the opponent.
  if (type_of(piece_on(from)) == KING)
      return !(attackers_to(to) & pieces(~us));

  // A non-king move is legal if and only if it is not pinned or it
  // is moving along the ray towards or away from the king.
  return !(blockers_for_king(us) & from)
      || aligned(from, to, square<KING>(us));
}


/// Position::pseudo_legal() takes a random move and tests whether the move is
/// pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.

bool Position::pseudo_legal(const Move m) const {

  Color us = sideToMove;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);

  // Use a slower but simpler function for uncommon cases
  // yet we skip the legality check of MoveList<LEGAL>().
  if (type_of(m) != NORMAL)
      return checkers() ? MoveList<    EVASIONS>(*this).contains(m)
                        : MoveList<NON_EVASIONS>(*this).contains(m);

  // Is not a promotion, so promotion piece must be empty
  if (promotion_type(m) - KNIGHT != NO_PIECE_TYPE)
      return false;

  // If the 'from' square is not occupied by a piece belonging to the side to
  // move, the move is obviously not legal.
  if (pc == NO_PIECE || color_of(pc) != us)
      return false;

  // The destination square cannot be occupied by a friendly piece
  if (pieces(us) & to)
      return false;

  // Handle the special case of a pawn move
  if (type_of(pc) == PAWN)
  {
      // We have already handled promotion moves, so destination
      // cannot be on the 8th/1st rank.
      if ((Rank8BB | Rank1BB) & to)
          return false;

      if (   !(pawn_attacks_bb(us, from) & pieces(~us) & to) // Not a capture
          && !((from + pawn_push(us) == to) && empty(to))       // Not a single push
          && !(   (from + 2 * pawn_push(us) == to)              // Not a double push
               && (relative_rank(us, from) == RANK_2)
               && empty(to)
               && empty(to - pawn_push(us))))
          return false;
  }
  else if (!(attacks_bb(type_of(pc), from, pieces()) & to))
      return false;

  // Evasions generator already takes care to avoid some kind of illegal moves
  // and legal() relies on this. We therefore have to take care that the same
  // kind of moves are filtered out here.
  if (checkers())
  {
      if (type_of(pc) != KING)
      {
          // Double check? In this case a king move is required
          if (more_than_one(checkers()))
              return false;

          // Our move must be a blocking evasion or a capture of the checking piece
          if (!((between_bb(lsb(checkers()), square<KING>(us)) | checkers()) & to))
              return false;
      }
      // In case of king moves under check we have to remove king so as to catch
      // invalid moves like b1a1 when opposite queen is on c1.
      else if (attackers_to(to, pieces() ^ from) & pieces(~us))
          return false;
  }

  return true;
}


/// Position::gives_check() tests whether a pseudo-legal move gives a check

bool Position::gives_check(Move m) const {

  assert(is_ok(m));
  assert(color_of(moved_piece(m)) == sideToMove);

  Square from = from_sq(m);
  Square to = to_sq(m);

  // Is there a direct check?
  if (check_squares(type_of(piece_on(from))) & to)
      return true;

  // Is there a discovered check?
  if (   (blockers_for_king(~sideToMove) & from)
      && !aligned(from, to, square<KING>(~sideToMove)))
      return true;

  switch (type_of(m))
  {
  case NORMAL:
      return false;

  case PROMOTION:
      return attacks_bb(promotion_type(m), to, pieces() ^ from) & square<KING>(~sideToMove);

  // The double-pushed pawn blocked a check? En Passant will remove the blocker.
  // The only discovery check that wasn't handle is through capsq and fromsq
  // So the King must be in the same rank as fromsq to consider this possibility.
  // st->previous->blockersForKing consider capsq as empty.
  case EN_PASSANT:
      return st->previous->checkersBB
          || (   rank_of(square<KING>(~sideToMove)) == rank_of(from)
              && st->previous->blockersForKing[~sideToMove] & from);

  default: //CASTLING
  {
      // Castling is encoded as 'king captures the rook'
      Square ksq = square<KING>(~sideToMove);
      Square rto = relative_square(sideToMove, to > from ? SQ_F1 : SQ_D1);

      return   (attacks_bb<ROOK>(rto) & ksq)
            && (attacks_bb<ROOK>(rto, pieces() ^ from ^ to) & ksq);
  }
  }
}

// unused
void Position::do_move(Move move, StateInfo& newSt, bool givesCheck) {
  // avoid warnings
  if (move) {};
  st = &newSt;
  if (givesCheck) {};
}

// square attacked by the given side
bool Position::isSquareAttacked(Square s, Color c) {
  // by knights
  for (int direction = 0; direction < 4; direction++) {
    Square directionTarget = (Square)(s + DIAGONALS[direction]);
    
    if (board[directionTarget] == NO_PIECE) {
      for (int offset = 0; offset < 2; offset++) {
        Square knightTarget = (Square)(s + KNIGHT_ATTACK_OFFSETS[direction][offset]);
        if (board[knightTarget] == ((c == WHITE) ? W_KNIGHT : B_KNIGHT)) return true;
      }
    }
  }
  
  // by king (kings face each other), rooks & cannons
  for (int direction = 0; direction < 4; direction++) {
    Square directionTarget = (Square)(s + ORTHOGONALS[direction]);
    int jumpOver = 0;
    
    while (board[directionTarget] != OFFBOARD) {
      if (jumpOver == 0) {
        if (board[directionTarget] == ((c == WHITE) ? W_ROOK : B_ROOK) ||
          board[directionTarget] == ((c == WHITE) ? W_KING : B_KING))
          return 1;
      }

      if (board[directionTarget] != NO_PIECE) jumpOver++;
      if (jumpOver == 2 && board[directionTarget] == ((c == WHITE) ? W_CANNON : B_CANNON))
        return true;
      
      directionTarget = (Square)(directionTarget + ORTHOGONALS[direction]);
    }
  }

  // by pawns
  for (int direction = 0; direction < 2; direction++) {
    Square directionTarget = (Square)(s + PAWN_ATTACK_OFFSETS[c][direction]);
    if (board[directionTarget] == ((c == WHITE) ? W_PAWN : B_PAWN)) return true;
  }
  
  return false;
}


/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be pseudo legal.
 
bool Position::make_move(Move move, StateInfo& newSt, bool givesCheck) {
  // avoid warning
  if (givesCheck) {}

  // update plies
  ++searchPly;
  ++gamePly;
      
  // update repetition table
  //repetitionTable[gamePly] = hashKey;

  // push to stack
  st->hashKey = hashKey;
  st->rule60 = rule60;
  
  // copy current state info
  std::memcpy(&newSt, st, offsetof(StateInfo, key));
  newSt.previous = st;
  st = &newSt;
  
  // parse move
  Square sourceSquare = getSourceSquare(move);
  Square targetSquare = getTargetSquare(move);
  Piece sourcePiece = getSourcePiece(move);
  //Piece targetPiece = getTargetPiece(move);
  int captureFlag = getCaptureFlag(move);

  // move piece
  board[targetSquare] = sourcePiece;
  board[sourceSquare] = NO_PIECE;
  
  // hash piece
  //hashKey ^= pieceKeys[sourcePiece * board.length + sourceSquare];
  //hashKey ^= pieceKeys[sourcePiece * board.length + targetSquare];
  
  if (captureFlag) {
    rule60 = 0;
    //hashKey ^= pieceKeys[targetPiece * board.length + targetSquare];
  } else rule60++;

  // update king square (note: accessing data fields directly for performance reasons)
  if (board[targetSquare] == W_KING || board[targetSquare] == B_KING)
    kingSquare[sideToMove] = targetSquare;
  
  // switch side to move
  sideToMove = (Color)(sideToMove ^ BLACK);
  //hashKey ^= sideKey;
  
  if (isSquareAttacked(kingSquare[sideToMove ^ BLACK], sideToMove)) {
    undo_move(move);
    return false;
  } else return true;
}


/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move move) {
  // update plies
  --searchPly;
  --gamePly;

  // parse move   
  Square sourceSquare = getSourceSquare(move);
  Square targetSquare = getTargetSquare(move);
  Piece sourcePiece = getSourcePiece(move);
  Piece targetPiece = getTargetPiece(move);
  
  // move piece
  board[sourceSquare] = sourcePiece;
  board[targetSquare] = NO_PIECE;
  
  // restore captured piece
  if (getCaptureFlag(move)) put_piece(targetPiece, targetSquare);
  
  // update king square
  if (board[sourceSquare] == W_KING || board[sourceSquare] == B_KING)
    kingSquare[sideToMove ^ 1] = sourceSquare;

  // switch side to move
  sideToMove = (Color)(sideToMove ^ BLACK);
  
  // restore state variables
  rule60 = st->rule60;
  hashKey = st->hashKey;
  
  // Finally point our state pointer back to the previous state
  st = st->previous;
}

/// Position::do_castling() is a helper used to do/undo a castling move. This
/// is a bit tricky in Chess960 where from/to squares can overlap.
template<bool Do>
void Position::do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto) {

  bool kingSide = to > from;
  rfrom = to; // Castling is encoded as "king captures friendly rook"
  rto = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
  to = relative_square(us, kingSide ? SQ_G1 : SQ_C1);

  /*if (Do && Eval::useNNUE)
  {
      auto& dp = st->dirtyPiece;
      dp.piece[0] = make_piece(us, KING);
      dp.from[0] = from;
      dp.to[0] = to;
      dp.piece[1] = make_piece(us, ROOK);
      dp.from[1] = rfrom;
      dp.to[1] = rto;
      dp.dirty_num = 2;
  }*/

  // Remove both pieces first since squares could overlap in Chess960
  remove_piece(Do ? from : to);
  remove_piece(Do ? rfrom : rto);
  board[Do ? from : to] = board[Do ? rfrom : rto] = NO_PIECE; // Since remove_piece doesn't do this for us
  put_piece(make_piece(us, KING), Do ? to : from);
  put_piece(make_piece(us, ROOK), Do ? rto : rfrom);
}


/// Position::do(undo)_null_move() is used to do(undo) a "null move": it flips
/// the side to move without executing any move on the board.

void Position::do_null_move(StateInfo& newSt) {

  assert(!checkers());
  assert(&newSt != st);

  std::memcpy(&newSt, st, offsetof(StateInfo, accumulator));

  newSt.previous = st;
  st = &newSt;

  st->dirtyPiece.dirty_num = 0;
  st->dirtyPiece.piece[0] = NO_PIECE; // Avoid checks in UpdateAccumulator()
  //st->accumulator.state[WHITE] = Eval::NNUE::EMPTY;
  //st->accumulator.state[BLACK] = Eval::NNUE::EMPTY;

  if (st->epSquare != SQ_NONE)
  {
      st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
      st->epSquare = SQ_NONE;
  }

  st->key ^= Zobrist::side;
  prefetch(TT.first_entry(key()));

  ++st->rule50;
  st->pliesFromNull = 0;

  sideToMove = ~sideToMove;

  set_check_info(st);

  st->repetition = 0;

  assert(pos_is_ok());
}

void Position::undo_null_move() {

  assert(!checkers());

  st = st->previous;
  sideToMove = ~sideToMove;
}

void Position::set_king_square(Color side, Square s) {
  kingSquare[side] = s;
}

Square Position::get_king_square(Color side) const {
  return kingSquare[side];
}

/// Position::key_after() computes the new hash key after the given move. Needed
/// for speculative prefetch. It doesn't recognize special moves like castling,
/// en passant and promotions.

Key Position::key_after(Move m) const {

  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = piece_on(from);
  Piece captured = piece_on(to);
  Key k = st->key ^ Zobrist::side;

  if (captured)
      k ^= Zobrist::psq[captured][to];

  return k ^ Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];
}


/// Position::see_ge (Static Exchange Evaluation Greater or Equal) tests if the
/// SEE value of move is greater or equal to the given threshold. We'll use an
/// algorithm similar to alpha-beta pruning with a null window.

bool Position::see_ge(Move m, Value threshold) const {

  assert(is_ok(m));

  // Only deal with normal moves, assume others pass a simple SEE
  if (type_of(m) != NORMAL)
      return VALUE_ZERO >= threshold;

  Square from = from_sq(m), to = to_sq(m);

  int swap = PieceValue[MG][piece_on(to)] - threshold;
  if (swap < 0)
      return false;

  swap = PieceValue[MG][piece_on(from)] - swap;
  if (swap <= 0)
      return true;

  Bitboard occupied = pieces() ^ from ^ to;
  Color stm = color_of(piece_on(from));
  Bitboard attackers = attackers_to(to, occupied);
  Bitboard stmAttackers, bb;
  int res = 1;

  while (true)
  {
      stm = ~stm;
      attackers &= occupied;

      // If stm has no more attackers then give up: stm loses
      if (!(stmAttackers = attackers & pieces(stm)))
          break;

      // Don't allow pinned pieces to attack (except the king) as long as
      // there are pinners on their original square.
      if (pinners(~stm) & occupied)
          stmAttackers &= ~blockers_for_king(stm);

      if (!stmAttackers)
          break;

      res ^= 1;

      // Locate and remove the next least valuable attacker, and add to
      // the bitboard 'attackers' any X-ray attackers behind it.
      if ((bb = stmAttackers & pieces(PAWN)))
      {
          if ((swap = PawnValueMg - swap) < res)
              break;

          occupied ^= lsb(bb);
          attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
      }

      else if ((bb = stmAttackers & pieces(KNIGHT)))
      {
          if ((swap = KnightValueMg - swap) < res)
              break;

          occupied ^= lsb(bb);
      }

      else if ((bb = stmAttackers & pieces(BISHOP)))
      {
          if ((swap = BishopValueMg - swap) < res)
              break;

          occupied ^= lsb(bb);
          attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
      }

      else if ((bb = stmAttackers & pieces(ROOK)))
      {
          if ((swap = RookValueMg - swap) < res)
              break;

          occupied ^= lsb(bb);
          attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
      }

      else if ((bb = stmAttackers & pieces(QUEEN)))
      {
          if ((swap = QueenValueMg - swap) < res)
              break;

          occupied ^= lsb(bb);
          attackers |=  (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                      | (attacks_bb<ROOK  >(to, occupied) & pieces(ROOK  , QUEEN));
      }

      else // KING
           // If we "capture" with the king but opponent still has attackers,
           // reverse the result.
          return (attackers & ~pieces(stm)) ? res ^ 1 : res;
  }

  return bool(res);
}


/// Position::is_draw() tests whether the position is drawn by 50-move rule
/// or by repetition. It does not detect stalemates.

bool Position::is_draw(int ply) const {

  if (st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
      return true;

  // Return a draw score if a position repeats once earlier but strictly
  // after the root, or repeats twice before or at the root.
  return st->repetition && st->repetition < ply;
}


// Position::has_repeated() tests whether there has been at least one repetition
// of positions since the last capture or pawn move.

bool Position::has_repeated() const {

    StateInfo* stc = st;
    int end = std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}


/// Position::has_game_cycle() tests if the position has a move which draws by repetition,
/// or an earlier position has a move that directly reaches the current position.

bool Position::has_game_cycle(int ply) const {

  int j;

  int end = std::min(st->rule50, st->pliesFromNull);

  if (end < 3)
    return false;

  Key originalKey = st->key;
  StateInfo* stp = st->previous;

  for (int i = 3; i <= end; i += 2)
  {
      stp = stp->previous->previous;

      Key moveKey = originalKey ^ stp->key;
      if (   (j = H1(moveKey), cuckoo[j] == moveKey)
          || (j = H2(moveKey), cuckoo[j] == moveKey))
      {
          Move move = cuckooMove[j];
          Square s1 = from_sq(move);
          Square s2 = to_sq(move);

          if (!(between_bb(s1, s2) & pieces()))
          {
              if (ply > i)
                  return true;

              // For nodes before or at the root, check that the move is a
              // repetition rather than a move to the current position.
              // In the cuckoo table, both moves Rc1c5 and Rc5c1 are stored in
              // the same location, so we have to select which square to check.
              if (color_of(piece_on(empty(s1) ? s2 : s1)) != side_to_move())
                  continue;

              // For repetitions before or at the root, require one more
              if (stp->repetition)
                  return true;
          }
      }
  }
  return false;
}


/// Position::flip() flips position with the white and black sides reversed. This
/// is only useful for debugging e.g. for finding evaluation symmetry bugs.

void Position::flip() {

  string f, token;
  std::stringstream ss(fen());

  for (Rank r = RANK_8; r >= RANK_1; --r) // Piece placement
  {
      std::getline(ss, token, r > RANK_1 ? '/' : ' ');
      f.insert(0, token + (f.empty() ? " " : "/"));
  }

  ss >> token; // Active color
  f += (token == "w" ? "B " : "W "); // Will be lowercased later

  ss >> token; // Castling availability
  f += token + " ";

  std::transform(f.begin(), f.end(), f.begin(),
                 [](char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

  ss >> token; // En passant square
  f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

  std::getline(ss, token); // Half and full moves
  f += token;

  set(f, st); // changed to avoid error

  assert(pos_is_ok());
}


/// Position::pos_is_ok() performs some consistency checks for the
/// position object and raises an asserts if something wrong is detected.
/// This is meant to be helpful when debugging.

bool Position::pos_is_ok() const {

  constexpr bool Fast = true; // Quick (default) or full check?

  if (   (sideToMove != WHITE && sideToMove != BLACK)
      || piece_on(square<KING>(WHITE)) != W_KING
      || piece_on(square<KING>(BLACK)) != B_KING
      || (   ep_square() != SQ_NONE
          && relative_rank(sideToMove, ep_square()) != RANK_6))
      assert(0 && "pos_is_ok: Default");

  if (Fast)
      return true;

  if (   pieceCount[W_KING] != 1
      || pieceCount[B_KING] != 1
      || attackers_to(square<KING>(~sideToMove)) & pieces(sideToMove))
      assert(0 && "pos_is_ok: Kings");

  if (   (pieces(PAWN) & (Rank1BB | Rank8BB))
      || pieceCount[W_PAWN] > 8
      || pieceCount[B_PAWN] > 8)
      assert(0 && "pos_is_ok: Pawns");

  if (   (pieces(WHITE) & pieces(BLACK))
      || (pieces(WHITE) | pieces(BLACK)) != pieces()
      || popcount(pieces(WHITE)) > 16
      || popcount(pieces(BLACK)) > 16)
      assert(0 && "pos_is_ok: Bitboards");

  for (PieceType p1 = PAWN; p1 <= KING; ++p1)
      for (PieceType p2 = PAWN; p2 <= KING; ++p2)
          if (p1 != p2 && (pieces(p1) & pieces(p2)))
              assert(0 && "pos_is_ok: Bitboards");

  StateInfo si = *st;
  //ASSERT_ALIGNED(&si, Eval::NNUE::kCacheLineSize);

  set_state(&si);
  if (std::memcmp(&si, st, sizeof(StateInfo)))
      assert(0 && "pos_is_ok: State");

  for (Piece pc : Pieces)
      if (   pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
          || pieceCount[pc] != std::count(board, board + SQUARE_NB, pc))
          assert(0 && "pos_is_ok: Pieces");

  for (Color c : { WHITE, BLACK })
      for (CastlingRights cr : {c & KING_SIDE, c & QUEEN_SIDE})
      {
          if (!can_castle(cr))
              continue;

          if (   piece_on(castlingRookSquare[cr]) != make_piece(c, ROOK)
              || castlingRightsMask[castlingRookSquare[cr]] != cr
              || (castlingRightsMask[square<KING>(c)] & cr) != cr)
              assert(0 && "pos_is_ok: Castling");
      }

  return true;
}

} // namespace Stockfish
