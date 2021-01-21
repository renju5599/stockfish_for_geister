#ifndef GAME_GEISTER_H_INCLUDED
#define GAME_GEISTER_H_INCLUDED

#include <string>
#include "types.h"

//�S�� position.h �ɈڐA����������������
namespace Game_
{

	extern char board[6][6];			//board[y][x] = {R:�����̐�, B:�����̐�, u:����̋�, '.':��}�X, ������y=5�̑��ɂ���
	extern char komaName[6][6];		//komaName[y][x] = {��M����, (y, x)�ɂ����̖��O}
	extern int rNum, uNum;				//�Ֆʂɂ���G�̐ԃR�}�̌�, �G�̃R�}�̌�

	const int WON = 1;
	const int LST = 2;
	const int DRW = 3;

	//s�̐擪��t �� true
	bool startWith(std::string& s, std::string t);

	//�Q�[���̏I������. dispFlag = true�ɂ����, ���ʂ�\���ł���B
	int isEnd(std::string s, bool dispFlag);

	//�{�[�h�̎�M
	//position����position.h��
	//��������komaname�Ƃ��p
	void recvBoard(std::string msg);

	//uci.cpp�ɐV����������(MoveStr)
	////�R�}���h�̕ϊ�
	//string move(int y, int x, int dir) {
	//	string moveStr = "NESW";
	//	string ret;
	//	
	//	ret += "MOV:";
	//	ret += komaName[y][x];
	//	ret += ",";
	//	ret += moveStr[dir];
	//	return ret;
	//}

	//�I���̌���
	std::string getEndInfo(std::string recv_msg);

}

namespace Red {

	extern int histCnt;
	extern char hist[350][6][6];	//R, B, u
	extern int eval[350][6][6];	//�ԓx
	extern bool existRed;


	//�����J�n���ɌĂяo��
	void init();

	//���������ł����Ƃ��ɌĂяo��
	void myMove(Move mv);

	//2��ڈȍ~�̎�����Ԃ̍ŏ��ɌĂяo���B
	void myTurn(char board[6][6]);

	//myMove�Ƃ�myTurn�Ƃ����Ăяo��������ɌĂяo�������B
	//�ԓxeval��臒l�ȏ�ɂȂ����Ԃ̌��݈ʒu���A�ԓx���傫�����̂��烊�X�g�A�b�v
	//���͍ő�̂�����g���ĂȂ������̂ŉ���
	int listUpRed(int posY[], int posX[], int X);
	Square picUpRed(int X);
}
#endif