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


#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <ctime>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

#define NOMINMAX
#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>

#include "types.h"
#include "Game_geister.h"

using namespace std;

extern vector<string> setup_bench(const Position&, istream&);

namespace {

  // FEN string of the initial position, normal chess
  //const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  //const char* StartFEN = "1g4g1/2uuuu2/2uuuu2/8/8/2RRRR2/2BBBB2/1G4G1 w KQkq - 0 1";
  //const char* StartFEN = "MOV?01B99b99b99b04R99r99r99r05u99b99b99b50u99r99r99r";
  const char* StartFEN = "MOV?04B24B35B99r15B01R32R99r54u99r12u99r43u30u20u10u";

  // position() is called when engine receives the "position" UCI command.
  // The function sets up the position described in the given FEN string ("fen")
  // or the starting position ("startpos") and then makes the moves given in the
  // following move list ("moves").

  void position(Position& pos, istringstream& is, StateListPtr& states) {

    Move m;
    string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token; // Consume "moves" token if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
    pos.set(fen, Options["UCI_Chess960"], &states->back(), Threads.main());

    // Parse move list (if any)
    while (is >> token && (m = UCI::to_move(pos, token)) != MOVE_NONE)
    {
        states->emplace_back();
        pos.do_move(m, states->back());
    }
  }

  // trace_eval() prints the evaluation for the current position, consistent with the UCI
  // options set so far.

  void trace_eval(Position& pos) {

    StateListPtr states(new std::deque<StateInfo>(1));
    Position p;
    p.set(pos.fen(), Options["UCI_Chess960"], &states->back(), Threads.main());

    //Eval::NNUE::verify();

    //sync_cout << "\n" << Eval::trace(p) << sync_endl;
  }


  // setoption() is called when engine receives the "setoption" UCI command. The
  // function updates the UCI option ("name") to the given value ("value").

  void setoption(istringstream& is) {

    string token, name, value;

    is >> token; // Consume "name" token

    // Read option name (can contain spaces)
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;

    // Read option value (can contain spaces)
    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    if (Options.count(name))
        Options[name] = value;
    else
        sync_cout << "No such option: " << name << sync_endl;
  }


  // go() is called when engine receives the "go" UCI command. The function sets
  // the thinking time and other parameters from the input string, then starts
  // the search.

  void go(Position& pos, istringstream& is, StateListPtr& states) {

    Search::LimitsType limits;
    string token;
    bool ponderMode = false;

    limits.startTime = now(); // As early as possible!

    while (is >> token)
        if (token == "searchmoves") // Needs to be the last command on the line
            while (is >> token)
                limits.searchmoves.push_back(UCI::to_move(pos, token));

        else if (token == "wtime")     is >> limits.time[WHITE];
        else if (token == "btime")     is >> limits.time[BLACK];
        else if (token == "winc")      is >> limits.inc[WHITE];
        else if (token == "binc")      is >> limits.inc[BLACK];
        else if (token == "movestogo") is >> limits.movestogo;
        else if (token == "depth")     is >> limits.depth;
        else if (token == "nodes")     is >> limits.nodes;
        else if (token == "movetime")  is >> limits.movetime;
        else if (token == "mate")      is >> limits.mate;
        else if (token == "perft")     is >> limits.perft;
        else if (token == "infinite")  limits.infinite = 1;
        else if (token == "ponder")    ponderMode = true;

    Threads.start_thinking(pos, states, limits, ponderMode);
  }


  // bench() is called when engine receives the "bench" command. Firstly
  // a list of UCI commands is setup according to bench parameters, then
  // it is run one by one printing a summary at the end.

  void bench(Position& pos, istream& args, StateListPtr& states) {

    string token;
    uint64_t num, nodes = 0, cnt = 1;

    vector<string> list = setup_bench(pos, args);
    num = count_if(list.begin(), list.end(), [](string s) { return s.find("go ") == 0 || s.find("eval") == 0; });

    TimePoint elapsed = now();

    for (const auto& cmd : list)
    {
        istringstream is(cmd);
        is >> skipws >> token;

        if (token == "go" || token == "eval")
        {
            cerr << "\nPosition: " << cnt++ << '/' << num << " (" << pos.fen() << ")" << endl;
            if (token == "go")
            {
               go(pos, is, states);
               Threads.main()->wait_for_search_finished();
               nodes += Threads.nodes_searched();
            }
            else
               trace_eval(pos);
        }
        else if (token == "setoption")  setoption(is);
        else if (token == "position")   position(pos, is, states);
        else if (token == "ucinewgame") { Search::clear(); elapsed = now(); } // Search::clear() may take some while
    }

    elapsed = now() - elapsed + 1; // Ensure positivity to avoid a 'divide by zero'

    dbg_print(); // Just before exiting

    cerr << "\n==========================="
         << "\nTotal time (ms) : " << elapsed
         << "\nNodes searched  : " << nodes
         << "\nNodes/second    : " << 1000 * nodes / elapsed << endl;
  }

