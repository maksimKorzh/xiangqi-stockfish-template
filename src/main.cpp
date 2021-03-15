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


#include "movegen.h"
void perft(Position& pos, Depth depth) {

    StateInfo st;
    ASSERT_ALIGNED(&st, Eval::NNUE::kCacheLineSize);

    uint64_t cnt, nodes = 0;
    const bool leaf = (depth == 2);

    for (const auto& m : MoveList<LEGAL>(pos))
    {
        if (depth <= 1)
            {}//cnt = 1, nodes++;
        else
        {
            pos.do_move(m, st);
            //cnt = leaf ? MoveList<LEGAL>(pos).size() : perft(pos, depth - 1);
            nodes += cnt;
            pos.undo_move(m);
        }

        sync_cout << UCI::move(m, pos.is_chess960()) << ": " << cnt << sync_endl;
    }
    return;
  }

void debug() {
  Position pos;
  StateListPtr states(new std::deque<StateInfo>(1));

  pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, &states->back(), Threads.main());
  
  /* loop over moves
  for (const auto& m : MoveList<LEGAL>(pos))
  {
    sync_cout << UCI::move(m, pos.is_chess960()) << sync_endl;
  }*/
  
  perft(pos, (Depth)1);
  
  pos.xq_set(XQ_START_FEN);
  std::cout << pos << std::endl;
  
  
  return;
}

int main(int argc, char* argv[]) {

  std::cout << engine_info() << std::endl;

  CommandLine::init(argc, argv);
  UCI::init(Options);
  Tune::init();
  PSQT::init();
  Bitboards::init();
  Position::init();
  Bitbases::init();
  Endgames::init();
  Threads.set(size_t(Options["Threads"]));
  Search::clear(); // After threads are up
  Eval::NNUE::init();

  // debug
  debug();
  
  //UCI::loop(argc, argv);

  Threads.set(0);
  return 0;
}
