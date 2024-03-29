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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>   // For std::memset
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <streambuf>
#include <vector>

#include "bitboard.h"
#include "evaluate.h"
#include "material.h"
#include "misc.h"
#include "pawns.h"
#include "thread.h"
#include "uci.h"
#include "incbin/incbin.h"


// Macro to embed the default NNUE file data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEmbeddedNNUEData[];  // a pointer to the embedded data
//     const unsigned char *const gEmbeddedNNUEEnd;     // a marker to the end
//     const unsigned int         gEmbeddedNNUESize;    // the size of the embedded file
// Note that this does not work in Microsof Visual Studio.
#if !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
INCBIN(EmbeddedNNUE, EvalFileDefaultName);
#else
const unsigned char        gEmbeddedNNUEData[1] = { 0x0 };
const unsigned char* const gEmbeddedNNUEEnd = &gEmbeddedNNUEData[1];
const unsigned int         gEmbeddedNNUESize = 1;
#endif


using namespace std;
using namespace Eval::NNUE;

namespace Eval {

  bool useNNUE;
  string eval_file_loaded = "None";

  /// NNUE::init() tries to load a nnue network at startup time, or when the engine
  /// receives a UCI command "setoption name EvalFile value nn-[a-z0-9]{12}.nnue"
  /// The name of the nnue network is always retrieved from the EvalFile option.
  /// We search the given network in three locations: internally (the default
  /// network may be embedded in the binary), in the active working directory and
  /// in the engine directory. Distro packagers may define the DEFAULT_NNUE_DIRECTORY
  /// variable to have the engine search in a special directory in their distro.

  void NNUE::init() {

    useNNUE = Options["Use NNUE"];
    useNNUE = false;////////
    if (!useNNUE)
      return;

    string eval_file = string(Options["EvalFile"]);

#if defined(DEFAULT_NNUE_DIRECTORY)
#define stringify2(x) #x
#define stringify(x) stringify2(x)
    vector<string> dirs = { "<internal>" , "" , CommandLine::binaryDirectory , stringify(DEFAULT_NNUE_DIRECTORY) };
#else
    vector<string> dirs = { "<internal>" , "" , CommandLine::binaryDirectory };
#endif

    for (string directory : dirs)
      if (eval_file_loaded != eval_file)
      {
        if (directory != "<internal>")
        {
          ifstream stream(directory + eval_file, ios::binary);
          if (load_eval(eval_file, stream))
            eval_file_loaded = eval_file;
        }

        if (directory == "<internal>" && eval_file == EvalFileDefaultName)
        {
          // C++ way to prepare a buffer for a memory stream
          class MemoryBuffer : public basic_streambuf<char> {
          public: MemoryBuffer(char* p, size_t n) { setg(p, p, p + n); setp(p, p + n); }
          };

          MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
            size_t(gEmbeddedNNUESize));

          istream stream(&buffer);
          if (load_eval(eval_file, stream))
            eval_file_loaded = eval_file;
        }
      }
  }

  /// NNUE::verify() verifies that the last net used was loaded successfully
  void NNUE::verify() {

    string eval_file = string(Options["EvalFile"]);

    if (useNNUE && eval_file_loaded != eval_file)
    {
      UCI::OptionsMap defaults;
      UCI::init(defaults);

      string msg1 = "If the UCI option \"Use NNUE\" is set to true, network evaluation parameters compatible with the engine must be available.";
      string msg2 = "The option is set to true, but the network file " + eval_file + " was not loaded successfully.";
      string msg3 = "The UCI option EvalFile might need to specify the full path, including the directory name, to the network file.";
      string msg4 = "The default net can be downloaded from: https://tests.stockfishchess.org/api/nn/" + string(defaults["EvalFile"]);
      string msg5 = "The engine will be terminated now.";

      sync_cout << "info string ERROR: " << msg1 << sync_endl;
      sync_cout << "info string ERROR: " << msg2 << sync_endl;
      sync_cout << "info string ERROR: " << msg3 << sync_endl;
      sync_cout << "info string ERROR: " << msg4 << sync_endl;
      sync_cout << "info string ERROR: " << msg5 << sync_endl;

      exit(EXIT_FAILURE);
    }

    if (useNNUE)
      sync_cout << "info string NNUE evaluation using " << eval_file << " enabled" << sync_endl;
    else
      sync_cout << "info string classical evaluation enabled" << sync_endl;
  }
}

namespace Trace {

  enum Tracing { NO_TRACE, TRACE };

  enum Term { // The first 8 entries are reserved for PieceType
    MATERIAL = 8, IMBALANCE, MOBILITY, THREAT, PASSED, SPACE, WINNABLE, TOTAL, TERM_NB
  };

  Score scores[TERM_NB][COLOR_NB];

  double to_cp(Value v) { return double(v) / PawnValueEg; }

  void add(int idx, Color c, Score s) {
    scores[idx][c] = s;
  }

  void add(int idx, Score w, Score b = SCORE_ZERO) {
    scores[idx][WHITE] = w;
    scores[idx][BLACK] = b;
  }

  std::ostream& operator<<(std::ostream& os, Score s) {
    os << std::setw(5) << to_cp(mg_value(s)) << " "
      << std::setw(5) << to_cp(eg_value(s));
    return os;
  }

  std::ostream& operator<<(std::ostream& os, Term t) {

    if (t == MATERIAL || t == IMBALANCE || t == WINNABLE || t == TOTAL)
      os << " ----  ----" << " | " << " ----  ----";
    else
      os << scores[t][WHITE] << " | " << scores[t][BLACK];

    os << " | " << scores[t][WHITE] - scores[t][BLACK] << "\n";
    return os;
  }
}

using namespace Trace;

namespace {

  // Threshold for lazy and space evaluation
  constexpr Value LazyThreshold1 = Value(1400);
  constexpr Value LazyThreshold2 = Value(1300);
  constexpr Value SpaceThreshold = Value(12222);
  constexpr Value NNUEThreshold1 = Value(550);
  constexpr Value NNUEThreshold2 = Value(150);

  // KingAttackWeights[PieceType] contains king attack weights by piece type
  constexpr int KingAttackWeights[PIECE_TYPE_NB] = { 0, 0, 81, 52, 44, 10 };

  // SafeCheck[PieceType][single/multiple] contains safe check bonus by piece type,
  // higher if multiple safe checks are possible for that piece type.
  constexpr int SafeCheck[][2] = {
      {}, {}, {803, 1292}, {639, 974}, {1087, 1878}, {759, 1132}
  };

#define S(mg, eg) make_score(mg, eg)

  // MobilityBonus[PieceType-2][attacked] contains bonuses for middle and end game,
  // indexed by piece type and number of attacked squares in the mobility area.
  constexpr Score MobilityBonus[][32] = {
    { S(-62,-79), S(-53,-57), S(-12,-31), S(-3,-17), S(3,  7), S(12, 13), // Knight
      S(21, 16), S(28, 21), S(37, 26) },
    { S(-47,-59), S(-20,-25), S(14, -8), S(29, 12), S(39, 21), S(53, 40), // Bishop
      S(53, 56), S(60, 58), S(62, 65), S(69, 72), S(78, 78), S(83, 87),
      S(91, 88), S(96, 98) },
    { S(-61,-82), S(-20,-17), S(2, 23) ,S(3, 40), S(4, 72), S(11,100), // Rook
      S(22,104), S(31,120), S(39,134), S(40 ,138), S(41,158), S(47,163),
      S(59,168), S(60,169), S(64,173) },
    { S(-29,-49), S(-16,-29), S(-8, -8), S(-8, 17), S(18, 39), S(25, 54), // Queen
      S(23, 59), S(37, 73), S(41, 76), S(54, 95), S(65, 95) ,S(68,101),
      S(69,124), S(70,128), S(70,132), S(70,133) ,S(71,136), S(72,140),
      S(74,147), S(76,149), S(90,153), S(104,169), S(105,171), S(106,171),
      S(112,178), S(114,185), S(114,187), S(119,221) }
  };

  // KingProtector[knight/bishop] contains penalty for each distance unit to own king
  constexpr Score KingProtector[] = { S(8, 9), S(6, 9) };

  // Outpost[knight/bishop] contains bonuses for each knight or bishop occupying a
  // pawn protected square on rank 4 to 6 which is also safe from a pawn attack.
  constexpr Score Outpost[] = { S(56, 34), S(31, 23) };

