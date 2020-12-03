#include "tcp.h"
#include "Game.h"
#include "Search.h"
#include "KanzenSearch.h"
#include <vector>
#include <map>
#include <ctime>
#include <algorithm>
#include <cassert>
#include <tuple>
#include <functional>
using namespace std;
using namespace tcp;
using namespace Game_;

//����, �ǂ������̔���
int dy[4] = {-1, 0, 1, 0};
int dx[4] = {0, 1, 0, -1};
bool isNige(char board[][6], MoveCommand te) {
	int ny = te.y + dy[te.dir];
	int nx = te.x + dx[te.dir];
	if (ny < 0 || ny >= 6 || nx < 0 || nx >= 6) return false;	//�E�o��́u�����v�ł͂Ȃ�
	if (board[ny][nx] == 'u') return false;			//�������́u�����v�ł͂Ȃ�

	int i;
	for (i = 0; i < 4; i++) {
		int y = te.y + dy[i];
		int x = te.x + dx[i];
		if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
			break;
		}
	}
	if (i == 4) { return false; }	//��������́u�������O�̃}�X�v�Ɨאڂ���}�X�ɑ���̋�Ȃ�������A�u�����v�ł͂Ȃ�

	for (i = 0; i < 4; i++) {
		int y = ny + dy[i];
		int x = nx + dx[i];
		if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
			break;
		}
	}
	if (i < 4) { return false; }	//��������́u����������̃}�X�v�Ɨאڂ���}�X�ɑ���̋��������A�u�����v�ł͂Ȃ�
	return true;	//�����ł���
}

bool isOikake(char board[][6], MoveCommand te) {
	int ny = te.y + dy[te.dir];
	int nx = te.x + dx[te.dir];
	if (ny < 0 || ny >= 6 || nx < 0 || nx >= 6) return false;	//�E�o��́u�ǂ������v�ł͂Ȃ�
	if (board[ny][nx] == 'u') return false;			//�������́u�ǂ������v�ł͂Ȃ�

	int i;
	for (i = 0; i < 4; i++) {
		int y = te.y + dy[i];
		int x = te.x + dx[i];
		if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
			break;
		}
	}
	if (i < 4) { return false; }	//��������́u�������O�̃}�X�v�Ɨאڂ���}�X�ɑ���̋��������u�ǂ������v�ł͂Ȃ�

	for (i = 0; i < 4; i++) {
		int y = ny + dy[i];
		int x = nx + dx[i];
		if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
			break;
		}
	}
	if (i == 4) { return false; }	//��������́u����������̃}�X�v�Ɨאڂ���}�X�ɑ���̋�Ȃ�������u�ǂ������v�ł͂Ȃ�
	return true;	//�u�ǂ������v�ł���
}

//����, �ǂ������̉�
int nigeR, nigeB, oikakeR, oikakeB;
void AddNigeR(bool printLog = false) { nigeR++; if (printLog) cout << "Add nigeR" << endl; }
void AddNigeB(bool printLog = false) { nigeB++; if (printLog) cout << "Add nigeB" << endl; }
void AddOikakeR(bool printLog = false) { oikakeR++; if (printLog) cout << "Add oikakeR" << endl; }
void AddOikakeB(bool printLog = false) { oikakeB++; if (printLog) cout << "Add oikakeB" << endl; }

//BEGIN: �ԓx�𐄒肷��n��
namespace red {
	int histCnt;
	char hist[350][6][6];	//R, B, u
	double eval[350][6][6];	//�ԓx
	
	void moveHist(char prev[6][6], char now[6][6], MoveCommand mv);
	void moveEval(double prev[6][6], double now[6][6], MoveCommand mv);
	MoveCommand detectMove(char prev[6][6], char now[6][6]);
	int toDir(int y, int x, int ny, int nx);
	
	//�����I����ɌĂяo��
	void saveGame() {
		int i, j, k;
		for (i = 0; i < histCnt; i++) {
			for (j = 0; j < 6; j++) {
				for (k = 0; k < 6; k++) {
					cout << hist[i][j][k];
				}
				cout << endl;
			}
			cout << endl;
			
			for (j = 0; j < 6; j++) {
				for (k = 0; k < 6; k++) {
					cout << eval[i][j][k] << " ";
				}
				cout << endl;
			}
			cout << endl;
		}
	}
	
