#ifndef KANZENBOARD_H_INCLUDED
#define KANZENBOARD_H_INCLUDED

//i�sj���(i>=0, j>=0)���}�Xi * 6 + j�Ƃ���.
//�p�ӂ��郁�\�b�h�F�I������(2���), ������
#pragma once
#include <string>
#include <algorithm>
#include "MoveCommand.h"

namespace kbb {
	int weight1 = 1000;		//��̌��̕]���̏d��(1�`1000)
	int weight2 = 1;		//��̈ʒu�̕]���̏d��(1�`1000)
	int bitCountTable[1 << 18];	//bitCountTable[s] = s��2�i���\�L�ɂ����錅�̘a
	int _myGoalDist[1 << 18];	//_myGoalDist[s] = (s��i�r�b�g�ڂ�1�̃}�Xi�ɋ����)�Ƃ��̃S�[���܂ł̃}���n�b�^�������̘a.
	int _yourGoalDist[1 << 18];
	
	//x��0�`2^36 - 1
	inline int bitCount(long long x) {
		return bitCountTable[x & 262143] + bitCountTable[x >> 18];
	}
	
	//�O����. AI.cpp�ŌĂяo��.
	void prepare() {
		int i, j;
		
		for (i = 0; i < (1 << 9); i++) {
			int cnt = 0;
			for (j = 0; j < 9; j++) {
				if ((i >> j) & 1) cnt++;
			}
			bitCountTable[i] = cnt;
		}
		for (i = (1 << 9); i < (1 << 18); i++) {
			bitCountTable[i] = bitCountTable[i & 511] + bitCountTable[i >> 9];
		}
		
		for (i = 0; i < (1 << 18); i++) {
			_myGoalDist[i] = 0;
			_yourGoalDist[i] = 0;
			for (j = 0; j < 18; j++) {
				if ((i >> j) % 2 == 0) continue;
				//�}�Xj����S�[���܂ł̃}���n�b�^������
				//����w��y = 0����, ���w��y = 5���ɂ���Ƃ���B
				int y = j / 6;
				int x = j % 6;
				int dist1 = y + std::min(x, 5 - x);
				int dist2 = 5 - y + std::min(x, 5 - x);
				_myGoalDist[i] += dist1;
				_yourGoalDist[i] += dist2;
			}
		}
	}
	
	//s = 0�`2^36 - 1. (s >> i) & 1 == 1 �� �}�Xi�Ɏ������
	inline int myGoalDist(long long s) {
		return _myGoalDist[s & 262143] + _myGoalDist[s >> 18] + 3 * bitCountTable[s >> 18];
	}
	
	//s = 0�`2^36 - 1. (s >> i) & 1 == 1 �� �}�Xi�ɑ�������
	inline int yourGoalDist(long long s) {
		return _yourGoalDist[s & 262143] + _yourGoalDist[s >> 18] - 3 * bitCountTable[s >> 18];
	}
}

struct KanzenBoard
{
	long long existR;	//�}�Xi�Ɏ����̐Ԃ����� �� (existR >> i) & 1 == 1�Ƃ���B
	long long existB;
	long long existr;	//�}�Xi�ɓG�̐ԋ���� �� (existr >> i) & 1 == 1�Ƃ���B
	long long existb;
	
	//board[i]�c�}�Xi�ɂ����̎��(R, B, u)
	void toBitBoard(string board) {
		existR = existB = existr = existb = 0;
		for (int i = 0; i < 36; i++) {
			if (board[i] == 'R') { existR |= (1LL << i); }
			if (board[i] == 'B') { existB |= (1LL << i); }
			if (board[i] == 'r') { existr |= (1LL << i); }
			if (board[i] == 'b') { existb |= (1LL << i); }
		}
	}
	
	//��̐���. ��̌���Ԃ�(teban=0 : �������, teban=1:�G���). from[], to[]�Ɏ���i�[. (from, to�̓T�C�Y32�ȏ�̔z��j
	//kiki[5 * i + j] = �}�Xi�Ɨאڂ���j�Ԗڂ̃}�X�̔ԍ�. �Ȃ����-1�B
	int makeMoves(int teban, int kiki[], int from[], int to[]) {
		int pos, i, cnt = 0;
		for (pos = 0; pos < 36; pos++) {
			if (teban == 0 && !(((existR | existB) >> pos) & 1)) continue;
			if (teban == 1 && !(((existr | existb) >> pos) & 1)) continue;
			for (i = pos * 5; kiki[i] != -1; i++) {
				int npos = kiki[i];
				//�}�Xpos -> �}�Xnpos�Ƌ�𓮂����邩�H�i����ƂԂ���Ȃ����j
				if (teban == 0 && (((existR | existB) >> npos) & 1)) continue;
				if (teban == 1 && (((existr | existb) >> npos) & 1)) continue;
				//��������
				from[cnt] = pos;
				to[cnt] = npos;
				cnt++;
			}
		}
		return cnt;
	}
	