  // PassedRank[Rank] contains a bonus according to the rank of a passed pawn
  constexpr Score PassedRank[RANK_NB] = {
    S(0, 0), S(9, 28), S(15, 31), S(17, 39), S(64, 70), S(171, 177), S(277, 260)
  };

  // RookOnFile[semiopen/open] contains bonuses for each rook when there is
  // no (friendly) pawn on the rook file.
  constexpr Score RookOnFile[] = { S(19, 7), S(48, 27) };

  // ThreatByMinor/ByRook[attacked PieceType] contains bonuses according to
  // which piece type attacks which one. Attacks on lesser pieces which are
  // pawn-defended are not considered.
  constexpr Score ThreatByMinor[PIECE_TYPE_NB] = {
    S(0, 0), S(5, 32), S(55, 41), S(77, 56), S(89, 119), S(79, 162)
  };

  constexpr Score ThreatByRook[PIECE_TYPE_NB] = {
    S(0, 0), S(3, 44), S(37, 68), S(42, 60), S(0, 39), S(58, 43)
  };

  // Assorted bonuses and penalties
  constexpr Score BadOutpost = S(-7, 36);
  constexpr Score BishopOnKingRing = S(24, 0);
  constexpr Score BishopPawns = S(3, 7);
  constexpr Score BishopXRayPawns = S(4, 5);
  constexpr Score CorneredBishop = S(50, 50);
  constexpr Score FlankAttacks = S(8, 0);
  constexpr Score Hanging = S(69, 36);
  constexpr Score KnightOnQueen = S(16, 11);
  constexpr Score LongDiagonalBishop = S(45, 0);
  constexpr Score MinorBehindPawn = S(18, 3);
  constexpr Score PassedFile = S(11, 8);
  constexpr Score PawnlessFlank = S(17, 95);
  constexpr Score ReachableOutpost = S(31, 22);
  constexpr Score RestrictedPiece = S(7, 7);
  constexpr Score RookOnKingRing = S(16, 0);
  constexpr Score RookOnQueenFile = S(6, 11);
  constexpr Score SliderOnQueen = S(60, 18);
  constexpr Score ThreatByKing = S(24, 89);
  constexpr Score ThreatByPawnPush = S(48, 39);
  constexpr Score ThreatBySafePawn = S(173, 94);
  constexpr Score TrappedRook = S(55, 13);
  constexpr Score WeakQueenProtection = S(14, 0);
  constexpr Score WeakQueen = S(56, 15);


#undef S

  // Evaluation class computes and stores attacks tables and other working data
  template<Tracing T>
  class Evaluation {

  public:
    Evaluation() = delete;
    explicit Evaluation(const Position& p) : pos(p) {}
    Evaluation& operator=(const Evaluation&) = delete;
    Value value();

  private:
    template<Color Us> void initialize();
    template<Color Us, PieceType Pt> Score pieces();
    template<Color Us> Score king() const;
    template<Color Us> Score threats() const;
    template<Color Us> Score passed() const;
    template<Color Us> Score space() const;
    Value winnable(Score score) const;

    const Position& pos;
    Material::Entry* me;
    //Pawns::Entry* pe;
    Bitboard mobilityArea[COLOR_NB];
    Score mobility[COLOR_NB] = { SCORE_ZERO, SCORE_ZERO };

    // attackedBy[color][piece type] is a bitboard representing all squares
    // attacked by a given color and piece type. Special "piece types" which
    // is also calculated is ALL_PIECES.
    Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB];

    // attackedBy2[color] are the squares attacked by at least 2 units of a given
    // color, including x-rays. But diagonal x-rays through pawns are not computed.
    Bitboard attackedBy2[COLOR_NB];

    // kingRing[color] are the squares adjacent to the king plus some other
    // very near squares, depending on king position.
    Bitboard kingRing[COLOR_NB];

    // kingAttackersCount[color] is the number of pieces of the given color
    // which attack a square in the kingRing of the enemy king.
    int kingAttackersCount[COLOR_NB];

    // kingAttackersWeight[color] is the sum of the "weights" of the pieces of
    // the given color which attack a square in the kingRing of the enemy king.
    // The weights of the individual piece types are given by the elements in
    // the KingAttackWeights array.
    int kingAttackersWeight[COLOR_NB];

