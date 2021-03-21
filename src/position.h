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

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <cassert>
#include <deque>
#include <memory> // For std::unique_ptr
#include <string>

#include "evaluate.h"
#include "types.h"

namespace Stockfish {

/// StateInfo struct stores information needed to restore a Position object to
/// its previous state when we retract a move. Whenever a move is made on the
/// board (by calling Position::make_move), a StateInfo object must be passed.

struct StateInfo {
  // Actually used
  Key hashKey;
  int rule60;
  StateInfo* previous;
};

/// A list to keep track of the position states along the setup moves (from the
/// start position to the position just before the search starts). Needed by
/// 'draw by repetition' detection. Use a std::deque because pointers to
/// elements are not invalidated upon list resizing.
typedef std::unique_ptr<std::deque<StateInfo>> StateListPtr;


/// Position class stores information regarding the board representation as
/// pieces, side to move, hash keys, castling info, etc. Important methods are
/// make_move() and undo_move(), used by the search to update node info when
/// traversing the search tree.
class Thread;

class Position {
public:
  static void init();

  Position() = default;
  Position(const Position&) = delete;
  Position& operator=(const Position&) = delete;

  // FEN string input/output
  Position& set(const std::string& fenStr, StateInfo* si);
  std::string fen() const; // TODO
  
  // board interface
  Piece piece_on(Square s) const;
  Piece moved_piece(Move m) const;
  Piece captured_piece() const;
  
  bool isSquareAttacked(Square s, Color c);
  bool make_move(Move m, StateInfo& newSt, bool givesCheck);
  
  void undo_move(Move m);
  void do_null_move(StateInfo& newSt);
  void undo_null_move();
  
  // king square interface
  void set_king_square(Color side, Square s);
  Square get_king_square(Color side) const;

  // Accessing hash keys
  Key hash_key() const; // actually used

  // Other properties of the position
  Color side_to_move() const;
  int game_ply() const;
  int rule60_count() const;

  // state info
  StateInfo* state() const;

private:
  // Initialization helpers (used while setting up a position)
  void set_castling_right(Color c, Square rfrom);
  void set_state(StateInfo* si) const;
  void set_check_info(StateInfo* si) const;

  // Other helpers
  void put_piece(Piece pc, Square s);
  void remove_piece(Square s);
  void move_piece(Square from, Square to);
  template<bool Do>
  void do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto);

  /*
     
    Board representation
       (11x14 Mailbox)
    
    x x x x x x x x x x x
    x x x x x x x x x x x
    x r n b a k a b n r x
    x . . . . . . . . . x
    x . c . . . . . c . x
    x p . p . p . p . p x
    x . . . . . . . . . x
    x . . . . . . . . . x
    x P . P . P . P . P x
    x . C . . . . . C . x
    x . . . . . . . . . x
    x R N B A K A B N R x
    x x x x x x x x x x x
    x x x x x x x x x x x
        
  */
  
  // Data members
  Piece board[SQUARE_NB];
    
  // actually used
  int searchPly;
  int gamePly;
  Color sideToMove;
  int rule60;
  Key hashKey;
  Square kingSquare[2];
  
  Score psq;
  Thread* thisThread;
  StateInfo* st;
  bool chess960;
};

extern std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const {
  return sideToMove;
}

inline Piece Position::piece_on(Square s) const {
  return board[s];
}

inline Piece Position::moved_piece(Move m) const {
  return piece_on(getSourceSquare(m));
}

inline Key Position::hash_key() const {
  return hashKey;
}

inline int Position::game_ply() const {
  return gamePly;
}

inline int Position::rule60_count() const {
  return rule60;
}

inline void Position::put_piece(Piece pc, Square s) {
  board[s] = pc;
}

inline void Position::remove_piece(Square s) {
  board[s] = NO_PIECE; // Not needed, overwritten by the capturing one
}

inline void Position::move_piece(Square from, Square to) {
  Piece pc = board[from];
  board[from] = NO_PIECE;
  board[to] = pc;
}

inline StateInfo* Position::state() const {
  return st;
}

} // namespace Stockfish

#endif // #ifndef POSITION_H_INCLUDED
