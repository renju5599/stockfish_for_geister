/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2020 The Stockfish developers (see AUTHORS file)

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
#include <cassert>

#include "bitboard.h"
#include "endgame.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"
#include "evaluate.h"

/*
namespace PSQT {
  void init();
}
*/

int main(int argc, char* argv[]) {

  std::cout << engine_info() << std::endl;

  CommandLine::init(argc, argv);
  UCI::init(Options);

  int n, port; std::string destination;
  std::cout << "対戦回数 ポート番号 IPアドレスを入力↓" << std::endl;
  std::cin >> n >> port >> destination;

  Tune::init();
  //PSQT::init();
  Bitboards::init();
  Position::init();
  //Bitbases::init();
  //Endgames::init();
  //std::cout << size_t(Options["Threads"]) << std::endl;
  //Threads.set(size_t(Options["Threads"]));
  Threads.set(1);
  Search::clear(); // After threads are up
  //Eval::NNUE::init();
  Eval::init();
  

  //UCI::loop(argc, argv);
  tcp::playGame(n, port, destination);

  Threads.set(0);
  return 0;
}