    // kingAttacksCount[color] is the number of attacks by the given color to
    // squares directly adjacent to the enemy king. Pieces which attack more
    // than one square are counted multiple times. For instance, if there is
    // a white knight on g5 and black's king is on g8, this white knight adds 2
    // to kingAttacksCount[WHITE].
    int kingAttacksCount[COLOR_NB];
  };


  // Evaluation::initialize() computes king and pawn attacks, and the king ring
  // bitboard for a given color. This is done at the beginning of the evaluation.

  template<Tracing T> template<Color Us>
  void Evaluation<T>::initialize() {

    constexpr Color     Them = ~Us;
    constexpr Direction Up = pawn_push(Us);
    constexpr Direction Down = -Up;
    constexpr Bitboard LowRanks = (Us == WHITE ? Rank2BB | Rank3BB : Rank7BB | Rank6BB);

    const Square ksq = pos.square<KING>(Us);

    Bitboard dblAttackByPawn = pawn_double_attacks_bb<Us>(pos.pieces(Us, PAWN));

    // Find our pawns that are blocked or on the first two ranks
    Bitboard b = pos.pieces(Us, PAWN) & (shift<Down>(pos.pieces()) | LowRanks);

    // Squares occupied by those pawns, by our king or queen, by blockers to attacks on our king
    // or controlled by enemy pawns are excluded from the mobility area.
    mobilityArea[Us] = ~(b | pos.pieces(Us, KING, QUEEN) | pos.blockers_for_king(Us) | pe->pawn_attacks(Them));

    // Initialize attackedBy[] for king and pawns
    attackedBy[Us][KING] = attacks_bb<KING>(ksq);
    attackedBy[Us][PAWN] = pe->pawn_attacks(Us);
    attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
    attackedBy2[Us] = dblAttackByPawn | (attackedBy[Us][KING] & attackedBy[Us][PAWN]);

    // Init our king safety tables
    Square s = make_square(std::clamp(file_of(ksq), FILE_B, FILE_G),
      std::clamp(rank_of(ksq), RANK_2, RANK_7));
    kingRing[Us] = attacks_bb<KING>(s) | s;

    kingAttackersCount[Them] = popcount(kingRing[Us] & pe->pawn_attacks(Them));
    kingAttacksCount[Them] = kingAttackersWeight[Them] = 0;

    // Remove from kingRing[] the squares defended by two pawns
    kingRing[Us] &= ~dblAttackByPawn;
  }


  // Evaluation::pieces() scores pieces of a given color and type

  template<Tracing T> template<Color Us, PieceType Pt>
  Score Evaluation<T>::pieces() {

    constexpr Color     Them = ~Us;
    constexpr Direction Down = -pawn_push(Us);
    constexpr Bitboard OutpostRanks = (Us == WHITE ? Rank4BB | Rank5BB | Rank6BB
      : Rank5BB | Rank4BB | Rank3BB);
    const Square* pl = pos.squares<Pt>(Us);

    Bitboard b, bb;
    Score score = SCORE_ZERO;

    attackedBy[Us][Pt] = 0;

    for (Square s = *pl; s != SQ_NONE; s = *++pl)
    {
      // Find attacked squares, including x-ray attacks for bishops and rooks
      //b = Pt == BISHOP ? attacks_bb<BISHOP>(s, pos.pieces() ^ pos.pieces(QUEEN))
      //  : Pt ==   ROOK ? attacks_bb<  ROOK>(s, pos.pieces() ^ pos.pieces(QUEEN) ^ pos.pieces(Us, ROOK))
      //                 : attacks_bb<Pt>(s, pos.pieces());
      b = attacks_bb<Pt>(s, pos.pieces());

      if (pos.blockers_for_king(Us) & s)
        b &= line_bb(pos.square<KING>(Us), s);

      attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & b;
      attackedBy[Us][Pt] |= b;
      attackedBy[Us][ALL_PIECES] |= b;

      if (b & kingRing[Them])
      {
        kingAttackersCount[Us]++;
        kingAttackersWeight[Us] += KingAttackWeights[Pt];
        kingAttacksCount[Us] += popcount(b & attackedBy[Them][KING]);
      }

      //else if (Pt == ROOK && (file_bb(s) & kingRing[Them]))
      //    score += RookOnKingRing;

      //else if (Pt == BISHOP && (attacks_bb<BISHOP>(s, pos.pieces(PAWN)) & kingRing[Them]))
      //    score += BishopOnKingRing;

      int mob = popcount(b & mobilityArea[Us]);

      mobility[Us] += MobilityBonus[Pt - 2][mob];

      /*
      if (Pt == BISHOP || Pt == KNIGHT)
      {
          // Bonus if the piece is on an outpost square or can reach one
          // Reduced bonus for knights (BadOutpost) if few relevant targets
          bb = OutpostRanks & (attackedBy[Us][PAWN] | shift<Down>(pos.pieces(PAWN)))
                            & ~pe->pawn_attacks_span(Them);
          Bitboard targets = pos.pieces(Them) & ~pos.pieces(PAWN);

          if (   Pt == KNIGHT
              && bb & s & ~CenterFiles // on a side outpost
              && !(b & targets)        // no relevant attacks
              && (!more_than_one(targets & (s & QueenSide ? QueenSide : KingSide))))
              score += BadOutpost;
          else if (bb & s)
              score += Outpost[Pt == BISHOP];
          else if (Pt == KNIGHT && bb & b & ~pos.pieces(Us))
              score += ReachableOutpost;

          // Bonus for a knight or bishop shielded by pawn
          if (shift<Down>(pos.pieces(PAWN)) & s)
              score += MinorBehindPawn;

          // Penalty if the piece is far from the king
          score -= KingProtector[Pt == BISHOP] * distance(pos.square<KING>(Us), s);

          if (Pt == BISHOP)
          {
              // Penalty according to the number of our pawns on the same color square as the
              // bishop, bigger when the center files are blocked with pawns and smaller
              // when the bishop is outside the pawn chain.
              Bitboard blocked = pos.pieces(Us, PAWN) & shift<Down>(pos.pieces());

              score -= BishopPawns * pos.pawns_on_same_color_squares(Us, s)
                                   * (!(attackedBy[Us][PAWN] & s) + popcount(blocked & CenterFiles));

              // Penalty for all enemy pawns x-rayed
              score -= BishopXRayPawns * popcount(attacks_bb<BISHOP>(s) & pos.pieces(Them, PAWN));

              // Bonus for bishop on a long diagonal which can "see" both center squares
              if (more_than_one(attacks_bb<BISHOP>(s, pos.pieces(PAWN)) & Center))
                  score += LongDiagonalBishop;

              // An important Chess960 pattern: a cornered bishop blocked by a friendly
              // pawn diagonally in front of it is a very serious problem, especially
              // when that pawn is also blocked.
              if (   pos.is_chess960()
                  && (s == relative_square(Us, SQ_A1) || s == relative_square(Us, SQ_H1)))
              {
                  Direction d = pawn_push(Us) + (file_of(s) == FILE_A ? EAST : WEST);
                  if (pos.piece_on(s + d) == make_piece(Us, PAWN))
                      score -= !pos.empty(s + d + pawn_push(Us))                ? CorneredBishop * 4
                              : pos.piece_on(s + d + d) == make_piece(Us, PAWN) ? CorneredBishop * 2
                                                                                : CorneredBishop;
              }
          }
      }
      */

      /*
      if (Pt == ROOK)
      {
          // Bonus for rook on the same file as a queen
          if (file_bb(s) & pos.pieces(QUEEN))
              score += RookOnQueenFile;

          // Bonus for rook on an open or semi-open file
          if (pos.is_on_semiopen_file(Us, s))
              score += RookOnFile[pos.is_on_semiopen_file(Them, s)];

          // Penalty when trapped by the king, even more if the king cannot castle
          else if (mob <= 3)
          {
              File kf = file_of(pos.square<KING>(Us));
              if ((kf < FILE_E) == (file_of(s) < kf))
                  score -= TrappedRook * (1 + !pos.castling_rights(Us));
          }
      }
      */
      /*
      if (Pt == QUEEN)
      {
          // Penalty if any relative pin or discovered attack against the queen
          Bitboard queenPinners;
          if (pos.slider_blockers(pos.pieces(Them, ROOK, BISHOP), s, queenPinners))
              score -= WeakQueen;
      }
      */
    }
    if (T)
      Trace::add(Pt, Us, score);

    return score;
  }


  // Evaluation::king() assigns bonuses and penalties to a king of a given color

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::king() const {

    constexpr Color    Them = ~Us;
    constexpr Bitboard Camp = (Us == WHITE ? AllSquares ^ Rank6BB ^ Rank7BB ^ Rank8BB
      : AllSquares ^ Rank1BB ^ Rank2BB ^ Rank3BB);

    Bitboard weak, b1, b2, b3, safe, unsafeChecks = 0;
    Bitboard rookChecks, queenChecks, bishopChecks, knightChecks;
    int kingDanger = 0;
    const Square ksq = pos.square<KING>(Us);

    // Init the score with king shelter and enemy pawns storm
    Score score = pe->king_safety<Us>(pos);

    // Attacked squares defended at most once by our queen or king
    weak = attackedBy[Them][ALL_PIECES]
      & ~attackedBy2[Us]
      //& (~attackedBy[Us][ALL_PIECES] | attackedBy[Us][KING] | attackedBy[Us][QUEEN]);
      & (~attackedBy[Us][ALL_PIECES] | attackedBy[Us][KING]);

    // Analyse the safe enemy's checks which are possible on next move
    safe = ~pos.pieces(Them);
    safe &= ~attackedBy[Us][ALL_PIECES] | (weak & attackedBy2[Them]);

    //b1 = attacks_bb<ROOK  >(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));
    //b2 = attacks_bb<BISHOP>(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));

    /*
    // Enemy rooks checks
    rookChecks = b1 & attackedBy[Them][ROOK] & safe;
    if (rookChecks)
        kingDanger += SafeCheck[ROOK][more_than_one(rookChecks)];
    else
        unsafeChecks |= b1 & attackedBy[Them][ROOK];

    */
    /*
    // Enemy queen safe checks: count them only if the checks are from squares from
    // which opponent cannot give a rook check, because rook checks are more valuable.
    queenChecks =  (b1 | b2) & attackedBy[Them][QUEEN] & safe
                 & ~(attackedBy[Us][QUEEN] | rookChecks);
    if (queenChecks)
        kingDanger += SafeCheck[QUEEN][more_than_one(queenChecks)];

    */
    /*
    // Enemy bishops checks: count them only if they are from squares from which
    // opponent cannot give a queen check, because queen checks are more valuable.
    bishopChecks =  b2 & attackedBy[Them][BISHOP] & safe
                  & ~queenChecks;
    if (bishopChecks)
        kingDanger += SafeCheck[BISHOP][more_than_one(bishopChecks)];

    else
        unsafeChecks |= b2 & attackedBy[Them][BISHOP];

    */
    /*
    // Enemy knights checks
    knightChecks = attacks_bb<KNIGHT>(ksq) & attackedBy[Them][KNIGHT];
    if (knightChecks & safe)
        kingDanger += SafeCheck[KNIGHT][more_than_one(knightChecks & safe)];
    else
        unsafeChecks |= knightChecks;

    */

    // Find the squares that opponent attacks in our king flank, the squares
    // which they attack twice in that flank, and the squares that we defend.
    b1 = attackedBy[Them][ALL_PIECES] & KingFlank[file_of(ksq)] & Camp;
    b2 = b1 & attackedBy2[Them];
    b3 = attackedBy[Us][ALL_PIECES] & KingFlank[file_of(ksq)] & Camp;

    int kingFlankAttack = popcount(b1) + popcount(b2);
    int kingFlankDefense = popcount(b3);

    kingDanger += kingAttackersCount[Them] * kingAttackersWeight[Them]
      + 185 * popcount(kingRing[Us] & weak)
      + 148 * popcount(unsafeChecks)
      + 98 * popcount(pos.blockers_for_king(Us))
      + 69 * kingAttacksCount[Them]
      + 3 * kingFlankAttack * kingFlankAttack / 8
      + mg_value(mobility[Them] - mobility[Us])
      //- 873 * !pos.count<QUEEN>(Them)
      //- 100 * bool(attackedBy[Us][KNIGHT] & attackedBy[Us][KING])
      - 6 * mg_value(score) / 8
      - 4 * kingFlankDefense
      + 37;

    // Transform the kingDanger units into a Score, and subtract it from the evaluation
    if (kingDanger > 100)
      score -= make_score(kingDanger * kingDanger / 4096, kingDanger / 16);

    /*
    // Penalty when our king is on a pawnless flank
    if (!(pos.pieces(PAWN) & KingFlank[file_of(ksq)]))
        score -= PawnlessFlank;
    */

    // Penalty if king flank is under attack, potentially moving toward the king
    score -= FlankAttacks * kingFlankAttack;

    if (T)
      Trace::add(KING, Us, score);

    return score;
  }


  // Evaluation::threats() assigns bonuses according to the types of the
  // attacking and the attacked pieces.

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::threats() const {

    constexpr Color     Them = ~Us;
    constexpr Direction Up = pawn_push(Us);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);

    Bitboard b, weak, defended, nonPawnEnemies, stronglyProtected, safe;
    Score score = SCORE_ZERO;

    // Non-pawn enemies
    //nonPawnEnemies = pos.pieces(Them) & ~pos.pieces(PAWN);
    nonPawnEnemies = pos.pieces(Them);

    // Squares strongly protected by the enemy, either because they defend the
    // square with a pawn, or because they defend the square twice and we don't.
    //stronglyProtected =  attackedBy[Them][PAWN]
    //                   | (attackedBy2[Them] & ~attackedBy2[Us]);
    stronglyProtected = (attackedBy2[Them] & ~attackedBy2[Us]);

    // Non-pawn enemies, strongly protected
    defended = nonPawnEnemies & stronglyProtected;

    // Enemies not strongly protected and under our attack
    weak = pos.pieces(Them) & ~stronglyProtected & attackedBy[Us][ALL_PIECES];

    // Bonus according to the kind of attacking pieces
    if (defended | weak)
    {
      /*
      //b = (defended | weak) & (attackedBy[Us][KNIGHT] | attackedBy[Us][BISHOP]);
      b = (defended | weak);  //こういう修正は、「全員の攻撃」となる？
      while (b)
          score += ThreatByMinor[type_of(pos.piece_on(pop_lsb(&b)))];
      */
      /*
      b = weak & attackedBy[Us][ROOK];
      while (b)
          score += ThreatByRook[type_of(pos.piece_on(pop_lsb(&b)))];
      */
      if (weak & attackedBy[Us][KING])
        score += ThreatByKing;

      b = ~attackedBy[Them][ALL_PIECES]
        | (nonPawnEnemies & attackedBy2[Us]);
      score += Hanging * popcount(weak & b);

      // Additional bonus if weak piece is only protected by a queen
      //score += WeakQueenProtection * popcount(weak & attackedBy[Them][QUEEN]);
    }

    // Bonus for restricting their piece moves
    b = attackedBy[Them][ALL_PIECES]
      & ~stronglyProtected
      & attackedBy[Us][ALL_PIECES];
    score += RestrictedPiece * popcount(b);

    // Protected or unattacked squares
    safe = ~attackedBy[Them][ALL_PIECES] | attackedBy[Us][ALL_PIECES];

    /*
    // Bonus for attacking enemy pieces with our relatively safe pawns
    b = pos.pieces(Us, PAWN) & safe;
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatBySafePawn * popcount(b);
    */

    /*
    // Find squares where our pawns can push on the next move
    b  = shift<Up>(pos.pieces(Us, PAWN)) & ~pos.pieces();
    b |= shift<Up>(b & TRank3BB) & ~pos.pieces();
    */

    // Keep only the squares which are relatively safe
    b &= ~attackedBy[Them][PAWN] & safe;

    // Bonus for safe pawn threats on the next move
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatByPawnPush * popcount(b);

    // Bonus for threats on the next moves against enemy queen
    if (pos.count<QUEEN>(Them) == 1)
    {
      bool queenImbalance = pos.count<QUEEN>() == 1;

      Square s = pos.square<QUEEN>(Them);
      safe = mobilityArea[Us]
        & ~pos.pieces(Us, PAWN)
        & ~stronglyProtected;

      b = attackedBy[Us][KNIGHT] & attacks_bb<KNIGHT>(s);

      score += KnightOnQueen * popcount(b & safe) * (1 + queenImbalance);

      b = (attackedBy[Us][BISHOP] & attacks_bb<BISHOP>(s, pos.pieces()))
        | (attackedBy[Us][ROOK] & attacks_bb<ROOK  >(s, pos.pieces()));

      score += SliderOnQueen * popcount(b & safe & attackedBy2[Us]) * (1 + queenImbalance);
    }

    if (T)
      Trace::add(THREAT, Us, score);

    return score;
  }

  // Evaluation::passed() evaluates the passed pawns and candidate passed
  // pawns of the given color.

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::passed() const {

    constexpr Color     Them = ~Us;
    constexpr Direction Up = pawn_push(Us);
    constexpr Direction Down = -Up;

    auto king_proximity = [&](Color c, Square s) {
      return std::min(distance(pos.square<KING>(c), s), 5);
    };

    Bitboard b, bb, squaresToQueen, unsafeSquares, blockedPassers, helpers;
    Score score = SCORE_ZERO;

    b = pe->passed_pawns(Us);

    blockedPassers = b & shift<Down>(pos.pieces(Them, PAWN));
    if (blockedPassers)
    {
      helpers = shift<Up>(pos.pieces(Us, PAWN))
        & ~pos.pieces(Them)
        & (~attackedBy2[Them] | attackedBy[Us][ALL_PIECES]);

      // Remove blocked candidate passers that don't have help to pass
      b &= ~blockedPassers
        | shift<WEST>(helpers)
        | shift<EAST>(helpers);
    }

    while (b)
    {
      Square s = pop_lsb(&b);

      assert(!(pos.pieces(Them, PAWN) & forward_file_bb(Us, s + Up)));

      int r = relative_rank(Us, s);

      Score bonus = PassedRank[r];

      if (r > RANK_3)
      {
        int w = 5 * r - 13;
        Square blockSq = s + Up;

        // Adjust bonus based on the king's proximity
        bonus += make_score(0, (king_proximity(Them, blockSq) * 19 / 4
          - king_proximity(Us, blockSq) * 2) * w);

        // If blockSq is not the queening square then consider also a second push
        if (r != RANK_7)
          bonus -= make_score(0, king_proximity(Us, blockSq + Up) * w);

        // If the pawn is free to advance, then increase the bonus
        if (pos.empty(blockSq))
        {
          squaresToQueen = forward_file_bb(Us, s);
          unsafeSquares = passed_pawn_span(Us, s);

          bb = forward_file_bb(Them, s) & pos.pieces(ROOK, QUEEN);

          if (!(pos.pieces(Them) & bb))
            unsafeSquares &= attackedBy[Them][ALL_PIECES];

          // If there are no enemy attacks on passed pawn span, assign a big bonus.
          // Otherwise assign a smaller bonus if the path to queen is not attacked
          // and even smaller bonus if it is attacked but block square is not.
          int k = !unsafeSquares ? 35 :
            !(unsafeSquares & squaresToQueen) ? 20 :
            !(unsafeSquares & blockSq) ? 9 :
            0;

          // Assign a larger bonus if the block square is defended
          if ((pos.pieces(Us) & bb) || (attackedBy[Us][ALL_PIECES] & blockSq))
            k += 5;

          bonus += make_score(k * w, k * w);
        }
      } // r > RANK_3

      score += bonus - PassedFile * edge_distance(file_of(s));
    }

    if (T)
      Trace::add(PASSED, Us, score);

    return score;
  }


  // Evaluation::space() computes a space evaluation for a given side, aiming to improve game
  // play in the opening. It is based on the number of safe squares on the four central files
  // on ranks 2 to 4. Completely safe squares behind a friendly pawn are counted twice.
  // Finally, the space bonus is multiplied by a weight which decreases according to occupancy.

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::space() const {

    // Early exit if, for example, both queens or 6 minor pieces have been exchanged
    if (pos.non_pawn_material() < SpaceThreshold)
      return SCORE_ZERO;

    constexpr Color Them = ~Us;
    constexpr Direction Down = -pawn_push(Us);
    constexpr Bitboard SpaceMask =
      Us == WHITE ? CenterFiles & (Rank2BB | Rank3BB | Rank4BB)
      : CenterFiles & (Rank7BB | Rank6BB | Rank5BB);

    // Find the available squares for our pieces inside the area defined by SpaceMask
    Bitboard safe = SpaceMask
      & ~pos.pieces(Us, PAWN)
      & ~attackedBy[Them][PAWN];

    // Find all squares which are at most three squares behind some friendly pawn
    Bitboard behind = pos.pieces(Us, PAWN);
    behind |= shift<Down>(behind);
    behind |= shift<Down + Down>(behind);

    int bonus = popcount(safe) + popcount(behind & safe & ~attackedBy[Them][ALL_PIECES]);
    int weight = pos.count<ALL_PIECES>(Us) - 3 + std::min(pe->blocked_count(), 9);
    Score score = make_score(bonus * weight * weight / 16, 0);

    if (T)
      Trace::add(SPACE, Us, score);

    return score;
  }


  // Evaluation::winnable() adjusts the midgame and endgame score components, based on
  // the known attacking/defending status of the players. The final value is derived
  // by interpolation from the midgame and endgame values.

  template<Tracing T>
  Value Evaluation<T>::winnable(Score score) const {

    int outflanking = distance<File>(pos.square<KING>(WHITE), pos.square<KING>(BLACK))
      - distance<Rank>(pos.square<KING>(WHITE), pos.square<KING>(BLACK));

    bool pawnsOnBothFlanks = (pos.pieces(PAWN) & QueenSide)
      && (pos.pieces(PAWN) & KingSide);

    bool almostUnwinnable = outflanking < 0
      && !pawnsOnBothFlanks;

    bool infiltration = rank_of(pos.square<KING>(WHITE)) > RANK_4
      || rank_of(pos.square<KING>(BLACK)) < RANK_5;

    // Compute the initiative bonus for the attacking side
    int complexity = 9 * pe->passed_count()
      + 12 * pos.count<PAWN>()
      + 9 * outflanking
      + 21 * pawnsOnBothFlanks
      + 24 * infiltration
      + 51 * !pos.non_pawn_material()
      - 43 * almostUnwinnable
      - 110;

    Value mg = mg_value(score);
    Value eg = eg_value(score);

    // Now apply the bonus: note that we find the attacking side by extracting the
    // sign of the midgame or endgame values, and that we carefully cap the bonus
    // so that the midgame and endgame scores do not change sign after the bonus.
    int u = ((mg > 0) - (mg < 0)) * std::clamp(complexity + 50, -abs(mg), 0);
    int v = ((eg > 0) - (eg < 0)) * std::max(complexity, -abs(eg));

    mg += u;
    eg += v;

    // Compute the scale factor for the winning side
    Color strongSide = eg > VALUE_DRAW ? WHITE : BLACK;
    int sf = me->scale_factor(pos, strongSide);

    // If scale factor is not already specific, scale down via general heuristics
    if (sf == SCALE_FACTOR_NORMAL)
    {
      if (pos.opposite_bishops())
      {
        if (pos.non_pawn_material(WHITE) == BishopValueMg
          && pos.non_pawn_material(BLACK) == BishopValueMg)
          sf = 18 + 4 * popcount(pe->passed_pawns(strongSide));
        else
          sf = 22 + 3 * pos.count<ALL_PIECES>(strongSide);
      }
      else if (pos.non_pawn_material(WHITE) == RookValueMg
        && pos.non_pawn_material(BLACK) == RookValueMg
        && pos.count<PAWN>(strongSide) - pos.count<PAWN>(~strongSide) <= 1
        && bool(KingSide & pos.pieces(strongSide, PAWN)) != bool(QueenSide & pos.pieces(strongSide, PAWN))
        && (attacks_bb<KING>(pos.square<KING>(~strongSide)) & pos.pieces(~strongSide, PAWN)))
        sf = 36;
      else if (pos.count<QUEEN>() == 1)
        sf = 37 + 3 * (pos.count<QUEEN>(WHITE) == 1 ? pos.count<BISHOP>(BLACK) + pos.count<KNIGHT>(BLACK)
          : pos.count<BISHOP>(WHITE) + pos.count<KNIGHT>(WHITE));
      else
        sf = std::min(sf, 36 + 7 * pos.count<PAWN>(strongSide));
    }

    // Interpolate between the middlegame and (scaled by 'sf') endgame score
    v = mg * int(me->game_phase())
      + eg * int(PHASE_MIDGAME - me->game_phase()) * ScaleFactor(sf) / SCALE_FACTOR_NORMAL;
    v /= PHASE_MIDGAME;

    if (T)
    {
      Trace::add(WINNABLE, make_score(u, eg * ScaleFactor(sf) / SCALE_FACTOR_NORMAL - eg_value(score)));
      Trace::add(TOTAL, make_score(mg, eg * ScaleFactor(sf) / SCALE_FACTOR_NORMAL));
    }

    return Value(v);
  }


  // Evaluation::value() is the main function of the class. It computes the various
  // parts of the evaluation and returns the value of the position from the point
  // of view of the side to move.

  template<Tracing T>
  Value Evaluation<T>::value() {

    assert(!pos.checkers());

    // Probe the material hash table
    me = Material::probe(pos);

    // If we have a specialized evaluation function for the current material
    // configuration, call it and return.
    if (me->specialized_eval_exists())
      return me->evaluate(pos);

    // Initialize score by reading the incrementally updated scores included in
    // the position object (material + piece square tables) and the material
    // imbalance. Score is computed internally from the white point of view.
    Score score = pos.psq_score() + me->imbalance() + pos.this_thread()->contempt;

    // Probe the pawn hash table
    pe = Pawns::probe(pos);
    score += pe->pawn_score(WHITE) - pe->pawn_score(BLACK);

    // Early exit if score is high
    auto lazy_skip = [&](Value lazyThreshold) {
      return abs(mg_value(score) + eg_value(score)) / 2 > lazyThreshold + pos.non_pawn_material() / 64;
    };

    if (lazy_skip(LazyThreshold1))
      goto make_v;

    // Main evaluation begins here
    initialize<WHITE>();
    initialize<BLACK>();

    // Pieces evaluated first (also populates attackedBy, attackedBy2).
    // Note that the order of evaluation of the terms is left unspecified.
    score += pieces<WHITE, KNIGHT>() - pieces<BLACK, KNIGHT>()
      + pieces<WHITE, BISHOP>() - pieces<BLACK, BISHOP>()
      + pieces<WHITE, ROOK  >() - pieces<BLACK, ROOK  >()
      + pieces<WHITE, QUEEN >() - pieces<BLACK, QUEEN >();

    score += mobility[WHITE] - mobility[BLACK];

    // More complex interactions that require fully populated attack bitboards
    score += king<   WHITE>() - king<   BLACK>()
      + passed< WHITE>() - passed< BLACK>();

    if (lazy_skip(LazyThreshold2))
      goto make_v;

    score += threats<WHITE>() - threats<BLACK>()
      + space<  WHITE>() - space<  BLACK>();

  make_v:
    // Derive single value from mg and eg parts of score
    Value v = winnable(score);

    // In case of tracing add all remaining individual evaluation terms
    if (T)
    {
      Trace::add(MATERIAL, pos.psq_score());
      Trace::add(IMBALANCE, me->imbalance());
      Trace::add(PAWN, pe->pawn_score(WHITE), pe->pawn_score(BLACK));
      Trace::add(MOBILITY, mobility[WHITE], mobility[BLACK]);
    }

    // Evaluation grain
    v = (v / 16) * 16;

    // Side to move point of view
    v = (pos.side_to_move() == WHITE ? v : -v) + Tempo;

    return v;
  }

} // namespace


