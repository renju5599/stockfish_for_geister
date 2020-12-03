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

#ifdef false

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>

#include "types.h"

class Position;

namespace Eval {

  std::string trace(const Position& pos);
  Value evaluate(const Position& pos);

  extern bool useNNUE;
  extern std::string eval_file_loaded;

  // The default net name MUST follow the format nn-[SHA256 first 12 digits].nnue
  // for the build process (profile-build and fishtest) to work. Do not change the
  // name of the macro, as it is used in the Makefile.
  #define EvalFileDefaultName   "nn-baeb9ef2d183.nnue"

  namespace NNUE {

    Value evaluate(const Position& pos);
    bool load_eval(std::string name, std::istream& stream);
    void init();
    void verify();

  } // namespace NNUE

} // namespace Eval
#endif
#endif

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>

#include "MoveCommand.h"
#include "KanzenBoard.h"
#include "types.h"

class Position;


namespace Eval {

  std::string trace(const Position& pos);
  Value evaluate(const Position& pos);

  //逃げ, 追いかけの判定
  bool isNige(char board[][6], MoveCommand te);

  bool isOikake(char board[][6], MoveCommand te);

  //逃げ, 追いかけの回数
  void AddNigeR(bool printLog = false);
  void AddNigeB(bool printLog = false);
  void AddOikakeR(bool printLog = false);
  void AddOikakeB(bool printLog = false);


  //BEGIN: 赤度を推定する系統
  namespace Red {
    int histCnt;
    char hist[350][6][6];	//R, B, u
    double eval[350][6][6];	//赤度

    void moveHist(char prev[6][6], char now[6][6], MoveCommand mv);
    void moveEval(double prev[6][6], double now[6][6], MoveCommand mv);
    MoveCommand detectMove(char prev[6][6], char now[6][6]);
    int toDir(int y, int x, int ny, int nx);

    //自分が手を打ったときに呼び出す
    void myMove(MoveCommand mv);

    //2手目以降の自分手番の最初に呼び出す。
    void myTurn(char board[6][6]);

    //myMoveとかmyTurnとかを呼び出した直後に呼び出したい。
    //赤度evalが閾値以上になった赤の現在位置を、赤度が大きいものからリストアップ
    int listUpRed(int posY[], int posX[], int X);

    void moveHist(char prev[6][6], char now[6][6], MoveCommand mv);

    void moveEval(double prev[6][6], double now[6][6], MoveCommand mv);

    MoveCommand detectMove(char prev[6][6], char now[6][6]);

    int toDir(int y, int x, int ny, int nx);
  }
  //END


  //それ以外全部青！
  std::pair<MoveCommand, int> thinkKanzen(int X);

  //紫駒
  std::pair<MoveCommand, int> thinkPurple();

  //手を決める
  std::pair<MoveCommand, int> thinkMove();

  //手を決めるのと、いろんな処理
  std::string solve(int turnCnt);

}

#endif // #ifndef EVALUATE_H_INCLUDED