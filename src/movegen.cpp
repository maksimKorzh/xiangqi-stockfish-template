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

#include <cassert>

#include "movegen.h"
#include "position.h"

namespace Stockfish {

// push move into move list
static ExtMove* pushMove(const Position& pos, ExtMove* moveList, Square sourceSquare, Square targetSquare, Piece sourcePiece, Piece targetPiece, bool onlyCaptures) {
  if (targetPiece == NO_PIECE || PIECE_COLOR[targetPiece] == (pos.side_to_move() ^ 1)) {
    Move move = MOVE_NONE;
    
    if (targetPiece) {
      move = move_encode(sourceSquare, targetSquare, sourcePiece, targetPiece, 1);
    } else {
      if (onlyCaptures == 0) {
        move = move_encode(sourceSquare, targetSquare, sourcePiece, targetPiece, 0);
      }
    }

    // push move into move list
    if (move) *moveList++ = move;
  }
  
  return moveList;
}

// generate pseudo legal moves
static ExtMove* generateMoves(const Position& pos, ExtMove* moveList, bool onlyCaptures) {
  // loop over all board squares
  for (Square sourceSquare = SQ_A1; sourceSquare < SQUARE_NB; ++sourceSquare) {
    // make sure square is on board
    if (pos.piece_on(sourceSquare) != OFFBOARD) {
      Piece piece = pos.piece_on(sourceSquare);
      PieceType pieceType = PIECE_TYPE[piece];
      Color pieceColor = PIECE_COLOR[piece];
      Color side = pos.side_to_move();
      
      if (pieceColor == side) {
        // pawns
        if (pieceType == PAWN) {
          for (int direction = 0; direction < 3; direction++) {
            Square targetSquare = (Square)(sourceSquare + PAWN_MOVE_OFFSETS[side][direction]);
            Piece targetPiece = pos.piece_on(targetSquare);
            
            if (targetPiece != OFFBOARD)
              moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
            
            if (BOARD_ZONES[side][sourceSquare]) break; 
          }
        }
        
        // kings & advisors
        if (pieceType == KING || pieceType == ADVISOR) {
          for (int direction = 0; direction < 4; direction++) {
            const int *offsets = (pieceType == KING) ? ORTHOGONALS : DIAGONALS;
            Square targetSquare = (Square)(sourceSquare + offsets[direction]);
            Piece targetPiece = pos.piece_on(targetSquare);
            
            if (BOARD_ZONES[side][targetSquare] == 2)
              moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
          }
        }
        
        // bishops
        if (pieceType == BISHOP) {
          for (int direction = 0; direction < 4; direction++) {
            Square targetSquare = (Square)(sourceSquare + BISHOP_MOVE_OFFSETS[direction]);
            Square jumpOver = (Square)(sourceSquare + DIAGONALS[direction]);
            Piece targetPiece = pos.piece_on(targetSquare);
            
            if (BOARD_ZONES[side][targetSquare] && pos.piece_on(jumpOver) == NO_PIECE)
              moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
          }
        }
        
        // knights
        if (pieceType == KNIGHT) {
          for (int direction = 0; direction < 4; direction++) {
            Square targetDirection = (Square)(sourceSquare + ORTHOGONALS[direction]);
      
            if (pos.piece_on(targetDirection) == NO_PIECE) {
              for (int offset = 0; offset < 2; offset++) {
                Square targetSquare = (Square)(sourceSquare + KNIGHT_MOVE_OFFSETS[direction][offset]);
                Piece targetPiece = pos.piece_on(targetSquare);
                
                if (targetPiece != OFFBOARD)
                  moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
              }
            }
          }
        }
        
        // rooks & cannons
        if (pieceType == ROOK || pieceType == CANNON) {
          for (int direction = 0; direction < 4; direction++) {
            Square targetSquare = (Square)(sourceSquare + ORTHOGONALS[direction]);
            int jumpOver = 0;
            
            while (pos.piece_on(targetSquare) != OFFBOARD) {
              Piece targetPiece = pos.piece_on(targetSquare);
              
              if (jumpOver == 0) {
                // all rook moves
                if (pieceType == ROOK /*&& PIECE_COLOR[targetPiece] == side ^ 1*/) // WARNING: potentially redudant second expression
                  moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
                
                // quiet cannon moves
                else if (pieceType == CANNON && targetPiece == NO_PIECE)
                  moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
              }

              if (targetPiece) jumpOver++;
              if (targetPiece && pieceType == CANNON && /*PIECE_COLOR[targetPiece] == side ^ 1) &&*/ jumpOver == 2) {
                // capture cannon moves
                moveList = pushMove(pos, moveList, sourceSquare, targetSquare, pos.piece_on(sourceSquare), targetPiece, onlyCaptures);
                break;
              }

              targetSquare = (Square)(targetSquare + ORTHOGONALS[direction]);
            }
          }
        }
      }
    }
  }
  
  return moveList;
} 

/// generate <PSEUDO_LEGAL> generates all the pseudo legal moves in the given position

template<>
ExtMove* generate<PSEUDO_LEGAL>(const Position& pos, ExtMove* moveList) {
  // generate pseudo legal moves 
  return generateMoves(pos, moveList, false);
}

} // namespace Stockfish
