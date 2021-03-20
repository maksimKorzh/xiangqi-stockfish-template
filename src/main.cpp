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

#include "bitboard.h"
#include "endgame.h"
#include "position.h"
#include "psqt.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"

using namespace Stockfish;

// rnbakabnr/9/1c5c1/p1p1p1p1p/2P6/9/P111P1P1P/1C5C1/9/RNBAKABNR w - - 0 1

int main(int argc, char* argv[]) {
  std::cout << engine_info() << std::endl; 
  CommandLine::init(argc, argv);
  UCI::init(Options);
  //Position::init();  
  //Threads.set(size_t(Options["Threads"]));
  //Search::clear(); // After threads are up
  //Eval::NNUE::init();

  /*Position pos;
  StateListPtr states(new std::deque<StateInfo>(1));
  
  // rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1
  // r1ba1a3/4kn3/2n1b4/pNp1p1p1p/4c4/6P2/P1P2R2P/1CcC5/9/2BAKAB2 w - - 0 1
  pos.set("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1", &states->back());
  std::cout << pos << "\n";
  Search::perftTest(pos, (Depth)5);
  */
  
  UCI::loop(argc, argv);

  Threads.set(0);
  return 0;
}