	//�����J�n���ɌĂяo��
	void initGame(char initBoard[6][6]) {
		int i, j;
		
		histCnt = 0;
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				hist[0][i][j] = initBoard[i][j];
				eval[0][i][j] = 0;
			}
		}
		histCnt++;
	}
	
	//���������ł����Ƃ��ɌĂяo��
	void myMove(MoveCommand mv) {
		moveHist(hist[histCnt - 1], hist[histCnt], mv);
		moveEval(eval[histCnt - 1], eval[histCnt], mv);
		histCnt++;
	}
	
	//2��ڈȍ~�̎�����Ԃ̍ŏ��ɌĂяo���B
	void myTurn(char board[6][6]) {
		int i, j;
		
		for (i = 0; i < 6; i++)
			for (j = 0; j < 6; j++)
				hist[histCnt][i][j] = board[i][j];
		histCnt++;
		
		MoveCommand mv = detectMove(hist[histCnt - 2], hist[histCnt - 1]);
		moveEval(eval[histCnt - 2], eval[histCnt - 1], mv);
		int ny = mv.y + dy[mv.dir];
		int nx = mv.x + dx[mv.dir];
		
		char block[6][6];
		int prevMyRed = 0;
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				char c = hist[histCnt - 2][i][j];
				if (c == 'R' || c == 'B') block[i][j] = 'u';
				else block[i][j] = '.';
				if (c == 'R') prevMyRed++;
			}
		}
		
		int weightOikake = 5;
		int weightOikakePinti = 1;	//���肪�s���`�ȂƂ��A�킯�킩���s���������Ȃ̂ŁA����̐M�����߂�
		if (isOikake(block, mv)) {
			cerr << "Enemy(" << ny << ", " << nx << ") is Oikaked." << endl;
			if (prevMyRed == 1) {
				cerr << "����̓s���`������" << endl;
				eval[histCnt - 1][ny][nx] += weightOikakePinti;
			}
			else {
				cerr << "����͗]�T������" << endl;
				eval[histCnt - 1][ny][nx] += weightOikake;
			}
		}
		
		//���肪��������������āA���ꂪ�������玩�����ǂ��撣���Ă��K��������Ƃ��A�Ԃ��Ǝv����
		//���̂Ă�B
		//�{���͂����Ɓu���葤�̕K����T���v�������������������ǁA���Ԃ��Ȃ��̂Ŏ蔲���ŁB
		int weightHairi = 1000;
		if ((mv.y == 5 && mv.x == 0) || (mv.y == 5 && mv.x == 5)) {	//�����E�o���Ȃ���������Ԃ��]
			cerr << "Enemy(" << ny << ", " << nx << ") �͒E�o���Ȃ���������Ԃ��]" << endl;
			eval[histCnt - 1][ny][nx] += weightHairi;
		}
		
		//���E�o���ɂ��鑊���A���O�ɓ������Ă������̂łȂ���΁A��
		if (hist[histCnt - 1][5][0] == 'u' && !(ny == 5 && nx == 0)) {
			cerr << "Enemy(" << 5 << ", " << 0 << ") �͗��܂��Ă邩��Ԃ��]" << endl;
			eval[histCnt - 1][5][0] += weightHairi;
		}
		if (hist[histCnt - 1][5][5] == 'u' && !(ny == 5 && nx == 5)) {
			cerr << "Enemy(" << 5 << ", " << 5 << ") �͗��܂��Ă邩��Ԃ��]" << endl;
			eval[histCnt - 1][5][5] += weightHairi;
		}
	}
	
	//myMove�Ƃ�myTurn�Ƃ����Ăяo��������ɌĂяo�������B
	//�ԓxeval��臒l�ȏ�ɂȂ����Ԃ̌��݈ʒu���A�ԓx���傫�����̂��烊�X�g�A�b�v
	int listUpRed(int posY[], int posX[], int X) {
		int i, j;
		
		typedef tuple<double, int, int> T;
		vector<T> vec;
		
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				if (eval[histCnt - 1][i][j] >= X) {
					vec.push_back(T(eval[histCnt - 1][i][j], i, j));
				}
			}
		}
		
		sort(vec.begin(), vec.end(), greater<T>());
		for (i = 0; i < vec.size(); i++) {
			posY[i] = get<1>(vec[i]);
			posX[i] = get<2>(vec[i]);
		}
		return vec.size();
	}

	void moveHist(char prev[6][6], char now[6][6], MoveCommand mv) {
		int y = mv.y;
		int x = mv.x;
		int ny = mv.y + dy[mv.dir];
		int nx = mv.x + dx[mv.dir];
		int i, j;
		
		for (i = 0; i < 6; i++)
			for (j = 0; j < 6; j++)
				now[i][j] = prev[i][j];
		
		char color = prev[y][x];	//R, B, u
		now[ny][nx] = color;
		now[y][x] = '.';
	}
	
	void moveEval(double prev[6][6], double now[6][6], MoveCommand mv) {
		int y = mv.y;
		int x = mv.x;
		int ny = mv.y + dy[mv.dir];
		int nx = mv.x + dx[mv.dir];
		int i, j;
		
		for (i = 0; i < 6; i++)
			for (j = 0; j < 6; j++)
				now[i][j] = prev[i][j];
		
		double tmp = prev[y][x];
		now[ny][nx] = tmp;
		now[y][x] = 0;
	}
	
	MoveCommand detectMove(char prev[6][6], char now[6][6]) {
		int i, j;
		int cnt = 0;
		int posY[2], posX[2];
		
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				if (prev[i][j] != now[i][j]) {
					posY[cnt] = i;
					posX[cnt] = j;
					cnt++;
				}
			}
		}
		assert(cnt == 2);
		
		int y, x, ny, nx;
		if (now[posY[0]][posX[0]] == '.') {
			y = posY[0];
			x = posX[0];
			ny = posY[1];
			nx = posX[1];
		}
		else {
			y = posY[1];
			x = posX[1];
			ny = posY[0];
			nx = posX[0];
		}
		
		int dir = toDir(y, x, ny, nx);
		return MoveCommand(y, x, dir);
	}
	
	int toDir(int y, int x, int ny, int nx) {
		int dir;
		for (dir = 0; dir < 4; dir++) {
			if (dy[dir] == ny - y && dx[dir] == nx - x) {
				break;
			}
		}
		assert(dir <= 3);
		return dir;
	}
}
//END

