#ifndef GAME_GEISTER_H_INCLUDED
#define GAME_GEISTER_H_INCLUDED

#include <string>
#include "types.h"

//全部 position.h に移植した方がいいかも
namespace Game_
{

	extern char board[6][6];			//board[y][x] = {R:自分の赤, B:自分の青, u:相手の駒, '.':空マス, 自分はy=5の側にいる
	extern char komaName[6][6];		//komaName[y][x] = {受信時に, (y, x)にある駒の名前}
	extern int rNum, uNum;				//盤面にある敵の赤コマの個数, 敵のコマの個数

	const int WON = 1;
	const int LST = 2;
	const int DRW = 3;

	//sの先頭がt ⇔ true
	bool startWith(std::string& s, std::string t);

	//ゲームの終了判定. dispFlag = trueにすると, 結果を表示できる。
	int isEnd(std::string s, bool dispFlag);

	//ボードの受信
	//position役はposition.hに
	//こっちはkomanameとか用
	void recvBoard(std::string msg);

	//uci.cppに新しく書いた(MoveStr)
	////コマンドの変換
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

	//終了の原因
	std::string getEndInfo(std::string recv_msg);

}

namespace Red {

	extern int histCnt;
	extern char hist[350][6][6];	//R, B, u
	extern int eval[350][6][6];	//赤度
	extern bool existRed;


	//試合開始時に呼び出す
	void init();

	//自分が手を打ったときに呼び出す
	void myMove(Move mv);

	//2手目以降の自分手番の最初に呼び出す。
	void myTurn(char board[6][6]);

	//myMoveとかmyTurnとかを呼び出した直後に呼び出したい。
	//赤度evalが閾値以上になった赤の現在位置を、赤度が大きいものからリストアップ
	//実は最大のやつしか使ってなかったので下へ
	int listUpRed(int posY[], int posX[], int X);
	Square picUpRed(int X);
}
#endif