  // The win rate model returns the probability (per mille) of winning given an eval
  // and a game-ply. The model fits rather accurately the LTC fishtest statistics.
  int win_rate_model(Value v, int ply) {

     // The model captures only up to 240 plies, so limit input (and rescale)
     double m = std::min(240, ply) / 64.0;

     // Coefficients of a 3rd order polynomial fit based on fishtest data
     // for two parameters needed to transform eval to the argument of a
     // logistic function.
     double as[] = {-8.24404295, 64.23892342, -95.73056462, 153.86478679};
     double bs[] = {-3.37154371, 28.44489198, -56.67657741,  72.05858751};
     double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
     double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

     // Transform eval to centipawns with limited range
     double x = std::clamp(double(100 * v) / RePawnValueEg, -1000.0, 1000.0);

     // Return win rate in per mille (rounded to nearest)
     return int(0.5 + 1000 / (1 + std::exp((a - x) / b)));
  }

} // namespace


/// UCI::loop() waits for a command from stdin, parses it and calls the appropriate
/// function. Also intercepts EOF from stdin to ensure gracefully exiting if the
/// GUI dies unexpectedly. When called with some command line arguments, e.g. to
/// run 'bench', once the command is executed the function returns immediately.
/// In addition to the UCI ones, also some additional debug commands are supported.

void UCI::loop(int argc, char* argv[]) {

  Position pos;
  string token, cmd;
  StateListPtr states(new std::deque<StateInfo>(1));

  pos.set(StartFEN, false, &states->back(), Threads.main());

  for (int i = 1; i < argc; ++i)
      cmd += std::string(argv[i]) + " ";

  do {
      if (argc == 1 && !getline(cin, cmd)) // Block here waiting for input or EOF
          cmd = "quit";

      istringstream is(cmd);

      token.clear(); // Avoid a stale if getline() returns empty or blank line
      is >> skipws >> token;

      if (    token == "quit"
          ||  token == "stop")
          Threads.stop = true;

      // The GUI sends 'ponderhit' to tell us the user has played the expected move.
      // So 'ponderhit' will be sent if we were told to ponder on the same move the
      // user has played. We should continue searching but switch from pondering to
      // normal search.
      else if (token == "ponderhit")
          Threads.main()->ponder = false; // Switch to normal search

      else if (token == "uci")
          sync_cout << "id name " << engine_info(true)
                    << "\n"       << Options
                    << "\nuciok"  << sync_endl;

      else if (token == "setoption")  setoption(is);
      else if (token == "go")         go(pos, is, states);
      else if (token == "position")   position(pos, is, states);
      else if (token == "ucinewgame") Search::clear();
      else if (token == "isready")    sync_cout << "readyok" << sync_endl;

      // Additional custom non-UCI commands, mainly for debugging.
      // Do not use these commands during a search!
      else if (token == "flip")     pos.flip();
      else if (token == "bench")    bench(pos, is, states);
      else if (token == "d")        sync_cout << pos << sync_endl;
      else if (token == "eval")     trace_eval(pos);
      else if (token == "compiler") sync_cout << compiler_info() << sync_endl;
      else
          sync_cout << "Unknown command: " << cmd << sync_endl;

  } while (token != "quit" && argc == 1); // Command line args are one-shot
}


/// UCI::value() converts a Value to a string suitable for use with the UCI
/// protocol specification:
///
/// cp <x>    The score from the engine's point of view in centipawns.
/// mate <y>  Mate in y moves, not plies. If the engine is getting mated
///           use negative values for y.

string UCI::value(Value v) {

  assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

  stringstream ss;

  if (abs(v) < VALUE_MATE_IN_MAX_PLY)
      ss << "cp " << v * 100 / RePawnValueEg;
  else
      ss << "mate " << (v > 0 ? VALUE_MATE - v + 1 : -VALUE_MATE - v) / 2;

  return ss.str();
}


/// UCI::wdl() report WDL statistics given an evaluation and a game ply, based on
/// data gathered for fishtest LTC games.

