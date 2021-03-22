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
#include <cmath>
#include <cstring>   // For std::memset
#include <iostream>
#include <sstream>

#include "evaluate.h"
#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "timeman.h"
#include "uci.h"

namespace Stockfish {

namespace Search {

  LimitsType Limits;  
  
}

using std::string;
using Eval::evaluate;
using namespace Search;

namespace {

  // define alpha beta search
  // define quiescence
  
  // perft() is our utility to verify move generation. All the leaf nodes up
  // to the given depth are generated and counted, and the sum is returned.

  uint64_t nodes_cnt = 0;

  // local perft driver
  void perftDriver(Position& pos, Depth depth) {
    StateInfo st;
    
    if (depth == 0)
    {
      nodes_cnt++;
      return;
    }
    
    for (const auto& m : MoveList<PSEUDO_LEGAL>(pos))
    {   
      if (pos.do_move(m, st) == false) continue;
      perftDriver(pos, depth - 1);
      pos.undo_move(m);
    }
  }
  
  // perft test
  uint64_t perft(Position& pos, Depth depth) {
    nodes_cnt = 0;
    LimitsType limits;
    limits.startTime = now();
    StateInfo st;
    
    for (const auto& m : MoveList<PSEUDO_LEGAL>(pos))
    {
      if (pos.do_move(m, st) == false) continue;
      uint64_t cum_nodes = nodes_cnt;
      perftDriver(pos, depth - 1);
      pos.undo_move(m);
      
      uint64_t old_nodes = nodes_cnt - cum_nodes;
      std::cout << "move: " << UCI::move(m);
      std::cout << " nodes: " << old_nodes <<"\n";
    }
    
    std::cout << "\nTime spent: " << now() - limits.startTime << " ms\n";
    return nodes_cnt;
  }

} // namespace


/// globally seen perft routine wrapper

void Search::perftTest(Position& pos, Depth depth) {
  uint64_t nodes = perft(pos, depth);
  std::cout << "\nNodes searched: " << nodes << "\n" << sync_endl;
  return;
}


/// Search::init() is called at startup to initialize various lookup tables

void Search::init() {

}


/// Search::clear() resets search state to its initial value

void Search::clear() {

}

/// Main search is started when the program receives the UCI 'go'
/// command. It searches from the root position and outputs the "bestmove".

void Search::sync_search(Position& pos, LimitsType& limits) {
  if ((Depth)limits.perft)
  {
      Search::perftTest(pos, limits.perft);
      return;
  }
}

namespace {

  // search<>() is the main search function for both PV and non-PV nodes
  // alphabeta search body



} // namespace

} // namespace Stockfish