/// evaluate() is the evaluator for the outer world. It returns a static
/// evaluation of the position from the point of view of the side to move.

Value Eval::evaluate(const Position& pos) {

  Value v;

  if (!Eval::useNNUE)
    v = Evaluation<NO_TRACE>(pos).value();
  else
  {
    // Scale and shift NNUE for compatibility with search and classical evaluation
    auto  adjusted_NNUE = [&]() {
      int mat = pos.non_pawn_material() + PieceValue[MG][PAWN] * pos.count<PAWN>();
      return NNUE::evaluate(pos) * (720 + mat / 32) / 1024 + Tempo;
    };

    // If there is PSQ imbalance use classical eval, with small probability if it is small
    Value psq = Value(abs(eg_value(pos.psq_score())));
    int   r50 = 16 + pos.rule50_count();
    bool  largePsq = psq * 16 > (NNUEThreshold1 + pos.non_pawn_material() / 64) * r50;
    bool  classical = largePsq || (psq > PawnValueMg / 4 && !(pos.this_thread()->nodes & 0xB));

    v = classical ? Evaluation<NO_TRACE>(pos).value() : adjusted_NNUE();

    // If the classical eval is small and imbalance large, use NNUE nevertheless.
    // For the case of opposite colored bishops, switch to NNUE eval with
    // small probability if the classical eval is less than the threshold.
    if (largePsq
      && (abs(v) * 16 < NNUEThreshold2 * r50
        || (pos.opposite_bishops()
          && abs(v) * 16 < (NNUEThreshold1 + pos.non_pawn_material() / 64) * r50
          && !(pos.this_thread()->nodes & 0xB))))
      v = adjusted_NNUE();
  }

  // Damp down the evaluation linearly when shuffling
  v = v * (100 - pos.rule50_count()) / 100;

  // Guarantee evaluation does not hit the tablebase range
  v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

  return v;
}

