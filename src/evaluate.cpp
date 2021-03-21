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

#include <iostream>
#include "evaluate.h"
#include "position.h"


/// I took evaluation parameters from Mark Dirish's
/// javascript xiangqi engine: https://github.com/markdirish/xiangqi
///    
/// Credits to initial sources (from Mark's sources):
///    
/// material weights: by Yen et al. 2004, "Computer Chinese Chess" ICGA Journal
///      PST weights: by Li, Cuanqi 2008, "Using AdaBoost to Implement Chinese
///                                           Chess Evaluation Functions", UCLA thesis

using namespace std;
//using namespace Stockfish::Eval::NNUE;

namespace Stockfish {

// material weights
const int MATERIAL_WEIGHTS[] = {
  //  P     A     B     N     C     R      K   
  0, 30,  120,  120,  270,  285,  600,  6000,
  
  //  p     a     b     n     c     r      k
    -30, -120, -120, -270, -285, -600, -6000
};

// piece square tables
const int PST[4][154] = {
  // pawns
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,  -2,   0,   4,   0,  -2,   0,   0,   0,
  0,   2,   0,   8,   0,   8,   0,   8,   0,   2,   0, 
  0,   6,  12,  18,  18,  20,  18,  18,  12,   6,   0, 
  0,  10,  20,  30,  34,  40,  34,  30,  20,  10,   0, 
  0,  14,  26,  42,  60,  80,  60,  42,  26,  14,   0, 
  0,  18,  36,  56,  80, 120,  80,  56,  36,  18,   0, 
  0,   0,   3,   6,   9,  12,   9,   6,   3,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  
  // knights
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,  -4,   0,   0,   0,   0,   0,  -4,   0,   0, 
  0,   0,   2,   4,   4,  -2,   4,   4,   2,   0,   0, 
  0,   4,   2,   8,   8,   4,   8,   8,   2,   4,   0, 
  0,   2,   6,   8,   6,  10,   6,   8,   6,   2,   0, 
  0,   4,  12,  16,  14,  12,  14,  16,  12,   4,   0, 
  0,   6,  16,  14,  18,  16,  18,  14,  16,   6,   0, 
  0,   8,  24,  18,  24,  20,  24,  18,  24,   8,   0, 
  0,   12, 14,  16,  20,  18,  20,  16,  14,  12,   0, 
  0,   4,  10,  28,  16,   8,  16,  28,  10,   4,   0, 
  0,   4,   8,  16,  12,   4,  12,  16,   8,   4,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  
  // cannon
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   2,   6,   6,   6,   2,   0,   0,   0, 
  0,   0,   2,   4,   6,   6,   6,   4,   2,   0,   0, 
  0,   4,   0,   8,   6,  10,   6,   8,   0,   4,   0, 
  0,   0,   0,   0,   2,   4,   2,   0,   0,   0,   0, 
  0,  -2,   0,   4,   2,   6,   2,   4,   0,  -2,   0, 
  0,   0,   0,   0,   2,   8,   2,   0,   0,   0,   0, 
  0,   0,   0,  -2,   4,  10,   4,  -2,   0,   0,   0, 
  0,   2,   2,   0, -10,  -8, -10,   0,   2,   2,   0, 
  0,   2,   2,   0,  -4, -14,  -4,   0,   2,   2,   0, 
  0,   6,   4,   0, -10, -12, -10,   0,   4,   6,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  
  // rooks
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,  -2,  10,   6,  14,  12,  14,   6,  10,  -2,   0, 
  0,   8,   4,   8,  16,   8,  16,   8,   4,   8,   0, 
  0,   4,   8,   6,  14,  12,  14,   6,   8,   4,   0, 
  0,   6,  10,   8,  14,  14,  14,   8,  10,   6,   0, 
  0,  12,  16,  14,  20,  20,  20,  14,  16,  12,   0, 
  0,  12,  14,  12,  18,  18,  18,  12,  14,  12,   0, 
  0,  12,  18,  16,  22,  22,  22,  16,  18,  12,   0, 
  0,  12,  12,  12,  18,  18,  18,  12,  12,  12,   0, 
  0,  16,  20,  18,  24,  26,  24,  18,  20,  16,   0, 
  0,  14,  14,  12,  18,  16,  18,  12,  14,  14,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// mirror square for black
const int MIRROR_SQUARE[] = {
  SQ_K14, SQ_J14, SQ_I14, SQ_H14, SQ_G14, SQ_F14, SQ_E14, SQ_D14, SQ_C14, SQ_B14, SQ_A14, 
  SQ_K13, SQ_J13, SQ_I13, SQ_H13, SQ_G13, SQ_F13, SQ_E13, SQ_D13, SQ_C13, SQ_B13, SQ_A13, 
  SQ_K12, SQ_J12, SQ_I12, SQ_H12, SQ_G12, SQ_F12, SQ_E12, SQ_D12, SQ_C12, SQ_B12, SQ_A12, 
  SQ_K11, SQ_J11, SQ_I11, SQ_H11, SQ_G11, SQ_F11, SQ_E11, SQ_D11, SQ_C11, SQ_B11, SQ_A11, 
  SQ_K10, SQ_J10, SQ_I10, SQ_H10, SQ_G10, SQ_F10, SQ_E10, SQ_D10, SQ_C10, SQ_B10, SQ_A10, 
  SQ_K9, SQ_J9, SQ_I9, SQ_H9, SQ_G9, SQ_F9, SQ_E9, SQ_D9, SQ_C9, SQ_B9, SQ_A9, 
  SQ_K8, SQ_J8, SQ_I8, SQ_H8, SQ_G8, SQ_F8, SQ_E8, SQ_D8, SQ_C8, SQ_B8, SQ_A8, 
  SQ_K7, SQ_J7, SQ_I7, SQ_H7, SQ_G7, SQ_F7, SQ_E7, SQ_D7, SQ_C7, SQ_B7, SQ_A7, 
  SQ_K6, SQ_J6, SQ_I6, SQ_H6, SQ_G6, SQ_F6, SQ_E6, SQ_D6, SQ_C6, SQ_B6, SQ_A6, 
  SQ_K5, SQ_J5, SQ_I5, SQ_H5, SQ_G5, SQ_F5, SQ_E5, SQ_D5, SQ_C5, SQ_B5, SQ_A5, 
  SQ_K4, SQ_J4, SQ_I4, SQ_H4, SQ_G4, SQ_F4, SQ_E4, SQ_D4, SQ_C4, SQ_B4, SQ_A4, 
  SQ_K3, SQ_J3, SQ_I3, SQ_H3, SQ_G3, SQ_F3, SQ_E3, SQ_D3, SQ_C3, SQ_B3, SQ_A3, 
  SQ_K2, SQ_J2, SQ_I2, SQ_H2, SQ_G2, SQ_F2, SQ_E2, SQ_D2, SQ_C2, SQ_B2, SQ_A2, 
  SQ_K1, SQ_J1, SQ_I1, SQ_H1, SQ_G1, SQ_F1, SQ_E1, SQ_D1, SQ_C1, SQ_B1, SQ_A1
};

/// evaluate() is the evaluator for the outer world. It returns a static
/// evaluation of the position from the point of view of the side to move.

Value Eval::evaluate(const Position& pos) {
  Value score = (Value)0;
    
  for (Square square = SQ_A1; square < SQUARE_NB; ++square) {
    if (pos.piece_on(square) != OFFBOARD) {
      if (pos.piece_on(square)) {
        // extract piece
        Piece piece = pos.piece_on(square);
                
        // material score
        score += (Value)MATERIAL_WEIGHTS[piece];
        
        // piece-square table positional score
        switch((int)piece) {
          case W_PAWN: score += PST[0][square]; break;
          case W_KNIGHT: score += PST[1][square]; break;
          case W_CANNON: score += PST[2][square]; break;
          case W_ROOK: score += PST[3][square]; break;
          
          case B_PAWN: score -= PST[0][MIRROR_SQUARE[square]]; break;
          case B_KNIGHT: score -= PST[1][MIRROR_SQUARE[square]]; break;
          case B_CANNON: score -= PST[2][MIRROR_SQUARE[square]]; break;
          case B_ROOK: score -= PST[3][MIRROR_SQUARE[square]]; break;
        }        
      }
    }
  }

  return (pos.side_to_move() == WHITE) ? score : -score;
}


/// trace() is like evaluate(), but instead of returning a value, it returns
/// a string (suitable for outputting to stdout) that contains the detailed
/// descriptions and values of each evaluation term. Useful for debugging.
/// Trace scores are from white's point of view

void Eval::trace(const Position& pos) {
  Value score = evaluate(pos);
  std::cout << "Score: " << ((pos.side_to_move() == WHITE) ? score : -score) << std::endl;
}

} // namespace Stockfish
