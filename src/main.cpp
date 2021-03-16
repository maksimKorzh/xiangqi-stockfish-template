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

int main(int argc, char* argv[]) {
  printf("breakpoint 1\n");
  
  std::cout << engine_info() << std::endl; 
  
  printf("breakpoint 2\n");
  
  CommandLine::init(argc, argv);
  
  printf("breakpoint 3\n");
  
  UCI::init(Options);
  
  printf("breakpoint 4\n");
  
  Tune::init();
  
  printf("breakpoint 5\n");
  
  //PSQT::init();
  
  printf("breakpoint 6\n");
  
  //Bitboards::init();
  
  printf("breakpoint 7\n");
  
  Position::init();
  
  printf("breakpoint 8\n");
  
  Bitbases::init();
  
  printf("breakpoint 9\n"); 
  
  //Endgames::init();
  
  printf("breakpoint 10\n"); 
  
  Threads.set(size_t(Options["Threads"]));
  
  printf("breakpoint 11\n");
  
  Search::clear(); // After threads are up
  
  printf("breakpoint 12\n");
  
  Eval::NNUE::init();
  
  printf("breakpoint 13\n");

  UCI::loop(argc, argv);

  Threads.set(0);
  return 0;
}