string UCI::wdl(Value v, int ply) {

  stringstream ss;

  int wdl_w = win_rate_model( v, ply);
  int wdl_l = win_rate_model(-v, ply);
  int wdl_d = 1000 - wdl_w - wdl_l;
  ss << " wdl " << wdl_w << " " << wdl_d << " " << wdl_l;

  return ss.str();
}


/// UCI::square() converts a Square to a string in algebraic notation (g1, a7, etc.)

std::string UCI::square(Square s) {
  return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}


/// UCI::move() converts a Move to a string in coordinate notation (g1f3, a7a8q).
/// The only special case is castling, where we print in the e1g1 notation in
/// normal chess mode, and in e1h1 notation in chess960 mode. Internally all
/// castling moves are always encoded as 'king captures rook'.

string UCI::move(Move m, bool chess960) {

  Square from = from_sq(m);
  Square to = to_sq(m);

  if (m == MOVE_NONE)
      return "(none)";

  if (m == MOVE_NULL)
      return "0000";

  //if (type_of(m) == CASTLING && !chess960)
  //    to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

  string move = UCI::square(from) + UCI::square(to);

  //if (type_of(m) == PROMOTION)
  //    move += " pnbrqk"[promotion_type(m)];

  return move;
}


/// UCI::to_move() converts a string representing a move in coordinate notation
/// (g1f3, a7a8q) to the corresponding legal Move, if any.

Move UCI::to_move(const Position& pos, string& str) {

  if (str.length() == 5) // Junior could send promotion piece in uppercase
      str[4] = char(tolower(str[4]));

  for (const auto& m : MoveList<LEGAL>(pos))
      if (str == UCI::move(m, pos.is_chess960()))
          return m;

  return MOVE_NONE;
}



namespace {
  bool openPort(int& dstSocket, int port = -1, string dest = "")
  {
    // IP アドレス，ポート番号，ソケット，sockaddr_in 構造体
    char destination[32];
    sockaddr_in dstAddr;
    int PORT;

    // Windows の場合
    WSADATA data;
    WSAStartup(MAKEWORD(2, 0), &data);

    // 相手先アドレスの入力と送る文字の入力
    if (port == -1) {
      printf("ポート番号は？：");
      scanf("%d", &PORT);
    }
    else {
      PORT = port;
    }

    if (dest.length() == 0) {
      printf("サーバーマシンのIPは？:");
      scanf("%s", destination);
    }
    else {
      for (int i = 0; i < dest.size(); i++) destination[i] = dest[i];
      destination[dest.size()] = '\0';
    }

    // sockaddr_in 構造体のセット
    memset(&dstAddr, 0, sizeof(dstAddr));
    dstAddr.sin_port = htons(PORT);
    dstAddr.sin_family = AF_INET;
    //dstAddr.sin_addr.s_addr = inet_addr(destination);
    assert(inet_pton(dstAddr.sin_family, destination, &dstAddr.sin_addr.S_un.S_addr) == 1);

    // ソケットの生成
    dstSocket = socket(AF_INET, SOCK_STREAM, 0);

    //接続
    if (connect(dstSocket, (struct sockaddr*)&dstAddr, sizeof(dstAddr)) && WSAGetLastError() != WSAEWOULDBLOCK) {
      printf("%d\n", WSAGetLastError());
      printf("%s　に接続できませんでした\n", destination);
      return false;
    }
    printf("%s に接続しました\n", destination);

    return true;
  }

  void closePort(int& dstSocket)
  {
    // Windows でのソケットの終了
    closesocket(dstSocket);
    WSACleanup();
  }

  string setInitRedName(int allNum = 0, int redNum = 0, string initRedName = "") {
    if (allNum == 8) return initRedName;
    int ransu = rand() % (8 - allNum);
    if (ransu < 4 - redNum) {
      return setInitRedName(allNum + 1, redNum + 1, initRedName + (char)('A' + allNum));
    }
    else {
      return setInitRedName(allNum + 1, redNum, initRedName);
    }
  }