/// trace() is like evaluate(), but instead of returning a value, it returns
/// a string (suitable for outputting to stdout) that contains the detailed
/// descriptions and values of each evaluation term. Useful for debugging.
/// Trace scores are from white's point of view

std::string Eval::trace(const Position& pos) {

  if (pos.checkers())
    return "Final evaluation: none (in check)";

  std::stringstream ss;
  ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);

  Value v;

  std::memset(scores, 0, sizeof(scores));

  pos.this_thread()->contempt = SCORE_ZERO; // Reset any dynamic contempt

  v = Evaluation<TRACE>(pos).value();

  ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
    << "     Term    |    White    |    Black    |    Total   \n"
    << "             |   MG    EG  |   MG    EG  |   MG    EG \n"
    << " ------------+-------------+-------------+------------\n"
    << "    Material | " << Term(MATERIAL)
    << "   Imbalance | " << Term(IMBALANCE)
    << "       Pawns | " << Term(PAWN)
    << "     Knights | " << Term(KNIGHT)
    << "     Bishops | " << Term(BISHOP)
    << "       Rooks | " << Term(ROOK)
    << "      Queens | " << Term(QUEEN)
    << "    Mobility | " << Term(MOBILITY)
    << " King safety | " << Term(KING)
    << "     Threats | " << Term(THREAT)
    << "      Passed | " << Term(PASSED)
    << "       Space | " << Term(SPACE)
    << "    Winnable | " << Term(WINNABLE)
    << " ------------+-------------+-------------+------------\n"
    << "       Total | " << Term(TOTAL);

  v = pos.side_to_move() == WHITE ? v : -v;

  ss << "\nClassical evaluation: " << to_cp(v) << " (white side)\n";

  if (Eval::useNNUE)
  {
    v = NNUE::evaluate(pos);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "\nNNUE evaluation:      " << to_cp(v) << " (white side)\n";
  }

  v = evaluate(pos);
  v = pos.side_to_move() == WHITE ? v : -v;
  ss << "\nFinal evaluation:     " << to_cp(v) << " (white side)\n";

  return ss.str();
}
#endif

