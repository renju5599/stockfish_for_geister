//�K�C�X�^�[�̒T��
//�s���S��񕔕�, ���S���K�C�X�^�[��2�i�K����Ȃ�B
//i�sj���(i>=0, j>=0)���}�Xi * 6 + j�Ƃ����B���̒E�o���̓}�X0, 5�B
//cornerId�c����0, �E��1, ����2, �E��3
#pragma once
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include "KanzenBoard.h"
#include "Game.h"
using namespace std;

class KanzenSearch
{
	int kiki[36 * 5];				//kiki[i * 5 + j] = �}�Xi����s����j�Ԗڂ̃}�X�̔ԍ�. (�Ȃ����-1)
	int INF;						//INF         = �]���l�̏�� (������-INF�ȏ�)
	int maxDepth;					//maxDepth    = �T���̐[���̍ő�l
	
public:

	KanzenSearch()	//�Q�[�����n�܂�O�̏���
	{
		INF = 100000000;
		int y, x, dir, i, j;
		int dy[4] = {-1, 0, 1, 0};
		int dx[4] = {0, 1, 0, -1};
		
		for (i = 0; i < 180; i++) kiki[i] = -1;
		for (y = 0; y < 6; y++) {
			for (x = 0; x < 6; x++) {
				i = y * 6 + x;
				j = 0;
				for (dir = 0; dir < 4; dir++) {
					int ny = y + dy[dir];
					int nx = x + dx[dir];
					if (0 <= ny && ny < 6 && 0 <= nx && nx < 6) {
						kiki[5 * i + j] = ny * 6 + nx;
						j++;
					}
				}
			}
		}
	}
	
	//board[i] = �}�Xi�̏��. ([i / 6]�s, i % 6��ځj
	//���S���B����Ԃ�K�ŁA���͑S���I�݂����ȔՖʂ��^������B
	pair<MoveCommand, int> think(string board, int maxDepth) {
		KanzenBoard bb;
		
		//�T���̐ݒ�
		this->maxDepth = maxDepth;
		
		//1��ŒE�o�ł��邩�H
		bb.toBitBoard(board);
		MoveCommand escapeTe = bb.getEscapeCommand(0);
		if (escapeTe.y >= 0) return pair<MoveCommand, int>(escapeTe, INF);
		
		//����̋������Ƃ������S���T�� (�[��0�T�����ɍőP����i�[�j
		int eval = negamax(bb, 0, -INF - 1, INF + 1);
		return pair<MoveCommand, int>(bestMove, eval);
	}
	
private:
	MoveCommand bestMove;	//�[��0�iR,B�𓮂�����ԁj�ɂ�����ŗǎ�
	
	//�T������ (�����K���FINF, �����K��-INF), �߂�l��(alpha, beta)�͈̔͂𒴂�����K���ɕԂ�
	int negamax(KanzenBoard bb, int depth, int alpha, int beta) {
		int player = depth % 2;
		int winPlayer = bb.getWinPlayer(player);
		if (winPlayer <= 1) return player == winPlayer ? INF - depth : -INF + depth;
		if (depth == maxDepth) return bb.evaluate(player);
		
		int from[32], to[32];
		int moveNum = bb.makeMoves(player, kiki, from, to);
		
		for (int i = 0; i < moveNum; i++) {
			KanzenBoard nextbb = bb;
			nextbb.move(from[i], to[i]);
			int res = -negamax(nextbb, depth + 1, -beta, -alpha);
			if (alpha < res) { alpha = res; if (depth == 0) { bestMove = MoveCommand::parse(from[i], to[i]); } }
			if (alpha >= beta) { return beta; }	//��cut
		}
		
		return alpha;
	}
};