  void go(Position& pos, StateListPtr& states) {

    Search::LimitsType limits;
    string token;
    bool ponderMode = false;

    limits.startTime = now(); // As early as possible!

    //while (is >> token)
      //if (token == "searchmoves") // Needs to be the last command on the line
        //while (is >> token)
          //limits.searchmoves.push_back(UCI::to_move(pos, token));

      //else if (token == "wtime")     is >> limits.time[WHITE];
      //else if (token == "btime")     is >> limits.time[BLACK];
      //else if (token == "winc")      is >> limits.inc[WHITE];
      //else if (token == "binc")      is >> limits.inc[BLACK];
      //else if (token == "movestogo") is >> limits.movestogo;
      //movestogoがわからないので保留
      //limits.movestogo = 50;

      //else if (token == "depth")     is >> limits.depth;
      //limits.depth = 5;
      //else if (token == "nodes")     is >> limits.nodes;
      //limits.nodes = 250000;
      //ここは適当

      //else if (token == "movetime")  is >> limits.movetime;
      limits.movetime = 1000;
      //else if (token == "mate")      is >> limits.mate;
      limits.mate = VALUE_MATE;  //よくわからん

      //else if (token == "perft")     is >> limits.perft;
      //perftがわからない

      //else if (token == "infinite")  limits.infinite = 1;

      //else if (token == "ponder")    ponderMode = true;
      //ponderをtrueにするべきなのかわからない

    Threads.start_thinking(pos, states, limits, ponderMode);
    //pos.print();
  }


}//namespace

namespace Game_ {
  char board[6][6];			//board[y][x] = {R:自分の赤, B:自分の青, u:相手の駒, '.':空マス, 自分はy=5の側にいる
  char komaName[6][6];		//komaName[y][x] = {受信時に, (y, x)にある駒の名前}
  int rNum, uNum, bNum;				//盤面にある敵の赤コマの個数, 敵のコマの個数
  int lost_pattern;
  int eval_pattern;
  int myrNum, mybNum;
}

//sの先頭がt ⇔ true
bool Game_::startWith(string& s, string t) {
  for (int i = 0; i < t.length(); i++) {
    if (i >= s.length() || s[i] != t[i]) return false;
  }
  return true;
}

//ゲームの終了判定. dispFlag = trueにすると, 結果を表示できる。
int Game_::isEnd(string s, bool dispFlag = true) {
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

//ボードの受信
void Game_::recvBoard(string msg) {
  int i, j;

  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      Game_::board[i][j] = '.';
      Game_::komaName[i][j] = '.';
    }
  }

  const int baius = 4;
  Game_::rNum = 4;	//敵の赤い駒の個数
  Game_::uNum = 0;
  Game_::myrNum = 4;
  Game_::mybNum = 4;

  for (i = 0; i < 16; i++) {
    int x = msg[baius + 3 * i] - '0';
    int y = msg[baius + 3 * i + 1] - '0';
    char type = msg[baius + 3 * i + 2];

    if (0 <= x && x < 6 && 0 <= y && y < 6) {
      if (type == 'R' || type == 'B' || type == 'u') {
        Game_::board[y][x] = type;
      }
      if (i < 8) {
        Game_::komaName[y][x] = (char)(i + 'A');
      }
      else {
        Game_::komaName[y][x] = (char)(i - 8 + 'a');
      }
    }
    else {
      if (type == 'r' && i >= 8) Game_::rNum--;
      if (type == 'r' && i < 8) Game_::myrNum--;
      if (type == 'b' && i < 8) Game_::mybNum--;
    }
    if (type == 'u') Game_::uNum++;
  }
  Game_::bNum = Game_::uNum - Game_::rNum;
}