#include <tuple>
#include <algorithm>
#include <vector>

#include "evaluate.h"
#include "position.h"
#include "Game_geister.h"
#include "search.h"

namespace {
  int _myGoalDist_0[1 << 16];	//_myGoalDist[s] = (sのiビット目が1⇔マスiに駒がある)ときのゴールまでのマンハッタン距離の和.
  int _yourGoalDist_0[1 << 16];
  int _myGoalDist_1[1 << 16]; //位置のスコア的な
  int _yourGoalDist_1[1 << 16];
  int bitCountTable[1 << 16];

  inline int myGoalDist_0(long long s) {
    return _myGoalDist_0[s >> 8 & 65535] + _myGoalDist_0[s >> 24 & 65535] + bitCountTable[s >> 24 & 65535] * 2 + _myGoalDist_0[s >> 40 & 65535] + bitCountTable[s >> 40 & 65535] * 4;
  }
  inline int yourGoalDist_0(long long s) {
    return _yourGoalDist_0[s >> 8 & 65535] + bitCountTable[s >> 8 & 65535] * 4 + _yourGoalDist_0[s >> 24 & 65535] + bitCountTable[s >> 24 & 65535] * 2 + _yourGoalDist_0[s >> 40 & 65535];
  }
  inline int myGoalDist_1(long long s) {
    return _myGoalDist_1[s >> 8 & 65535] + _myGoalDist_1[s >> 24 & 65535] + bitCountTable[s >> 24 & 65535] * 4 + _myGoalDist_1[s >> 40 & 65535] + bitCountTable[s >> 40 & 65535] * 8;
  }
  inline int yourGoalDist_1(long long s) {
    return _yourGoalDist_1[s >> 8 & 65535] + bitCountTable[s >> 8 & 65535] * 8 + _yourGoalDist_1[s >> 24 & 65535] + bitCountTable[s >> 24 & 65535] * 4 + _yourGoalDist_1[s >> 40 & 65535];
  }


  Value getWinPlayer_K(const Position& pos, int ply) {
    if (pos.side_to_move() == WHITE) {
      if (pos.count<GOAL>(WHITE) < 2)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<BLUE>(WHITE) == 0)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<RED>(WHITE) == 0)
        return VALUE_MATE_IN_MAX_PLY;

      if (pos.count<RED>(BLACK) == 0)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<PURPLE>(BLACK) == 0) //相手の青駒はプログラム内では紫駒のまま（判定だけ青駒）
        return VALUE_MATE_IN_MAX_PLY;
      if (pos.count<GOAL>(BLACK) < 2)
        return VALUE_MATE_IN_MAX_PLY;
    }
    else {
      if (pos.count<PURPLE>(BLACK) == 0)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<GOAL>(BLACK) < 2)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<RED>(BLACK) == 0)
        return VALUE_MATE_IN_MAX_PLY;

      if (pos.count<RED>(WHITE) == 0)
        return -VALUE_MATE_IN_MAX_PLY;
      if (pos.count<GOAL>(WHITE) < 2)
        return VALUE_MATE_IN_MAX_PLY;
      if (pos.count<BLUE>(WHITE) == 0)
        return VALUE_MATE_IN_MAX_PLY;
    }

    return VALUE_ZERO;
  }
  Value getWinPlayer_P(int pnum, const Position& pos, int ply) {
    if (pos.side_to_move() == WHITE) {
      if (pos.count<PURPLE>(BLACK) <= pnum)
        return -VALUE_MATE_IN_MAX_PLY / 2;  //青の可能性が一応あるという考え(+10)

      if (pos.count<RED>(WHITE) == 0)
        return VALUE_MATE_IN_MAX_PLY;
      if (pos.count<GOAL>(WHITE) < 2)
        return -VALUE_MATE_IN_MAX_PLY;  //赤取るより脱出される方がマシという考え(+20)
      if (pos.count<BLUE>(WHITE) == 0)
        return -VALUE_MATE_IN_MAX_PLY;  //100%青だと思って取られることはないという考え(+30)


      if (pos.count<GOAL>(BLACK) < 2)
        return VALUE_MATE_IN_MAX_PLY;
    }
    else {
      if (pos.count<PURPLE>(BLACK) <= pnum)
        return VALUE_MATE_IN_MAX_PLY / 2;
      if (pos.count<GOAL>(BLACK) < 2)
        return -VALUE_MATE_IN_MAX_PLY;

      if (pos.count<GOAL>(WHITE) < 2)
        return VALUE_MATE_IN_MAX_PLY;
      if (pos.count<BLUE>(WHITE) == 0)
        return VALUE_MATE_IN_MAX_PLY;
      if (pos.count<RED>(WHITE) == 0)
        return -VALUE_MATE_IN_MAX_PLY;
    }

    return VALUE_ZERO;
  }
}


