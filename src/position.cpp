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

#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "uci.h"

using std::string;

namespace Stockfish {

namespace Zobrist {
  Key psq[PIECE_NB][SQUARE_NB];
  Key side;
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
  os << "\n    Hash key: " << std::hex << pos.hash_key() << std::dec;
  os << "\nKing squares: ";
  os << COORDINATES[pos.get_king_square(WHITE)] << " ";
  os << COORDINATES[pos.get_king_square(BLACK)];
  os << "\n     Rule 60: " << pos.rule60_count();
  os << "\n    Game ply: " << pos.game_ply() << "\n";

  return os;
}


/// Position::init() initializes at startup the various arrays used to compute hash keys

void Position::init() {
  PRNG rng(1070372);

  for (Piece pc : Pieces)
      for (Square s = SQ_A1; s < SQUARE_NB; ++s)
          Zobrist::psq[pc][s] = rng.rand<Key>();

  Zobrist::side = rng.rand<Key>();
}

// generate unique position identifier
Key Position::generate_hash_key() {
  uint64_t finalKey = 0;
      
  // hash board position
  for (Square s = SQ_A1; s < SQUARE_NB; ++s) {
    if (board[s] != OFFBOARD) {
      Piece piece = board[s];
      if (piece != NO_PIECE) finalKey ^= Zobrist::psq[piece][s];
    }
  }
  
  
  // hash board state variables
  if (sideToMove == WHITE) finalKey ^= Zobrist::side;
  
  return (Key)finalKey;
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
  
  // reset repetition table (should be done on ucinewgame command, not here)
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
  
  // 5. Halfmove clock
  for (int i = 0; i < 4; ++i) ss >> token;
  ss >> rule60;
  
  // 6.  and fullmove number
  ss >> token;
  ss >> gamePly;

  // Convert from fullmove starting from 1 to gamePly starting from 0,
  // handle also common incorrect FEN with fullmove = 0.
  gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);
  
  // generate hash key
  hashKey = generate_hash_key();

  return *this;
}


/// Position::fen() returns a FEN representation of the position. In case of
/// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.

string Position::fen() const {
  // TODO
  return "fen";
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


/// Position::make_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be pseudo legal.
 
bool Position::do_move(Move move, StateInfo& newSt) {
  // update plies
  ++searchPly;
  ++gamePly;
      
  // update repetition table
  //repetitionTable[gamePly] = hashKey;

  // push to stack
  st->hashKey = hashKey;
  st->rule60 = rule60;
  
  // copy current state info
  std::memcpy(&newSt, st, offsetof(StateInfo, hashKey));
  newSt.previous = st;
  st = &newSt;
  
  // parse move
  Square sourceSquare = move_source_square(move);
  Square targetSquare = move_target_square(move);
  Piece sourcePiece = move_source_piece(move);
  //Piece targetPiece = move_target_piece(move);
  int captureFlag = move_capture_flag(move);

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
  Square sourceSquare = move_source_square(move);
  Square targetSquare = move_target_square(move);
  Piece sourcePiece = move_source_piece(move);
  Piece targetPiece = move_target_piece(move);
  
  // move piece
  board[sourceSquare] = sourcePiece;
  board[targetSquare] = NO_PIECE;
  
  // restore captured piece
  if (move_capture_flag(move)) put_piece(targetPiece, targetSquare);
  
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


/// Position::do(undo)_null_move() is used to do(undo) a "null move": it flips
/// the side to move without executing any move on the board.

void Position::do_null_move(StateInfo& newSt) {
  // avaiod warning
  if (newSt.hashKey) {}
}

void Position::undo_null_move() {

}

void Position::set_king_square(Color side, Square s) {
  kingSquare[side] = s;
}

Square Position::get_king_square(Color side) const {
  return kingSquare[side];
}

} // namespace Stockfish