//�w��������߂� (Search.h��, (�Ֆ�, pnum)�������Ȃ�K���������Ԃ��A���S���Y���Ȃ̂ŁA���������Ă����Ȃ��j
Search searchObj;
KanzenSearch kanzenObj;
clock_t sumThinkTime = 0;
int maxDepth;		//�T���̐[��(�����F5�`6�j

//����ȊO�S���I
pair<MoveCommand, int> thinkKanzen(int X) {
	int i, j;
	
	int posY[8], posX[8];
	int cnt = red::listUpRed(posY, posX, X);
	if (cnt == 0) return pair<MoveCommand, int>(MoveCommand(-1, -1, -1), 0);
	
	string s;
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			if (i == posY[0] && j == posX[0]) {
				s += "r";
			}
			else if (board[i][j] == 'u') {
				s += "b";
			}
			else {
				s += board[i][j];
			}
		}
	}
	return kanzenObj.think(s, maxDepth);
}

//����
pair<MoveCommand, int> thinkPurple() {
	int i, j;
	
	string s;
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 6; j++) {
			s += board[i][j];
		}
	}
	int pnum = Game_::uNum - Game_::rNum;
	BitBoard bb;
	bb.toBitBoard(s);
	return searchObj.think(bb, pnum, maxDepth);
}

//������߂�
pair<MoveCommand, int> thinkMove() {
	pair<MoveCommand, int> resK, resP;
	resK = thinkKanzen(1000);
	if (resK.first.y >= 0) return resK;
	
	if (Game_::rNum >= 1) {
		resP = thinkPurple();
		if (resP.second >= -searchObj.INF / 2) return resP;
	}
	
	resK = thinkKanzen(1000);
	if (resK.first.y >= 0) return resK;
	return resP;
}

//������߂�̂ƁA�����ȏ���
string solve(int turnCnt) {
	//���Ԍv���J�n
	clock_t startTime = clock();
	
	//�Ԃ炵���̍X�V
	if (turnCnt == 0) {
		red::initGame(board);
	}
	else {
		red::myTurn(board);
		int i, j;
		cerr << "�ԓx" << endl;
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 6; j++) {
				cerr << red::eval[red::histCnt - 1][i][j] << " ";
			}
			cerr << endl;
		}
	}
	
	//������߂�
	pair<MoveCommand, int> res = thinkMove();
	MoveCommand te = res.first;
	
	//�v�l����
	sumThinkTime += clock() - startTime;
	
	//���̍X�V
	if (isNige(board, te)) { if (board[te.y][te.x] == 'R') AddNigeR(true); else if (board[te.y][te.x] == 'B') AddNigeB(true); else assert(0); }
	if (isOikake(board, te)) { if (board[te.y][te.x] == 'R') AddOikakeR(true); else if (board[te.y][te.x] == 'B') AddOikakeB(true); else assert(0); }
	red::myMove(te);
	
	//�\��
	cerr << "�I����(" << te.y << ", " << te.x << ", " << te.dir << "), �]���l = " << res.second << endl;
	return move(te.y, te.x, te.dir);
}