//前処理
void Eval::init() {
  int i, j;
  int dist1_0[16] = { 1,0,1,2,2,1,0,1,2,1,2,3,3,2,1,2 };
  int dist1_1[16] = { 2,1,2,3,3,2,1,2,4,3,4,5,5,4,3,4 };
  int dist2_0[16] = { 2,1,2,3,3,2,1,2,1,0,1,2,2,1,0,1 };
  int dist2_1[16] = { 4,3,4,5,5,4,3,4,3,1,2,3,3,2,1,2 };

  for (i = 0; i < (1 << 16); i++) {
    _myGoalDist_0[i] = 0;
    _yourGoalDist_0[i] = 0;
    bitCountTable[i] = 0;
    for (j = 0; j < 16; j++) {
      if (((i >> j) & 1) == 0) continue;
      //マスjからゴールまでのマンハッタン距離
      _myGoalDist_0[i] += dist1_0[j];
      _yourGoalDist_0[i] += dist2_0[j];
      _myGoalDist_1[i] += dist1_1[j];
      _yourGoalDist_1[i] += dist2_1[j];
      bitCountTable[i]++;
    }
  }
}


Value Eval::evaluate_K(const Position& pos, int ply) {
  //Value v = getWinPlayer_K(pos, ply);
  //if (v != VALUE_ZERO) {
  //  return v;
  //}


  Value s0 = VALUE_ZERO, s1 = VALUE_ZERO;
  if(Game_::eval_pattern == 0){
    s0 = ExistWeight * pos.count<ALL_PIECES>(WHITE) - DistWeight * myGoalDist_1(pos.pieces(WHITE));
    s1 = ExistWeight * pos.count<PURPLE>(BLACK) - DistWeight * yourGoalDist_1(pos.pieces(BLACK));
  }
  else if (Game_::eval_pattern == 1) {
    s0 = /*ExistWeight * pos.count<BLUE>(WHITE)*/ - DistWeight * myGoalDist_1(pos.pieces(WHITE, BLUE));
    s1 = ExistWeight * pos.count<PURPLE>(BLACK) - DistWeight * yourGoalDist_1(pos.pieces(BLACK));
  }
  if (pos.side_to_move() == WHITE) return s0 - s1;
  else return s1 - s0;
}

//見るキャラ(WHITE,BLACK)にとってどれだけ嬉しいかを返す
//大きい値返せば、勝ちと見做してくれるかと思ったらそうでもなかった...
Value Eval::evaluate_P(const Position& pos, int ply) {
  if (Red::existRed) {
    return Eval::evaluate_K(pos, ply);
  }
  else {
    //Value v = getWinPlayer_P(Game_::bNum, pos, ply);
    //if (v != VALUE_ZERO) {
    //  return v;
    //}

    Value v = VALUE_ZERO;
    Color us = pos.side_to_move();
    //自分のGOALに近い場所にいる方が良い（敵駒より
    int MyMIN = 10, OpMIN = 10;
    const Square* wsq = pos.squares<ALL_PIECES>(us);
    for (Square sq = *wsq; sq != SQ_NONE; sq = *++wsq) {
      //int scr = yourGoalDist_0(1LL << sq);
      int scr;
      if (us == WHITE)
        scr = yourGoalDist_0(1LL << sq);
      else
        scr = myGoalDist_0(1LL << sq);
      if (MyMIN > scr)
        MyMIN = scr;
    }
    const Square* bsq = pos.squares<ALL_PIECES>(~us);
    for (Square sq = *bsq; sq != SQ_NONE; sq = *++bsq) {
      //int scr = yourGoalDist_0(1LL << sq);
      int scr;
      if (us == WHITE)
        scr = yourGoalDist_0(1LL << sq);
      else
        scr = myGoalDist_0(1LL << sq);
      if (OpMIN > scr)
        OpMIN = scr;
    }
    if (MyMIN > OpMIN) {
      v -= 10000;
    }
    //const Square* ksqs = pos.squares<GOAL>(us);
    //for (int i = 0; i < 2; i++) {
    //  int ksq_r = (int)rank_of(ksqs[0]);
    //  int ksq_f = (int)file_of(ksqs[0]);
    //  int MyMIN = 10, OpMIN = 10;
    //  for (int f = FILE_B; f <= FILE_G; f++) {
    //    for (int r = RANK_2; r <= RANK_7; r++) {
    //      if (pos.piece_on(make_square((File)f, (Rank)r)) == NO_PIECE)
    //        continue;
    //      if (color_of(pos.piece_on(make_square((File)f, (Rank)r))) == us) {
    //        int scr = abs((int)f - ksq_f) + abs((int)r - ksq_r);
    //        if(MyMIN > scr)
    //          MyMIN = scr;
    //      }
    //      else {
    //        int scr = abs((int)f - ksq_f) + abs((int)r - ksq_r);
    //        if(OpMIN > scr)
    //          OpMIN = scr;
    //      }
    //    }
    //  }
    //  if (MyMIN > OpMIN) {
    //    v -= 10000;
    //  }
    //}
    

    Value s0 = VALUE_ZERO, s1 = VALUE_ZERO;
    //赤晒し
    if (Game_::eval_pattern == 0) {
      s0 = ExistWeight * pos.count<ALL_PIECES>(WHITE) - DistWeight * myGoalDist_1(pos.pieces(WHITE));
      s1 = ExistWeight * pos.count<ALL_PIECES>(BLACK) - DistWeight * yourGoalDist_1(pos.pieces(BLACK));
    }
    else if (Game_::eval_pattern == 1) {
      s0 = /*ExistWeight * pos.count<BLUE>(WHITE)*/ - DistWeight * myGoalDist_1(pos.pieces(WHITE, RED));
      s1 = -DistWeight * yourGoalDist_1(pos.pieces(BLACK));
    }
    if (us == WHITE) return s0 - s1 + v;
    else return s1 - s0 + v;
  }
}
////評価関数. tebanプレイヤーの有利さを返す. teban=0…自分手番.
//int evaluate(int teban) {
//  int s0 = bb::weight1 * bb::bitCount(existB) - bb::weight2 * bb::myGoalDist(existB | existR);
//  int s1 = -bb::weight2 * bb::yourGoalDist(existP);
//  if (teban == 0) return s0 - s1;
//  return s1 - s0;
//}

namespace {

  //追いかけの判定
  bool isOikake(char board[][6], Move mv) {
    const int dy[4] = { -1, 0, 1, 0 };
    const int dx[4] = { 0, 1, 0, -1 };

    int from_y = rank_of(from_sq(mv)) - 1;
    int to_y = rank_of(to_sq(mv)) - 1;
    int from_x = file_of(from_sq(mv)) - 1;
    int to_x = file_of(to_sq(mv)) - 1;
    if (!is_ok_R(to_sq(mv))) return false;	//脱出手は「追いかけ」ではない
    if (board[to_y][to_x] == 'u') return false;			//駒を取る手は「追いかけ」ではない

    int i;
    for (i = 0; i < 4; i++) {
      int y = from_y + dy[i];
      int x = from_x + dx[i];
      if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
        break;
      }
    }
    if (i < 4) { return false; }	//動かす駒の「動かす前のマス」と隣接するマスに相手の駒があったら「追いかけ」ではない

    for (i = 0; i < 4; i++) {
      int y = to_y + dy[i];
      int x = to_x + dx[i];
      if (0 <= y && y < 6 && 0 <= x && x < 6 && board[y][x] == 'u') {
        break;
      }
    }
    if (i == 4) { return false; }	//動かす駒の「動かした後のマス」と隣接するマスに相手の駒がなかったら「追いかけ」ではない
    return true;	//「追いかけ」である
  }

  void moveHist(char prev[6][6], char now[6][6], Move mv) {
    int from_y = rank_of(from_sq(mv)) - 1;
    int from_x = file_of(from_sq(mv)) - 1;
    int to_y = rank_of(to_sq(mv)) - 1;
    int to_x = file_of(to_sq(mv)) - 1;
    int i, j;

    for (i = 0; i < 6; i++)
      for (j = 0; j < 6; j++)
        now[i][j] = prev[i][j];

    char color = prev[from_y][from_x];	//R, B, u
    now[to_y][to_x] = color;
    now[from_y][from_x] = '.';
  }
  void moveEval(int prev[6][6], int now[6][6], Move mv) {
    int from_y = rank_of(from_sq(mv)) - 1;
    int from_x = file_of(from_sq(mv)) - 1;
    int to_y = rank_of(to_sq(mv)) - 1;
    int to_x = file_of(to_sq(mv)) - 1;
    int i, j;

    for (i = 0; i < 6; i++)
      for (j = 0; j < 6; j++)
        now[i][j] = prev[i][j];

    now[to_y][to_x] = prev[from_y][from_x];
    now[from_y][from_x] = 0;
  }
  Move detectMove(char prev[6][6], char now[6][6]) {
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

    return make_move(make_square((File)(x + 1), (Rank)(y + 1)), make_square((File)(nx + 1), (Rank)(ny + 1)));
  }
}