//終了の原因
string Game_:: getEndInfo(string recv_msg) {
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


namespace tcp {
  int dstSocket;
}//namespace tcp

void tcp::mySend(int dstSocket, string str = "")
{
  if (str.length() == 0) {	//null文字なら「入力」を受け付ける
    cin >> str;
  }
  if (str.length() < 2 || str[str.length() - 2] != '\r' || str[str.length() - 1] != '\n') {	//\r\nが末尾になければ追加
    str += '\r';
    str += '\n';
  }
  int byte = send(dstSocket, str.c_str(), str.length(), 0);	//文字列を送信
  sync_cout << str << sync_endl;
  if (byte <= 0) {
    cout << "送信エラー" << endl;
  }
}

string tcp::myRecv(int dstSocket)
{
  char buffer[10];
  string msg;

  do {
    int byte = recv(dstSocket, buffer, 1, 0);	//文字を受信

    if (byte == 0) break;
    if (byte < 0) { cout << "受信に失敗しました" << endl; return msg; }

    msg += buffer[0];
  } while (msg.length() < 2 || msg[msg.length() - 2] != '\r' || msg[msg.length() - 1] != '\n');

  cout << "受信 = " << msg << endl;

  return msg;
}

string tcp::MoveStr(Move mv) {
  string ret;
  Square from = from_sq(mv);
  Square to = to_sq(mv);
  int x = file_of(from) - 1;
  int y = rank_of(from) - 1;

  ret += "MOV:";
  ret += Game_::komaName[y][x];
  ret += ",";
  if (from + NORTH == to) ret += 'S';
  else if (from + EAST == to) ret += 'E';
  else if (from + WEST == to) ret += 'W';
  else if (from + SOUTH == to) ret += 'N';
  else {
    std::cout << file_of(from) << ',' << rank_of(from) << ' ';
    std::cout << file_of(to) << ',' << rank_of(to) << endl;
    //assert(false);
  }
  return ret;
}


//UCI::loop の代わりになるように動かそうと思っている
int tcp::playGame(int n, int port = -1, string destination = "") {
  
  int total = 0;

  //ファイル出力の奴ら
  string filename = "result.txt";
  ofstream wfile;
  wfile.open(filename, std::ios::out);

  while (n--) {

    if (!openPort(dstSocket, port, destination)) return 0;
    srand((unsigned)time(NULL));
    string initRedName = setInitRedName();
    tcp::myRecv(dstSocket);							//SET ?の受信
    tcp::mySend(dstSocket, "SET:" + initRedName);	//SET:EFGHのように入力 (して, [\r][\n][\0]を末尾につけて送信)
    tcp::myRecv(dstSocket);							//OK, NGの受信

    Search::clear();

    Game_::lost_pattern = rand() % 2;
    //Game_::lost_pattern = 1;
    Game_::eval_pattern = rand() % 2;
    //Game_::eval_pattern = 1;

    int res;
    string recv_msg;

    Position pos;
    StateListPtr states(new deque<StateInfo>(1));

    pos.set(StartFEN, false, &states->back(), Threads.main());
    Red::init();
    
    while (1) {

      recv_msg = tcp::myRecv(dstSocket);	//盤面の受信
      //recv_msg = StartFEN;

      res = Game_::isEnd(recv_msg);
      if (res) break;					//終了判定

      //position.cppに recvBoardを移植して、Threadとかをいじれるようにする？
      Game_::recvBoard(recv_msg);			//駒を配置
      states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
      pos.set(recv_msg, Options["UCI_Chess960"], &states->back(), Threads.main());
      Red::myTurn(Game_::board, pos);
      if (Red::bare)
        cerr << "バレている" << endl;
      cerr << "赤度" << endl;
      for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
          cerr << Red::eval[Red::histCnt - 1][i][j] << " ";
        }
        cerr << endl;
      }
      
      Square sq_red = Red::picUpRed(1000);
      //sq_red = SQUARE_ZERO;
      //sq_red += 1 * EAST;
      //sq_red += 5 * NORTH;
      if (Red::existRed = (sq_red != SQ_NONE)) {
        std::cout << Game_::komaName[rank_of(sq_red) - 1][file_of(sq_red) - 1] << " が赤っぽい" << std::endl;
        pos.piece_change(B_RED, sq_red);
      }

      //多分大丈夫そう
      //string mv = solve(turnCnt);		//思考
      if (pos.piece_on(SQ_B2) == W_BLUE) {
        Move mv = make_move(SQ_B2, SQ_B1);
        Red::myMove(mv);
        tcp::mySend(tcp::dstSocket, tcp::MoveStr(mv));			//行動の送信
      }
      else if (pos.piece_on(SQ_G2) == W_BLUE) {
        Move mv = make_move(SQ_G2, SQ_G1);
        Red::myMove(mv);
        tcp::mySend(tcp::dstSocket, tcp::MoveStr(mv));			//行動の送信
      }
      else
        go(pos, states);

      tcp::myRecv(tcp::dstSocket);				//ACKの受信
      //break;
    }

    //終了の原因(Game.h)
    string s = Game_::getEndInfo(recv_msg);
    //if (endInfo.find(s) == endInfo.end()) endInfo[s] = 0;
    //endInfo[s]++;
    s += (Red::existRed ? " Red" : " Purple");
    s += " " + initRedName;
    s += " R.";
    s += (char)('0' + Game_::rNum);
    s += " losP.";
    s += (char)('0' + Game_::lost_pattern);
    s += " evP.";
    s += (char)('0' + Game_::eval_pattern);
    wfile << s << endl;


    closePort(dstSocket);

    //red::saveGame();
    total += res;

    Sleep(1000);
  }

  wfile.close();

  return total;
}
