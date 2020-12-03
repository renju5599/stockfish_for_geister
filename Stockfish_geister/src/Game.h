#pragma once
#include <string>

namespace Game_
{
	using namespace std;
	
	char board[6][6];			//board[y][x] = {R:�����̐�, B:�����̐�, u:����̋�, '.':��}�X, ������y=5�̑��ɂ���
	char komaName[6][6];		//komaName[y][x] = {��M����, (y, x)�ɂ����̖��O}
	int rNum, uNum;				//�Ֆʂɂ���G�̐ԃR�}�̌�, �G�̃R�}�̌�
	
	const int WON = 1;
	const int LST = 2;
	const int DRW = 3;
	
	//s�̐擪��t �� true
	bool startWith(string &s, string t) {
		for (int i = 0; i < t.length(); i++) {
			if (i >= s.length() || s[i] != t[i]) return false;
		}
		return true;
	}

	//�Q�[���̏I������. dispFlag = true�ɂ����, ���ʂ�\���ł���B
	int isEnd(string s, bool dispFlag = true) {
		if (startWith(s, "WON")) {
			if (dispFlag) cout << "won" << endl;
			return WON;
		}
		if (startWith(s, "LST")) {
			if (dispFlag) cout << "lost" << endl;
			return LST;
		}
		if (startWith(s, "DRW")) {
			if (dispFlag) cout << "draw" << endl;
			return DRW;
		}
		return 0;
	}

	//�I���̌���
	string getEndInfo(string recv_msg) {
		if (startWith(recv_msg, "DRW")) return "draw";
		
		int i, Rnum = 0, Bnum = 0, rnum = 0, bnum = 0;
		const int baius = 4;
		
		for (i = 0; i < 16; i++) {
			int x = recv_msg[baius + 3 * i] - '0';
			int y = recv_msg[baius + 3 * i + 1] - '0';
			char type = recv_msg[baius + 3 * i + 2];
			
			if (0 <= x && x < 6 && 0 <= y && y < 6) {
				if (type == 'R') Rnum++;
				if (type == 'B') Bnum++;
				if (type == 'r') rnum++;
				if (type == 'b') bnum++;
			}
		}
		
		if (startWith(recv_msg, "WON")) {
			if (Rnum == 0) { return "won taken R"; }
			if (bnum == 0) { return "won taked b"; }
			return "won escaped B";
		}
		
		if (rnum == 0) { return "lost taked r"; }
		if (Bnum == 0) { return "lost taken B"; }
		return "lost escaped b";
	}
	
	//�{�[�h�̎�M
	void recvBoard(string msg) {
		int i, j;
		
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				board[i][j] = '.';
				komaName[i][j] = '.';
			}
		}
		
		const int baius = 4;
		rNum = 4;	//�G�̐Ԃ���̌�
		uNum = 0;
		
		for (i = 0; i < 16; i++) {
			int x = msg[baius + 3 * i] - '0';
			int y = msg[baius + 3 * i + 1] - '0';
			char type = msg[baius + 3 * i + 2];
			
			if (0 <= x && x < 6 && 0 <= y && y < 6) {
				if (type == 'R' || type == 'B' || type == 'u') {
					board[y][x] = type;
				}
				if (i < 8) {
					komaName[y][x] = (char)(i + 'A');
				}
				else {
					komaName[y][x] = (char)(i - 8 + 'a');
				}
			}
			else {
				if (type == 'r' && i >= 8) rNum--;
			}
			if (type == 'u') uNum++;
		}
	}
	
	//�R�}���h�̕ϊ�
	string move(int y, int x, int dir) {
		string moveStr = "NESW";
		string ret;
		
		ret += "MOV:";
		ret += komaName[y][x];
		ret += ",";
		ret += moveStr[dir];
		return ret;
	}
}