	//���̐Ԃ𓮂�������̏�ԂɍX�V����B
	inline void moveR(int from, int to) {
		existR &= ~(1LL << from);
		existR |= (1LL << to);
		existr &= ~(1LL << to);
		existb &= ~(1LL << to);
	}
	
	inline void moveB(int from, int to) {
		existB &= ~(1LL << from);
		existB |= (1LL << to);
		existr &= ~(1LL << to);
		existb &= ~(1LL << to);
	}
	
	inline void mover(int from, int to) {
		existr &= ~(1LL << from);
		existr |= (1LL << to);
		existR &= ~(1LL << to);
		existB &= ~(1LL << to);
	}
	
	inline void moveb(int from, int to) {
		existb &= ~(1LL << from);
		existb |= (1LL << to);
		existR &= ~(1LL << to);
		existB &= ~(1LL << to);
	}
	
	void move(int from, int to) {
		if ((existR >> from) & 1)
			moveR(from, to);
		else if ((existB >> from) & 1)
			moveB(from, to);
		else if ((existr >> from) & 1)
			mover(from, to);
		else if ((existb >> from) & 1)
			moveb(from, to);
		else
			assert(0);
	}
	
	//�ǂ��炪������Ԃ���Ԃ�. (0�c����, 1�c�G, 2�c�s��). 
	//teban �c 0�Ȃ玩�����. 1�Ȃ�G���. (��� �c ���ł��O�̃v���C���[)
	int getWinPlayer(int teban) {
		if (existR == 0 || existb == 0) return 0;
		if (existB == 0 || existr == 0) return 1;
		if (teban == 0 && ((existB & 1LL) || ((existB >> 5) & 1LL))) return 0;
		if (teban == 1 && (((existb >> 30) & 1LL) || ((existb >> 35) & 1LL))) return 1;
		return 2;
	}
	
	//teban�v���C���[��1��ŒE�o�ł��邩�H (teban��0�Ȃ玩�����)
	//�E�ł��Ȃ��cMoveCommand(-1, -1, -1)
	//�E�ł���  �c���������iMoveCommand)��Ԃ�
	//�[��0�ł̔���ł�, �������p����. �f�o�b�O�p�Ƃ���, ����Ԃł����p�\�ɂ���.
	MoveCommand getEscapeCommand(int teban) {
		if (teban == 0) {
			if (existB & 1LL) return MoveCommand(0, 0, 3);
			if ((existB >> 5) & 1LL) return MoveCommand(0, 5, 1);
			return MoveCommand(-1, -1, -1);
		}
		else {
			if ((existb >> 30) & 1LL) return MoveCommand(5, 0, 3);
			if ((existb >> 35) & 1LL) return MoveCommand(5, 5, 1);
			return MoveCommand(-1, -1, -1);
		}
	}
	
	//�]���֐�. teban�v���C���[�̗L������Ԃ�. teban=0�c�������.
	int evaluate(int teban) {
		int s0 = kbb::weight1 * kbb::bitCount(existB) - kbb::weight2 * kbb::myGoalDist(existB | existR);
		int s1 = kbb::weight1 * kbb::bitCount(existb) - kbb::weight2 * kbb::yourGoalDist(existb | existr);
		if (teban == 0) return s0 - s1;
		return s1 - s0;
	}
	
	//�f�o�b�O�p
	void printBoard() {
		for (int y = 0; y < 6; y++) {
			for (int x = 0; x < 6; x++) {
				int id = y * 6 + x;
				if ((existR >> id) & 1) std::cout << "R";
				else if ((existB >> id) & 1) std::cout << "B";
				else if ((existr >> id) & 1) std::cout << "r";
				else if ((existb >> id) & 1) std::cout << "b";
				else std::cout << ".";
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
};
#endif