namespace Red {
  int histCnt;
  char hist[350][6][6];	//R, B, u
  int eval[350][6][6];	//赤度
  bool existRed;
  bool bare;
}

//試合開始時に呼び出す
void Red::init() {
  Red::histCnt = 0;
  Red::bare = 0;
}

//自分が手を打ったときに呼び出す
void Red::myMove(Move mv) {
  moveHist(Red::hist[Red::histCnt - 1], Red::hist[Red::histCnt], mv);
  moveEval(Red::eval[Red::histCnt - 1], Red::eval[Red::histCnt], mv);
  Red::histCnt++;
}
//2手目以降の自分手番の最初に呼び出す。
void Red::myTurn(char board[6][6], const Position& pos) {
  int i, j;

  if (Red::histCnt == 0) {
    for (i = 0; i < 6; i++) {
      for (j = 0; j < 6; j++) {
        Red::hist[0][i][j] = board[i][j];
        Red::eval[0][i][j] = 0;
      }
    }
    Red::histCnt++;
    return;
  }

  for (i = 0; i < 6; i++)
    for (j = 0; j < 6; j++)
      Red::hist[Red::histCnt][i][j] = board[i][j];
  Red::histCnt++;

  Move mv = detectMove(Red::hist[Red::histCnt - 2], Red::hist[Red::histCnt - 1]);
  moveEval(Red::eval[Red::histCnt - 2], Red::eval[Red::histCnt - 1], mv);
  Square fsq = from_sq(mv), tsq = to_sq(mv);
  int from_y = rank_of(fsq) - 1;
  int to_y = rank_of(tsq) - 1;
  int from_x = file_of(fsq) - 1;
  int to_x = file_of(tsq) - 1;

  char block_op[6][6];
  int prevMyRed = 0;
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      char c = Red::hist[Red::histCnt - 2][i][j];
      if (c == 'R' || c == 'B')
        block_op[i][j] = 'u';
      else
        block_op[i][j] = '.';

      if (c == 'R')
        prevMyRed++;
    }
  }

  const int weightOikake = 5;
  const int weightOikakePinti = 1;	//相手がピンチなとき、わけわからん行動しそうなので、推定の信頼を低めに
  if (isOikake(block_op, mv)) {
    if (prevMyRed == 1) {
      Red::eval[Red::histCnt - 1][to_y][to_x] += weightOikakePinti;
    }
    else {
      Red::eval[Red::histCnt - 1][to_y][to_x] += weightOikake;
    }
  }

  //相手が動かした駒を見て、それが青だったら自分がどう頑張っても必ず負けるとき、赤だと思って
  //見捨てる。
  //本当はちゃんと「相手側の必勝手探索」を実装したかったけど、時間がないので手抜きで。
  const int weightHairi = 1000;
  if ((from_y == 5 && from_x == 0) || (from_y == 5 && from_x == 5)) {	//こいつ脱出しなかったから赤だゾ
    Red::eval[Red::histCnt - 1][to_y][to_x] += weightHairi;
  }


  //GOALできる位置の敵駒は赤（じゃないと勝てない）
  int MyMIN = 20, OpMIN = 20;
  int ri = 0, rj = 0;
  //const Square* wsq = pos.squares<ALL_PIECES>(WHITE);
  //for (Square sq = *wsq; sq != SQ_NONE; sq = *++wsq) {
  //  int scr = yourGoalDist_0(1LL<<sq);
  //  if (MyMIN > scr)
  //    MyMIN = scr;
  //}
  //const Square* bsq = pos.squares<ALL_PIECES>(BLACK);
  //for (Square sq = *bsq; sq != SQ_NONE; sq = *++bsq) {
  //  int scr = yourGoalDist_0(1LL << sq);
  //  if (OpMIN > scr) {
  //    OpMIN = scr;
  //    ri = rank_of(sq);
  //    rj = file_of(sq);
  //  }
  //}
  for (int i = 0; i <= 5; i++) {
    for (int j = 0; j <= 5; j++) {
      if (Red::hist[histCnt - 2][i][j] == '.')
        continue;
      if (Red::hist[histCnt-2][i][j] == 'u') {
        int scr = 5-i + j;
        if (OpMIN > scr) {
          OpMIN = scr;
          ri = i;
          rj = j;
        }
      }
      else {
        int scr = 5-i + j;
        if (MyMIN > scr)
          MyMIN = scr;
      }
    }
  }
  if (MyMIN > OpMIN) {
    if (from_x == rj && from_y == ri) {
      Red::eval[Red::histCnt - 1][to_y][to_x] += weightHairi;
    }
    else
      Red::eval[Red::histCnt - 1][ri][rj] += weightHairi;
  }
  MyMIN = 20; OpMIN = 20;
  for (int i = 0; i <= 5; i++) {
    for (int j = 0; j <= 5; j++) {
      if (Red::hist[histCnt - 2][i][j] == '.')
        continue;
      if (Red::hist[histCnt - 2][i][j] == 'u') {
        int scr = 5-i + 5-j;
        if (OpMIN > scr) {
          OpMIN = scr;
          ri = i;
          rj = j;
        }
      }
      else {
        int scr = 5-i + 5-j;
        if (MyMIN > scr)
          MyMIN = scr;
      }
    }
  }
  if (MyMIN > OpMIN) {
    if (from_x == rj && from_y == ri) {
      Red::eval[Red::histCnt - 1][to_y][to_x] += weightHairi;
    }
    else
      Red::eval[Red::histCnt - 1][ri][rj] += weightHairi;
  }


  //今脱出口にある相手駒が、直前に動かしてきたものでなければ、赤
  if (Red::hist[Red::histCnt - 1][5][0] == 'u' && !(to_y == 5 && to_x == 0)) {
    Red::eval[Red::histCnt - 1][5][0] += weightHairi;
  }
  if (Red::hist[Red::histCnt - 1][5][5] == 'u' && !(to_y == 5 && to_x == 5)) {
    Red::eval[Red::histCnt - 1][5][5] += weightHairi;
  }

  if (Red::hist[Red::histCnt - 1][0][0] == 'R') {
    Red::bare = 1;
  }
  if (Red::hist[Red::histCnt - 1][0][5] == 'R') {
    Red::bare = 1;
  }
}

//myMoveとかmyTurnとかを呼び出した直後に呼び出したい。
//赤度evalが閾値以上になった赤の現在位置を、赤度が大きいものからリストアップ
//実は最大のやつしか使ってなかったのでpicUpを作って使う
//閾値以上のやつは全部赤にしてしまえばよいのでは
//ソートされるということは評価値に使おうとしていたのかも
int Red::listUpRed(int posY[], int posX[], int X) {
  int i, j;

  typedef std::tuple<int, int, int> T;
  std::vector<T> vec;

  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      if (Red::eval[Red::histCnt - 1][i][j] >= X) {
        vec.push_back(T(Red::eval[Red::histCnt - 1][i][j], i, j));
      }
    }
  }

  sort(vec.begin(), vec.end(), std::greater<T>());
  for (i = 0; i < vec.size(); i++) {
    posY[i] = std::get<1>(vec[i]);
    posX[i] = std::get<2>(vec[i]);
  }
  return vec.size();
}

Square Red::picUpRed(int X) {
  int i, j;
  int max_eval = X;
  Square resq = SQ_NONE;
  for (i = 0; i < 6; i++) {
    for (j = 0; j < 6; j++) {
      if (Red::eval[Red::histCnt - 1][i][j] >= max_eval) {
        max_eval = Red::eval[Red::histCnt - 1][i][j];
        resq = make_square((File)(j + 1), (Rank)(i + 1));
      }
    }
  }
  return resq;
}