//��̏����z�u
string initRedName;
void setInitRedName(int allNum = 0, int redNum = 0) {
	if (allNum == 8) return;
	int ransu = rand() % (8 - allNum);
	if (ransu < 4 - redNum) {
		initRedName += (char)('A' + allNum);
		setInitRedName(allNum + 1, redNum + 1);
	}
	else {
		setInitRedName(allNum + 1, redNum);
	}
}

map<string, int> endInfo;

int playGame(int port = -1, string destination = "") {
	int dstSocket;

	if (!openPort(dstSocket, port, destination)) return 0;
	initRedName = "";
	setInitRedName();

	myRecv(dstSocket);							//SET ?�̎�M
	mySend(dstSocket, "SET:" + initRedName);	//SET:EFGH�̂悤�ɓ��� (����, [\r][\n][\0]�𖖔��ɂ��đ��M)
	myRecv(dstSocket);							//OK, NG�̎�M
	
	int turnCnt = 0;
	int res;
	string recv_msg;
	
	while (1) {
		recv_msg = myRecv(dstSocket);	//�Ֆʂ̎�M
		res = isEnd(recv_msg);
		if (res) break;					//�I������
		recvBoard(recv_msg);			//���z�u
		string mv = solve(turnCnt);		//�v�l
		mySend(dstSocket, mv);			//�s���̑��M
		myRecv(dstSocket);				//ACK�̎�M
		turnCnt += 2;
	}
	
	//�I���̌���(Game.h)
	string s = getEndInfo(recv_msg);
	if (endInfo.find(s) == endInfo.end()) endInfo[s] = 0;
	endInfo[s]++;

	closePort(dstSocket);
	//red::saveGame();
	return res;
}

int main() {

	int n, port; string destination;

	srand((unsigned)time(NULL));

	bb::prepare();
	kbb::prepare();
	cout << "�ΐ�� �|�[�g�ԍ� IP�A�h���X �T���[������́�" << endl;
	cin >> n >> port >> destination >> maxDepth;
	bb::weight1 = 1000;
	bb::weight2 = 1;
	kbb::weight1 = 1000;
	kbb::weight2 = 1;
	
	nigeR = nigeB = oikakeR = oikakeB = 0;	//������, �ǂ������񐔂̏�����

	int win = 0, draw = 0, lose = 0;
	bool noCountIfDraw = false;
	for (int i = 0; i < n; i++) {
		int nR = nigeR, nB = nigeB, oR = oikakeR, oB = oikakeB;
		int res = playGame(port, destination);
		if (res == Game_::WON) { win++; }
		if (res == Game_::DRW) { draw++; if (noCountIfDraw) { nigeR = nR; nigeB = nB; oikakeR = oR; oikakeB = oB; } }
		if (res == Game_::LST) { lose++; }
		Sleep(500);
	}

	cout << "(����, ��������, ����) = (" << win << ", " << draw << ", " << lose << ")" << endl;

	cout << "nige = " << nigeR + nigeB << endl;
	cout << "nigeR = " << nigeR << endl;
	cout << "nigeB = " << nigeB << endl;
	cout << endl;
	cout << "oikake = " << oikakeR + oikakeB << endl;
	cout << "oikakeR = " << oikakeR << endl;
	cout << "oikakeB = " << oikakeB << endl;

	cout << endl;
	for (map<string, int>::iterator it = endInfo.begin(); it != endInfo.end(); it++) {
		cout << it->first << " : " << it->second << "[counts]" << endl;
	}

	cout << endl;
	cout << "���v�v�l���� = " << (double)sumThinkTime / CLOCKS_PER_SEC << "[sec]" << endl;
	return 0;
}



/* KanzenBoard 173~
int evaluate(int teban) {
		int s0 = kbb::weight1 * kbb::bitCount(existB) - kbb::weight2 * kbb::myGoalDist(existB | existR);
		int s1 = kbb::weight1 * kbb::bitCount(existb) - kbb::weight2 * kbb::yourGoalDist(existb | existr);
		if (teban == 0) return s0 - s1;
		return s1 - s0;
	}
*/