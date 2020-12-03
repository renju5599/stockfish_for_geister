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

  //����, �ǂ������̔���
  bool isNige(char board[][6], MoveCommand te);

  bool isOikake(char board[][6], MoveCommand te);

  //����, �ǂ������̉�
  void AddNigeR(bool printLog = false);
  void AddNigeB(bool printLog = false);
  void AddOikakeR(bool printLog = false);
  void AddOikakeB(bool printLog = false);


  //BEGIN: �ԓx�𐄒肷��n��
  namespace Red {
    int histCnt;
    char hist[350][6][6];	//R, B, u
    double eval[350][6][6];	//�ԓx

    void moveHist(char prev[6][6], char now[6][6], MoveCommand mv);
    void moveEval(double prev[6][6], double now[6][6], MoveCommand mv);
    MoveCommand detectMove(char prev[6][6], char now[6][6]);
    int toDir(int y, int x, int ny, int nx);

    //���������ł����Ƃ��ɌĂяo��
    void myMove(MoveCommand mv);

    //2��ڈȍ~�̎�����Ԃ̍ŏ��ɌĂяo���B
    void myTurn(char board[6][6]);

    //myMove�Ƃ�myTurn�Ƃ����Ăяo��������ɌĂяo�������B
    //�ԓxeval��臒l�ȏ�ɂȂ����Ԃ̌��݈ʒu���A�ԓx���傫�����̂��烊�X�g�A�b�v
    int listUpRed(int posY[], int posX[], int X);

    void moveHist(char prev[6][6], char now[6][6], MoveCommand mv);

    void moveEval(double prev[6][6], double now[6][6], MoveCommand mv);

    MoveCommand detectMove(char prev[6][6], char now[6][6]);

    int toDir(int y, int x, int ny, int nx);
  }
  //END


  //����ȊO�S���I
  std::pair<MoveCommand, int> thinkKanzen(int X);

  //����
  std::pair<MoveCommand, int> thinkPurple();

  //������߂�
  std::pair<MoveCommand, int> thinkMove();

  //������߂�̂ƁA�����ȏ���
  std::string solve(int turnCnt);

}

#endif // #ifndef EVALUATE_H_INCLUDED