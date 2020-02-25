#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <stdlib.h>
#include <intrin.h>
typedef uint64_t u_int64_t;
#else
#include <unistd.h>
#endif

#ifdef __BMI2__
#include <immintrin.h>
#endif

#include "json.hpp"

const std::string MinicVersion = "dev";

// *** options
#define WITH_UCI
#define WITH_XBOARD
#define WITH_MAGIC
#define WITH_SYZYGY

// *** Add-ons
//#define IMPORTBOOK
//#define DEBUG_TOOL
//#define WITH_TEST_SUITE
//#define WITH_PGN_PARSER

// *** Tuning
//#define WITH_TIMER
//#define WITH_CLOP_SEARCH
#define WITH_TEXEL_TUNING

// *** Debug
//#define DEBUG_HASH
//#define DEBUG_PHASH
//#define DEBUG_MATERIAL
//#define DEBUG_APPLY
//#define DEBUG_PSEUDO_LEGAL
//#define DEBUG_HASH_ENTRY
//#define DEBUG_KING_CAP
//#define DEBUG_ACC
//#define DEBUG_PERFT

#ifdef WITH_TEXEL_TUNING
#define CONST_TEXEL_TUNING
#else
#define CONST_TEXEL_TUNING const
#endif

#ifdef WITH_CLOP_SEARCH
#define CONST_CLOP_TUNING
#else
#define CONST_CLOP_TUNING const
#endif

#define INFINITETIME TimeType(60ull*60ull*1000ull*24ull*30ull) // 1 month ...
#define STOPSCORE   ScoreType(-20000)
#define INFSCORE    ScoreType(15000)
#define MATE        ScoreType(10000)
#define WIN         ScoreType(5000)
#define INVALIDMOVE     int32_t(0xFFFF0000)
#define INVALIDBOOKMOVE int32_t(0xFFFFFFFF) // different for retrocompatibility ///@todo regenerate some books !
#define INVALIDMINIMOVE int16_t(0x0000)
#define NULLMOVE        int32_t(0xFFFF1111)
#define INVALIDSQUARE  -1
#define MAX_PLY      1024
#define MAX_MOVE      256   // 256 is enough I guess/home ...
#define MAX_DEPTH     127   // DepthType is a char, !!!do not go above 127!!!
#define MAX_HISTORY  1000

#define SQFILE(s) ((s)&7)
#define SQRANK(s) ((s)>>3)
#define ISOUTERFILE(x) (SQFILE(x) == 0 || SQFILE(x) == 7)
#define ISNEIGHBOUR(x,y) ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && abs(SQRANK(x) - SQRANK(y)) <= 1 && abs(SQFILE(x) - SQFILE(y)) <= 1)
#define PROMOTION_RANK(x) (SQRANK(x) == 0 || SQRANK(x) == 7)
#define PROMOTION_RANK_C(x,c) ((c==Co_Black && SQRANK(x) == 0) || (c==Co_White && SQRANK(x) == 7))
#define MakeSquare(f,r) Square(((r)<<3) + (f))
#define VFlip(s) ((s)^Sq_a8)
#define HFlip(s) ((s)^7)
#define SQR(x) ((x)*(x))
#define HSCORE(depth) ScoreType(SQR(std::min((int)depth, 16))*4)

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)
#define LINE_NAME(prefix, suffix) JOIN(JOIN(prefix,__LINE__),suffix)
#define JOIN(symbol1,symbol2) _DO_JOIN(symbol1,symbol2 )
#define _DO_JOIN(symbol1,symbol2) symbol1##symbol2

typedef std::chrono::system_clock Clock;
typedef signed short int DepthType;
typedef int32_t Move;         // invalid if < 0
typedef int16_t MiniMove;     // invalid if < 0
typedef signed char Square;   // invalid if < 0
typedef uint64_t Hash;        // invalid if == 0ull
typedef uint32_t MiniHash;    // invalid if == 0
typedef uint64_t Counter;
typedef uint64_t BitBoard;
typedef int16_t  ScoreType;
typedef int64_t  TimeType;
typedef int16_t  GenerationType;

const Hash nullHash = 0ull;
const BitBoard empty = 0ull;

inline bool VALIDMOVE(const Move & m){ return m != NULLMOVE && m != INVALIDMOVE; }
inline bool VALIDMOVE(const MiniMove & m){ return m != INVALIDMINIMOVE; }

#ifdef WITH_TIMER
#include "Add-On/timers.cc"
#else
#define START_TIMER
#define STOP_AND_SUM_TIMER(name)
#endif

inline MiniHash Hash64to32   (Hash h) { return (h >> 32) & 0xFFFFFFFF; }
inline MiniMove Move2MiniMove(Move m) { return m & 0xFFFF;} // skip score

enum GamePhase { MG=0, EG=1, GP_MAX=2 };
GamePhase operator++(GamePhase & g){g=GamePhase(g+1); return g;}

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }

struct EvalScore{ ///@todo use Stockfish trick (two short in one int) but it's hard to make it compatible with Texel tuning !
    std::array<ScoreType,GP_MAX> sc = {0};
    EvalScore(ScoreType mg,ScoreType eg):sc{mg,eg}{}
    EvalScore(ScoreType s):sc{s,s}{}
    EvalScore():sc{0,0}{}
    EvalScore(const EvalScore & e):sc{e.sc[MG],e.sc[EG]}{}

    inline ScoreType & operator[](GamePhase g){ return sc[g];}
    inline const ScoreType & operator[](GamePhase g)const{ return sc[g];}

    EvalScore& operator*=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]*=s[g]; return *this;}
    EvalScore& operator/=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]/=s[g]; return *this;}
    EvalScore& operator+=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]+=s[g]; return *this;}
    EvalScore& operator-=(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]-=s[g]; return *this;}
    EvalScore  operator *(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]*=s[g]; return e;}
    EvalScore  operator /(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]/=s[g]; return e;}
    EvalScore  operator +(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]+=s[g]; return e;}
    EvalScore  operator -(const EvalScore& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]-=s[g]; return e;}
    void       operator =(const EvalScore& s){for(GamePhase g=MG; g<GP_MAX; ++g){sc[g]=s[g];}}

    EvalScore& operator*=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]*=s; return *this;}
    EvalScore& operator/=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]/=s; return *this;}
    EvalScore& operator+=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]+=s; return *this;}
    EvalScore& operator-=(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g)sc[g]-=s; return *this;}
    EvalScore  operator *(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]*=s; return e;}
    EvalScore  operator /(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]/=s; return e;}
    EvalScore  operator +(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]+=s; return e;}
    EvalScore  operator -(const ScoreType& s)const{EvalScore e(*this); for(GamePhase g=MG; g<GP_MAX; ++g)e[g]-=s; return e;}
    void       operator =(const ScoreType& s){for(GamePhase g=MG; g<GP_MAX; ++g){sc[g]=s;}}

    EvalScore scale(float s_mg,float s_eg)const{ EvalScore e(*this); e[MG]= ScoreType(s_mg*e[MG]); e[EG]= ScoreType(s_eg*e[EG]); return e;}
};

template < typename T, int SIZE > struct OptList : public std::vector<T>{ OptList() : std::vector<T>(){std::vector<T>::reserve(SIZE);};};

typedef OptList<Move,MAX_MOVE> MoveList;
typedef std::vector<Move> PVList;

namespace DynamicConfig{
    bool mateFinder        = false;
    bool disableTT         = false;
    unsigned int ttSizeMb  = 128; // here in Mb, will be converted to real size next
    bool fullXboardOutput  = false;
    bool debugMode         = false;
    bool quiet             = true;
    std::string debugFile  = "minic.debug";
    unsigned int level     = 100;
    bool book              = false;
    std::string bookFile   = "book.bin";
    unsigned int threads   = 1;
    std::string syzygyPath = "";
    bool FRC               = false;
    bool UCIPonder         = false;
}

namespace Logging {
    enum COMType { CT_xboard = 0, CT_uci = 1 };
    COMType ct = CT_xboard;
    inline std::string showDate() {
        std::stringstream str;
        auto msecEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
        char buffer[64];
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point(msecEpoch));
        std::strftime(buffer, 63, "%Y-%m-%d %H:%M:%S", localtime(&tt));
        str << buffer << "-" << std::setw(3) << std::setfill('0') << msecEpoch.count() % 1000;
        return str.str();
    }
    enum LogLevel : unsigned char { logTrace = 0, logDebug = 1, logInfo = 2, logGUI = 3, logWarn = 4, logError = 5, logFatal = 6};
    const std::string _protocolComment[2] = { "# ", "info string " };
    const std::string _levelNames[7] = { "Trace ", "Debug ", "Info  ", "", "Warn  ", "Error ", "Fatal " };
    std::string backtrace() { return "@todo:: backtrace"; } ///@todo find a very simple portable implementation
    class LogIt {
        friend void init();
    public:
        LogIt(LogLevel loglevel) :_level(loglevel) {}
        template <typename T> Logging::LogIt & operator<<(T const & value) { _buffer << value; return *this; }
        ~LogIt() {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_level != logGUI) {
	           if ( ! DynamicConfig::quiet || _level > logGUI ){
                  std::cout       << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
                  if (_of) (*_of) << _protocolComment[ct] << _levelNames[_level] << showDate() << ": " << _buffer.str() << std::endl;
	           }
            }
            else {
                std::cout       << _buffer.str() << std::flush << std::endl;
                if (_of) (*_of) << _buffer.str() << std::flush << std::endl;
            }
            if (_level >= logError) std::cout << backtrace() << std::endl;
            if (_level >= logFatal) {
                exit(1);
            }
        }
    private:
        static std::mutex      _mutex;
        std::ostringstream     _buffer;
        LogLevel               _level;
        static std::unique_ptr<std::ofstream> _of;
    };
    inline void hellooo() { std::cout << Logging::_protocolComment[Logging::ct] << "This is Minic version " << MinicVersion << std::endl; }
    void init(){
        if ( DynamicConfig::debugMode ){
            if ( DynamicConfig::debugFile.empty()) DynamicConfig::debugFile = "minic.debug";
            LogIt::_of = std::unique_ptr<std::ofstream>(new std::ofstream(DynamicConfig::debugFile + "_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())));
        }
    }
    std::mutex LogIt::_mutex;
    std::unique_ptr<std::ofstream> LogIt::_of;
}

namespace SearchConfig{
const bool doWindow         = true;
const bool doPVS            = true;
const bool doNullMove       = true;
const bool doFutility       = true;
const bool doLMR            = true;
const bool doLMP            = true;
const bool doStaticNullMove = true;
const bool doRazoring       = true;
const bool doQFutility      = false;
const bool doProbcut        = true;
const bool doHistoryPruning = true;
const bool doCMHPruning     = true;
// first value if eval score is used, second if hash score is used
CONST_CLOP_TUNING ScoreType qfutilityMargin          [2] = {90 , 90};
CONST_CLOP_TUNING DepthType staticNullMoveMaxDepth   [2] = {6  , 6};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthCoeff [2] = {80 , 80};
CONST_CLOP_TUNING ScoreType staticNullMoveDepthInit  [2] = {0  , 0};
CONST_CLOP_TUNING DepthType razoringMaxDepth         [2] = {3  , 3};
CONST_CLOP_TUNING ScoreType razoringMarginDepthCoeff [2] = {0  , 0};
CONST_CLOP_TUNING ScoreType razoringMarginDepthInit  [2] = {200, 200};
CONST_CLOP_TUNING DepthType nullMoveMinDepth             = 2;
CONST_CLOP_TUNING DepthType nullMoveVerifDepth           = 64; // means off ...
CONST_CLOP_TUNING DepthType historyPruningMaxDepth       = 3;
CONST_CLOP_TUNING ScoreType historyPruningThresholdInit  = 0;
CONST_CLOP_TUNING ScoreType historyPruningThresholdDepth = 0;
CONST_CLOP_TUNING DepthType CMHMaxDepth                  = 4;
CONST_CLOP_TUNING DepthType futilityMaxDepth         [2] = {10 , 10};
CONST_CLOP_TUNING ScoreType futilityDepthCoeff       [2] = {160, 160};
CONST_CLOP_TUNING ScoreType futilityDepthInit        [2] = {0  , 0};
CONST_CLOP_TUNING DepthType iidMinDepth                  = 5;
CONST_CLOP_TUNING DepthType iidMinDepth2                 = 8;
CONST_CLOP_TUNING DepthType probCutMinDepth              = 5;
CONST_CLOP_TUNING int       probCutMaxMoves              = 5;
CONST_CLOP_TUNING ScoreType probCutMargin                = 80;
CONST_CLOP_TUNING DepthType lmrMinDepth                  = 2;
CONST_CLOP_TUNING DepthType singularExtensionDepth       = 8;
// on move / opponent
CONST_CLOP_TUNING ScoreType dangerLimitPruning[2]        = {900,900};
CONST_CLOP_TUNING ScoreType dangerLimitReduction[2]      = {700,700};

const int nlevel = 100;
const DepthType levelDepthMax[nlevel/10+1]   = {0,1,1,2,4,6,8,10,12,14,MAX_DEPTH};

const DepthType lmpMaxDepth = 10;
const int lmpLimit[][SearchConfig::lmpMaxDepth + 1] = { { 0, 3, 4, 6, 10, 15, 21, 28, 36, 45, 55 }, { 0, 5, 6, 9, 15, 23, 32, 42, 54, 68, 83 } };

DepthType lmrReduction[MAX_DEPTH][MAX_MOVE];
void initLMR() {
    Logging::LogIt(Logging::logInfo) << "Init lmr";
    for (int d = 0; d < MAX_DEPTH; d++) for (int m = 0; m < MAX_MOVE; m++) lmrReduction[d][m] = DepthType( log(d) * log(m) * 0.5);
}

ScoreType MvvLvaScores[6][6];
void initMvvLva(){
    Logging::LogIt(Logging::logInfo) << "Init mvv-lva" ;
    static const ScoreType IValues[6] = { 1, 2, 3, 5, 9, 20 }; ///@todo try N=B=3 !
    for(int v = 0; v < 6 ; ++v) for(int a = 0; a < 6 ; ++a) MvvLvaScores[v][a] = IValues[v] * 20 - IValues[a];
}

} // SearchConfig

namespace EvalConfig {

CONST_TEXEL_TUNING EvalScore imbalance_mines[5][5] = {
    // pawn knight bishop rook queen
    {  { 12, 99}                                                      }, // Pawn
    {  {205,234}, {-119,-247}                                         }, // Knight
    {  {262,129}, {-356,-290}, {-161,-242}                            }, // Bishop
    {  {393,395}, {-299,-251}, {-328,-404},{ -255, -462}              }, // Rook
    {  {507,473}, {-425,-350}, {-608,-510},{-1162,-1092}, {-392,-388} }  // Queen
};

CONST_TEXEL_TUNING EvalScore imbalance_theirs[5][5] = {
    // pawn knight bishop rook queen
    { {-188,-301}                                                  }, // Pawn
    { { 294, 284}, { -23,-17}                                      }, // Knight
    { { 243, 416}, {   3,-45},  { -58, -53}                        }, // Bishop
    { { 284, 626}, {  31,-54},  {-116,-153}, {-108,-146}           }, // Rook
    { { 705, 753}, { 394,351},  { 381, 307}, { 327, 329},  {36,25} }  // Queen
};

CONST_TEXEL_TUNING EvalScore PST[6][64] = {
  {
    {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
    {  98,  68},{ 133,  69},{  60,  78},{  95,  59},{  68,  91},{ 126,  65},{  34, 129},{ -11, 134},
    {  -6,  45},{   7,  40},{  23,  19},{  28, -14},{  65, -18},{  56,   3},{  25,  27},{ -20,  34},
    {  -2,  29},{  15,  13},{  13,   3},{  39, -20},{  39, -16},{  16,   0},{  18,   7},{  -9,  11},
    { -11,  19},{ -13,  14},{   7,   0},{  27, -14},{  27, -13},{  24,  -8},{   2,  -2},{ -11,   1},
    {  -6,   8},{ -11,   8},{   5,  -1},{   8,  -5},{  17,  -3},{  25,  -7},{  33, -14},{   9, -11},
    { -18,  17},{ -14,   6},{ -12,   9},{  -3,  -1},{   0,   8},{  30,  -4},{  33,  -9},{  -5, -10},
    {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0}
  },
  {
    {-167, -58},{ -89, -38},{ -34, -13},{ -49, -28},{  61, -31},{ -97, -27},{ -15, -63},{-107, -99},
    { -73, -85},{ -42, -85},{  71, -94},{  36, -55},{  23, -64},{  62, -90},{   7, -59},{ -17, -57},
    { -47, -33},{  60, -53},{  34, -27},{  62, -23},{  84, -31},{ 129, -33},{  73, -40},{  44, -44},
    {  -9, -17},{  12,   0},{  13,  18},{  44,  20},{  26,  22},{  69,   7},{   7,   6},{  22, -18},
    {  -9, -18},{   4,  -6},{  21,  20},{   1,  31},{  21,  19},{  15,  20},{  21,   4},{  -5, -18},
    { -28, -23},{  -8,  -3},{   7,   2},{  17,  15},{  18,   9},{  21,  -2},{  13, -20},{ -20, -22},
    { -29, -42},{ -53, -20},{  -3, -10},{   5, -14},{   9,  -8},{  -2, -20},{ -14, -23},{ -19, -44},
    {-105, -29},{ -23, -51},{ -58, -23},{ -33, -15},{ -17, -22},{ -28, -18},{ -26, -50},{ -23, -64}
  },
  {
    { -29, -14},{   4, -21},{ -82, -11},{ -37,  -8},{ -25,  -7},{ -42,  -9},{   7, -17},{  -8, -24},
    { -26,  -8},{  16,  -4},{ -18,   7},{ -13, -12},{  30,  -3},{  59, -13},{  18,  -4},{ -47, -14},
    { -15,   2},{  37,  -7},{  43,   0},{  40,  -1},{  35,  -2},{  50,   6},{  37,   0},{  -2,   4},
    {  -4,   0},{  22,  12},{  19,  14},{  51,   9},{  37,  14},{  37,  10},{  29,   2},{   0,   2},
    {  -6,  -5},{  13,   6},{  32,  13},{  28,  27},{  44,   7},{  33,  11},{  13,  -2},{   4,  -9},
    {  25, -12},{  42,   1},{  44,  12},{  36,   5},{  38,  11},{  60,   0},{  44,  -5},{  18, -15},
    {   4, -14},{  44,  -8},{  45,  -7},{  33,   1},{  39,   0},{  24,  -6},{  57,  -9},{   1, -27},
    { -33, -23},{  -3,  -9},{  26,  -7},{  -9,  11},{  -7,   5},{  12,   6},{ -39,  -5},{ -21, -17}
  },
  {
    {  32,  14},{  42,  10},{  32,  14},{  51,  11},{  63,  10},{   9,  12},{  31,   8},{  43,   5},
    {  27,  14},{  32,   9},{  58,   4},{  62,   2},{  80,  -7},{  67,   1},{  26,  10},{  44,   3},
    {  -5,   9},{  19,   6},{  26,   2},{  36,   1},{  17,   3},{  45,  -3},{  61,  -5},{  16,  -5},
    { -24,  14},{ -11,   4},{   7,  12},{  26,  -4},{  24,  -3},{  35,   0},{  -8,  -1},{ -20,   2},
    { -36,  10},{ -26,  11},{ -12,  10},{  -2,   3},{   9,  -8},{  -7,  -5},{   6,  -8},{ -22,  -9},
    { -45,   6},{ -23,   3},{ -16,  -2},{ -17,   0},{   3,  -9},{   0, -11},{  -5,  -8},{ -31, -14},
    { -33,   2},{ -14,  -3},{ -20,   5},{  -9,   3},{   0,  -8},{  10, -10},{  -6, -11},{ -60,  -1},
    {  -5,   3},{  -1,   8},{  12,   1},{  22,  -7},{  24,  -4},{  20,   4},{ -24,  14},{  -2, -21}
  },
  {
    { -28,  -9},{   0,  22},{  29,  22},{  12,  27},{  59,  27},{  44,  19},{  43,  10},{  45,  20},
    { -24, -17},{ -43,  20},{  -5,  32},{   1,  41},{ -16,  58},{  57,  25},{  28,  30},{  54,   0},
    { -13, -20},{ -17,   6},{   7,   9},{   8,  49},{  29,  47},{  56,  35},{  47,  19},{  57,   9},
    { -27,   3},{ -27,  22},{ -16,  24},{ -17,  45},{  -1,  57},{  17,  40},{  -2,  57},{  -1,  36},
    {   3, -18},{ -26,  28},{  -8,  19},{ -19,  47},{  -2,  31},{  -4,  34},{   5,  39},{  -3,  23},
    { -14, -16},{  20, -27},{  -7,  15},{   3,   6},{  -3,   9},{  12,  17},{  16,  10},{   5,   5},
    { -35, -22},{   0, -23},{  24, -30},{  20, -16},{  27, -16},{  15, -23},{  -3, -36},{   1, -32},
    {  -1, -33},{  -5, -28},{  10, -22},{  30, -41},{  -3,  -5},{ -25, -32},{ -31, -20},{ -50, -41}
  },
  {
    { -65, -74},{  23, -35},{  16, -18},{ -15, -18},{ -56, -11},{ -34,  15},{   2,   4},{  13, -17},
    {  29, -12},{  -1,  17},{ -20,  14},{  -7,  17},{  -8,  17},{  -4,  38},{ -38,  23},{ -29,  11},
    {  -9,  10},{  24,  17},{   2,  23},{ -16,  15},{ -20,  20},{   6,  45},{  22,  44},{ -22,  13},
    { -17,  -8},{ -20,  22},{ -12,  25},{ -27,  30},{ -30,  28},{ -25,  32},{ -14,  26},{ -36,   3},
    { -49, -18},{  -1,  -4},{ -27,  23},{ -39,  26},{ -46,  29},{ -44,  19},{ -33,  -1},{ -51, -14},
    { -14, -19},{ -14,  -4},{ -22,  14},{ -46,  21},{ -44,  22},{ -31,  15},{ -16,   3},{ -27, -11},
    {   1, -28},{   7, -17},{  -8,   4},{ -62,  20},{ -39,  17},{ -20,   3},{  25, -17},{  15, -23},
    { -15, -53},{  34, -52},{  16, -25},{ -55,  -6},{   2, -34},{ -27, -12},{  37, -41},{  16, -43}
  }
};

CONST_TEXEL_TUNING EvalScore   pawnShieldBonus       = {2, -2};
CONST_TEXEL_TUNING EvalScore   passerBonus[8]        = { { 0, 0 }, {20, -40} , {6, -19}, {-10, 13}, {10, 37}, {33, 71}, {42, 109}, {0, 0}};
CONST_TEXEL_TUNING EvalScore   rookBehindPassed      = { -8,41};
CONST_TEXEL_TUNING EvalScore   kingNearPassedPawn    = { -9,13};
enum PawnEvalSemiOpen{ Close=0, SemiOpen=1};
///@todo make this depends on file! (/rank?)
CONST_TEXEL_TUNING EvalScore   doublePawnMalus[2]    = {{ 28, 10 },{ 12, 17 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   isolatedPawnMalus[2]  = {{ 10,  5 },{ 15, 17 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   backwardPawnMalus[2]  = {{  2,  1 },{ 26, -6 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   holesMalus            = { -5, 1};
CONST_TEXEL_TUNING EvalScore   pieceFrontPawn        = { -13,13};
CONST_TEXEL_TUNING EvalScore   outpost               = { 14,19};
CONST_TEXEL_TUNING EvalScore   candidate[8]          = { {0, 0}, {-30, 11}, {-15,  0}, { 14,  6}, { 24, 51}, {-11, 14}, {-11, 14}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   protectedPasserBonus[8]={ {0, 0}, {  8, 17}, { 8 ,  4}, { 14,  2}, { 14, 11}, { 12, 19}, { 8 , 16}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   freePasserBonus[8]    = { {0, 0}, { -5, 27}, {-24, 19}, {-22, 22}, {-19, 41}, { -5, 74}, {-23, 77}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   pawnMobility          = { -1,15};
CONST_TEXEL_TUNING EvalScore   pawnSafeAtt           = { 34,12};
CONST_TEXEL_TUNING EvalScore   pawnSafePushAtt       = { 20, 6};
CONST_TEXEL_TUNING EvalScore   pawnlessFlank         = {-10,-19};
CONST_TEXEL_TUNING EvalScore   pawnStormMalus        = { 14,-22};
CONST_TEXEL_TUNING EvalScore   rookOnOpenFile        = { 54, 0};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOur = {  8, 3};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOpp = { 22, 0};

CONST_TEXEL_TUNING EvalScore   rookQueenSameFile     = {  6, -3};
CONST_TEXEL_TUNING EvalScore   rookFrontQueenMalus   = { -8,-18};
CONST_TEXEL_TUNING EvalScore   rookFrontKingMalus    = {-12,  5};
CONST_TEXEL_TUNING EvalScore   minorOnOpenFile       = { 11, -3};
//CONST_TEXEL_TUNING EvalScore   attQueenMalus[5]      = {{2,-5},{-16,4},{-40,-16},{-57,-3},{32,39}};

CONST_TEXEL_TUNING EvalScore   pinnedKing [5]        = { { -5, -9}, { 13, 65}, { -8, 66}, {-16, 61}, {-1, 27} };
CONST_TEXEL_TUNING EvalScore   pinnedQueen[5]        = { { 12,-34}, {-25, 10}, {  5, 10}, {  1, 10}, {31, 34} };

CONST_TEXEL_TUNING EvalScore   hangingPieceMalus     = {-24, -11};

CONST_TEXEL_TUNING EvalScore   threatByMinor[6]      = { { -11, -5 },{ -20,-27 },{ -19, -10 },{ -25, 11 },{ -16, -9 },{ 0, 0 } };
CONST_TEXEL_TUNING EvalScore   threatByRook[6]       = { {  -5, -5 },{ -11, -1 },{  -4,  -3 },{  -4, -1 },{  -7, -9 },{ 0, 0 } };
CONST_TEXEL_TUNING EvalScore   threatByQueen[6]      = { {  -4, 21 },{   1, -2 },{  20,  -9 },{  31, -7 },{  16, -6 },{ 0, 0 } };
CONST_TEXEL_TUNING EvalScore   threatByKing[6]       = { {  -6,-17 },{  -5,  1 },{  -4, -10 },{  -5, -7 },{   0,  0 },{ 0, 0 } };

CONST_TEXEL_TUNING EvalScore   adjKnight[9]          = { {-24,-27}, { -12, 9}, { -4, 18}, {  1, 17}, { 12, 22}, { 17, 24}, { 14, 46}, { 26, 40}, { 22, 10} };
CONST_TEXEL_TUNING EvalScore   adjRook[9]            = { { 24, 22}, {  9,  7}, { 15,  1}, {  1,  4}, {-17, 13}, {-23, 24}, {-24, 32}, {-23, 46}, { -3, 17} };
CONST_TEXEL_TUNING EvalScore   badBishop[9]          = { {-16,  3}, {-12, -1}, {-11,  8}, { -6, 14}, {  1, 18}, {  4, 30}, { 14, 31}, { 19, 48}, { 36, 68} };
CONST_TEXEL_TUNING EvalScore   bishopPairBonus[9]    = { { 31, 56}, { 31, 57}, { 27, 63}, { 14, 77}, { 24, 62}, { 29, 62}, { 37, 61}, { 38, 58}, { 33, 53} };
CONST_TEXEL_TUNING EvalScore   knightPairMalus       = { 4, -9};
CONST_TEXEL_TUNING EvalScore   rookPairMalus         = { 3,-14};
CONST_TEXEL_TUNING EvalScore   queenNearKing         = {-2,  9};

CONST_TEXEL_TUNING EvalScore MOB[6][29] = { {{ 23,-46}, { 23,  5}, { 29, 15}, { 32, 19}, { 40, 17}, { 39, 15}, { 28, 23}, { 35, 46}, { 29, 47} },
                                            {{-25,-34}, {-12, 10}, { -4, 22}, {  0, 28}, {  2, 31}, {  4, 33}, {  1, 33}, { 21, 19}, { 41, 22}, {45, 36}, {55, 37}, {71, 65}, {71, 56}, {120, 95} },
                                            {{  4,-43}, {  5, 14}, { 10, 48}, { 16, 51}, { 14, 60}, { 18, 62}, { 18, 67}, { 22, 69}, { 26, 72}, {41, 69}, {57, 66}, {56, 66}, {48, 70}, {57, 69}, {72, 56}},
                                            {{  6,-41}, { 13,-26}, { 16, -8}, { 18, -5}, { 21,  0}, { 17, 12}, { 17, 23}, { 17, 41}, { 13, 13}, {23, 47}, {28, 50}, {34, 24}, {16, 32}, {24, 87} },
                                            {{ -3,-64}, { -3,-25}, {  2,-11}, { -1, -7}, {  8,-10}, {  8, -4}, {  3, 15}, {  7, 14}, { 13, 15}, {21, 26}, { 8, 37}, {21, 43}, {25, 48}, {19, 45}, {24, 54}},
                                            {{  4,-17}, { -5, 31}, {-11, 41}, {-15, 46}, {-22, 48}, {-27, 40}, {-25, 43}, {-30, 40}, {-40, 23} } };

CONST_TEXEL_TUNING EvalScore initiative[4] = {{1,5}, {58,39}, {115,64}, {71,78}};

enum katt_att_def : unsigned char { katt_attack = 0, katt_defence = 1 };
CONST_TEXEL_TUNING ScoreType kingAttMax    = 423;
CONST_TEXEL_TUNING ScoreType kingAttTrans  = 44;
CONST_TEXEL_TUNING ScoreType kingAttScale  = 16;
CONST_TEXEL_TUNING ScoreType kingAttOffset = 10;
CONST_TEXEL_TUNING ScoreType kingAttWeight[2][6]    = { { 64, 224, 256, 129, 289, -32}, { 160, 96, 160, -2, 31, 0} };
CONST_TEXEL_TUNING ScoreType kingAttSafeCheck[6]    = {   128, 1184, 1152, 1056, 1024, 0};
CONST_TEXEL_TUNING ScoreType kingAttOpenfile        = 192;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOpp = 97;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOur = 63;
ScoreType kingAttTable[64]       = {0};

CONST_TEXEL_TUNING EvalScore tempo = {0,0}; //{27, 26};

double sigmoid(double x, double m = 1.f, double trans = 0.f, double scale = 1.f, double offset = 0.f){ return m / (1 + exp((trans - x) / scale)) - offset;}
void initEval(){ for(Square i = 0; i < 64; i++){ EvalConfig::kingAttTable[i] = (int) sigmoid(i,EvalConfig::kingAttMax,EvalConfig::kingAttTrans,EvalConfig::kingAttScale,EvalConfig::kingAttOffset); } }// idea taken from Topple

} // EvalConfig

inline ScoreType ScaleScore(EvalScore s, float gp){ return ScoreType(gp*s[MG] + (1.f-gp)*s[EG]);}

std::string startPosition = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
std::string fine70        = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1";
std::string shirov        = "6r1/2rp1kpp/2qQp3/p3Pp1P/1pP2P2/1P2KP2/P5R1/6R1 w - - 0 1";

enum Piece    : signed char{ P_bk = -6, P_bq = -5, P_br = -4, P_bb = -3, P_bn = -2, P_bp = -1, P_none = 0, P_wp = 1, P_wn = 2, P_wb = 3, P_wr = 4, P_wq = 5, P_wk = 6 };
Piece operator++(Piece & pp){pp=Piece(pp+1); return pp;}
const int PieceShift = 6;
enum Mat      : unsigned char{ M_t = 0, M_p, M_n, M_b, M_r, M_q, M_k, M_bl, M_bd, M_M, M_m };
Mat operator++(Mat & m){m=Mat(m+1); return m;}

ScoreType   Values[13]        = { -8000, -1103, -538, -393, -359, -85, 0, 85, 359, 393, 538, 1103, 8000 };
ScoreType   ValuesEG[13]      = { -8000, -1076, -518, -301, -290, -93, 0, 93, 290, 301, 518, 1076, 8000 };
std::string PieceNames[13]    = { "k", "q", "r", "b", "n", "p", " ", "P", "N", "B", "R", "Q", "K" };
const ScoreType dummyScore = 0;
const ScoreType *absValues[7]   = { &dummyScore, &Values  [P_wp + PieceShift], &Values  [P_wn + PieceShift], &Values  [P_wb + PieceShift], &Values  [P_wr + PieceShift], &Values  [P_wq + PieceShift], &Values  [P_wk + PieceShift] };
const ScoreType *absValuesEG[7] = { &dummyScore, &ValuesEG[P_wp + PieceShift], &ValuesEG[P_wn + PieceShift], &ValuesEG[P_wb + PieceShift], &ValuesEG[P_wr + PieceShift], &ValuesEG[P_wq + PieceShift], &ValuesEG[P_wk + PieceShift] };

std::string SquareNames[64]   = { "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
                                  "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
                                  "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
                                  "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
                                  "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
                                  "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
                                  "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
                                  "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8" };
std::string FileNames[8]      = { "a", "b", "c", "d", "e", "f", "g", "h" };
std::string RankNames[8]      = { "1", "2", "3", "4", "5", "6", "7", "8" };
enum Sq   : unsigned char { Sq_a1  = 0,Sq_b1,Sq_c1,Sq_d1,Sq_e1,Sq_f1,Sq_g1,Sq_h1,
                                 Sq_a2,Sq_b2,Sq_c2,Sq_d2,Sq_e2,Sq_f2,Sq_g2,Sq_h2,
                                 Sq_a3,Sq_b3,Sq_c3,Sq_d3,Sq_e3,Sq_f3,Sq_g3,Sq_h3,
                                 Sq_a4,Sq_b4,Sq_c4,Sq_d4,Sq_e4,Sq_f4,Sq_g4,Sq_h4,
                                 Sq_a5,Sq_b5,Sq_c5,Sq_d5,Sq_e5,Sq_f5,Sq_g5,Sq_h5,
                                 Sq_a6,Sq_b6,Sq_c6,Sq_d6,Sq_e6,Sq_f6,Sq_g6,Sq_h6,
                                 Sq_a7,Sq_b7,Sq_c7,Sq_d7,Sq_e7,Sq_f7,Sq_g7,Sq_h7,
                                 Sq_a8,Sq_b8,Sq_c8,Sq_d8,Sq_e8,Sq_f8,Sq_g8,Sq_h8};
enum File : unsigned char { File_a = 0,File_b,File_c,File_d,File_e,File_f,File_g,File_h};
enum Rank : unsigned char { Rank_1 = 0,Rank_2,Rank_3,Rank_4,Rank_5,Rank_6,Rank_7,Rank_8};

const Rank PromRank[2] = { Rank_8 , Rank_1 };
const Rank EPRank[2]   = { Rank_6 , Rank_3 };

enum CastlingTypes : unsigned char { CT_OOO = 0, CT_OO = 1 };
enum CastlingRights : unsigned char{ C_none = 0, C_wks = 1, C_wqs = 2, C_bks = 4, C_bqs = 8 };
CastlingRights operator&(const CastlingRights & a, const CastlingRights &b){return CastlingRights(char(a)&char(b));}
CastlingRights operator|(const CastlingRights & a, const CastlingRights &b){return CastlingRights(char(a)|char(b));}
CastlingRights operator~(const CastlingRights & a){return CastlingRights(~char(a));}
void operator&=(CastlingRights & a, const CastlingRights &b){ a = a & b;}
void operator|=(CastlingRights & a, const CastlingRights &b){ a = a | b;}

inline Square stringToSquare(const std::string & str) { return (str.at(1) - 49) * 8 + (str.at(0) - 97); }

enum MType : unsigned char{
    T_std        = 0,   T_capture    = 1,   T_reserved   = 2,   T_ep         = 3,
    T_promq      = 4,   T_promr      = 5,   T_promb      = 6,   T_promn      = 7,
    T_cappromq   = 8,   T_cappromr   = 9,   T_cappromb   = 10,  T_cappromn   = 11,
    T_wks        = 12,  T_wqs        = 13,  T_bks        = 14,  T_bqs        = 15
};

Piece promShift(MType mt){ assert(mt>=T_promq); assert(mt<=T_cappromn); return Piece(P_wq - (mt%4));} // awfull hack

enum Color : signed char{ Co_None  = -1,   Co_White = 0,   Co_Black = 1,   Co_End };
constexpr Color operator~(Color c){return Color(c^Co_Black);} // switch color
Color operator++(Color & c){c=Color(c+1); return c;}

// ttmove 10000, promcap >7000, cap 7000, checks 6000, killers 1800-1700-1600, counter 1500, castling 200, other by -1000 < history < 1000, bad cap <-7000.
ScoreType MoveScoring[16] = { 0, 7000, 7100, 6000, 3950, 3500, 3350, 3300, 7950, 7500, 7350, 7300, 200, 200, 200, 200 };

#ifdef __MINGW32__
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(BitBoard bb) { assert(bb != empty); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#define swapbits32(x) (__builtin_bswap32 (x))
#else
#ifdef _WIN32
#ifdef _WIN64
#define POPCOUNT(x)   __popcnt64(x)
#define bsf(x,i)      _BitScanForward64(&i,x)
#define swapbits(x)   (_byteswap_uint64 (x))
#define swapbits32(x) (_byteswap_ulong  (x))
#else // we are _WIN32 but not _WIN64
int popcount(uint64_t b){
    b = (b & 0x5555555555555555LU) + (b >> 1 & 0x5555555555555555LU);
    b = (b & 0x3333333333333333LU) + (b >> 2 & 0x3333333333333333LU);
    b = b + (b >> 4) & 0x0F0F0F0F0F0F0F0FLU;
    b = b + (b >> 8);
    b = b + (b >> 16);
    b = b + (b >> 32) & 0x0000007F;
    return (int)b;
}
const int index64[64] = {
    0,  1, 48,  2, 57, 49, 28,  3,
   61, 58, 50, 42, 38, 29, 17,  4,
   62, 55, 59, 36, 53, 51, 43, 22,
   45, 39, 33, 30, 24, 18, 12,  5,
   63, 47, 56, 27, 60, 41, 37, 16,
   54, 35, 52, 21, 44, 32, 23, 11,
   46, 26, 40, 15, 34, 20, 31, 10,
   25, 14, 19,  9, 13,  8,  7,  6
};
int bitScanForward(int64_t bb) {
    const uint64_t debruijn64 = 0x03f79d71b4cb0a89;
    assert(bb != empty);
    return index64[((bb & -bb) * debruijn64) >> 58];
}
#define POPCOUNT(x)   popcount(x)
#define bsf(x,i)      (i=bitScanForward(x))
#define swapbits(x)   (_byteswap_uint64 (x))
#define swapbits32(x) (_byteswap_ulong  (x))
#endif // _WIN64
#else // linux
#define POPCOUNT(x)   int(__builtin_popcountll(x))
inline int BitScanForward(BitBoard bb) { assert(bb != empty); return __builtin_ctzll(bb);}
#define bsf(x,i)      (i=BitScanForward(x))
#define swapbits(x)   (__builtin_bswap64 (x))
#define swapbits32(x) (__builtin_bswap32 (x))
#endif // linux
#endif

#define SquareToBitboard(k) BitBoard(1ull<<(k))
#define SquareToBitboardTable(k) BBTools::mask[k].bbsquare

inline ScoreType countBit(const BitBoard & b)           { return ScoreType(POPCOUNT(b));}
inline void      setBit  (      BitBoard & b, Square k) { b |= SquareToBitboard(k);}
inline void      unSetBit(      BitBoard & b, Square k) { b &= ~SquareToBitboard(k);}
inline bool      isSet   (const BitBoard & b, Square k) { return (SquareToBitboard(k) & b) != 0;}

enum BBSq : BitBoard { BBSq_a1 = SquareToBitboard(Sq_a1),BBSq_b1 = SquareToBitboard(Sq_b1),BBSq_c1 = SquareToBitboard(Sq_c1),BBSq_d1 = SquareToBitboard(Sq_d1),BBSq_e1 = SquareToBitboard(Sq_e1),BBSq_f1 = SquareToBitboard(Sq_f1),BBSq_g1 = SquareToBitboard(Sq_g1),BBSq_h1 = SquareToBitboard(Sq_h1),
                       BBSq_a2 = SquareToBitboard(Sq_a2),BBSq_b2 = SquareToBitboard(Sq_b2),BBSq_c2 = SquareToBitboard(Sq_c2),BBSq_d2 = SquareToBitboard(Sq_d2),BBSq_e2 = SquareToBitboard(Sq_e2),BBSq_f2 = SquareToBitboard(Sq_f2),BBSq_g2 = SquareToBitboard(Sq_g2),BBSq_h2 = SquareToBitboard(Sq_h2),
                       BBSq_a3 = SquareToBitboard(Sq_a3),BBSq_b3 = SquareToBitboard(Sq_b3),BBSq_c3 = SquareToBitboard(Sq_c3),BBSq_d3 = SquareToBitboard(Sq_d3),BBSq_e3 = SquareToBitboard(Sq_e3),BBSq_f3 = SquareToBitboard(Sq_f3),BBSq_g3 = SquareToBitboard(Sq_g3),BBSq_h3 = SquareToBitboard(Sq_h3),
                       BBSq_a4 = SquareToBitboard(Sq_a4),BBSq_b4 = SquareToBitboard(Sq_b4),BBSq_c4 = SquareToBitboard(Sq_c4),BBSq_d4 = SquareToBitboard(Sq_d4),BBSq_e4 = SquareToBitboard(Sq_e4),BBSq_f4 = SquareToBitboard(Sq_f4),BBSq_g4 = SquareToBitboard(Sq_g4),BBSq_h4 = SquareToBitboard(Sq_h4),
                       BBSq_a5 = SquareToBitboard(Sq_a5),BBSq_b5 = SquareToBitboard(Sq_b5),BBSq_c5 = SquareToBitboard(Sq_c5),BBSq_d5 = SquareToBitboard(Sq_d5),BBSq_e5 = SquareToBitboard(Sq_e5),BBSq_f5 = SquareToBitboard(Sq_f5),BBSq_g5 = SquareToBitboard(Sq_g5),BBSq_h5 = SquareToBitboard(Sq_h5),
                       BBSq_a6 = SquareToBitboard(Sq_a6),BBSq_b6 = SquareToBitboard(Sq_b6),BBSq_c6 = SquareToBitboard(Sq_c6),BBSq_d6 = SquareToBitboard(Sq_d6),BBSq_e6 = SquareToBitboard(Sq_e6),BBSq_f6 = SquareToBitboard(Sq_f6),BBSq_g6 = SquareToBitboard(Sq_g6),BBSq_h6 = SquareToBitboard(Sq_h6),
                       BBSq_a7 = SquareToBitboard(Sq_a7),BBSq_b7 = SquareToBitboard(Sq_b7),BBSq_c7 = SquareToBitboard(Sq_c7),BBSq_d7 = SquareToBitboard(Sq_d7),BBSq_e7 = SquareToBitboard(Sq_e7),BBSq_f7 = SquareToBitboard(Sq_f7),BBSq_g7 = SquareToBitboard(Sq_g7),BBSq_h7 = SquareToBitboard(Sq_h7),
                       BBSq_a8 = SquareToBitboard(Sq_a8),BBSq_b8 = SquareToBitboard(Sq_b8),BBSq_c8 = SquareToBitboard(Sq_c8),BBSq_d8 = SquareToBitboard(Sq_d8),BBSq_e8 = SquareToBitboard(Sq_e8),BBSq_f8 = SquareToBitboard(Sq_f8),BBSq_g8 = SquareToBitboard(Sq_g8),BBSq_h8 = SquareToBitboard(Sq_h8)};

const BitBoard whiteSquare               = 0x55AA55AA55AA55AA; const BitBoard blackSquare               = 0xAA55AA55AA55AA55;
//const BitBoard whiteSideSquare           = 0x00000000FFFFFFFF; const BitBoard blackSideSquare           = 0xFFFFFFFF00000000;
const BitBoard fileA                     = 0x0101010101010101;
const BitBoard fileB                     = 0x0202020202020202;
const BitBoard fileC                     = 0x0404040404040404;
const BitBoard fileD                     = 0x0808080808080808;
const BitBoard fileE                     = 0x1010101010101010;
const BitBoard fileF                     = 0x2020202020202020;
const BitBoard fileG                     = 0x4040404040404040;
const BitBoard fileH                     = 0x8080808080808080;
const BitBoard files[8] = {fileA,fileB,fileC,fileD,fileE,fileF,fileG,fileH};
const BitBoard rank1                     = 0x00000000000000ff;
const BitBoard rank2                     = 0x000000000000ff00;
const BitBoard rank3                     = 0x0000000000ff0000;
const BitBoard rank4                     = 0x00000000ff000000;
const BitBoard rank5                     = 0x000000ff00000000;
const BitBoard rank6                     = 0x0000ff0000000000;
const BitBoard rank7                     = 0x00ff000000000000;
const BitBoard rank8                     = 0xff00000000000000;
const BitBoard ranks[8] = {rank1,rank2,rank3,rank4,rank5,rank6,rank7,rank8};
//const BitBoard center = BBSq_d4 | BBSq_d5 | BBSq_e4 | BBSq_e5;
/*
const BitBoard extendedCenter = BBSq_c3 | BBSq_c4 | BBSq_c5 | BBSq_c6
                              | BBSq_d3 | BBSq_d4 | BBSq_d5 | BBSq_d6
                              | BBSq_e3 | BBSq_e4 | BBSq_e5 | BBSq_e6
                              | BBSq_f3 | BBSq_f4 | BBSq_f5 | BBSq_f6;
*/

const BitBoard holesZone[2] = { rank2 | rank3 | rank4 | rank5,  rank4 | rank5 | rank6 | rank7 };
const BitBoard queenSide   = fileA | fileB | fileC | fileD;
const BitBoard centerFiles = fileC | fileD | fileE | fileF;
const BitBoard kingSide    = fileE | fileF | fileG | fileH;
const BitBoard kingFlank[8] = { queenSide ^ fileD, queenSide, queenSide, centerFiles, centerFiles, kingSide, kingSide, kingSide ^ fileE };

std::string showBitBoard(const BitBoard & b) {
    std::bitset<64> bs(b);
    std::stringstream ss;
    for (int j = 7; j >= 0; --j) {
        ss << "\n" << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+" << std::endl << Logging::_protocolComment[Logging::ct] << "|";
        for (int i = 0; i < 8; ++i) ss << (bs[i + j * 8] ? "X" : " ") << '|';
    }
    ss << "\n" << Logging::_protocolComment[Logging::ct] << "+-+-+-+-+-+-+-+-+";
    return ss.str();
}

struct Position; // forward decl
bool readFEN(const std::string & fen, Position & p, bool silent = false, bool withMoveount = false); // forward decl

struct Position{
    std::array<Piece,64>    b    {{ P_none }}; // works because P_none is in fact 0 ...
    std::array<BitBoard,13> allB {{ empty }};

    BitBoard allPieces[2] = {empty};
    BitBoard occupancy    = empty;

    // t p n b r q k bl bd M n  (total is first so that pawn to king is same a Piece)
    typedef std::array<std::array<char,11>,2> Material;
    Material mat = {{{{0}}}}; // such a nice syntax ...

    mutable Hash h = nullHash, ph = nullHash;
    Move lastMove = INVALIDMOVE;
    Square ep = INVALIDSQUARE, king[2] = { INVALIDSQUARE, INVALIDSQUARE }, rooksInit[2][2] = { {INVALIDSQUARE, INVALIDSQUARE}, {INVALIDSQUARE, INVALIDSQUARE}}, kingInit[2] = {INVALIDSQUARE, INVALIDSQUARE};
    unsigned char fifty = 0;
    unsigned short int moves = 0, halfmoves = 0;
    CastlingRights castling = C_none;
    Color c = Co_White;

    inline const BitBoard & blackKing  ()const {return allB[0];}
    inline const BitBoard & blackQueen ()const {return allB[1];}
    inline const BitBoard & blackRook  ()const {return allB[2];}
    inline const BitBoard & blackBishop()const {return allB[3];}
    inline const BitBoard & blackKnight()const {return allB[4];}
    inline const BitBoard & blackPawn  ()const {return allB[5];}
    inline const BitBoard & whitePawn  ()const {return allB[7];}
    inline const BitBoard & whiteKnight()const {return allB[8];}
    inline const BitBoard & whiteBishop()const {return allB[9];}
    inline const BitBoard & whiteRook  ()const {return allB[10];}
    inline const BitBoard & whiteQueen ()const {return allB[11];}
    inline const BitBoard & whiteKing  ()const {return allB[12];}

    template<Piece pp>
    inline const BitBoard & pieces(Color c)const{ return allB[(1-2*c)*pp+PieceShift]; }
    inline const BitBoard & pieces(Color c, Piece pp)const{ return allB[(1-2*c)*pp+PieceShift]; }

    Position(){}
    Position(const std::string & fen, bool withMoveCount = true){readFEN(fen,*this,true,withMoveCount);}
};

namespace PieceTools{
inline Piece getPieceIndex  (const Position &p, Square k){ assert(k >= 0 && k < 64); return Piece(p.b[k] + PieceShift);}
inline Piece getPieceType   (const Position &p, Square k){ assert(k >= 0 && k < 64); return (Piece)std::abs(p.b[k]);}
inline std::string getName  (const Position &p, Square k){ assert(k >= 0 && k < 64); return PieceNames[getPieceIndex(p,k)];}
inline ScoreType getValue   (const Position &p, Square k){ assert(k >= 0 && k < 64); return Values[getPieceIndex(p,k)];}
inline ScoreType getAbsValue(const Position &p, Square k){ assert(k >= 0 && k < 64); return std::abs(Values[getPieceIndex(p,k)]); }
}

namespace BBTools {
inline constexpr BitBoard _shiftSouth    (BitBoard b) { return b >> 8; }
inline constexpr BitBoard _shiftNorth    (BitBoard b) { return b << 8; }
inline constexpr BitBoard _shiftWest     (BitBoard b) { return b >> 1 & ~fileH; }
inline constexpr BitBoard _shiftEast     (BitBoard b) { return b << 1 & ~fileA; }
inline constexpr BitBoard _shiftNorthEast(BitBoard b) { return b << 9 & ~fileA; }
inline constexpr BitBoard _shiftNorthWest(BitBoard b) { return b << 7 & ~fileH; }
inline constexpr BitBoard _shiftSouthEast(BitBoard b) { return b >> 7 & ~fileA; }
inline constexpr BitBoard _shiftSouthWest(BitBoard b) { return b >> 9 & ~fileH; }

template<Color C> inline constexpr BitBoard shiftN  (const BitBoard b) { return C==Co_White? BBTools::_shiftNorth(b)     : BBTools::_shiftSouth(b);    }
template<Color C> inline constexpr BitBoard shiftS  (const BitBoard b) { return C!=Co_White? BBTools::_shiftNorth(b)     : BBTools::_shiftSouth(b);    }
template<Color C> inline constexpr BitBoard shiftSW (const BitBoard b) { return C==Co_White? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);}
template<Color C> inline constexpr BitBoard shiftSE (const BitBoard b) { return C==Co_White? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);}
template<Color C> inline constexpr BitBoard shiftNW (const BitBoard b) { return C!=Co_White? BBTools::_shiftSouthWest(b) : BBTools::_shiftNorthWest(b);}
template<Color C> inline constexpr BitBoard shiftNE (const BitBoard b) { return C!=Co_White? BBTools::_shiftSouthEast(b) : BBTools::_shiftNorthEast(b);}

template<Color  > inline constexpr BitBoard fillForward(BitBoard b);
template<>        inline constexpr BitBoard fillForward<Co_White>(BitBoard b) {  b |= (b << 8u);    b |= (b << 16u);    b |= (b << 32u);    return b;}
template<>        inline constexpr BitBoard fillForward<Co_Black>(BitBoard b) {  b |= (b >> 8u);    b |= (b >> 16u);    b |= (b >> 32u);    return b;}
template<Color C> inline constexpr BitBoard frontSpan(BitBoard b) { return fillForward<C>(shiftN<C>(b));}
template<Color C> inline constexpr BitBoard rearSpan (BitBoard b) { return frontSpan<~C>(b);}
template<Color C> inline constexpr BitBoard pawnSemiOpen(BitBoard own, BitBoard opp) { return own & ~frontSpan<~C>(opp);}
inline constexpr BitBoard fillFile(BitBoard b) { return fillForward<Co_White>(b) | fillForward<Co_Black>(b);}
inline constexpr BitBoard openFiles(BitBoard w, BitBoard b) { return ~fillFile(w) & ~fillFile(b);}

template<Color C> inline constexpr BitBoard pawnAttacks(BitBoard b)       { return shiftNW<C>(b) | shiftNE<C>(b);}
template<Color C> inline constexpr BitBoard pawnDoubleAttacks(BitBoard b) { return shiftNW<C>(b) & shiftNE<C>(b);}
template<Color C> inline constexpr BitBoard pawnSingleAttacks(BitBoard b) { return shiftNW<C>(b) ^ shiftNE<C>(b);}

template<Color C> inline constexpr BitBoard pawnHoles(BitBoard b)   { return ~fillForward<C>(pawnAttacks<C>(b));}
template<Color C> inline constexpr BitBoard pawnDoubled(BitBoard b) { return frontSpan<C>(b) & b;}

inline constexpr BitBoard pawnIsolated(BitBoard b) { return b & ~fillFile(_shiftEast(b)) & ~fillFile(_shiftWest(b));}

template<Color C> inline constexpr BitBoard pawnBackward       (BitBoard own, BitBoard opp) {return shiftN<~C>( (shiftN<C>(own)& ~opp) & ~fillForward<C>(pawnAttacks<C>(own)) & (pawnAttacks<~C>(opp)));}
template<Color C> inline constexpr BitBoard pawnForwardCoverage(BitBoard bb               ) { BitBoard spans = frontSpan<C>(bb); return spans | _shiftEast(spans) | _shiftWest(spans);}
template<Color C> inline constexpr BitBoard pawnPassed         (BitBoard own, BitBoard opp) { return own & ~pawnForwardCoverage<~C>(opp);}
template<Color C> inline constexpr BitBoard pawnCandidates     (BitBoard own, BitBoard opp) { return pawnSemiOpen<C>(own, opp) & shiftN<~C>((pawnSingleAttacks<C>(own) & pawnSingleAttacks<~C>(opp)) | (pawnDoubleAttacks<C>(own) & pawnDoubleAttacks<~C>(opp)));}
//template<Color C> inline constexpr BitBoard pawnStraggler      (BitBoard own, BitBoard opp, BitBoard own_backwards) { return own_backwards & pawnSemiOpen<C>(own, opp) & (C ? 0x00ffff0000000000ull : 0x0000000000ffff00ull);} ///@todo use this !

int popBit(BitBoard & b) {
    assert( b != empty);
    unsigned long i = 0;
    bsf(b, i);
    b &= b - 1;
    return i;
}

// HQ BB code and init inspired by Amoeba/Dumb
struct Mask {
    static int ranks[512];
    BitBoard bbsquare, diagonal, antidiagonal, file, kingZone, pawnAttack[2], push[2], dpush[2], enpassant, knight, king, frontSpan[2], rearSpan[2], passerSpan[2], attackFrontSpan[2], between[64];
    Mask():bbsquare(empty), diagonal(empty), antidiagonal(empty), file(empty), kingZone(empty), pawnAttack{ empty,empty }, push{ empty,empty }, dpush{ empty,empty }, enpassant(empty), knight(empty), king(empty), frontSpan{empty}, rearSpan{empty}, passerSpan{empty}, attackFrontSpan{empty}, between{empty}{}
};
int Mask::ranks[512] = {0};
Mask mask[64];

inline void initMask() {
    Logging::LogIt(Logging::logInfo) << "Init mask" ;
    int d[64][64] = { {0} };
    for (Square x = 0; x < 64; ++x) {
        mask[x].bbsquare = SquareToBitboard(x);
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                for (int r = SQRANK(x) + i, f = SQFILE(x) + j; 0 <= r && r < 8 && 0 <= f && f < 8; r += i, f += j) {
                    const int y = 8 * r + f;
                    d[x][y] = (8 * i + j);
                    for (int z = x + d[x][y]; z != y; z += d[x][y]) mask[x].between[y] |= SquareToBitboard(z);
                }
                const int r = SQRANK(x);
                const int f = SQFILE(x);
                if ( 0 <= r+i && r+i < 8 && 0 <= f+j && f+j < 8) mask[x].kingZone |= SquareToBitboard((SQRANK(x) + i)*8+SQFILE(x) + j);
            }
        }

        for (int y = x - 9; y >= 0 && d[x][y] == -9; y -= 9) mask[x].diagonal |= SquareToBitboard(y);
        for (int y = x + 9; y < 64 && d[x][y] ==  9; y += 9) mask[x].diagonal |= SquareToBitboard(y);

        for (int y = x - 7; y >= 0 && d[x][y] == -7; y -= 7) mask[x].antidiagonal |= SquareToBitboard(y);
        for (int y = x + 7; y < 64 && d[x][y] ==  7; y += 7) mask[x].antidiagonal |= SquareToBitboard(y);

        for (int y = x - 8; y >= 0; y -= 8) mask[x].file |= SquareToBitboard(y);
        for (int y = x + 8; y < 64; y += 8) mask[x].file |= SquareToBitboard(y);

        int f = SQFILE(x);
        int r = SQRANK(x);
        for (int i = -1, c = 1, dp = 6; i <= 1; i += 2, c = 0, dp = 1) {
            for (int j = -1; j <= 1; j += 2) if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) {  mask[x].pawnAttack[c] |= SquareToBitboard((r + i) * 8 + (f + j)); }
            if (0 <= r + i && r + i < 8) {
                mask[x].push[c] = SquareToBitboard((r + i) * 8 + f);
                if ( r == dp ) mask[x].dpush[c] = SquareToBitboard((r + 2*i) * 8 + f); // double push
            }
        }
        if (r == 3 || r == 4) {
            if (f > 0) mask[x].enpassant |= SquareToBitboard(x - 1);
            if (f < 7) mask[x].enpassant |= SquareToBitboard(x + 1);
        }

        for (int i = -2; i <= 2; i = (i == -1 ? 1 : i + 1)) {
            for (int j = -2; j <= 2; ++j) {
                if (i == j || i == -j || j == 0) continue;
                if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].knight |= SquareToBitboard(8 * (r + i) + (f + j)); }
            }
        }

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                if (0 <= r + i && r + i < 8 && 0 <= f + j && f + j < 8) { mask[x].king |= SquareToBitboard(8 * (r + i) + (f + j)); }
            }
        }

        BitBoard wspan = SquareToBitboardTable(x);
        wspan |= wspan << 8; wspan |= wspan << 16; wspan |= wspan << 32;
        wspan = _shiftNorth(wspan);
        BitBoard bspan = SquareToBitboardTable(x);
        bspan |= bspan >> 8; bspan |= bspan >> 16; bspan |= bspan >> 32;
        bspan = _shiftSouth(bspan);

        mask[x].frontSpan[Co_White] = mask[x].rearSpan[Co_Black] = wspan;
        mask[x].frontSpan[Co_Black] = mask[x].rearSpan[Co_White] = bspan;

        mask[x].passerSpan[Co_White] = wspan;
        mask[x].passerSpan[Co_White] |= _shiftWest(wspan);
        mask[x].passerSpan[Co_White] |= _shiftEast(wspan);
        mask[x].passerSpan[Co_Black] = bspan;
        mask[x].passerSpan[Co_Black] |= _shiftWest(bspan);
        mask[x].passerSpan[Co_Black] |= _shiftEast(bspan);

        mask[x].attackFrontSpan[Co_White] = _shiftWest(wspan);
        mask[x].attackFrontSpan[Co_White] |= _shiftEast(wspan);
        mask[x].attackFrontSpan[Co_Black] = _shiftWest(bspan);
        mask[x].attackFrontSpan[Co_Black] |= _shiftEast(bspan);
    }

    for (Square o = 0; o < 64; ++o) {
        for (int k = 0; k < 8; ++k) {
            int y = 0;
            for (int x = k - 1; x >= 0; --x) {
                const BitBoard b = SquareToBitboard(x);
                y |= b;
                if (((o << 1) & b) == b) break;
            }
            for (int x = k + 1; x < 8; ++x) {
                const BitBoard b = SquareToBitboard(x);
                y |= b;
                if (((o << 1) & b) == b) break;
            }
            Mask::ranks[o * 8 + k] = y;
        }
    }
}

#ifndef WITH_MAGIC // then use HQBB

inline BitBoard attack(const BitBoard occupancy, const Square x, const BitBoard m) {
    START_TIMER
    BitBoard forward = occupancy & m;
    BitBoard reverse = swapbits(forward);
    forward -= SquareToBitboard(x);
    reverse -= SquareToBitboard(x^63);
    forward ^= swapbits(reverse);
    forward &= m;
    STOP_AND_SUM_TIMER(Attack)
    return forward;
}

inline BitBoard rankAttack(const BitBoard occupancy, const Square x) {
    const int f = SQFILE(x); const int r = x & 56;
    return BitBoard(ranks[((occupancy >> r) & 126) * 4 + f]) << r;
}

inline BitBoard fileAttack        (const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].file);         }
inline BitBoard diagonalAttack    (const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].diagonal);     }
inline BitBoard antidiagonalAttack(const BitBoard occupancy, const Square x) { return attack(occupancy, x, mask[x].antidiagonal); }

template < Piece > BitBoard coverage(const Square x, const BitBoard occupancy = 0, const Color c = Co_White) { assert(false); return empty; }
template <       > BitBoard coverage<P_wp>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].pawnAttack[c]; }
template <       > BitBoard coverage<P_wn>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].knight; }
template <       > BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x); }
template <       > BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return fileAttack    (occupancy, x) | rankAttack        (occupancy, x); }
template <       > BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return diagonalAttack(occupancy, x) | antidiagonalAttack(occupancy, x) | fileAttack(occupancy, x) | rankAttack(occupancy, x); }
template <       > BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].king; }

template < Piece pp > inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

#else // MAGIC

namespace MagicBB{ // inspired by Rubi and Stockfish
#define BISHOP_INDEX_BITS 9
#define ROOK_INDEX_BITS 12
struct SMagic { BitBoard mask, magic; };
SMagic bishop[64];
SMagic rook[64];
BitBoard bishopAttacks[64][1 << BISHOP_INDEX_BITS];
BitBoard rookAttacks[64][1 << ROOK_INDEX_BITS];
#ifdef __BMI2__
#define MAGICBISHOPINDEX(m,x) (_pext_u64(m, MagicBB::bishop[x].mask))
#define MAGICROOKINDEX(m,x)   (_pext_u64(m, MagicBB::rook  [x].mask))
#else
#define MAGICBISHOPINDEX(m,x) (int)((((m) & MagicBB::bishop[x].mask) * MagicBB::bishop[x].magic) >> (64 - BISHOP_INDEX_BITS))
#define MAGICROOKINDEX(m,x)   (int)((((m) & MagicBB::rook  [x].mask) * MagicBB::rook  [x].magic) >> (64 - ROOK_INDEX_BITS))
#endif
#define MAGICBISHOPATTACKS(m,x) (MagicBB::bishopAttacks[x][MAGICBISHOPINDEX(m,x)])
#define MAGICROOKATTACKS(m,x)   (MagicBB::rookAttacks  [x][MAGICROOKINDEX(m,x)])

const BitBoard bishopMagics[] = {
    0x1002004102008200, 0x1002004102008200, 0x4310002248214800, 0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x402010c110014208, 0xa000a06240114001,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x100c009840001000, 0x4310002248214800, 0xa000a06240114001, 0x4310002248214800,
    0x4310002248214800, 0x822143005020a148, 0x0001901c00420040, 0x0880504024308060, 0x0100201004200002, 0xa000a06240114001, 0x822143005020a148, 0x1002004102008200,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x2008080100820102, 0x1481010004104010, 0x0002052000100024, 0xc880221002060081, 0xc880221002060081,
    0x4310002248214800, 0xc880221002060081, 0x0001901c00420040, 0x8400208020080201, 0x000e008400060020, 0x00449210e3902028, 0x402010c110014208, 0xc880221002060081,
    0x100c009840001000, 0xc880221002060081, 0x1000820800c00060, 0x2803101084008800, 0x2200608200100080, 0x0040900130840090, 0x0024010008800a00, 0x0400110410804810,
    0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200,
    0xa000a06240114001, 0x4310002248214800, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200
};

const BitBoard rookMagics[] = {
    0x8200108041020020, 0x8200108041020020, 0xc880221002060081, 0x0009100804021000, 0x0500010004107800, 0x0024010008800a00, 0x0400110410804810, 0x8300038100004222,
    0x004a800182c00020, 0x0009100804021000, 0x3002200010c40021, 0x0020100104000208, 0x01021001a0080020, 0x0884020010082100, 0x1000820800c00060, 0x8020480110020020,
    0x0002052000100024, 0x0200190040088100, 0x0030802001a00800, 0x8010002004000202, 0x0040010100080010, 0x2200608200100080, 0x0001901c00420040, 0x0001400a24008010,
    0x1400a22008001042, 0x8200108041020020, 0x2004500023002400, 0x8105100028001048, 0x8010024d00014802, 0x8000820028030004, 0x402010c110014208, 0x8300038100004222,
    0x0001804002800124, 0x0084022014041400, 0x0030802001a00800, 0x0110a01001080008, 0x0b10080850081100, 0x000010040049020c, 0x0024010008800a00, 0x014c800040100426,
    0x1100400010208000, 0x0009100804021000, 0x0010024871202002, 0x8014001028c80801, 0x1201082010a00200, 0x0002008004102009, 0x8300038100004222, 0x0000401001a00408,
    0x4520920010210200, 0x0400110410804810, 0x8105100028001048, 0x8105100028001048, 0x0802801009083002, 0x8200108041020020, 0x8200108041020020, 0x4000a12400848110,
    0x2000804026001102, 0x2000804026001102, 0x800040a010040901, 0x80001802002c0422, 0x0010b018200c0122, 0x200204802a080401, 0x8880604201100844, 0x80000cc281092402
};

BitBoard computeAttacks(int index, BitBoard occ, int delta){
    BitBoard attacks = empty, blocked = empty;
    for (int shift = index + delta; ISNEIGHBOUR(shift, shift - delta); shift += delta) {
        if (!blocked) attacks |= SquareToBitboard(shift);
        blocked |= ((1ULL << shift) & occ);
    }
    return attacks;
}

BitBoard occupiedFromIndex(int j, BitBoard mask){
    BitBoard occ = empty;
    int i = 0;
    while (mask){
        const int k = popBit(mask);
        if (j & SquareToBitboard(i)) occ |= (1ULL << k);
        i++;
    }
    return occ;
}

void initMagic(){
    Logging::LogIt(Logging::logInfo) << "Init magic" ;
    for (Square from = 0; from < 64; from++) {
        bishop[from].mask = empty;
        rook[from].mask = empty;
        for (Square j = 0; j < 64; j++){
            if (from == j) continue;
            if (SQRANK(from) == SQRANK(j) && !ISOUTERFILE(j))    rook[from].mask |= SquareToBitboard(j);
            if (SQFILE(from) == SQFILE(j) && !PROMOTION_RANK(j)) rook[from].mask |= SquareToBitboard(j);
            if (abs(SQRANK(from) - SQRANK(j)) == abs(SQFILE(from) - SQFILE(j)) && !ISOUTERFILE(j) && !PROMOTION_RANK(j)) bishop[from].mask |= SquareToBitboard(j);
        }
        bishop[from].magic = bishopMagics[from];
        for (int j = 0; j < (1 << BISHOP_INDEX_BITS); j++) {
            const BitBoard occ = occupiedFromIndex(j, bishop[from].mask);
            bishopAttacks[from][MAGICBISHOPINDEX(occ, from)] = (computeAttacks(from, occ, -7) | computeAttacks(from, occ, 7) | computeAttacks(from, occ, -9) | computeAttacks(from, occ, 9));
        }
        rook[from].magic = rookMagics[from];
        for (int j = 0; j < (1 << ROOK_INDEX_BITS); j++) {
            const BitBoard occ = occupiedFromIndex(j, rook[from].mask);
            rookAttacks[from][MAGICROOKINDEX(occ, from)] = (computeAttacks(from, occ, -1) | computeAttacks(from, occ, 1) | computeAttacks(from, occ, -8) | computeAttacks(from, occ, 8));
        }
    }
}

} // MagicBB

template < Piece > BitBoard coverage      (const Square x, const BitBoard occupancy, const Color c) { assert(false); return empty; }
template <       > BitBoard coverage<P_wp>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].pawnAttack[c]; }
template <       > BitBoard coverage<P_wn>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].knight; }
template <       > BitBoard coverage<P_wb>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return MAGICBISHOPATTACKS(occupancy, x); }
template <       > BitBoard coverage<P_wr>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return MAGICROOKATTACKS  (occupancy, x); }
template <       > BitBoard coverage<P_wq>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return MAGICBISHOPATTACKS(occupancy, x) | MAGICROOKATTACKS(occupancy, x); }
template <       > BitBoard coverage<P_wk>(const Square x, const BitBoard occupancy, const Color c) { assert( x >= 0 && x < 64); return mask[x].king; }

template < Piece pp > inline BitBoard attack(const Square x, const BitBoard target, const BitBoard occupancy = 0, const Color c = Co_White) { return coverage<pp>(x, occupancy, c) & target; }

#endif // MAGIC

constexpr BitBoard(*const pfCoverage[])(const Square, const BitBoard, const Color)                 = { &BBTools::coverage<P_wp>, &BBTools::coverage<P_wn>, &BBTools::coverage<P_wb>, &BBTools::coverage<P_wr>, &BBTools::coverage<P_wq>, &BBTools::coverage<P_wk> };
//constexpr BitBoard(*const pfAttack[])  (const Square, const BitBoard, const BitBoard, const Color) = { &BBTools::attack<P_wp>,   &BBTools::attack<P_wn>,   &BBTools::attack<P_wb>,   &BBTools::attack<P_wr>,   &BBTools::attack<P_wq>,   &BBTools::attack<P_wk>   };

Square SquareFromBitBoard(const BitBoard & b) { // return first square only
    assert(b != empty);
    unsigned long i = 0;
    bsf(b, i);
    return Square(i);
}

bool isAttackedBB(const Position &p, const Square x, Color c) { ///@todo try to optimize order better ?
    assert(x != INVALIDSQUARE);
    if (c == Co_White) return attack<P_wb>(x, p.blackBishop() | p.blackQueen(), p.occupancy) || attack<P_wr>(x, p.blackRook() | p.blackQueen(), p.occupancy) || attack<P_wp>(x, p.blackPawn(), p.occupancy, Co_White) || attack<P_wn>(x, p.blackKnight()) || attack<P_wk>(x, p.blackKing());
    else               return attack<P_wb>(x, p.whiteBishop() | p.whiteQueen(), p.occupancy) || attack<P_wr>(x, p.whiteRook() | p.whiteQueen(), p.occupancy) || attack<P_wp>(x, p.whitePawn(), p.occupancy, Co_Black) || attack<P_wn>(x, p.whiteKnight()) || attack<P_wk>(x, p.whiteKing());
}

inline void unSetBit(Position & p, Square k)           { assert(k >= 0 && k < 64); ::unSetBit(p.allB[PieceTools::getPieceIndex(p, k)], k);}
inline void unSetBit(Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); ::unSetBit(p.allB[pp + PieceShift]                , k);}
inline void setBit  (Position & p, Square k, Piece pp) { assert(k >= 0 && k < 64); ::setBit  (p.allB[pp + PieceShift]                , k);}

void initBitBoards(Position & p) {
    p.allB.fill(empty);
    p.allPieces[Co_White] = p.allPieces[Co_Black] = p.occupancy = empty;
}

void setBitBoards(Position & p) {
    initBitBoards(p);
    for (Square k = 0; k < 64; ++k) { setBit(p,k,p.b[k]); }
    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy  = p.allPieces[Co_White] | p.allPieces[Co_Black];
}

} // BBTools

inline ScoreType Move2Score(Move m) { assert(VALIDMOVE(m)); return (m >> 16) & 0xFFFF; }
inline Square    Move2From (Move m) { assert(VALIDMOVE(m)); return (m >> 10) & 0x3F  ; }
inline Square    Move2To   (Move m) { assert(VALIDMOVE(m)); return (m >>  4) & 0x3F  ; }
inline MType     Move2Type (Move m) { assert(VALIDMOVE(m)); return MType(m & 0xF)    ; }
inline Move      ToMove(Square from, Square to, MType type)                  { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return                 (from << 10) | (to << 4) | type; }
inline Move      ToMove(Square from, Square to, MType type, ScoreType score) { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); return (score << 16) | (from << 10) | (to << 4) | type; }

inline bool isMatingScore (ScoreType s) { return (s >=  MATE - MAX_DEPTH); }
inline bool isMatedScore  (ScoreType s) { return (s <= -MATE + MAX_DEPTH); }
inline bool isMateScore   (ScoreType s) { return (std::abs(s) >= MATE - MAX_DEPTH); }

inline bool isCapture  (const MType & mt){ return mt == T_capture || mt == T_ep || mt == T_cappromq || mt == T_cappromr || mt == T_cappromb || mt == T_cappromn; }
inline bool isCapture  (const Move & m  ){ return isCapture(Move2Type(m)); }
inline bool isCastling (const MType & mt){ return mt == T_bks || mt == T_bqs || mt == T_wks || mt == T_wqs; }
inline bool isCastling (const Move & m  ){ return isCastling(Move2Type(m)); }
inline bool isPromotion(const MType & mt){ return mt >= T_promq && mt <= T_cappromn;}
inline bool isPromotion(const Move & m  ){ return isPromotion(Move2Type(m));}
inline bool isBadCap   (const Move & m  ){ return Move2Score(m) < -MoveScoring[T_capture] + 800;}
inline ScoreType badCapScore (const Move & m ){ return Move2Score(m) + MoveScoring[T_capture];}

inline Square chebyshevDistance(Square sq1, Square sq2) { return std::max(std::abs(SQRANK(sq2) - SQRANK(sq1)) , std::abs(SQFILE(sq2) - SQFILE(sq1))); }
inline Square manatthanDistance(Square sq1, Square sq2) { return std::abs(SQRANK(sq2) - SQRANK(sq1)) + std::abs(SQFILE(sq2) - SQFILE(sq1)); }
inline Square minDistance      (Square sq1, Square sq2) { return std::min(std::abs(SQRANK(sq2) - SQRANK(sq1)) , std::abs(SQFILE(sq2) - SQFILE(sq1))); }

std::string ToString(const Move & m    , bool withScore = false);
std::string ToString(const Position & p, bool noEval = false);
std::string ToString(const Position::Material & mat);

namespace MaterialHash { // idea from Gull
    const int MatWQ = 1;
    const int MatBQ = 3;
    const int MatWR = (3 * 3);
    const int MatBR = (3 * 3 * 3);
    const int MatWL = (3 * 3 * 3 * 3);
    const int MatBL = (3 * 3 * 3 * 3 * 2);
    const int MatWD = (3 * 3 * 3 * 3 * 2 * 2);
    const int MatBD = (3 * 3 * 3 * 3 * 2 * 2 * 2);
    const int MatWN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2);
    const int MatBN = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3);
    const int MatWP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3);
    const int MatBP = (3 * 3 * 3 * 3 * 2 * 2 * 2 * 2 * 3 * 3 * 9);
    const int TotalMat = ((2 * (MatWQ + MatBQ) + MatWL + MatBL + MatWD + MatBD + 2 * (MatWR + MatBR + MatWN + MatBN) + 8 * (MatWP + MatBP)) + 1);

    inline Hash getMaterialHash(const Position::Material & mat) {
        if (mat[Co_White][M_q] > 2 || mat[Co_Black][M_q] > 2 || mat[Co_White][M_r] > 2 || mat[Co_Black][M_r] > 2 || mat[Co_White][M_bl] > 1 || mat[Co_Black][M_bl] > 1 || mat[Co_White][M_bd] > 1 || mat[Co_Black][M_bd] > 1 || mat[Co_White][M_n] > 2 || mat[Co_Black][M_n] > 2 || mat[Co_White][M_p] > 8 || mat[Co_Black][M_p] > 8) return nullHash;
        return mat[Co_White][M_p] * MatWP + mat[Co_Black][M_p] * MatBP + mat[Co_White][M_n] * MatWN + mat[Co_Black][M_n] * MatBN + mat[Co_White][M_bl] * MatWL + mat[Co_Black][M_bl] * MatBL + mat[Co_White][M_bd] * MatWD + mat[Co_Black][M_bd] * MatBD + mat[Co_White][M_r] * MatWR + mat[Co_Black][M_r] * MatBR + mat[Co_White][M_q] * MatWQ + mat[Co_Black][M_q] * MatBQ;
    }

    inline Position::Material indexToMat(int index){
        Position::Material m = {{{{0}}}};
        m[Co_White][M_q] = index % 3; index /= 3;
        m[Co_Black][M_q] = index % 3; index /= 3;
        m[Co_White][M_r] = index % 3; index /= 3;
        m[Co_Black][M_r] = index % 3; index /= 3;
        m[Co_White][M_bl]= index % 2; index /= 2;
        m[Co_Black][M_bl]= index % 2; index /= 2;
        m[Co_White][M_bd]= index % 2; index /= 2;
        m[Co_Black][M_bd]= index % 2; index /= 2;
        m[Co_White][M_n] = index % 3; index /= 3;
        m[Co_Black][M_n] = index % 3; index /= 3;
        m[Co_White][M_p] = index % 9; index /= 9;
        m[Co_Black][M_p] = index;
        m[Co_White][M_b] = m[Co_White][M_bl] + m[Co_White][M_bd];
        m[Co_Black][M_b] = m[Co_Black][M_bl] + m[Co_Black][M_bd];
        m[Co_White][M_M] = m[Co_White][M_q] + m[Co_White][M_r];  m[Co_Black][M_M] = m[Co_Black][M_q] + m[Co_Black][M_r];
        m[Co_White][M_m] = m[Co_White][M_b] + m[Co_White][M_n];  m[Co_Black][M_m] = m[Co_Black][M_b] + m[Co_Black][M_n];
        m[Co_White][M_t] = m[Co_White][M_M] + m[Co_White][M_m];  m[Co_Black][M_t] = m[Co_Black][M_M] + m[Co_Black][M_m];
        return m;
    }

    inline Position::Material getMatReverseColor(const Position::Material & mat) {
        Position::Material rev = {{{{0}}}};
        rev[Co_White][M_k]  = mat[Co_Black][M_k];   rev[Co_Black][M_k]  = mat[Co_White][M_k];
        rev[Co_White][M_q]  = mat[Co_Black][M_q];   rev[Co_Black][M_q]  = mat[Co_White][M_q];
        rev[Co_White][M_r]  = mat[Co_Black][M_r];   rev[Co_Black][M_r]  = mat[Co_White][M_r];
        rev[Co_White][M_b]  = mat[Co_Black][M_b];   rev[Co_Black][M_b]  = mat[Co_White][M_b];
        rev[Co_White][M_bl] = mat[Co_Black][M_bl];  rev[Co_Black][M_bl] = mat[Co_White][M_bl];
        rev[Co_White][M_bd] = mat[Co_Black][M_bd];  rev[Co_Black][M_bd] = mat[Co_White][M_bd];
        rev[Co_White][M_n]  = mat[Co_Black][M_n];   rev[Co_Black][M_n]  = mat[Co_White][M_n];
        rev[Co_White][M_p]  = mat[Co_Black][M_p];   rev[Co_Black][M_p]  = mat[Co_White][M_p];
        rev[Co_White][M_M]  = mat[Co_Black][M_M];   rev[Co_Black][M_M]  = mat[Co_White][M_M];
        rev[Co_White][M_m]  = mat[Co_Black][M_m];   rev[Co_Black][M_m]  = mat[Co_White][M_m];
        rev[Co_White][M_t]  = mat[Co_Black][M_t];   rev[Co_Black][M_t]  = mat[Co_White][M_t];
        return rev;
    }

    Position::Material materialFromString(const std::string & strMat) {
        Position::Material mat = {{{{0}}}};
        Color c = Co_Black;
        for (auto it = strMat.begin(); it != strMat.end(); ++it) {
            switch (*it) {
            case 'K': c = ~c;             mat[c][M_k] += 1;   break;
            case 'Q': mat[c][M_q] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'R': mat[c][M_r] += 1;                       mat[c][M_M] += 1;  mat[c][M_t] += 1;  break;
            case 'L': mat[c][M_bl]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'D': mat[c][M_bd]+= 1;   mat[c][M_b] += 1;   mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'N': mat[c][M_n] += 1;                       mat[c][M_m] += 1;  mat[c][M_t] += 1;  break;
            case 'P': mat[c][M_p] += 1;   break;
            default:  Logging::LogIt(Logging::logFatal) << "Bad char in material definition";
            }
        }
        return mat;
    }

    enum Terminaison : unsigned char { Ter_Unknown = 0, Ter_WhiteWinWithHelper, Ter_WhiteWin, Ter_BlackWinWithHelper, Ter_BlackWin, Ter_Draw, Ter_MaterialDraw, Ter_LikelyDraw, Ter_HardToWin };

    inline Terminaison reverseTerminaison(Terminaison t) {
        switch (t) {
        case Ter_Unknown: case Ter_Draw: case Ter_MaterialDraw: case Ter_LikelyDraw: case Ter_HardToWin: return t;
        case Ter_WhiteWin:             return Ter_BlackWin;
        case Ter_WhiteWinWithHelper:   return Ter_BlackWinWithHelper;
        case Ter_BlackWin:             return Ter_WhiteWin;
        case Ter_BlackWinWithHelper:   return Ter_WhiteWinWithHelper;
        default:                       return Ter_Unknown;
        }
    }

    const ScoreType pushToEdges[64] = {
      100, 90, 80, 70, 70, 80, 90, 100,
       90, 70, 60, 50, 50, 60, 70,  90,
       80, 60, 40, 30, 30, 40, 60,  80,
       70, 50, 30, 20, 20, 30, 50,  70,
       70, 50, 30, 20, 20, 30, 50,  70,
       80, 60, 40, 30, 30, 40, 60,  80,
       90, 70, 60, 50, 50, 60, 70,  90,
      100, 90, 80, 70, 70, 80, 90, 100
    };

    const ScoreType pushToCorners[64] = {
      200, 190, 180, 170, 160, 150, 140, 130,
      190, 180, 170, 160, 150, 140, 130, 140,
      180, 170, 155, 140, 140, 125, 140, 150,
      170, 160, 140, 120, 110, 140, 150, 160,
      160, 150, 140, 110, 120, 140, 160, 170,
      150, 140, 125, 140, 140, 155, 170, 180,
      140, 130, 140, 150, 160, 170, 180, 190,
      130, 140, 150, 160, 170, 180, 190, 200
    };

    const ScoreType pushClose[8] = { 0, 0, 100, 80, 60, 40, 20,  10 };
    //const ScoreType pushAway [8] = { 0, 5,  20, 40, 60, 80, 90, 100 };

    ScoreType helperKXK(const Position &p, Color winningSide, ScoreType s){
        if (p.c != winningSide ){ // stale mate detection for losing side
           ///@todo
        }
        const Square winningK = p.king[winningSide];
        const Square losingK  = p.king[~winningSide];
        const ScoreType sc = pushToEdges[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN));
    }
    ScoreType helperKmmK(const Position &p, Color winningSide, ScoreType s){
        Square winningK = p.king[winningSide];
        Square losingK  = p.king[~winningSide];
        if ( ((p.whiteBishop()|p.blackBishop()) & whiteSquare) != 0 ){
            winningK = VFlip(winningK);
            losingK  = VFlip(losingK);
        }
        const ScoreType sc = pushToCorners[losingK] + pushClose[chebyshevDistance(winningK,losingK)];
        return s + ((winningSide == Co_White)?(sc+WIN):(-sc-WIN));
    }
    ScoreType helperDummy(const Position &, Color , ScoreType){ return 0; } ///@todo not 0 for debug purpose ??

    // idea taken from public-domain KPK from HGM
    namespace KPK{

    Square normalizeSquare(const Position& p, Color strongSide, Square sq) {
       assert(countBit(p.pieces<P_wp>(strongSide)) == 1); // only for KPK !
       if (SQFILE(BBTools::SquareFromBitBoard(p.pieces<P_wp>(strongSide))) >= File_e) sq = Square(HFlip(sq)); // we know there is at least one pawn
       return strongSide == Co_White ? sq : VFlip(sq);
    }

    constexpr unsigned maxIndex = 2*24*64*64; // color x pawn x wk x bk
    uint32_t KPKBitbase[maxIndex/32]; // force 32bit uint
    unsigned index(Color us, Square bksq, Square wksq, Square psq) { return wksq | (bksq << 6) | (us << 12) | (SQFILE(psq) << 13) | ((6 - SQRANK(psq)) << 15);}
    enum kpk_result : unsigned char { kpk_invalid = 0, kpk_unknown = 1, kpk_draw = 2, kpk_win = 4};
    kpk_result& operator|=(kpk_result& r, kpk_result v) { return r = kpk_result(r | v); }

    #pragma pack(push, 1)
    struct KPKPosition {
        KPKPosition() = default;
        explicit KPKPosition(unsigned idx){ // first init
            ksq[Co_White] = Square( idx & 0x3F); ksq[Co_Black] = Square((idx >> 6) & 0x3F); us = Color ((idx >> 12) & 0x01);  psq = MakeSquare(File((idx >> 13) & 0x3), Rank(6 - ((idx >> 15) & 0x7)));
            if ( chebyshevDistance(ksq[Co_White], ksq[Co_Black]) <= 1 || ksq[Co_White] == psq || ksq[Co_Black] == psq || (us == Co_White && (BBTools::mask[psq].pawnAttack[Co_White] & SquareToBitboard(ksq[Co_Black])))) result = kpk_invalid;
            else if ( us == Co_White && SQRANK(psq) == 6 && ksq[us] != psq + 8 && ( chebyshevDistance(ksq[~us], psq + 8) > 1 || (BBTools::mask[ksq[us]].king & SquareToBitboard(psq + 8)))) result = kpk_win;
            else if ( us == Co_Black && ( !(BBTools::mask[ksq[us]].king & ~(BBTools::mask[ksq[~us]].king | BBTools::mask[psq].pawnAttack[~us])) || (BBTools::mask[ksq[us]].king & SquareToBitboard(psq) & ~BBTools::mask[ksq[~us]].king))) result = kpk_draw;
            else result = kpk_unknown; // done later
        }
        operator kpk_result() const { return result; }
        kpk_result preCompute(const std::vector<KPKPosition>& db) { return us == Co_White ? preCompute<Co_White>(db) : preCompute<Co_Black>(db); }
        template<Color Us> kpk_result preCompute(const std::vector<KPKPosition>& db) {
            constexpr Color Them = (Us == Co_White ? Co_Black : Co_White);
            constexpr kpk_result good = (Us == Co_White ? kpk_win  : kpk_draw);
            constexpr kpk_result bad  = (Us == Co_White ? kpk_draw : kpk_win);
            kpk_result r = kpk_invalid;
            BitBoard b = BBTools::mask[ksq[us]].king;
            while (b){ r |= (Us == Co_White ? db[index(Them, ksq[Them], BBTools::popBit(b), psq)] : db[index(Them, BBTools::popBit(b), ksq[Them], psq)]); }
            if (Us == Co_White){
                if (SQRANK(psq) < 6) r |= db[index(Them, ksq[Them], ksq[Us], psq + 8)];
                if (SQRANK(psq) == 1 && psq + 8 != ksq[Us] && psq + 8 != ksq[Them]) r |= db[index(Them, ksq[Them], ksq[Us], psq + 8 + 8)];
            }
            return result = r & good ? good : r & kpk_unknown ? kpk_unknown : bad;
        }
        Square ksq[2], psq;
        kpk_result result;
        Color us;
    };
    #pragma pack(pop)

    bool probe(Square wksq, Square wpsq, Square bksq, Color us) {
        assert(SQFILE(wpsq) <= 4);
        const unsigned idx = index(us, bksq, wksq, wpsq);
        assert(idx >= 0);
        assert(idx < maxIndex);
        return KPKBitbase[idx/32] & (1<<(idx&0x1F));
    }

    void init(){
        Logging::LogIt(Logging::logInfo) << "KPK init";
        Logging::LogIt(Logging::logInfo) << "KPK table size : " << maxIndex/32*sizeof(uint32_t)/1024 << "Kb";
        std::vector<KPKPosition> db(maxIndex);
        unsigned idx, repeat = 1;
        for (idx = 0; idx < maxIndex; ++idx) db[idx] = KPKPosition(idx); // init
        while (repeat) for (repeat = idx = 0; idx < maxIndex; ++idx) repeat |= (db[idx] == kpk_unknown && db[idx].preCompute(db) != kpk_unknown); // loop
        for (idx = 0; idx < maxIndex; ++idx){ if (db[idx] == kpk_win) { KPKBitbase[idx / 32] |= 1 << (idx & 0x1F); } } // compress
    }

    } // KPK

    ScoreType helperKPK(const Position &p, Color winningSide, ScoreType ){
       const Square psq = KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces<P_wp>(winningSide))); // we know there is at least one pawn
       if (!KPK::probe(KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces<P_wk>(winningSide))), psq, KPK::normalizeSquare(p, winningSide, BBTools::SquareFromBitBoard(p.pieces<P_wk>(~winningSide))), winningSide == p.c ? Co_White:Co_Black)) return 0; // shall be drawScore but this is not a 3rep case so don't bother too much ...
       return ((winningSide == Co_White)?+1:-1)*(WIN + ValuesEG[P_wp+PieceShift] + 10*SQRANK(psq));
    }

    ScoreType (* helperTable[TotalMat])(const Position &, Color, ScoreType );
    struct MaterialHashEntry  { Terminaison t = Ter_Unknown; EvalScore score={0,0}; float gp = 1.f;};
    MaterialHashEntry materialHashTable[TotalMat];

    EvalScore Imbalance(const Position::Material & mat, Color c) {
        EvalScore bonus = 0;
        // Second-degree polynomial material imbalance, by Tord Romstad (old version, not the current Stockfish stuff)
        for (Mat m1 = M_p; m1 <= M_q; ++m1) {
            if (!mat[c][m1]) continue;
            for (Mat m2 = M_p; m2 <= m1; ++m2) {
                bonus += EvalConfig::imbalance_mines[m1-1][m2-1] * mat[c][m1] * mat[c][m2] + EvalConfig::imbalance_theirs[m1-1][m2-1] * mat[c][m1] * mat[~c][m2];
            }
        }
        return bonus/16;
    }

    void InitMaterialScore(bool display = true){
        if ( display) Logging::LogIt(Logging::logInfo) << "Material hash init";
        const float totalMatScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp];
        for (int k = 0 ; k < TotalMat ; ++k){
            const Position::Material mat = indexToMat(k);
            const ScoreType matPieceScoreW = mat[Co_White][M_q] * *absValues[P_wq] + mat[Co_White][M_r] * *absValues[P_wr] + mat[Co_White][M_b] * *absValues[P_wb] + mat[Co_White][M_n] * *absValues[P_wn];
            const ScoreType matPieceScoreB = mat[Co_Black][M_q] * *absValues[P_wq] + mat[Co_Black][M_r] * *absValues[P_wr] + mat[Co_Black][M_b] * *absValues[P_wb] + mat[Co_Black][M_n] * *absValues[P_wn];
            const ScoreType matPawnScoreW  = mat[Co_White][M_p] * *absValues[P_wp];
            const ScoreType matPawnScoreB  = mat[Co_Black][M_p] * *absValues[P_wp];
            const ScoreType matScoreW = matPieceScoreW + matPawnScoreW;
            const ScoreType matScoreB = matPieceScoreB + matPawnScoreB;
#ifdef WITH_TEXEL_TUNING
            const EvalScore imbalance = {0,0};
#else
            const EvalScore imbalance = Imbalance(mat, Co_White) - Imbalance(mat, Co_Black);
#endif
            materialHashTable[k].gp = (matScoreW + matScoreB ) / totalMatScore;
            materialHashTable[k].score[MG] = imbalance[MG] + matScoreW - matScoreB;
            materialHashTable[k].score[EG] = imbalance[EG] + (mat[Co_White][M_q] - mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (mat[Co_White][M_r] - mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (mat[Co_White][M_b] - mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (mat[Co_White][M_n] - mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (mat[Co_White][M_p] - mat[Co_Black][M_p]) * *absValuesEG[P_wp];
        }
       if ( display) Logging::LogIt(Logging::logInfo) << "...Done";
    }

    struct MaterialHashInitializer {
        MaterialHashInitializer(const Position::Material & mat, Terminaison t) { materialHashTable[getMaterialHash(mat)].t = t; }
        MaterialHashInitializer(const Position::Material & mat, Terminaison t, ScoreType (*helper)(const Position &, Color, ScoreType) ) { materialHashTable[getMaterialHash(mat)].t = t; helperTable[getMaterialHash(mat)] = helper; }
        static void init() {
            Logging::LogIt(Logging::logInfo) << "Material hash total : " << TotalMat;
            Logging::LogIt(Logging::logInfo) << "Material hash size : " << TotalMat*sizeof(MaterialHashEntry)/1024/1024 << "Mb";

            InitMaterialScore();

            for(size_t k = 0 ; k < TotalMat ; ++k) helperTable[k] = &helperDummy;
#define DEF_MAT(x,t)     const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer,MAT##x)( MAT##x ,t   );
#define DEF_MAT_H(x,t,h) const Position::Material MAT##x = materialFromString(TO_STR(x)); MaterialHashInitializer LINE_NAME(dummyMaterialInitializer,MAT##x)( MAT##x ,t, h);
#define DEF_MAT_REV(rev,x)     const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev,MAT##x)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)].t)   );
#define DEF_MAT_REV_H(rev,x,h) const Position::Material MAT##rev = MaterialHash::getMatReverseColor(MAT##x); MaterialHashInitializer LINE_NAME(dummyMaterialInitializerRev,MAT##x)( MAT##rev,reverseTerminaison(materialHashTable[getMaterialHash(MAT##x)].t), h);

            ///@todo some Ter_MaterialDraw are Ter_Draw (FIDE)

            // other FIDE draw
            DEF_MAT(KLKL,   Ter_MaterialDraw)        DEF_MAT(KDKD,   Ter_MaterialDraw)

            // sym (and pseudo sym) : all should be draw (or very nearly)
            DEF_MAT(KK,     Ter_MaterialDraw)        DEF_MAT(KQQKQQ, Ter_MaterialDraw)    DEF_MAT(KQKQ,   Ter_MaterialDraw)        DEF_MAT(KRRKRR, Ter_MaterialDraw)        
            DEF_MAT(KRKR,   Ter_MaterialDraw)        DEF_MAT(KLDKLD, Ter_MaterialDraw)    DEF_MAT(KLLKLL, Ter_MaterialDraw)        DEF_MAT(KDDKDD, Ter_MaterialDraw)
            DEF_MAT(KNNKNN, Ter_MaterialDraw)        DEF_MAT(KNKN  , Ter_MaterialDraw)
            DEF_MAT(KLDKLL, Ter_MaterialDraw)        DEF_MAT_REV(KLLKLD, KLDKLL)          DEF_MAT(KLDKDD, Ter_MaterialDraw)        DEF_MAT_REV(KDDKLD, KLDKDD)
            DEF_MAT(KLKD,   Ter_MaterialDraw)        DEF_MAT_REV(KDKL,   KLKD)
            
            // 2M M
            DEF_MAT(KQQKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQQ, KQQKQ)            DEF_MAT(KQQKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQQ, KQQKR)
            DEF_MAT(KRRKQ, Ter_LikelyDraw)           DEF_MAT_REV(KQKRR, KRRKQ)            DEF_MAT(KRRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKRR, KRRKR)
            DEF_MAT(KQRKQ, Ter_WhiteWin)             DEF_MAT_REV(KQKQR, KQRKQ)            DEF_MAT(KQRKR, Ter_WhiteWin)             DEF_MAT_REV(KRKQR, KQRKR)

            // 2M m
            DEF_MAT(KQQKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQQ, KQQKL)            DEF_MAT(KRRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKRR, KRRKL)
            DEF_MAT(KQRKL, Ter_WhiteWin)             DEF_MAT_REV(KLKQR, KQRKL)            DEF_MAT(KQQKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQQ, KQQKD)
            DEF_MAT(KRRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKRR, KRRKD)            DEF_MAT(KQRKD, Ter_WhiteWin)             DEF_MAT_REV(KDKQR, KQRKD)
            DEF_MAT(KQQKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQQ, KQQKN)            DEF_MAT(KRRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKRR, KRRKN)
            DEF_MAT(KQRKN, Ter_WhiteWin)             DEF_MAT_REV(KNKQR, KQRKN)

            // 2m M
            DEF_MAT(KLDKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLD,KLDKQ)            DEF_MAT(KLDKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLD,KLDKR)
            DEF_MAT(KLLKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLL,KLLKQ)            DEF_MAT(KLLKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLL,KLLKR)
            DEF_MAT(KDDKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKDD,KDDKQ)            DEF_MAT(KDDKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKDD,KDDKR)
            DEF_MAT(KNNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKNN,KNNKQ)            DEF_MAT(KNNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKNN,KNNKR)
            DEF_MAT(KLNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKLN,KLNKQ)            DEF_MAT(KLNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKLN,KLNKR)
            DEF_MAT(KDNKQ, Ter_MaterialDraw)         DEF_MAT_REV(KQKDN,KDNKQ)            DEF_MAT(KDNKR, Ter_MaterialDraw)         DEF_MAT_REV(KRKDN,KDNKR)

            // 2m m : all draw
            DEF_MAT(KLDKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLD,KLDKL)            DEF_MAT(KLDKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLD,KLDKD)
            DEF_MAT(KLDKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLD,KLDKN)            DEF_MAT(KLLKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLL,KLLKL)
            DEF_MAT(KLLKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLL,KLLKD)            DEF_MAT(KLLKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLL,KLLKN)
            DEF_MAT(KDDKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKDD,KDDKL)            DEF_MAT(KDDKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKDD,KDDKD)
            DEF_MAT(KDDKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKDD,KDDKN)            DEF_MAT(KNNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKNN,KNNKL)
            DEF_MAT(KNNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKNN,KNNKD)            DEF_MAT(KNNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKNN,KNNKN)
            DEF_MAT(KLNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKLN,KLNKL)            DEF_MAT(KLNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKLN,KLNKD)
            DEF_MAT(KLNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKLN,KLNKN)            DEF_MAT(KDNKL, Ter_MaterialDraw)         DEF_MAT_REV(KLKDN,KDNKL)
            DEF_MAT(KDNKD, Ter_MaterialDraw)         DEF_MAT_REV(KDKDN,KDNKD)            DEF_MAT(KDNKN, Ter_MaterialDraw)         DEF_MAT_REV(KNKDN,KDNKN)

            // Q x : all should be win
            DEF_MAT(KQKR, Ter_WhiteWin)              DEF_MAT_REV(KRKQ,KQKR)              DEF_MAT(KQKL, Ter_WhiteWin)              DEF_MAT_REV(KLKQ,KQKL)
            DEF_MAT(KQKD, Ter_WhiteWin)              DEF_MAT_REV(KDKQ,KQKD)              DEF_MAT(KQKN, Ter_WhiteWin)              DEF_MAT_REV(KNKQ,KQKN)

            // R x : all should be draw
            DEF_MAT(KRKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKR,KRKL)              DEF_MAT(KRKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKR,KRKD)
            DEF_MAT(KRKN, Ter_LikelyDraw)            DEF_MAT_REV(KNKR,KRKN)

            // B x : all are draw
            DEF_MAT(KLKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKL,KLKN)              DEF_MAT(KDKN, Ter_MaterialDraw)          DEF_MAT_REV(KNKD,KDKN)

            // X 0 : QR win, BN draw
            DEF_MAT_H(KQK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKQ,KQK,&helperKXK)
            DEF_MAT_H(KRK, Ter_WhiteWinWithHelper,&helperKXK)   DEF_MAT_REV_H(KKR,KRK,&helperKXK)
            DEF_MAT(KLK, Ter_MaterialDraw)                      DEF_MAT_REV(KKL,KLK)
            DEF_MAT(KDK, Ter_MaterialDraw)                      DEF_MAT_REV(KKD,KDK)
            DEF_MAT(KNK, Ter_MaterialDraw)                      DEF_MAT_REV(KKN,KNK)

            // 2X 0 : all win except LL, DD, NN
            DEF_MAT(KQQK, Ter_WhiteWin)                         DEF_MAT_REV(KKQQ,KQQK)
            DEF_MAT(KRRK, Ter_WhiteWin)                         DEF_MAT_REV(KKRR,KRRK)
            DEF_MAT_H(KLDK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLD,KLDK,&helperKmmK)
            DEF_MAT(KLLK, Ter_MaterialDraw)                     DEF_MAT_REV(KKLL,KLLK)
            DEF_MAT(KDDK, Ter_MaterialDraw)                     DEF_MAT_REV(KKDD,KDDK)
            DEF_MAT(KNNK, Ter_MaterialDraw)                     DEF_MAT_REV(KKNN,KNNK)
            DEF_MAT_H(KLNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKLN,KLNK,&helperKmmK)
            DEF_MAT_H(KDNK, Ter_WhiteWinWithHelper,&helperKmmK) DEF_MAT_REV_H(KKDN,KDNK,&helperKmmK)

            // Rm R : likely draws
            DEF_MAT(KRNKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRN,KRNKR)
            DEF_MAT(KRLKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRL,KRLKR)
            DEF_MAT(KRDKR, Ter_LikelyDraw)            DEF_MAT_REV(KRKRD,KRDKR)

            // Rm m : hard to win
            DEF_MAT(KRNKN, Ter_HardToWin)             DEF_MAT_REV(KNKRN,KRNKN)            DEF_MAT(KRNKL, Ter_HardToWin)             DEF_MAT_REV(KLKRN,KRNKL)
            DEF_MAT(KRNKD, Ter_HardToWin)             DEF_MAT_REV(KDKRN,KRNKD)            DEF_MAT(KRLKN, Ter_HardToWin)             DEF_MAT_REV(KNKRL,KRLKN)
            DEF_MAT(KRLKL, Ter_HardToWin)             DEF_MAT_REV(KLKRL,KRLKL)            DEF_MAT(KRLKD, Ter_HardToWin)             DEF_MAT_REV(KDKRL,KRLKD)
            DEF_MAT(KRDKN, Ter_HardToWin)             DEF_MAT_REV(KNKRD,KRDKN)            DEF_MAT(KRDKL, Ter_HardToWin)             DEF_MAT_REV(KLKRD,KRDKL)
            DEF_MAT(KRDKD, Ter_HardToWin)             DEF_MAT_REV(KDKRD,KRDKD)

            // Qm Q : hard to win
            DEF_MAT_H(KQNKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQN,KQNKQ,&helperKXK)
            DEF_MAT_H(KQLKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQL,KQLKQ,&helperKXK)
            DEF_MAT_H(KQDKQ, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KQKQD,KQDKQ,&helperKXK)

            // Qm R : wins
            DEF_MAT_H(KQNKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQN,KQNKR,&helperKXK)
            DEF_MAT_H(KQLKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQL,KQLKR,&helperKXK)
            DEF_MAT_H(KQDKR, Ter_WhiteWin,&helperKXK)              DEF_MAT_REV_H(KRKQD,KQRKR,&helperKXK)

            // Q Rm : hard to win
            DEF_MAT_H(KQKRN, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRNKQ,KQKRN,&helperKXK)
            DEF_MAT_H(KQKRL, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRLKQ,KQKRL,&helperKXK)
            DEF_MAT_H(KQKRD, Ter_HardToWin,&helperKXK)             DEF_MAT_REV_H(KRDKQ,KQKRD,&helperKXK)

            // Opposite bishop with P
            DEF_MAT(KLPKD, Ter_LikelyDraw)            DEF_MAT_REV(KDKLP,KLPKD)            DEF_MAT(KDPKL, Ter_LikelyDraw)            DEF_MAT_REV(KLKDP,KDPKL)
            DEF_MAT(KLPPKD, Ter_LikelyDraw)           DEF_MAT_REV(KDKLPP,KLPPKD)          DEF_MAT(KDPPKL, Ter_LikelyDraw)           DEF_MAT_REV(KLKDPP,KDPPKL)

            // KPK
            DEF_MAT_H(KPK, Ter_WhiteWinWithHelper,&helperKPK)    DEF_MAT_REV_H(KKP,KPK,&helperKPK)

            ///@todo other (with more pawn ...), especially KPsKB with wrong color bishop
        }
    };

    inline Terminaison probeMaterialHashTable(const Position::Material & mat) { return materialHashTable[getMaterialHash(mat)].t; }

    void updateMaterialOther(Position & p){
        p.mat[Co_White][M_M] = p.mat[Co_White][M_q] + p.mat[Co_White][M_r];  p.mat[Co_Black][M_M] = p.mat[Co_Black][M_q] + p.mat[Co_Black][M_r];
        p.mat[Co_White][M_m] = p.mat[Co_White][M_b] + p.mat[Co_White][M_n];  p.mat[Co_Black][M_m] = p.mat[Co_Black][M_b] + p.mat[Co_Black][M_n];
        p.mat[Co_White][M_t] = p.mat[Co_White][M_M] + p.mat[Co_White][M_m];  p.mat[Co_Black][M_t] = p.mat[Co_Black][M_M] + p.mat[Co_Black][M_m];
        p.mat[Co_White][M_bl] = (unsigned char)countBit(p.whiteBishop()&whiteSquare);   p.mat[Co_White][M_bd] = (unsigned char)countBit(p.whiteBishop()&blackSquare);
        p.mat[Co_Black][M_bl] = (unsigned char)countBit(p.blackBishop()&whiteSquare);   p.mat[Co_Black][M_bd] = (unsigned char)countBit(p.blackBishop()&blackSquare);
    }

    void initMaterial(Position & p){ // M_p .. M_k is the same as P_wp .. P_wk
        for( Color c = Co_White ; c < Co_End ; ++c) for( Piece pp = P_wp ; pp <= P_wk ; ++pp) p.mat[c][pp] = (unsigned char)countBit(p.pieces(c,pp));
        updateMaterialOther(p);
    }

    inline void updateMaterialProm(Position &p, const Square toBeCaptured, MType mt){
        p.mat[~p.c][PieceTools::getPieceType(p,toBeCaptured)]--; // capture if to square is not empty
        p.mat[p.c][M_p]--; // pawn
        p.mat[p.c][promShift(mt)]++;   // prom piece
    }
} // MaterialHash

namespace Zobrist {
    template < class T = Hash>
    T randomInt(T m, T M) {
        static std::mt19937 mt(42); // fixed seed for ZHash !!!
        static std::uniform_int_distribution<T> dist(m,M);
        return dist(mt);
    }
    Hash ZT[64][14]; // should be 13 but last ray is for castling[0 7 56 63][13] and ep [k][13] and color [3 4][13]
    void initHash() {
        Logging::LogIt(Logging::logInfo) << "Init hash";
        for (int k = 0; k < 64; ++k) for (int j = 0; j < 14; ++j) ZT[k][j] = randomInt(Hash(0),Hash(UINT64_MAX));
    }
}

struct ThreadContext; // forward decl

struct ThreadData{
    DepthType depth, seldepth;
    ScoreType sc;
    Position p;
    Move best;
    PVList pv;
};

struct Stats{
    enum StatId { sid_nodes = 0, sid_qnodes, sid_tthits, sid_ttInsert, sid_ttPawnhits, sid_ttPawnInsert, sid_ttschits, sid_ttscmiss, sid_materialTableHits, sid_materialTableMiss, sid_staticNullMove, sid_lmr, sid_lmrFail, sid_pvsFail, sid_razoringTry, sid_razoring, sid_nullMoveTry, sid_nullMoveTry2, sid_nullMoveTry3, sid_nullMove, sid_nullMove2, sid_probcutTry, sid_probcutTry2, sid_probcut, sid_lmp, sid_historyPruning, sid_futility, sid_CMHPruning, sid_see, sid_see2, sid_seeQuiet, sid_iid, sid_ttalpha, sid_ttbeta, sid_checkExtension, sid_checkExtension2, sid_recaptureExtension, sid_castlingExtension, sid_CMHExtension, sid_pawnPushExtension, sid_singularExtension, sid_singularExtension2, sid_singularExtension3, sid_queenThreatExtension, sid_BMExtension, sid_mateThreatExtension, sid_tbHit1, sid_tbHit2, sid_dangerPrune, sid_dangerReduce, sid_hashComputed, sid_maxid };
    static const std::array<std::string,sid_maxid> Names;
    std::array<Counter,sid_maxid> counters;
    void init(){ Logging::LogIt(Logging::logInfo) << "Init stat" ;  counters.fill(0ull); }
};

const std::array<std::string,Stats::sid_maxid> Stats::Names = { "nodes", "qnodes", "tthits", "ttInsert", "ttPawnhits", "ttPawnInsert", "ttScHits", "ttScMiss", "materialHits", "materialMiss", "staticNullMove", "lmr", "lmrfail", "pvsfail", "razoringTry", "razoring", "nullMoveTry", "nullMoveTry2", "nullMoveTry3", "nullMove", "nullMove2", "probcutTry", "probcutTry2", "probcut", "lmp", "historyPruning", "futility", "CMHPruning", "see", "see2", "seeQuiet", "iid", "ttalpha", "ttbeta", "checkExtension", "checkExtension2", "recaptureExtension", "castlingExtension", "CMHExtension", "pawnPushExtension", "singularExtension", "singularExtension2", "singularExtension3", "queenThreatExtension", "BMExtension", "mateThreatExtension", "TBHit1", "TBHit2", "dangerPrune", "dangerReduce", "computedHash"};

// singleton pool of threads
class ThreadPool : public std::vector<std::unique_ptr<ThreadContext>> {
public:
    static ThreadPool & instance();
    ~ThreadPool();
    void setup();
    ThreadContext & main();
    Move search(const ThreadData & d);
    void startOthers();
    void wait(bool otherOnly = false);
    bool stop;
    // gathering info from all threads
    Counter counter(Stats::StatId id) const;
    void DisplayStats()const{for(size_t k = 0 ; k < Stats::sid_maxid ; ++k) Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << counter((Stats::StatId)k);}
    static const int skipSize[20], skipPhase[20];
private:
    ThreadPool();
};

// Sizes and phases of the skip-blocks, used for distributing search depths across the threads, from stockfish
const unsigned int threadSkipSize = 20;
const int ThreadPool::skipSize[threadSkipSize]  = { 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };
const int ThreadPool::skipPhase[threadSkipSize] = { 0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7 };

namespace MoveDifficultyUtil {
    enum MoveDifficulty { MD_forced = 0, MD_easy, MD_std, MD_hardDefense, MD_hardAttack };
    const DepthType emergencyMinDepth = 14;
    const ScoreType emergencyMargin   = 90;
    const ScoreType easyMoveMargin    = 250;
    const int       emergencyFactor   = 5;
    const float     maxStealFraction  = 0.2f; // of remaining time
}

enum eScores : unsigned char { sc_Mat = 0, sc_PST, sc_Rand, sc_MOB, sc_ATT, sc_PieceBlockPawn, sc_Holes, sc_Outpost, sc_FreePasser, sc_PwnPush, sc_PwnSafeAtt, sc_PwnPushAtt, sc_Adjust, sc_OpenFile, sc_RookFrontKing, sc_RookFrontQueen, sc_RookQueenSameFile, sc_AttQueenMalus, sc_MinorOnOpenFile, sc_RookBehindPassed, sc_QueenNearKing, sc_Hanging, sc_Threat, sc_PinsK, sc_PinsQ, sc_PawnTT, sc_Tempo, sc_initiative, sc_NN, sc_max };
static const std::string scNames[sc_max] = { "Mat", "PST", "RAND", "MOB", "Att", "PieceBlockPawn", "Holes", "Outpost", "FreePasser", "PwnPush", "PwnSafeAtt", "PwnPushAtt" , "Adjust", "OpenFile", "RookFrontKing", "RookFrontQueen", "RookQueenSameFile", "AttQueenMalus", "MinorOnOpenFile", "RookBehindPassed", "QueenNearKing", "Hanging", "Threats", "PinsK", "PinsQ", "PawnTT", "Tempo", "initiative", "NN" };

#ifdef DEBUG_ACC
struct ScoreAcc{
    float scalingFactor = 1;
    std::array<EvalScore, sc_max> scores = { 0 };
    EvalScore & operator[](eScores e) { return scores[e]; }
    ScoreType Score(const Position &p, float gp){
        EvalScore sc;
        for(int k = 0 ; k < sc_max ; ++k){ sc += scores[k]; }
        return ScoreType(ScaleScore(sc,gp)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
    }
    void Display(const Position &p, float gp){
        EvalScore sc;
        for(int k = 0 ; k < sc_max ; ++k){
            Logging::LogIt(Logging::logInfo) << scNames[k] << "       " << scores[k][MG];
            Logging::LogIt(Logging::logInfo) << scNames[k] << "EG     " << scores[k][EG];
            sc += scores[k];
        }
        Logging::LogIt(Logging::logInfo) << "Score  " << sc[MG];
        Logging::LogIt(Logging::logInfo) << "EG     " << sc[EG];
        Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
        Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
        Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f,(110-p.fifty)/100.f);
        Logging::LogIt(Logging::logInfo) << "Total  " << ScoreType(ScaleScore(sc,gp)*scalingFactor*std::min(1.f,(110-p.fifty)/100.f));
    }
};
#else
struct ScoreAcc {
    float scalingFactor = 1;
    EvalScore score = { 0 };
    inline EvalScore & operator[](eScores ) { return score; }
    ScoreType Score(const Position &p, float gp) { return ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f)); }
    void Display(const Position &p, float gp) {
        Logging::LogIt(Logging::logInfo) << "Score  " << score[MG];
        Logging::LogIt(Logging::logInfo) << "EG     " << score[EG];
        Logging::LogIt(Logging::logInfo) << "Scaling factor " << scalingFactor;
        Logging::LogIt(Logging::logInfo) << "Game phase " << gp;
        Logging::LogIt(Logging::logInfo) << "Fifty  " << std::min(1.f, (110 - p.fifty) / 100.f);
        Logging::LogIt(Logging::logInfo) << "Total  " << ScoreType(ScaleScore(score, gp)*scalingFactor*std::min(1.f, (110 - p.fifty) / 100.f));
    }
};

#endif
bool sameMove(const Move & a, const Move & b) { return (a & 0x0000FFFF) == (b & 0x0000FFFF);}

struct EvalData{
    float gp = 0;
    ScoreType danger[2] = {0,0};
};

// former thread Stockfish style
struct ThreadContext{
    static bool stopFlag;
    static MoveDifficultyUtil::MoveDifficulty moveDifficulty;
    static TimeType currentMoveMs;
    static TimeType getCurrentMoveMs(); // use this (and not the variable) to take emergency time into account !

    struct StackData{
       Hash h = nullHash;
       ScoreType eval = 0;
       EvalData data = { 0, {0,0} };
       Move threat = INVALIDMOVE;
       Position p;
    };
    std::array<StackData,MAX_PLY> stack;

    Stats stats;

    Move previousBest;

    static const int MAX_CMH_PLY = 2;
    typedef std::array<ScoreType*,MAX_CMH_PLY> CMHPtrArray;

    struct KillerT{
        Move killers[MAX_DEPTH][2];
        inline void initKillers(){
            Logging::LogIt(Logging::logInfo) << "Init killers" ;
            for(int k = 0 ; k < MAX_DEPTH; ++k) for(int i = 0; i < 2; ++i) killers[k][i] = INVALIDMOVE;
        }
        inline bool isKiller(const Move m, const DepthType ply){
            return sameMove(m, killers[ply][0]) || sameMove(m, killers[ply][1]);
        }
        inline void update(Move m, DepthType ply){
           if (!sameMove(killers[0][ply], m)) {
              killers[ply][1] = killers[ply][0];
              killers[ply][0] = m;
           }
        }
    };

    struct HistoryT{
        ScoreType history[2][64][64]; // color, from, to
        ScoreType historyP[13][64]; // Piece, to
        ScoreType counter_history[13][64][13*64]; //previous moved piece, previous to, current moved piece * boardsize + current to
        inline void initHistory(){
            Logging::LogIt(Logging::logInfo) << "Init history" ;
            for(int i = 0; i < 64; ++i) for(int k = 0 ; k < 64; ++k) history[0][i][k] = history[1][i][k] = 0;
            for(int i = 0; i < 13; ++i) for(int k = 0 ; k < 64; ++k) historyP[i][k] = 0;
            for(int i = 0; i < 13; ++i) for(int j = 0 ; j < 64; ++j) for(int k = 0 ; k < 64; ++k)  counter_history[i][j][k] = -1;
        }
        template<int S>
        inline void update(DepthType depth, Move m, const Position & p, CMHPtrArray & cmhPtr){
            if ( Move2Type(m) == T_std ){
               const Color c = p.c;
               const Square from = Move2From(m);
               const Square to = Move2To(m);
               const ScoreType s = S * HSCORE(depth);
               const Piece pp = p.b[from];
               history[c][from][to] += s - history[c][from][to] * std::abs(s) / MAX_HISTORY;
               historyP[pp+PieceShift][to] += s - historyP[pp+PieceShift][to] * std::abs(s) / MAX_HISTORY;
               for (int i = 0; i < MAX_CMH_PLY; ++i){
                   if (cmhPtr[i]){
                      ScoreType & item = cmhPtr[i][(p.b[from]+PieceShift) * 64 + to];
                      item += s - item * std::abs(s) / MAX_HISTORY;
                   }
               }
            }
        }
    };

    struct CounterT{
        ScoreType counter[64][64];
        inline void initCounter(){
            Logging::LogIt(Logging::logInfo) << "Init counter" ;
            for(int i = 0; i < 64; ++i) for(int k = 0 ; k < 64; ++k) counter[i][k] = 0;
        }
        inline void update(Move m, const Position & p){ if ( VALIDMOVE(p.lastMove) ) counter[Move2From(p.lastMove)][Move2To(p.lastMove)] = m; }
    };
    KillerT killerT;
    HistoryT historyT;
    CounterT counterT;
    DepthType nullMoveMinPly = 0;

    void getCMHPtr(DepthType ply, CMHPtrArray & cmhPtr){
        cmhPtr.fill(0);
        for( int k = 0 ; k < MAX_CMH_PLY ; ++k){
            if( ply > k && VALIDMOVE(stack[ply-k].p.lastMove)){
               const Square to = Move2To(stack[ply-k].p.lastMove);
               cmhPtr[k] = historyT.counter_history[stack[ply-k-1].p.b[to]+PieceShift][to];
            }
        }
    }
    inline ScoreType getCMHScore(const Position & p, const Square from, const Square to, DepthType ply, const CMHPtrArray & cmhPtr) const {
        ScoreType ret = 0;
        for (int i = 0; i < MAX_CMH_PLY; i ++){ if (cmhPtr[i]){ ret += cmhPtr[i][(p.b[from]+PieceShift) * 64 + to]; } }
        return ret;
    }

    ScoreType drawScore() { return -1 + 2*((stats.counters[Stats::sid_nodes]+stats.counters[Stats::sid_qnodes]) % 2); }

    template <bool pvnode, bool canPrune = true> ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, bool cutNode, const Move skipMove = INVALIDMOVE);
    template <bool qRoot, bool pvnode> ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    ScoreType qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    bool SEE_GE(const Position & p, const Move & m, ScoreType threshold)const;
    ScoreType SEE(const Position & p, const Move & m)const;
    PVList search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth);
    template< bool withRep = true, bool isPv = true, bool INR = true> MaterialHash::Terminaison interiorNodeRecognizer(const Position & p)const;
    bool isRep(const Position & p, bool isPv)const;
    static void displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, const std::string & mark = "");

    void idleLoop(){
        while (true){
            std::unique_lock<std::mutex> lock(_mutex);
            _searching = false;
            _cv.notify_one(); // Wake up anyone waiting for search finished
            _cv.wait(lock, [&]{ return _searching; });
            if (_exit) return;
            lock.unlock();
            search();
        }
    }

    void start(){
        std::lock_guard<std::mutex> lock(_mutex);
        Logging::LogIt(Logging::logInfo) << "Starting worker " << id() ;
        _searching = true;
        _cv.notify_one(); // Wake up the thread in IdleLoop()
    }

    void wait(){
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [&]{ return !_searching; });
    }

    void search(){
        Logging::LogIt(Logging::logInfo) << "Search launched for thread " << id() ;
        if ( isMainThread() ){ ThreadPool::instance().startOthers(); } // started other threads but locked for now ...
        _data.pv = search(_data.p, _data.best, _data.depth, _data.sc, _data.seldepth);
    }

    size_t id()const { return _index;}
    bool   isMainThread()const { return id() == 0 ; }

    ThreadContext(size_t n):_index(n),_exit(false),_searching(true),_stdThread(&ThreadContext::idleLoop, this){ wait(); }

    ~ThreadContext(){
        _exit = true;
        start();
        Logging::LogIt(Logging::logInfo) << "Waiting for workers to join...";
        _stdThread.join();
    }

    void setData(const ThreadData & d){ _data = d;}
    const ThreadData & getData()const{ return _data;}

    static std::atomic<bool> startLock;

    bool searching()const{return  _searching;}

    #pragma pack(push, 1)
    struct PawnEntry{
        BitBoard pawnTargets[2]   = {empty,empty};
        BitBoard holes[2]         = {empty,empty};
        BitBoard semiOpenFiles[2] = {empty,empty};
        BitBoard passed[2]        = {empty,empty};
        BitBoard openFiles        = empty;
        EvalScore score           = {0,0};
        ScoreType danger[2]       = {0,0};
        MiniHash h                = 0;
        void reset(){
            score[MG] = 0;   score[EG] = 0;
            danger[0] = 0;   danger[1] = 0;
        }
    };
    #pragma pack(pop)

    static const unsigned long long int ttSizePawn;
    std::unique_ptr<PawnEntry[]> tablePawn = 0;

    void initPawnTable(){
        assert(tablePawn==0);
        assert(ttSizePawn>0);
        Logging::LogIt(Logging::logInfo) << "Init Pawn TT : " << ttSizePawn;
        Logging::LogIt(Logging::logInfo) << "PawnEntry size " << sizeof(PawnEntry);
        tablePawn.reset(new PawnEntry[ttSizePawn]);
        Logging::LogIt(Logging::logInfo) << "Size of Pawn TT " << ttSizePawn * sizeof(PawnEntry) / 1024 / 1024 << "Mb" ;
    }

    void clearPawnTT() {
        for (unsigned int k = 0; k < ttSizePawn; ++k) tablePawn[k].h = 0;
    }

    bool getPawnEntry(Hash h, PawnEntry *& pe){
        assert(h > 0);
        PawnEntry & _e = tablePawn[h&(ttSizePawn-1)];
        pe = &_e;
        if ( _e.h != Hash64to32(h) )     return false;
        ++stats.counters[Stats::sid_ttPawnhits];
        return true;
    }

    void prefetchPawn(Hash h) {
       void * addr = (&tablePawn[h&(ttSizePawn-1)]);
    #  if defined(__INTEL_COMPILER)
       __asm__ ("");
    #  elif defined(_MSC_VER)
       _mm_prefetch((char*)addr, _MM_HINT_T0);
    #  else
       __builtin_prefetch(addr);
    #  endif
    }

private:
    ThreadData              _data;
    size_t                  _index;
    std::mutex              _mutex;
    static std::mutex       _mutexDisplay;
    std::condition_variable _cv;
    // next two MUST be initialized BEFORE _stdThread
    bool                    _exit;
    bool                    _searching;
    std::thread             _stdThread;
};

bool ThreadContext::stopFlag           = true;
TimeType  ThreadContext::currentMoveMs = 777; // a dummy initial value, useful for debug
MoveDifficultyUtil::MoveDifficulty ThreadContext::moveDifficulty = MoveDifficultyUtil::MD_std;
std::atomic<bool> ThreadContext::startLock;
const unsigned long long int ThreadContext::ttSizePawn = 1024*32;

ThreadPool & ThreadPool::instance(){ static ThreadPool pool; return pool;}

ThreadPool::~ThreadPool(){ Logging::LogIt(Logging::logInfo) << "... ok threadPool deleted"; }

void ThreadPool::setup(){
    assert(DynamicConfig::threads > 0);
    clear();
    Logging::LogIt(Logging::logInfo) << "Using " << DynamicConfig::threads << " threads";
    unsigned int maxThreads = std::max(1u,std::thread::hardware_concurrency());
    if (DynamicConfig::threads > maxThreads) {
        Logging::LogIt(Logging::logWarn) << "Trying to use more threads than hardware core, I don't like that and will use only " << maxThreads << " threads";
        DynamicConfig::threads = maxThreads;
    }
    while (size() < DynamicConfig::threads) {
       push_back(std::unique_ptr<ThreadContext>(new ThreadContext(size())));
       back()->initPawnTable();
    }
}

ThreadContext & ThreadPool::main() { return *(front()); }

void ThreadPool::wait(bool otherOnly) {
    Logging::LogIt(Logging::logInfo) << "Wait for workers to be ready";
    for (auto & s : *this) { if (!otherOnly || !(*s).isMainThread()) (*s).wait(); }
    Logging::LogIt(Logging::logInfo) << "...ok";
}

Hash computeHash(const Position &p){
#ifdef DEBUG_HASH
    Hash h = p.h;
    p.h = nullHash;
#endif
    if (p.h != nullHash) return p.h;
    ++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
    for (Square k = 0; k < 64; ++k){ ///todo try if BB is faster here ?
        const Piece pp = p.b[k];
        if ( pp != P_none) p.h ^= Zobrist::ZT[k][pp+PieceShift];
    }
    if ( p.ep != INVALIDSQUARE ) p.h ^= Zobrist::ZT[p.ep][13];
    if ( p.castling & C_wks)     p.h ^= Zobrist::ZT[7][13];
    if ( p.castling & C_wqs)     p.h ^= Zobrist::ZT[0][13];
    if ( p.castling & C_bks)     p.h ^= Zobrist::ZT[63][13];
    if ( p.castling & C_bqs)     p.h ^= Zobrist::ZT[56][13];
    if ( p.c == Co_White)        p.h ^= Zobrist::ZT[3][13];
    if ( p.c == Co_Black)        p.h ^= Zobrist::ZT[4][13];
#ifdef DEBUG_HASH
    if ( h != nullHash && h != p.h ){ Logging::LogIt(Logging::logFatal) << "Hash error " << ToString(p.lastMove) << ToString(p,true); }
#endif
    return p.h;
}

Hash computePHash(const Position &p){
#ifdef DEBUG_PHASH
    Hash h = p.ph;
    p.ph = nullHash;
#endif
    if (p.ph != nullHash) return p.ph;
    ++ThreadPool::instance().main().stats.counters[Stats::sid_hashComputed]; // shall of course never happend !
    BitBoard bb = p.whitePawn();
    while (bb) { p.ph ^= Zobrist::ZT[BBTools::popBit(bb)][P_wp + PieceShift]; }
    bb = p.blackPawn();
    while (bb) { p.ph ^= Zobrist::ZT[BBTools::popBit(bb)][P_bp + PieceShift]; }
    bb = p.whiteKing();
    while (bb) { p.ph ^= Zobrist::ZT[BBTools::popBit(bb)][P_wk + PieceShift]; }
    bb = p.blackKing();
    while (bb) { p.ph ^= Zobrist::ZT[BBTools::popBit(bb)][P_bk + PieceShift]; }
#ifdef DEBUG_PHASH
    if ( h != nullHash && h != p.ph ){ Logging::LogIt(Logging::logFatal) << "Pawn Hash error " << ToString(p.lastMove) << ToString(p,true) << p.ph << " != " << h; }
#endif
    return p.ph;
}

Move ThreadPool::search(const ThreadData & d){ // distribute data and call main thread search
    Logging::LogIt(Logging::logInfo) << "Search Sync" ;
    wait();
    ThreadContext::startLock.store(true);
    for (auto & s : *this) (*s).setData(d); // this is a copy
    Logging::LogIt(Logging::logInfo) << "Calling main thread search" ;
    main().search(); ///@todo 1 thread for nothing here
    ThreadContext::stopFlag = true;
    wait();
    return main().getData().best;
}

void ThreadPool::startOthers(){ for (auto & s : *this) if (!(*s).isMainThread()) (*s).start();}

ThreadPool::ThreadPool():stop(false){ push_back(std::unique_ptr<ThreadContext>(new ThreadContext(size())));} // this one will be called "Main" thread

Counter ThreadPool::counter(Stats::StatId id) const { Counter n = 0; for (auto & it : *this ){ n += it->stats.counters[id];  } return n;}

bool apply(Position & p, const Move & m, bool noValidation = false); //forward decl
bool isPseudoLegal(const Position & p, Move m); // forward decl
template < bool display = false, bool safeMatEvaluator = true > ScoreType eval(const Position & p, EvalData & data, ThreadContext & context); //forward decl

namespace TT{

GenerationType curGen = 0;
enum Bound : unsigned char{ B_exact = 0, B_alpha = 1, B_beta = 2, B_none = 3};
#pragma pack(push, 1)
struct Entry{
    Entry():m(INVALIDMINIMOVE),h(0),s(0),e(0),b(B_none),d(-1)/*,generation(curGen)*/{}
    Entry(Hash h, Move m, ScoreType s, ScoreType e, Bound b, DepthType d) : h(Hash64to32(h)), m(Move2MiniMove(m)), s(s), e(e), /*generation(curGen),*/ b(b), d(d){}
    MiniHash h;            //32
    ScoreType s, e;        //16 + 16
    union{
        MiniHash _d;       //32
        struct{
           MiniMove m;     //16
           Bound b;        //8
           DepthType d;    //8
        };
    };
    //GenerationType generation;
};
#pragma pack(pop)

struct Bucket { static const int nbBucket = 3;  Entry e[nbBucket];};

unsigned long long int powerFloor(unsigned long long int x) {
    unsigned long long int power = 1;
    while (power < x) power *= 2;
    return power/2;
}

static unsigned long long int ttSize = 0;
static std::unique_ptr<Bucket[]> table;

void initTable(){
    assert(table==nullptr);
    Logging::LogIt(Logging::logInfo) << "Init TT" ;
    Logging::LogIt(Logging::logInfo) << "Bucket size " << sizeof(Bucket);
    ttSize = 1024 * powerFloor((DynamicConfig::ttSizeMb * 1024) / (unsigned long long int)sizeof(Bucket));
    table.reset(new Bucket[ttSize]);
    Logging::LogIt(Logging::logInfo) << "Size of TT " << ttSize * sizeof(Bucket) / 1024 / 1024 << "Mb" ;
}

void clearTT() {
    TT::curGen = 0;
    for (unsigned int k = 0; k < ttSize; ++k) for (unsigned int i = 0 ; i < Bucket::nbBucket ; ++i) table[k].e[i] = { 0, INVALIDMINIMOVE, 0, 0, B_alpha, 0 };
}

int hashFull(){
    unsigned long long count = 0;
    for (unsigned int k = 0; k < ttSize; ++k) for (unsigned int i = 0 ; i < Bucket::nbBucket ; ++i) if ( table[k].e[i].h ) ++count;
    return int((count*1000)/(ttSize*Bucket::nbBucket));
}

void age(){ ++TT::curGen;}

void prefetch(Hash h) {
   void * addr = (&table[h&(ttSize-1)].e[0]);
#  if defined(__INTEL_COMPILER)
   __asm__ ("");
#  elif defined(_MSC_VER)
   _mm_prefetch((char*)addr, _MM_HINT_T0);
#  else
   __builtin_prefetch(addr);
#  endif
}

bool getEntry(ThreadContext & context, const Position & p, Hash h, DepthType d, Entry & e, int nbuck = 0) {
    assert(h > 0);
    // e.h must not be reset here because of recursion ! (would dismiss first hash found when _e.d < d)
    if ( DynamicConfig::disableTT  ) return false;
    if ( nbuck >= Bucket::nbBucket ) return false; // no more bucket
    Entry & _e = table[h&(ttSize-1)].e[nbuck];
#ifdef DEBUG_HASH_ENTRY
    _e.d = Zobrist::randomInt<unsigned int>(0, UINT32_MAX);
#endif
    if ( _e.h == 0 ) return false; //early exist cause next ones are also empty ...
    if ( !VALIDMOVE(_e.m) || 
#ifndef DEBUG_HASH_ENTRY
        (_e.h ^ _e._d) != Hash64to32(h) ||
#endif
        !isPseudoLegal(p, _e.m)) { return _e.h = 0, getEntry(context, p, h, d, e, nbuck + 1); } // next one
    e = _e; // update entry only if no collision is detected !
    if ( _e.d >= d ){ ++context.stats.counters[Stats::sid_tthits]; return true; } // valid entry if depth is ok
    else return getEntry(context,p,h,d,e,nbuck+1); // next one
}

void setEntry(ThreadContext & context, Hash h, Move m, ScoreType s, ScoreType eval, Bound b, DepthType d){
    Entry e = {h,m,s,eval,b,d};
    e.h ^= e._d;
    assert(e.h > 0);
    if ( DynamicConfig::disableTT ) return;
    const size_t index = h&(ttSize-1);
    table[index].e[0] = e; // first is always replace
    Entry * toUpdate = &(table[index].e[1]);
    for (unsigned int i = 1 ; i < Bucket::nbBucket ; ++i){ // other replace by depth (and generation ?)
        Entry & curEntry = table[index].e[i];
        if( curEntry.h == 0 ) break;
        if( curEntry.h == e.h && curEntry.d < e.d ) { toUpdate = &curEntry; break; }
        else if( curEntry.d < toUpdate->d /*|| curEntry.generation < toUpdate->generation*/ ) toUpdate = &curEntry; // not same hash, replace the oldest or lowest depth
    }
    ++context.stats.counters[Stats::sid_ttInsert];
    *toUpdate = e;
}

void getPV(const Position & p, ThreadContext & context, PVList & pv){
    TT::Entry e;
    Hash hashStack[MAX_PLY] = { nullHash };
    Position p2 = p;
    bool stop = false;
    for( int k = 0 ; k < MAX_PLY && !stop; ++k){
      if ( !TT::getEntry(context, p2, computeHash(p2), 0, e)) break;
      if (e.h != 0) {
        hashStack[k] = computeHash(p2);
        pv.push_back(e.m);
        if ( !VALIDMOVE(e.m) || !apply(p2,e.m) ) break;
        const Hash h = computeHash(p2);
        for (int i = k-1; i >= 0; --i) if (hashStack[i] == h) {stop=true;break;}
      }
    }
}

} // TT

std::string trim(const std::string& str, const std::string& whitespace = " \t"){
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return ""; // no content
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

std::string GetFENShort(const Position &p ){ // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR"
    std::stringstream ss;
    int count = 0;
    for (int i = 7; i >= 0; --i) {
        for (int j = 0; j < 8; j++) {
            const Square k = 8 * i + j;
            if (p.b[k] == P_none) ++count;
            else {
                if (count != 0) { ss << count; count = 0; }
                ss << PieceTools::getName(p,k);
            }
            if (j == 7) {
                if (count != 0) { ss << count; count = 0; }
                if (i != 0) ss << "/";
            }
        }
    }
    return ss.str();
}

std::string GetFENShort2(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5
    std::stringstream ss;
    ss << GetFENShort(p) << " " << (p.c == Co_White ? "w" : "b") << " ";
    bool withCastling = false;
    if (p.castling & C_wks) { ss << "K"; withCastling = true; }
    if (p.castling & C_wqs) { ss << "Q"; withCastling = true; }
    if (p.castling & C_bks) { ss << "k"; withCastling = true; }
    if (p.castling & C_bqs) { ss << "q"; withCastling = true; }
    if (!withCastling) ss << "-";
    if (p.ep != INVALIDSQUARE) ss << " " << SquareNames[p.ep];
    else ss << " -";
    return ss.str();
}

std::string GetFEN(const Position &p) { // "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d5 0 2"
    std::stringstream ss;
    ss << GetFENShort2(p) << " " << (int)p.fifty << " " << (int)p.moves;
    return ss.str();
}

std::string ToString(const Move & m, bool withScore){
    if ( m == INVALIDMOVE ) return "invalid move";
    if ( m == NULLMOVE )    return "null move";
    std::string prom;
    const std::string score = (withScore ? " (" + std::to_string(Move2Score(m)) + ")" : "");
    switch (Move2Type(m)) {
    case T_bks: return (DynamicConfig::FRC?"O-O":"e8g8") + score;
    case T_wks: return (DynamicConfig::FRC?"O-O":"e1g1") + score;
    case T_bqs: return (DynamicConfig::FRC?"O-O-O":"e8c8") + score;
    case T_wqs: return (DynamicConfig::FRC?"O-O-O":"e1c1") + score;
    default:
        static const std::string promSuffixe[] = { "","","","","q","r","b","n","q","r","b","n" };
        prom = promSuffixe[Move2Type(m)];
        break;
    }
    return SquareNames[Move2From(m)] + SquareNames[Move2To(m)] + prom + score;
}

std::string ToString(const PVList & moves){
    std::stringstream ss;
    for (size_t k = 0; k < moves.size(); ++k) { if (moves[k] == INVALIDMOVE) break;  ss << ToString(moves[k]) << " "; }
    return ss.str();
}

std::string ToString(const Position::Material & mat) {
    std::stringstream str;
    str << "\n" << Logging::_protocolComment[Logging::ct] << "K  :" << (int)mat[Co_White][M_k] << "\n" << Logging::_protocolComment[Logging::ct] << "Q  :" << (int)mat[Co_White][M_q] << "\n" << Logging::_protocolComment[Logging::ct] << "R  :" << (int)mat[Co_White][M_r] << "\n" << Logging::_protocolComment[Logging::ct] << "B  :" << (int)mat[Co_White][M_b] << "\n" << Logging::_protocolComment[Logging::ct] << "L  :" << (int)mat[Co_White][M_bl] << "\n" << Logging::_protocolComment[Logging::ct] << "D  :" << (int)mat[Co_White][M_bd] << "\n" << Logging::_protocolComment[Logging::ct] << "N  :" << (int)mat[Co_White][M_n] << "\n" << Logging::_protocolComment[Logging::ct] << "P  :" << (int)mat[Co_White][M_p] << "\n" << Logging::_protocolComment[Logging::ct] << "Ma :" << (int)mat[Co_White][M_M] << "\n" << Logging::_protocolComment[Logging::ct] << "Mi :" << (int)mat[Co_White][M_m] << "\n" << Logging::_protocolComment[Logging::ct] << "T  :" << (int)mat[Co_White][M_t] << "\n";
    str << "\n" << Logging::_protocolComment[Logging::ct] << "k  :" << (int)mat[Co_Black][M_k] << "\n" << Logging::_protocolComment[Logging::ct] << "q  :" << (int)mat[Co_Black][M_q] << "\n" << Logging::_protocolComment[Logging::ct] << "r  :" << (int)mat[Co_Black][M_r] << "\n" << Logging::_protocolComment[Logging::ct] << "b  :" << (int)mat[Co_Black][M_b] << "\n" << Logging::_protocolComment[Logging::ct] << "l  :" << (int)mat[Co_Black][M_bl] << "\n" << Logging::_protocolComment[Logging::ct] << "d  :" << (int)mat[Co_Black][M_bd] << "\n" << Logging::_protocolComment[Logging::ct] << "n  :" << (int)mat[Co_Black][M_n] << "\n" << Logging::_protocolComment[Logging::ct] << "p  :" << (int)mat[Co_Black][M_p] << "\n" << Logging::_protocolComment[Logging::ct] << "ma :" << (int)mat[Co_Black][M_M] << "\n" << Logging::_protocolComment[Logging::ct] << "mi :" << (int)mat[Co_Black][M_m] << "\n" << Logging::_protocolComment[Logging::ct] << "t  :" << (int)mat[Co_Black][M_t] << "\n";
    return str.str();
}

std::string ToString(const Position & p, bool noEval){
    std::stringstream ss;
    ss << "Position" << std::endl;
    for (Square j = 7; j >= 0; --j) {
        ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl << Logging::_protocolComment[Logging::ct] << " |";
        for (Square i = 0; i < 8; ++i) ss << PieceTools::getName(p,i+j*8) << '|';
        ss << std::endl;
    }
    ss << Logging::_protocolComment[Logging::ct] << " +-+-+-+-+-+-+-+-+" << std::endl;
    if ( p.ep >=0 ) ss << Logging::_protocolComment[Logging::ct] << " ep " << SquareNames[p.ep] << std::endl;
    //ss << Logging::_protocolComment[Logging::ct] << " wk " << (p.king[Co_White]!=INVALIDSQUARE?SquareNames[p.king[Co_White]]:"none") << std::endl;
    //ss << Logging::_protocolComment[Logging::ct] << " bk " << (p.king[Co_Black]!=INVALIDSQUARE?SquareNames[p.king[Co_Black]]:"none") << std::endl;
    ss << Logging::_protocolComment[Logging::ct] << " Turn " << (p.c == Co_White ? "white" : "black") << std::endl;
    ScoreType sc = 0;
    if ( ! noEval ){
        EvalData data;
        sc = eval(p, data, ThreadPool::instance().main());
        ss << Logging::_protocolComment[Logging::ct] << " Phase " << data.gp << std::endl << Logging::_protocolComment[Logging::ct] << " Static score " << sc << std::endl << Logging::_protocolComment[Logging::ct] << " Hash " << computeHash(p) << std::endl << Logging::_protocolComment[Logging::ct] << " FEN " << GetFEN(p);
    }
    //ss << ToString(p.mat);
    return ss.str();
}

template < typename T > T readFromString(const std::string & s){ std::stringstream ss(s); T tmp; ss >> tmp; return tmp;}

bool readFEN(const std::string & fen, Position & p, bool silent, bool withMoveCount){
    static Position defaultPos;
    p = defaultPos;
    std::vector<std::string> strList;
    std::stringstream iss(fen);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    if ( !silent) Logging::LogIt(Logging::logInfo) << "Reading fen " << fen ;

    // reset position
    p.h = nullHash; p.ph = nullHash;
    for(Square k = 0 ; k < 64 ; ++k) p.b[k] = P_none;

    Square j = 1, i = 0;
    while ((j <= 64) && (i <= (char)strList[0].length())){
        char letter = strList[0].at(i);
        ++i;
        const Square k = (7 - (j - 1) / 8) * 8 + ((j - 1) % 8);
        switch (letter) {
        case 'p': p.b[k]= P_bp; break;
        case 'r': p.b[k]= P_br; break;
        case 'n': p.b[k]= P_bn; break;
        case 'b': p.b[k]= P_bb; break;
        case 'q': p.b[k]= P_bq; break;
        case 'k': p.b[k]= P_bk; p.king[Co_Black] = k; break;
        case 'P': p.b[k]= P_wp; break;
        case 'R': p.b[k]= P_wr; break;
        case 'N': p.b[k]= P_wn; break;
        case 'B': p.b[k]= P_wb; break;
        case 'Q': p.b[k]= P_wq; break;
        case 'K': p.b[k]= P_wk; p.king[Co_White] = k; break;
        case '/': j--; break;
        case '1': break;
        case '2': j++; break;
        case '3': j += 2; break;
        case '4': j += 3; break;
        case '5': j += 4; break;
        case '6': j += 5; break;
        case '7': j += 6; break;
        case '8': j += 7; break;
        default: Logging::LogIt(Logging::logFatal) << "FEN ERROR -1 : invalid character in fen string :" << letter ;
        }
        j++;
    }

    if ( p.king[Co_White] == INVALIDSQUARE || p.king[Co_Black] == INVALIDSQUARE ) { Logging::LogIt(Logging::logFatal) << "FEN ERROR 0 : missing king" ; return false; }

    p.c = Co_White; // set the turn; default is white
    if (strList.size() >= 2){
        if (strList[1] == "w")      p.c = Co_White;
        else if (strList[1] == "b") p.c = Co_Black;
        else { Logging::LogIt(Logging::logFatal) << "FEN ERROR 1 : bad color" ; return false; }
    }

    // Initialize all castle possibilities (default is none)
    p.castling = C_none;
    if (strList.size() >= 3){
        bool found = false;
        if ( !DynamicConfig::FRC){
           //Logging::LogIt(Logging::logInfo) << "is not FRC";
           if (strList[2].find('K') != std::string::npos){ p.castling |= C_wks; found = true; }
           if (strList[2].find('Q') != std::string::npos){ p.castling |= C_wqs; found = true; }
           if (strList[2].find('k') != std::string::npos){ p.castling |= C_bks; found = true; }
           if (strList[2].find('q') != std::string::npos){ p.castling |= C_bqs; found = true; }
        }
        else{
           Logging::LogIt(Logging::logInfo) << "is FCR";
           for ( const char & cr : strList[2] ){
               Logging::LogIt(Logging::logInfo) << cr;
               const Color c = std::isupper(cr) ? Co_White : Co_Black;
               const char kf = std::toupper(FileNames[SQFILE(p.king[c])].at(0));
               if ( std::toupper(cr) > kf ) { p.castling |= (c==Co_White ? C_wks:C_bks); found = true; }
               else                         { p.castling |= (c==Co_White ? C_wqs:C_bqs); found = true; }
           }
        }
        if (strList[2].find('-') != std::string::npos){ found = true; /*Logging::LogIt(Logging::logInfo) << "No castling right given" ;*/}
        if ( ! found ){ if ( !silent) Logging::LogIt(Logging::logWarn) << "No castling right given" ; }
        else{ ///@todo detect illegal stuff in here
            p.kingInit[Co_White] = p.king[Co_White];
            p.kingInit[Co_Black] = p.king[Co_Black];
            if ( p.castling & C_wqs ) { for( Square s = Sq_a1 ; s <= Sq_h1 ; ++s ){ if ( s < p.king[Co_White] && p.b[s]==P_wr ) { p.rooksInit[Co_White][CT_OOO] = s; break; } } }
            if ( p.castling & C_wks ) { for( Square s = Sq_a1 ; s <= Sq_h1 ; ++s ){ if ( s > p.king[Co_White] && p.b[s]==P_wr ) { p.rooksInit[Co_White][CT_OO]  = s; break; } } }
            if ( p.castling & C_bqs ) { for( Square s = Sq_a8 ; s <= Sq_h8 ; ++s ){ if ( s < p.king[Co_Black] && p.b[s]==P_br ) { p.rooksInit[Co_Black][CT_OOO] = s; break; } } }
            if ( p.castling & C_bks ) { for( Square s = Sq_a8 ; s <= Sq_h8 ; ++s ){ if ( s > p.king[Co_Black] && p.b[s]==P_br ) { p.rooksInit[Co_Black][CT_OO]  = s; break; } } }
        }
    }
    else if ( !silent) Logging::LogIt(Logging::logInfo) << "No castling right given" ;

    // read en passant and save it (default is invalid)
    p.ep = INVALIDSQUARE;
    if ((strList.size() >= 4) && strList[3] != "-" ){
        if (strList[3].length() >= 2){
            if ((strList[3].at(0) >= 'a') && (strList[3].at(0) <= 'h') && ((strList[3].at(1) == '3') || (strList[3].at(1) == '6'))) p.ep = stringToSquare(strList[3]);
            else { Logging::LogIt(Logging::logFatal) << "FEN ERROR 2 : bad en passant square : " << strList[3] ; return false; }
        }
        else{ Logging::LogIt(Logging::logFatal) << "FEN ERROR 3 : bad en passant square : " << strList[3] ; return false; }
    }
    else if ( !silent) Logging::LogIt(Logging::logInfo) << "No en passant square given" ;

    assert(p.ep == INVALIDSQUARE || (SQRANK(p.ep) == 2 || SQRANK(p.ep) == 5));

    // read 50 moves rules
    if (withMoveCount && strList.size() >= 5) p.fifty = (unsigned char)readFromString<int>(strList[4]);
    else p.fifty = 0;

    // read number of move
    if (withMoveCount && strList.size() >= 6) p.moves = (unsigned char)readFromString<int>(strList[5]);
    else p.moves = 1;

    if (p.moves < 1) { // fix a LittleBlitzer bug here ...
        Logging::LogIt(Logging::logWarn) << "Wrong move counter " << (int)p.moves << " using 1 instead";
        p.moves = 1;
    }

    p.halfmoves = (int(p.moves) - 1) * 2 + 1 + (p.c == Co_Black ? 1 : 0);

    BBTools::setBitBoards(p);
    MaterialHash::initMaterial(p);
    p.h = computeHash(p);
    p.ph = computePHash(p);
    return true;
}

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " ){
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);
    while (std::string::npos != pos || std::string::npos != lastPos) {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos     = str.find_first_of(delimiters, lastPos);
    }
}

inline std::string SanitizeCastling(const Position & p, const std::string & str){
    if (( str == "e1g1" && p.b[Sq_e1] == P_wk ) || ( str == "e8g8" && p.b[Sq_e8] == P_bk )) return "0-0";
    if (( str == "e1c1" && p.b[Sq_e1] == P_wk ) || ( str == "e8c8" && p.b[Sq_e8] == P_bk )) return "0-0-0";
    return str;
}

inline Move SanitizeCastling(const Position & p, const Move & m){
    if ( !VALIDMOVE(m) ) return m;
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    MType mtype = Move2Type(m);
    // convert GUI castling input notation to internal castling style if not FRC
    if ( !DynamicConfig::FRC ){
       bool whiteToMove = p.c == Co_White;
       if (mtype == T_std && from == p.kingInit[p.c] ) {
           if      (to == (whiteToMove ? Sq_c1 : Sq_c8)) return ToMove(from, to, whiteToMove ? T_wqs : T_bqs);
           else if (to == (whiteToMove ? Sq_g1 : Sq_g8)) return ToMove(from, to, whiteToMove ? T_wks : T_bks);
       }
    }
    return m;
}

inline Square kingSquare(const Position & p) { return p.king[p.c]; }

bool readMove(const Position & p, const std::string & ss, Square & from, Square & to, MType & moveType ) {
    if ( ss.empty()){
        Logging::LogIt(Logging::logFatal) << "Trying to read empty move ! " ;
        moveType = T_std;
        return false;
    }
    std::string str(ss);
    str = SanitizeCastling(p,str);
    // add space to go to own internal notation (if not castling)
    if ( str != "0-0" && str != "0-0-0" && str != "O-O" && str != "O-O-O" ) str.insert(2," ");

    std::vector<std::string> strList;
    std::stringstream iss(str);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));

    moveType = T_std;
    if ( strList.empty()){ Logging::LogIt(Logging::logError) << "Trying to read bad move, seems empty " << str ; return false; }

    // detect castling
    if (strList[0] == "0-0" || strList[0] == "O-O") {
        moveType = (p.c == Co_White) ? T_wks : T_bks;
        from = kingSquare(p);
        to = (p.c == Co_White) ? Sq_g1 : Sq_g8;
    }
    else if (strList[0] == "0-0-0" || strList[0] == "O-O-O") {
        moveType = (p.c == Co_White) ? T_wqs : T_bqs;
        from = kingSquare(p);
        to = (p.c == Co_White) ? Sq_c1 : Sq_c8;
    }
    else{
        if ( strList.size() == 1 ){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (=1) " << str ; return false; }
        if ( strList.size() > 2 && strList[2] != "ep"){ Logging::LogIt(Logging::logError) << "Trying to read bad move, malformed (>2)" << str ; return false; }
        if (strList[0].size() == 2 && (strList[0].at(0) >= 'a') && (strList[0].at(0) <= 'h') && ((strList[0].at(1) >= 1) && (strList[0].at(1) <= '8'))) from = stringToSquare(strList[0]);
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid from square " << str ; return false; }
        bool isCapture = false;
        // be carefull, promotion possible !
        if (strList[1].size() >= 2 && (strList[1].at(0) >= 'a') && (strList[1].at(0) <= 'h') &&  ((strList[1].at(1) >= '1') && (strList[1].at(1) <= '8'))) {
            if ( strList[1].size() > 2 ){ // promotion
                std::string prom;
                if ( strList[1].size() == 3 ){ // probably e7 e8q notation
                    prom = strList[1][2];
                    to = stringToSquare(strList[1].substr(0,2));
                }
                else{ // probably e7 e8=q notation
                    std::vector<std::string> strListTo;
                    tokenize(strList[1],strListTo,"=");
                    to = stringToSquare(strListTo[0]);
                    prom = strListTo[1];
                }
                isCapture = p.b[to] != P_none;
                if      ( prom == "Q" || prom == "q") moveType = isCapture ? T_cappromq : T_promq;
                else if ( prom == "R" || prom == "r") moveType = isCapture ? T_cappromr : T_promr;
                else if ( prom == "B" || prom == "b") moveType = isCapture ? T_cappromb : T_promb;
                else if ( prom == "N" || prom == "n") moveType = isCapture ? T_cappromn : T_promn;
                else{ Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
            }
            else{
                to = stringToSquare(strList[1]);
                isCapture = p.b[to] != P_none;
                if(isCapture) moveType = T_capture;
            }
        }
        else { Logging::LogIt(Logging::logError) << "Trying to read bad move, invalid to square " << str ; return false; }
        if (PieceTools::getPieceType(p,from) == P_wp && to == p.ep) moveType = T_ep;
    }
    if ( DynamicConfig::FRC ){
       // In FRC, some castling way be encoded king takes rooks ... Let's check that, the dirty way
       if ( p.b[from] == P_wk && p.b[to] == P_wr ){ moveType = (to<from ? T_wqs : T_wks); }
       if ( p.b[from] == P_bk && p.b[to] == P_br ){ moveType = (to<from ? T_bqs : T_bks); }
    }
    if (!isPseudoLegal(p, ToMove(from, to, moveType))) {
        Logging::LogIt(Logging::logError) << "Trying to read bad move, not legal " << ToString(p) << str;
        return false;
    }
    return true;
}

namespace TimeMan{
TimeType msecPerMove, msecInTC, nbMoveInTC, msecInc, msecUntilNextTC, overHead = 0;
DepthType moveToGo;
unsigned long long maxKNodes;
bool isDynamic;
bool isUCIPondering;
std::chrono::time_point<Clock> startTime;

void init(){
    Logging::LogIt(Logging::logInfo) << "Init timeman" ;
    msecPerMove = 777;
    msecInTC    = nbMoveInTC = msecInc = msecUntilNextTC = -1;
    moveToGo    = -1;
    maxKNodes   = 0;
    isDynamic   = false;
    isUCIPondering  = false;
    ///@todo a bool for possible emergency functionality
}

TimeType GetNextMSecPerMove(const Position & p){
    static const TimeType msecMarginMin = 100; // this is HUGE at short TC !
    static const TimeType msecMarginMax = 1000;
    static const float msecMarginCoef   = 0.01f;
    TimeType ms = -1;
    Logging::LogIt(Logging::logInfo) << "msecPerMove     " << msecPerMove;
    Logging::LogIt(Logging::logInfo) << "msecInTC        " << msecInTC   ;
    Logging::LogIt(Logging::logInfo) << "msecInc         " << msecInc    ;
    Logging::LogIt(Logging::logInfo) << "nbMoveInTC      " << nbMoveInTC ;
    Logging::LogIt(Logging::logInfo) << "msecUntilNextTC " << msecUntilNextTC;
    Logging::LogIt(Logging::logInfo) << "currentNbMoves  " << int(p.moves);
    Logging::LogIt(Logging::logInfo) << "moveToGo        " << int(moveToGo);
    Logging::LogIt(Logging::logInfo) << "maxKNodes       " << maxKNodes;
    TimeType msecIncLoc = (msecInc > 0) ? msecInc : 0;
    if ( maxKNodes > 0 ){
        Logging::LogIt(Logging::logInfo) << "Fixed nodes per move";
        ms =  INFINITETIME;
    }
    else if ( msecPerMove > 0 ) {
        Logging::LogIt(Logging::logInfo) << "Fixed time per move";
        ms =  msecPerMove;
    }
    else if ( nbMoveInTC > 0){ // mps is given (xboard style)
        Logging::LogIt(Logging::logInfo) << "Xboard style TC";
        assert(msecInTC > 0); assert(nbMoveInTC > 0);
        Logging::LogIt(Logging::logInfo) << "TC mode, xboard";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC - msecMarginMin) / (float)nbMoveInTC) + msecIncLoc ;
        else { ms = std::min(msecUntilNextTC - msecMargin, int((msecUntilNextTC - msecMargin) /float(nbMoveInTC - ((p.moves - 1) % nbMoveInTC))) + msecIncLoc); }
    }
    else if (moveToGo > 0) { // moveToGo is given (uci style)
        assert(msecUntilNextTC > 0);
        Logging::LogIt(Logging::logInfo) << "UCI style TC";
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecUntilNextTC)), msecMarginMin);
        if (!isDynamic) Logging::LogIt(Logging::logFatal) << "bad timing configuration ...";
        else { ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / float(moveToGo) + msecIncLoc)*(isUCIPondering?3:2)/2); }
    }
    else{ // mps is not given
        Logging::LogIt(Logging::logInfo) << "Suddendeath style";
        const int nmoves = 17; // always be able to play this more moves !
        Logging::LogIt(Logging::logInfo) << "nmoves    " << nmoves;
        Logging::LogIt(Logging::logInfo) << "p.moves   " << int(p.moves);
        assert(nmoves > 0); assert(msecInTC >= 0);
        const TimeType msecMargin = std::max(std::min(msecMarginMax, TimeType(msecMarginCoef*msecInTC)), msecMarginMin);
        if (!isDynamic) ms = int((msecInTC+msecIncLoc-msecMarginMin) / (float)(nmoves)) ;
        else ms = std::min(msecUntilNextTC - msecMargin, TimeType((msecUntilNextTC - msecMargin) / (float)nmoves + msecIncLoc )*(isUCIPondering?3:2)/2);
    }
    return std::max(ms-overHead, TimeType(20));// if not much time left, let's try that hoping for a friendly GUI...
}
} // TimeMan

TimeType ThreadContext::getCurrentMoveMs() {
    if (TimeMan::isUCIPondering) {
        return INFINITETIME;
    }
    TimeType ret = currentMoveMs;
    if (TimeMan::msecUntilNextTC > 0){
        switch (moveDifficulty) {
        case MoveDifficultyUtil::MD_forced:      ret = (ret >> 4); break;
        case MoveDifficultyUtil::MD_easy:        ret = (ret >> 3); break;
        case MoveDifficultyUtil::MD_std:         break;
        case MoveDifficultyUtil::MD_hardDefense: ret = (std::min(TimeType(TimeMan::msecUntilNextTC*MoveDifficultyUtil::maxStealFraction), ret*MoveDifficultyUtil::emergencyFactor)); break;
        case MoveDifficultyUtil::MD_hardAttack:  break;
        }
    }
    return std::max(ret, TimeType(20));// if not much time left, let's try that ...;
}

inline bool isAttacked(const Position & p, const Square k) { return k!=INVALIDSQUARE && BBTools::isAttackedBB(p, k, p.c);}
inline bool isAttacked(const Position & p, BitBoard bb) { // copy ///@todo should be done without iterate over Square !
    while ( bb ) if ( isAttacked(p, Square(BBTools::popBit(bb)))) return true;
    return false;
}

namespace MoveGen{

enum GenPhase { GP_all = 0, GP_cap = 1, GP_quiet = 2 };
inline void addMove(Square from, Square to, MType type, MoveList & moves) { assert(from >= 0 && from < 64); assert(to >= 0 && to < 64); moves.push_back(ToMove(from, to, type, 0)); }

template < GenPhase phase = GP_all >
void generateSquare(const Position & p, MoveList & moves, Square from){
    assert(from != INVALIDSQUARE);
    const Color side = p.c;
    const BitBoard myPieceBB  = p.allPieces[side];
    const BitBoard oppPieceBB = p.allPieces[~side];
    const Piece piece = p.b[from];
    const Piece ptype = (Piece)std::abs(piece);
    assert ( ptype != P_none );
    if (ptype != P_wp) {
        BitBoard bb = BBTools::pfCoverage[ptype-1](from, p.occupancy, p.c) & ~myPieceBB;
        if      (phase == GP_cap)   bb &= oppPieceBB;  // only target opponent piece
        else if (phase == GP_quiet) bb &= ~oppPieceBB; // do not target opponent piece
        while (bb) {
            const Square to = BBTools::popBit(bb);
            const bool isCap = (phase == GP_cap) || ((oppPieceBB&SquareToBitboard(to)) != empty);
            if (isCap) addMove(from,to,T_capture,moves);
            else addMove(from,to,T_std,moves);
        }
        if ( phase != GP_cap && ptype == P_wk ){ // castling
            if ( side == Co_White) {
                if ( p.castling & (C_wqs|C_wks) ){
                   if ( (p.castling & C_wqs)
                        && ( ( (BBTools::mask[p.king[Co_White]].between[Sq_c1] | BBTools::mask[p.rooksInit[Co_White][CT_OOO]].between[Sq_d1]) & p.occupancy) == empty)
                        && !isAttacked(p,BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White])) ) addMove(from, Sq_c1, T_wqs, moves); // wqs
                   if ( (p.castling & C_wks)
                        && ( ( (BBTools::mask[p.king[Co_White]].between[Sq_g1] | BBTools::mask[p.rooksInit[Co_White][CT_OO ]].between[Sq_f1]) & p.occupancy) == empty)
                        && !isAttacked(p,BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White])) ) addMove(from, Sq_g1, T_wks, moves); // wks
                }
            }
            else if ( p.castling & (C_bqs|C_bks)){
                if ( (p.castling & C_bqs)
                     && ( ( (BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BBTools::mask[p.rooksInit[Co_Black][CT_OOO]].between[Sq_d8]) & p.occupancy) == empty)
                     && !isAttacked(p,BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black])) ) addMove(from, Sq_c8, T_bqs, moves); // bqs
                if ( (p.castling & C_bks)
                     && ( ( (BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BBTools::mask[p.rooksInit[Co_Black][CT_OO ]].between[Sq_f8]) & p.occupancy) == empty)
                     && !isAttacked(p,BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black])) ) addMove(from, Sq_g8, T_bks, moves); // bks
            }
        }
    }
    else {
        BitBoard pawnmoves = empty;
        static const BitBoard rank1_or_rank8 = rank1 | rank8;
        if ( phase != GP_quiet) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & oppPieceBB;
        while (pawnmoves) {
            const Square to = BBTools::popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_cappromq, moves); // pawn capture with promotion
                addMove(from, to, T_cappromr, moves); // pawn capture with promotion
                addMove(from, to, T_cappromb, moves); // pawn capture with promotion
                addMove(from, to, T_cappromn, moves); // pawn capture with promotion
            } else addMove(from,to,T_capture,moves);
        }
        if ( phase != GP_cap) pawnmoves |= BBTools::mask[from].push[p.c] & ~p.occupancy;
        if ((phase != GP_cap) && (BBTools::mask[from].push[p.c] & p.occupancy) == empty) pawnmoves |= BBTools::mask[from].dpush[p.c] & ~p.occupancy;
        while (pawnmoves) {
            const Square to = BBTools::popBit(pawnmoves);
            if ( SquareToBitboard(to) & rank1_or_rank8 ) {
                addMove(from, to, T_promq, moves); // promotion Q
                addMove(from, to, T_promr, moves); // promotion R
                addMove(from, to, T_promb, moves); // promotion B
                addMove(from, to, T_promn, moves); // promotion N
            } else addMove(from,to,T_std,moves);
        }
        if ( p.ep != INVALIDSQUARE && phase != GP_quiet ) pawnmoves = BBTools::mask[from].pawnAttack[p.c] & ~myPieceBB & SquareToBitboard(p.ep);
        while (pawnmoves) addMove(from,BBTools::popBit(pawnmoves),T_ep,moves);
    }
}

template < GenPhase phase = GP_all >
void generate(const Position & p, MoveList & moves, bool doNotClear = false){
    START_TIMER
    if (!doNotClear) moves.clear();
    BitBoard myPieceBBiterator = ( (p.c == Co_White) ? p.allPieces[Co_White] : p.allPieces[Co_Black]);
    while (myPieceBBiterator) generateSquare<phase>(p,moves,BBTools::popBit(myPieceBBiterator));
    STOP_AND_SUM_TIMER(Generate)
}

} // MoveGen

inline void movePiece(Position & p, Square from, Square to, Piece fromP, Piece toP, bool isCapture = false, Piece prom = P_none) {
    START_TIMER
    const int fromId   = fromP + PieceShift;
    const int toId     = toP + PieceShift;
    const Piece toPnew = prom != P_none ? prom : fromP;
    const int toIdnew  = prom != P_none ? (prom + PieceShift) : fromId;
    assert(from>=0 && from<64);
    assert(to>=0 && to<64);
    assert(fromP != P_none);
    // update board
    p.b[from] = P_none;
    p.b[to]   = toPnew;
    // update bitboard
    BBTools::unSetBit(p, from, fromP);
    BBTools::unSetBit(p, to,   toP); // usefull only if move is a capture
    BBTools::setBit  (p, to,   toPnew);
    // update Zobrist hash
    p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
    p.h ^= Zobrist::ZT[to][toIdnew]; // add fromP (or prom) at to
    if ( abs(fromP) == P_wp || abs(fromP) == P_wk ){
       p.ph ^= Zobrist::ZT[from][fromId]; // remove fromP at from
       if ( prom == P_none) p.ph ^= Zobrist::ZT[to][toIdnew]; // add fromP (if not prom) at to
    }
    if (isCapture) { // if capture remove toP at to
        p.h ^= Zobrist::ZT[to][toId];
        if ( (abs(toP) == P_wp || abs(toP) == P_wk) ) p.ph ^= Zobrist::ZT[to][toId];
    }
    STOP_AND_SUM_TIMER(MovePiece)
}

template < Color c>
inline void movePieceCastle(Position & p, CastlingTypes ct, Square kingDest, Square rookDest){
    const Piece pk = c==Co_White?P_wk:P_bk;
    const Piece pr = c==Co_White?P_wr:P_br;
    const CastlingRights ks = c==Co_White?C_wks:C_bks;
    const CastlingRights qs = c==Co_White?C_wqs:C_bqs;
    const Square sks = c==Co_White?7:63;
    const Square sqs = c==Co_White?0:56;
    BBTools::unSetBit(p, p.king[c]);
    BBTools::unSetBit(p, p.rooksInit[c][ct]);
    BBTools::setBit(p, kingDest, pk);
    BBTools::setBit(p, rookDest, pr);
    p.b[p.king[c]] = P_none;
    p.b[p.rooksInit[c][ct]] = P_none;
    p.b[kingDest] = pk;
    p.b[rookDest] = pr;
    p.h ^= Zobrist::ZT[p.king[c]][pk+PieceShift];
    p.ph ^= Zobrist::ZT[p.king[c]][pk+PieceShift];
    p.h ^= Zobrist::ZT[p.rooksInit[c][ct]][pr+PieceShift];
    p.h ^= Zobrist::ZT[kingDest][pk+PieceShift];
    p.ph ^= Zobrist::ZT[kingDest][pk+PieceShift];
    p.h ^= Zobrist::ZT[rookDest][pr+PieceShift];
    p.king[c] = kingDest;
    if (p.castling & qs) p.h ^= Zobrist::ZT[sqs][13];
    if (p.castling & ks) p.h ^= Zobrist::ZT[sks][13];
    p.castling &= ~(ks | qs);
}

void applyNull(ThreadContext & context, Position & pN) {
    pN.c = ~pN.c;
    pN.h ^= Zobrist::ZT[3][13];
    pN.h ^= Zobrist::ZT[4][13];
    if (pN.ep != INVALIDSQUARE) pN.h ^= Zobrist::ZT[pN.ep][13];
    pN.ep = INVALIDSQUARE;
    pN.lastMove = NULLMOVE;
    if ( pN.c == Co_White ) ++pN.moves;
    ++pN.halfmoves;
}

bool apply(Position & p, const Move & m, bool noValidation){
    START_TIMER
    assert(VALIDMOVE(m));
#ifdef DEBUG_MATERIAL
    Position previous = p;
#endif
    const Square from  = Move2From(m);
    const Square to    = Move2To(m);
    const MType  type  = Move2Type(m);
    const Piece  fromP = p.b[from];
    const Piece  toP   = p.b[to];
    const int fromId   = fromP + PieceShift;
#ifdef DEBUG_APPLY
    if (!isPseudoLegal(p, m)) {
        Logging::LogIt(Logging::logError) << "Apply error, not legal " << ToString(p) << ToString(m);
        assert(false);
    }
#endif
    switch(type){
    case T_std:
    case T_capture:
    case T_reserved:
        p.mat[~p.c][std::abs(toP)]--;
        movePiece(p, from, to, fromP, toP, type == T_capture);

        // update castling rigths and king position
        if ( fromP == P_wk ){
            p.king[Co_White] = to;
            if (p.castling & C_wks) p.h ^= Zobrist::ZT[7][13];
            if (p.castling & C_wqs) p.h ^= Zobrist::ZT[0][13];
            p.castling &= ~(C_wks | C_wqs);
        }
        else if ( fromP == P_bk ){
            p.king[Co_Black] = to;
            if (p.castling & C_bks) p.h ^= Zobrist::ZT[63][13];
            if (p.castling & C_bqs) p.h ^= Zobrist::ZT[56][13];
            p.castling &= ~(C_bks | C_bqs);
        }
        // king capture : is that necessary ???
        if      ( toP == P_wk ) p.king[Co_White] = INVALIDSQUARE;
        else if ( toP == P_bk ) p.king[Co_Black] = INVALIDSQUARE;

        if ( p.castling != C_none ){
           if ( (p.castling & C_wqs) && from == p.rooksInit[Co_White][CT_OOO] && fromP == P_wr ){
               p.castling &= ~C_wqs;
               p.h ^= Zobrist::ZT[0][13];
           }
           else if ( (p.castling & C_wks) && from == p.rooksInit[Co_White][CT_OO] && fromP == P_wr ){
               p.castling &= ~C_wks;
               p.h ^= Zobrist::ZT[7][13];
           }
           else if ( (p.castling & C_bqs) && from == p.rooksInit[Co_Black][CT_OOO] && fromP == P_br){
               p.castling &= ~C_bqs;
               p.h ^= Zobrist::ZT[56][13];
           }
           else if ( (p.castling & C_bks) && from == p.rooksInit[Co_Black][CT_OO] && fromP == P_br ){
               p.castling &= ~C_bks;
               p.h ^= Zobrist::ZT[63][13];
           }
        }
        break;

    case T_ep: {
        assert(p.ep != INVALIDSQUARE);
        assert(SQRANK(p.ep) == EPRank[p.c]);
        const Square epCapSq = p.ep + (p.c == Co_White ? -8 : +8);
        assert(epCapSq>=0 && epCapSq<64);
        BBTools::unSetBit(p, epCapSq); // BEFORE setting p.b new shape !!!
        BBTools::unSetBit(p, from);
        BBTools::setBit(p, to, fromP);
        p.b[from] = P_none;
        p.b[to] = fromP;
        p.b[epCapSq] = P_none;

        p.h ^= Zobrist::ZT[from][fromId]; // remove fromP at from
        p.h ^= Zobrist::ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.h ^= Zobrist::ZT[to][fromId]; // add fromP at to

        p.ph ^= Zobrist::ZT[from][fromId]; // remove fromP at from
        p.ph ^= Zobrist::ZT[epCapSq][(p.c == Co_White ? P_bp : P_wp) + PieceShift]; // remove captured pawn
        p.ph ^= Zobrist::ZT[to][fromId]; // add fromP at to

        p.mat[~p.c][M_p]--;
    }
        break;

    case T_promq:
    case T_cappromq:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromq,(p.c == Co_White ? P_wq : P_bq));
        break;
    case T_promr:
    case T_cappromr:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromr, (p.c == Co_White ? P_wr : P_br));
        break;
    case T_promb:
    case T_cappromb:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromb, (p.c == Co_White ? P_wb : P_bb));
        break;
    case T_promn:
    case T_cappromn:
        MaterialHash::updateMaterialProm(p,to,type);
        movePiece(p, from, to, fromP, toP, type == T_cappromn, (p.c == Co_White ? P_wn : P_bn));
        break;
    case T_wks:
        movePieceCastle<Co_White>(p,CT_OO,Sq_g1,Sq_f1);
        break;
    case T_wqs:
        movePieceCastle<Co_White>(p,CT_OOO,Sq_c1,Sq_d1);
        break;
    case T_bks:
        movePieceCastle<Co_Black>(p,CT_OO,Sq_g8,Sq_f8);
        break;
    case T_bqs:
        movePieceCastle<Co_Black>(p,CT_OOO,Sq_c8,Sq_d8);
        break;
    }

    p.allPieces[Co_White] = p.whitePawn() | p.whiteKnight() | p.whiteBishop() | p.whiteRook() | p.whiteQueen() | p.whiteKing();
    p.allPieces[Co_Black] = p.blackPawn() | p.blackKnight() | p.blackBishop() | p.blackRook() | p.blackQueen() | p.blackKing();
    p.occupancy = p.allPieces[Co_White] | p.allPieces[Co_Black];

    if ( !noValidation && isAttacked(p,kingSquare(p)) ) return false; // this is the only legal move validation needed

    // Update castling right if rook captured
    if ( p.castling != C_none ){
       if ( toP == P_wr && to == p.rooksInit[Co_White][CT_OOO] && (p.castling & C_wqs) ){
           p.castling &= ~C_wqs;
           p.h ^= Zobrist::ZT[0][13];
       }
       else if ( toP == P_wr && to == p.rooksInit[Co_White][CT_OO] && (p.castling & C_wks) ){
           p.castling &= ~C_wks;
           p.h ^= Zobrist::ZT[7][13];
       }
       else if ( toP == P_br && to == p.rooksInit[Co_Black][CT_OOO] && (p.castling & C_bqs)){
           p.castling &= ~C_bqs;
           p.h ^= Zobrist::ZT[56][13];
       }
       else if ( toP == P_br && to == p.rooksInit[Co_Black][CT_OO] && (p.castling & C_bks)){
           p.castling &= ~C_bks;
           p.h ^= Zobrist::ZT[63][13];
       }
    }

    // update EP
    if (p.ep != INVALIDSQUARE) p.h  ^= Zobrist::ZT[p.ep][13];
    p.ep = INVALIDSQUARE;
    if ( abs(fromP) == P_wp && abs(to-from) == 16 ) p.ep = (from + to)/2;
    assert(p.ep == INVALIDSQUARE || SQRANK(p.ep) == EPRank[~p.c]);
    if (p.ep != INVALIDSQUARE) p.h  ^= Zobrist::ZT[p.ep][13];

    // update color
    p.c = ~p.c;
    p.h  ^= Zobrist::ZT[3][13] ; p.h  ^= Zobrist::ZT[4][13];

    // update game state
    if ( toP != P_none || abs(fromP) == P_wp ) p.fifty = 0;
    else ++p.fifty;
    if ( p.c == Co_White ) ++p.moves;
    ++p.halfmoves;

    MaterialHash::updateMaterialOther(p);
#ifdef DEBUG_MATERIAL
    Position::Material mat = p.mat;
    MaterialHash::initMaterial(p);
    if ( p.mat != mat ){ Logging::LogIt(Logging::logFatal) << "Material update error" << ToString(previous) << ToString(previous.mat) << ToString(p) << ToString(p.lastMove) << ToString(m) << ToString(mat) << ToString(p.mat); }
#endif
    p.lastMove = m;
    STOP_AND_SUM_TIMER(Apply)
    return true;
}

namespace Book {
template<typename T> struct bits_t { T t; };
template<typename T> bits_t<T&> bits(T &t) { return bits_t<T&>{t}; }
template<typename T> bits_t<const T&> bits(const T& t) { return bits_t<const T&>{t};}
template<typename S, typename T> S& operator<<(S &s, bits_t<T>  b) { s.write((char*)&b.t, sizeof(T)); return s;}
template<typename S, typename T> S& operator>>(S& s, bits_t<T&> b) { s.read ((char*)&b.t, sizeof(T)); return s;}

std::unordered_map<Hash, std::set<Move> > book;

bool fileExists(const std::string& name){ return std::ifstream(name.c_str()).good(); }

bool readBinaryBook(std::ifstream & stream) {
    Position ps;
    readFEN(startPosition,ps,true);
    Position p = ps;
    Move m = 0;
    while (!stream.eof()) {
        m = 0;
        stream >> bits(m);
        if (m == INVALIDBOOKMOVE) { ///@todo use MiniMove in book !!!!
            p = ps;
            stream >> bits(m);
            if (stream.eof()) break;
        }
        const Hash h = computeHash(p);
        if ( ! apply(p,m)){
            Logging::LogIt(Logging::logError) << "Unable to read book";
            return false;
        }
        book[h].insert(m);
    }
    return true;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd()); // here really random
    std::uniform_int_distribution<> dis(0, (int)std::distance(start, end) - 1);
    std::advance(start, dis(gen));
    return start;
}

const Move Get(const Hash h){
    std::unordered_map<Hash, std::set<Move> >::iterator it = book.find(h);
    if ( it == book.end() ) return INVALIDMOVE;
    Logging::LogIt(Logging::logInfo) << "Book hit";
    return *select_randomly(it->second.begin(),it->second.end());
}

void initBook() {
    if (DynamicConfig::book ) {
        if (DynamicConfig::bookFile.empty()) { Logging::LogIt(Logging::logWarn) << "json entry bookFile is empty, cannot load book"; }
        else {
            if (Book::fileExists(DynamicConfig::bookFile)) {
                Logging::LogIt(Logging::logInfo) << "Loading book ...";
                std::ifstream bbook(DynamicConfig::bookFile, std::ios::in | std::ios::binary);
                Book::readBinaryBook(bbook);
                Logging::LogIt(Logging::logInfo) << "... done";
            }
            else { Logging::LogIt(Logging::logWarn) << "book file " << DynamicConfig::bookFile << " not found, cannot load book"; }
        }
    }
}

#ifdef IMPORTBOOK
#include "Add-On/bookGenerationTools.cc"
#endif
} // Book

namespace BBTools { // re-open
    BitBoard allAttackedBB(const Position &p, const Square x, Color c) {
        if (c == Co_White) return attack<P_wb>(x, p.blackBishop() | p.blackQueen(), p.occupancy) | attack<P_wr>(x, p.blackRook() | p.blackQueen(), p.occupancy) | attack<P_wn>(x, p.blackKnight()) | attack<P_wp>(x, p.blackPawn(), p.occupancy, Co_White) | attack<P_wk>(x, p.blackKing());
        else               return attack<P_wb>(x, p.whiteBishop() | p.whiteQueen(), p.occupancy) | attack<P_wr>(x, p.whiteRook() | p.whiteQueen(), p.occupancy) | attack<P_wn>(x, p.whiteKnight()) | attack<P_wp>(x, p.whitePawn(), p.occupancy, Co_Black) | attack<P_wk>(x, p.whiteKing());
    }
}

ScoreType ThreadContext::SEE(const Position & p, const Move & m) const {
    if ( ! VALIDMOVE(m) ) return 0;

    Square from = Move2From(m);
    const Square to = Move2To(m);
    const MType mtype = Move2Type(m);
    BitBoard attackers = BBTools::allAttackedBB(p, to, p.c) | BBTools::allAttackedBB(p, to, ~p.c);
    BitBoard occupation_mask = 0xFFFFFFFFFFFFFFFF;
    ScoreType current_target_val = 0;
    const bool promPossible = PROMOTION_RANK(to);
    Color c = p.c;

    int nCapt = 0;
    ScoreType swapList[32]; // max 32 caps ... shall be ok

    Piece pp = PieceTools::getPieceType(p, from);
    if ( mtype == T_ep ){
        swapList[nCapt] = Values[P_wp+PieceShift];
        current_target_val = Values[pp+PieceShift];
        occupation_mask &= ~SquareToBitboard(p.ep);
    }
    else{
        swapList[nCapt] = PieceTools::getAbsValue(p, to);
        if (promPossible && pp == P_wp) {
            swapList[nCapt] += Values[promShift(mtype)+PieceShift] - Values[P_wp+PieceShift];
            current_target_val = Values[promShift(mtype)+PieceShift];
        }
        else current_target_val = Values[pp+PieceShift];
    }
    nCapt++;

    attackers &= ~SquareToBitboard(from);
    occupation_mask &= ~SquareToBitboard(from);

    attackers |= BBTools::attack<P_wr>(to, p.whiteQueen() | p.blackQueen() | p.whiteRook()   | p.blackRook(),   p.occupancy & occupation_mask, c) |
                 BBTools::attack<P_wb>(to, p.whiteQueen() | p.blackQueen() | p.whiteBishop() | p.blackBishop(), p.occupancy & occupation_mask, c) ;
    attackers &= occupation_mask;
    c = ~c;

    while (attackers) {
        if (!promPossible && attackers & p.pieces<P_wp>(c))        from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wp>(c)),pp=P_wp;
        else if (attackers & p.pieces<P_wn>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wn>(c)),pp=P_wn;
        else if (attackers & p.pieces<P_wb>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wb>(c)),pp=P_wb;
        else if (attackers & p.pieces<P_wr>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wr>(c)),pp=P_wr;
        else if (promPossible && (attackers & p.pieces<P_wp>(c)))  from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wp>(c)),pp=P_wp;
        else if (attackers & p.pieces<P_wq>(c))                    from = BBTools::SquareFromBitBoard(attackers & p.pieces<P_wq>(c)),pp=P_wq;
        else if ((attackers & p.pieces<P_wk>(c)) && !(attackers & p.allPieces[~c])) from = BBTools::SquareFromBitBoard(attackers &  p.pieces<P_wk>(c)),pp=P_wk;
        else break;

        swapList[nCapt] = -swapList[nCapt - 1] + current_target_val;
        if (promPossible && pp == P_wp) {
            swapList[nCapt] += Values[P_wq+PieceShift] - Values[P_wp+PieceShift];
            current_target_val = Values[P_wq+PieceShift];
        }
        else current_target_val = Values[pp+PieceShift];

        nCapt++;
        attackers &= ~SquareToBitboard(from);
        occupation_mask &= ~SquareToBitboard(from);

        attackers |= BBTools::attack<P_wr>(to, p.whiteQueen() | p.blackQueen() | p.whiteRook()   | p.blackRook(),   p.occupancy & occupation_mask, c) |
                     BBTools::attack<P_wb>(to, p.whiteQueen() | p.blackQueen() | p.whiteBishop() | p.blackBishop(), p.occupancy & occupation_mask, c) ;
        attackers &= occupation_mask;

        c = ~c;
    }

    while (--nCapt) if (swapList[nCapt] > -swapList[nCapt - 1])  swapList[nCapt - 1] = -swapList[nCapt];
    return swapList[0];
}

bool ThreadContext::SEE_GE(const Position & p, const Move & m, ScoreType threshold) const{
   return SEE(p,m) >= threshold;
}

/*
// Static Exchange Evaluation (cutoff version algorithm from Stockfish)
bool ThreadContext::SEE_GE(const Position & p, const Move & m, ScoreType threshold) const{
    assert(VALIDMOVE(m));
    START_TIMER
    const Square from = Move2From(m);
    const Square to   = Move2To(m);
    const MType type  = Move2Type(m);
    if (PieceTools::getPieceType(p, to) == P_wk) return true; // capture king !
    const bool promPossible = PROMOTION_RANK_C(to,p.c);
    if (promPossible) return true; // never treat possible prom case !
    Piece pp = PieceTools::getPieceType(p,from);
    bool prom = promPossible && pp == P_wp;
    Piece nextVictim  = prom ? P_wq : pp; ///@todo other prom
    const Color us    = p.c;
    ScoreType balance = (type==T_ep ? Values[P_wp+PieceShift] : PieceTools::getAbsValue(p,to)) - threshold + (prom?(Values[P_wq+PieceShift]-Values[P_wp+PieceShift]):0); // The opponent may be able to recapture so this is the best result we can hope for.
    if (balance < 0) return false;
    balance -= Values[nextVictim+PieceShift]; // Now assume the worst possible result: that the opponent can capture our piece for free.
    if (balance >= 0) return true;
    Position p2 = p;
    if (!apply(p2, m, true)) return false;
    bool endOfSEE = false;
    while (!endOfSEE){
        bool validThreatFound = false;
        for ( pp = P_wp ; pp <= P_wk && !validThreatFound ; ++pp){
           BitBoard att = BBTools::pfAttack[pp-1](to, p2.pieces(p2.c,pp), p2.occupancy, ~p2.c);
           if ( !att ) continue; // next piece type
           Square sqAtt = INVALIDSQUARE;
           while (!validThreatFound && att && (sqAtt = BBTools::popBit(att))) {
              if (PieceTools::getPieceType(p2,to) == P_wk) return us == p2.c; // capture king !
              Position p3 = p2;
              prom = promPossible && pp == P_wp;
              const Move mm = ToMove(sqAtt, to, prom ? T_cappromq : T_capture);
              if (!apply(p3,mm,true)) continue;
              validThreatFound = true;
              nextVictim = prom ? P_wq : pp; // CAREFULL here :: we don't care black or white, always use abs(value) next !!!
              if (prom) balance -= Values[P_wp + PieceShift];
              balance = -balance - 1 - Values[nextVictim+PieceShift]; ///@todo other prom ?
              if (balance >= 0 && nextVictim != P_wk) endOfSEE = true;
              p2 = p3;
           }
        }
        if (!validThreatFound) endOfSEE = true;
    }
    STOP_AND_SUM_TIMER(See)
    return us != p2.c; // we break the above loop when stm loses
}
*/

struct MoveSorter{

    MoveSorter(const ThreadContext & context, const Position & p, float gp, DepthType ply, const ThreadContext::CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const Move refutation = INVALIDMOVE):context(context),p(p),gp(gp),ply(ply),cmhPtr(cmhPtr),useSEE(useSEE),isInCheck(isInCheck),e(e),refutation(refutation){ assert(e==0||e->h!=0||e->m==INVALIDMOVE); }

    void computeScore(Move & m)const{
        assert(VALIDMOVE(m));
        const MType  t    = Move2Type(m);
        const Square from = Move2From(m);
        const Square to   = Move2To(m);
        ScoreType s       = Move2Score(m);
        if ( s != 0 ) return; // prob cut already computed captures score
        s = MoveScoring[t];
        if ( ply == 0 && sameMove(context.previousBest,m)) s += 20000; // previous root best
        else if (e && sameMove(e->m,m)) s += 15000; // TT move
        else{
            if (isInCheck && PieceTools::getPieceType(p, from) == P_wk) s += 10000; // king evasion
            if ( isCapture(t) && !isPromotion(t)){ 
                const Piece victim   = (t != T_ep) ? PieceTools::getPieceType(p,to) : P_wp;
                const Piece attacker = PieceTools::getPieceType(p,from);
                assert(victim>0); assert(attacker>0);
                if ( useSEE && !isInCheck ){
                    const ScoreType see = context.SEE(p,m);
                    s += see;
                    if ( see < -70 ) s -= 2*MoveScoring[T_capture]; // bad capture
                    else {
                        if ( isCapture(p.lastMove) && to == Move2To(p.lastMove) ) s += 400; // recapture bonus
                    }
                }
                else{ // MVVLVA
                    s += SearchConfig::MvvLvaScores[victim-1][attacker-1]; //[0 400]
                }
            }
            else if ( t == T_std ){
                if      (sameMove(m, context.killerT.killers[ply][0])) s += 1800; // quiet killer
                else if (sameMove(m, context.killerT.killers[ply][1])) s += 1750; // quiet killer
                else if (ply > 1 && sameMove(m, context.killerT.killers[ply-2][0])) s += 1700; // quiet killer
                else if (VALIDMOVE(p.lastMove) && sameMove(context.counterT.counter[Move2From(p.lastMove)][Move2To(p.lastMove)],m)) s+= 1650; // quiet counter
                else {
                    s += context.historyT.history[p.c][from][to] /4; // +/- MAX_HISTORY = 1000
                    s += context.historyT.historyP[p.b[from]+PieceShift][to] /2 ; // +/- MAX_HISTORY = 1000
                    s += context.getCMHScore(p, from, to, ply, cmhPtr) /4; // +/- MAX_HISTORY = 1000
                    if ( !isInCheck ){
                       if ( refutation != INVALIDMOVE && from == Move2To(refutation) && context.SEE_GE(p,m,-70)) s += 1000; // move (safely) leaving threat square from null move search
                       const bool isWhite = (p.allPieces[Co_White] & SquareToBitboard(from)) != empty;
                       const EvalScore * const  pst = EvalConfig::PST[PieceTools::getPieceType(p, from) - 1];
                       s += ScaleScore(pst[isWhite ? (to ^ 56) : to] - pst[isWhite ? (from ^ 56) : from],gp);
                    }
                }
            }
        }
        m = ToMove(from, to, t, s);
    }

    bool operator()(const Move & a, const Move & b)const{ assert( a != INVALIDMOVE); assert( b != INVALIDMOVE); return Move2Score(a) > Move2Score(b); }

    const Position & p;
    const TT::Entry * e;
    const ThreadContext & context;
    const bool useSEE;
    const bool isInCheck;
    const Move refutation;
    const DepthType ply;
    const ThreadContext::CMHPtrArray & cmhPtr;
    float gp;

    static void sort(const ThreadContext & context, MoveList & moves, const Position & p, float gp, DepthType ply, const ThreadContext::CMHPtrArray & cmhPtr, bool useSEE = true, bool isInCheck = false, const TT::Entry * e = NULL, const Move refutation = INVALIDMOVE){
        const MoveSorter ms(context,p,gp,ply,cmhPtr,useSEE,isInCheck,e,refutation);
        for(auto it = moves.begin() ; it != moves.end() ; ++it){ ms.computeScore(*it); }
        std::sort(moves.begin(),moves.end(),ms);
    }
};

inline bool ThreadContext::isRep(const Position & p, bool isPV)const{
    const int limit = isPV ? 3 : 1;
    if ( p.fifty < (2*limit-1) ) return false;
    int count = 0;
    const Hash h = computeHash(p);
    for (int k = p.halfmoves - 1; k >= 0; --k) {
        if (stack[k].h == nullHash) break;
        if (stack[k].h == h) ++count;
        if (count >= limit) return true;
    }
    return false;
}

template< bool withRep, bool isPV, bool INR>
MaterialHash::Terminaison ThreadContext::interiorNodeRecognizer(const Position & p)const{
    if (withRep && isRep(p,isPV)) return MaterialHash::Ter_Draw;
    if (p.fifty >= 100)           return MaterialHash::Ter_Draw;
    if ( INR && (p.mat[Co_White][M_p] + p.mat[Co_Black][M_p]) < 2 ) return MaterialHash::probeMaterialHashTable(p.mat);
    return MaterialHash::Ter_Unknown;
}

namespace{ // some Color / Piece helpers
   template<Color C> inline bool isPasser(const Position &p, Square k)     { return (BBTools::mask[k].passerSpan[C] & p.pieces<P_wp>(~C)) == empty;}
   template<Color C> inline Square ColorSquarePstHelper(Square k)          { return C==Co_White?(k^56):k;}
   template<Color C> inline constexpr ScoreType ColorSignHelper()          { return C==Co_White?+1:-1;}
   template<Color C> inline const Square PromotionSquare(const Square k)   { return C==Co_White? (SQFILE(k) + 56) : SQFILE(k);}
   template<Color C> inline const Rank ColorRank(const Square k)           { return Rank(C==Co_White? SQRANK(k) : (7-SQRANK(k)));}
   template<Color C> inline bool isBackward(const Position &p, Square k, const BitBoard pAtt[2], const BitBoard pAttSpan[2]){ return ((BBTools::shiftN<C>(SquareToBitboard(k))&~p.pieces<P_wp>(~C)) & pAtt[~C] & ~pAttSpan[C]) != empty; }
   template<Piece T> inline BitBoard alignedThreatPieceSlider(const Position & p, Color C);
   template<> inline BitBoard alignedThreatPieceSlider<P_wn>(const Position & p, Color C){ return empty;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wb>(const Position & p, Color C){ return p.pieces<P_wb>(C) | p.pieces<P_wq>(C) /*| p.pieces<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wr>(const Position & p, Color C){ return p.pieces<P_wr>(C) | p.pieces<P_wq>(C) /*| p.pieces<P_wn>(C)*/;}
   template<> inline BitBoard alignedThreatPieceSlider<P_wq>(const Position & p, Color C){ return p.pieces<P_wb>(C) | p.pieces<P_wr>(C) /*| p.pieces<P_wn>(C)*/;} ///@todo this is false ...
   template<> inline BitBoard alignedThreatPieceSlider<P_wk>(const Position & p, Color C){ return empty;}
}

template < Piece T , Color C>
inline void evalPiece(const Position & p, BitBoard pieceBBiterator, const BitBoard (& kingZone)[2], EvalScore & score, BitBoard & attBy, BitBoard & att, BitBoard & att2, ScoreType (& kdanger)[2], BitBoard & checkers){
    while (pieceBBiterator) {
        const Square k = BBTools::popBit(pieceBBiterator);
        const Square kk = ColorSquarePstHelper<C>(k);
        score += EvalConfig::PST[T-1][kk] * ColorSignHelper<C>();
        const BitBoard target = BBTools::pfCoverage[T-1](k, p.occupancy, C); // real targets
        if ( target ){
           attBy |= target;
           att2  |= att & target;
           att   |= target;
           if ( target & p.pieces<P_wk>(~C) ) checkers |= SquareToBitboard(k);
        }
        const BitBoard shadowTarget = BBTools::pfCoverage[T-1](k, p.occupancy ^ /*p.pieces<T>(C)*/ alignedThreatPieceSlider<T>(p,C), C); // aligned threats of same piece type also taken into account and knight in front also removed ///@todo better?
        if ( shadowTarget ){
           kdanger[C]  -= countBit(shadowTarget & kingZone[C])  * EvalConfig::kingAttWeight[EvalConfig::katt_defence][T-1];
           kdanger[~C] += countBit(shadowTarget & kingZone[~C]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack][T-1];
        }
    }
}
///@todo special version of evalPiece for king ??

template < Piece T ,Color C>
inline void evalMob(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const BitBoard mob = BBTools::pfCoverage[T-1](BBTools::popBit(pieceBBiterator), p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[T-2][countBit(mob)]*ColorSignHelper<C>();
    }
}

template < Color C >
inline void evalMobQ(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const Square s = BBTools::popBit(pieceBBiterator);
        BitBoard mob = BBTools::pfCoverage[P_wb-1](s, p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[3][countBit(mob)]*ColorSignHelper<C>();
        mob = BBTools::pfCoverage[P_wr-1](s, p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[4][countBit(mob)]*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalMobK(const Position & p, BitBoard pieceBBiterator, ScoreAcc & score, const BitBoard safe){
    while (pieceBBiterator){
        const BitBoard mob = BBTools::pfCoverage[P_wk-1](BBTools::popBit(pieceBBiterator), p.occupancy, C) & ~p.allPieces[C] & safe;
        score[sc_MOB] += EvalConfig::MOB[5][countBit(mob)]*ColorSignHelper<C>();
    }
}

template< Color C>
inline void evalPawnPasser(const Position & p, BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) {
        const Square k = BBTools::popBit(pieceBBiterator);
        const EvalScore kingNearBonus   = EvalConfig::kingNearPassedPawn * ScoreType( chebyshevDistance(p.king[~C], k) - chebyshevDistance(p.king[C], k) );
        const bool unstoppable          = (p.mat[~C][M_t] == 0)&&((chebyshevDistance(p.king[~C],PromotionSquare<C>(k))-int(p.c!=C)) > std::min(Square(5), chebyshevDistance(PromotionSquare<C>(k),k)));
        if (unstoppable) score += ColorSignHelper<C>()*(Values[P_wr+PieceShift] - Values[P_wp+PieceShift]); // yes rook not queen to force promotion asap
        else             score += (EvalConfig::passerBonus[ColorRank<C>(k)] + kingNearBonus)*ColorSignHelper<C>();
    }
}

template < Color C>
inline void evalPawn(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::PST[0][ColorSquarePstHelper<C>(BBTools::popBit(pieceBBiterator))] * ColorSignHelper<C>(); }
}

template< Color C>
inline void evalPawnFreePasser(const Position & p, BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) {
        const Square k = BBTools::popBit(pieceBBiterator);
        score += EvalConfig::freePasserBonus[ColorRank<C>(k)] * ScoreType( (BBTools::mask[k].frontSpan[C] & p.allPieces[~C]) == empty ) * ColorSignHelper<C>();
    }
}
template< Color C>
inline void evalPawnProtected(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::protectedPasserBonus[ColorRank<C>(BBTools::popBit(pieceBBiterator))] * ColorSignHelper<C>();}
}

template< Color C>
inline void evalPawnCandidate(BitBoard pieceBBiterator, EvalScore & score){
    while (pieceBBiterator) { score += EvalConfig::candidate[ColorRank<C>(BBTools::popBit(pieceBBiterator))] * ColorSignHelper<C>();}
}

template< Color C>
BitBoard getPinned(const Position & p, const Square s){
    BitBoard pinned = empty;
    if ( s == INVALIDSQUARE ) return pinned;
    BitBoard pinner = BBTools::attack<P_wb>(s, p.pieces<P_wb>(~C) | p.pieces<P_wq>(~C), p.allPieces[~C]) | BBTools::attack<P_wr>(s, p.pieces<P_wr>(~C) | p.pieces<P_wq>(~C), p.allPieces[~C]);
    while ( pinner ) { pinned |= BBTools::mask[BBTools::popBit(pinner)].between[p.king[C]] & p.allPieces[C]; }
    return pinned;
}

float gamePhase(const Position & p, ScoreType & matScoreW, ScoreType & matScoreB){
    const float totalMatScore = 2.f * *absValues[P_wq] + 4.f * *absValues[P_wr] + 4.f * *absValues[P_wb] + 4.f * *absValues[P_wn] + 16.f * *absValues[P_wp]; // cannot be static for tuning process ...
    const ScoreType matPieceScoreW = p.mat[Co_White][M_q] * *absValues[P_wq] + p.mat[Co_White][M_r] * *absValues[P_wr] + p.mat[Co_White][M_b] * *absValues[P_wb] + p.mat[Co_White][M_n] * *absValues[P_wn];
    const ScoreType matPieceScoreB = p.mat[Co_Black][M_q] * *absValues[P_wq] + p.mat[Co_Black][M_r] * *absValues[P_wr] + p.mat[Co_Black][M_b] * *absValues[P_wb] + p.mat[Co_Black][M_n] * *absValues[P_wn];
    const ScoreType matPawnScoreW  = p.mat[Co_White][M_p] * *absValues[P_wp];
    const ScoreType matPawnScoreB  = p.mat[Co_Black][M_p] * *absValues[P_wp];
    matScoreW = matPieceScoreW + matPawnScoreW;
    matScoreB = matPieceScoreB + matPawnScoreB;
    return (matScoreW + matScoreB ) / totalMatScore; // based on MG values
}

template < bool display, bool safeMatEvaluator >
ScoreType eval(const Position & p, EvalData & data, ThreadContext &context){
    START_TIMER
    ScoreAcc score;
    
    // king captured
    const bool white2Play = p.c == Co_White;
    if ( p.king[Co_White] == INVALIDSQUARE ) return data.gp=0,(white2Play?-1:+1)* MATE;
    if ( p.king[Co_Black] == INVALIDSQUARE ) return data.gp=0,(white2Play?+1:-1)* MATE;

    // level for the poor ...
    const int lra = std::max(0, 500 - int(10*DynamicConfig::level));
    if ( lra > 0 ) { score[sc_Rand] += Zobrist::randomInt<int>(-lra,lra); }

    context.prefetchPawn(computeHash(p));

    // Material evaluation
    const Hash matHash = MaterialHash::getMaterialHash(p.mat);
    if ( matHash ){
       ++context.stats.counters[Stats::sid_materialTableHits];
       // Hash data
       const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
       data.gp = MEntry.gp;
       score[sc_Mat][EG] += MEntry.score[EG];
       score[sc_Mat][MG] += MEntry.score[MG];
       // end game knowledge (helper or scaling)
       if ( safeMatEvaluator && (p.mat[Co_White][M_t]+p.mat[Co_Black][M_t]<6) ){
          const Color winningSideEG = score[sc_Mat][EG]>0?Co_White:Co_Black;
          if      ( MEntry.t == MaterialHash::Ter_WhiteWinWithHelper || MEntry.t == MaterialHash::Ter_BlackWinWithHelper ) return (white2Play?+1:-1)*(MaterialHash::helperTable[matHash](p,winningSideEG,score[sc_Mat][EG]));
          else if ( MEntry.t == MaterialHash::Ter_Draw)         { if (!isAttacked(p, kingSquare(p))) return context.drawScore(); }
          else if ( MEntry.t == MaterialHash::Ter_MaterialDraw) { if (!isAttacked(p, kingSquare(p))) return context.drawScore(); }
          else if ( MEntry.t == MaterialHash::Ter_WhiteWin || MEntry.t == MaterialHash::Ter_BlackWin) score.scalingFactor = 5 - 5*p.fifty/100.f;
          else if ( MEntry.t == MaterialHash::Ter_HardToWin)  score.scalingFactor = 0.5f - 0.5f*(p.fifty/100.f);
          else if ( MEntry.t == MaterialHash::Ter_LikelyDraw) score.scalingFactor = 0.3f - 0.3f*(p.fifty/100.f);
       }
    }
    else{ // game phase and material scores out of table
       ScoreType matScoreW = 0;
       ScoreType matScoreB = 0;
       data.gp = gamePhase(p,matScoreW, matScoreB);
       score[sc_Mat][EG] += (p.mat[Co_White][M_q] - p.mat[Co_Black][M_q]) * *absValuesEG[P_wq] + (p.mat[Co_White][M_r] - p.mat[Co_Black][M_r]) * *absValuesEG[P_wr] + (p.mat[Co_White][M_b] - p.mat[Co_Black][M_b]) * *absValuesEG[P_wb] + (p.mat[Co_White][M_n] - p.mat[Co_Black][M_n]) * *absValuesEG[P_wn] + (p.mat[Co_White][M_p] - p.mat[Co_Black][M_p]) * *absValuesEG[P_wp];
       score[sc_Mat][MG] += matScoreW - matScoreB;
       ++context.stats.counters[Stats::sid_materialTableMiss];
    }

#ifdef WITH_TEXEL_TUNING
    score[sc_Mat] += MaterialHash::Imbalance(p.mat, Co_White) - MaterialHash::Imbalance(p.mat, Co_Black);
#endif

    // usefull bitboards accumulator
    const BitBoard pawns[2]          = {p.whitePawn(), p.blackPawn()};
    const BitBoard allPawns          = pawns[Co_White] | pawns[Co_Black];
    ScoreType kdanger[2]             = {0, 0};
    BitBoard att[2]                  = {empty, empty};
    BitBoard att2[2]                 = {empty, empty};
    BitBoard attFromPiece[2][6]      = {{empty}}; ///@todo use this more!
    BitBoard checkers[2][6]          = {{empty}};

    const BitBoard kingZone[2]   = { BBTools::mask[p.king[Co_White]].kingZone, BBTools::mask[p.king[Co_Black]].kingZone};
    const BitBoard kingShield[2] = { kingZone[Co_White] & ~BBTools::shiftS<Co_White>(ranks[SQRANK(p.king[Co_White])]) , kingZone[Co_Black] & ~BBTools::shiftS<Co_Black>(ranks[SQRANK(p.king[Co_Black])]) };

    // PST, attack, danger
    evalPiece<P_wn,Co_White>(p,p.pieces<P_wn>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wn-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wn-1]);
    evalPiece<P_wb,Co_White>(p,p.pieces<P_wb>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wb-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wb-1]);
    evalPiece<P_wr,Co_White>(p,p.pieces<P_wr>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wr-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wr-1]);
    evalPiece<P_wq,Co_White>(p,p.pieces<P_wq>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wq-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wq-1]);
    evalPiece<P_wk,Co_White>(p,p.pieces<P_wk>(Co_White),kingZone,score[sc_PST],attFromPiece[Co_White][P_wk-1],att[Co_White],att2[Co_White],kdanger,checkers[Co_White][P_wk-1]);
    evalPiece<P_wn,Co_Black>(p,p.pieces<P_wn>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wn-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wn-1]);
    evalPiece<P_wb,Co_Black>(p,p.pieces<P_wb>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wb-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wb-1]);
    evalPiece<P_wr,Co_Black>(p,p.pieces<P_wr>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wr-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wr-1]);
    evalPiece<P_wq,Co_Black>(p,p.pieces<P_wq>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wq-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wq-1]);
    evalPiece<P_wk,Co_Black>(p,p.pieces<P_wk>(Co_Black),kingZone,score[sc_PST],attFromPiece[Co_Black][P_wk-1],att[Co_Black],att2[Co_Black],kdanger,checkers[Co_Black][P_wk-1]);

    /*
#ifndef WITH_TEXEL_TUNING
    // lazy evaluation
    ScoreType lazyScore = score.Score(p,data.gp); // scale both phase and 50 moves rule
    if ( std::abs(lazyScore) > ScaleScore({600,1000}, data.gp)){ // winning / losing position
        ScoreType ret = (white2Play?+1:-1)*lazyScore;
        STOP_AND_SUM_TIMER(Eval)
        return ret;
    }
#endif
    */

    ThreadContext::PawnEntry * pePtr = nullptr;
#ifdef WITH_TEXEL_TUNING
    ThreadContext::PawnEntry dummy; // used for texel tuning
    pePtr = &dummy;
    {
#else
    if ( !context.getPawnEntry(computePHash(p), pePtr) ){
#endif  
       assert(pePtr);
       ThreadContext::PawnEntry & pe = *pePtr;
       pe.reset();
       const BitBoard backward      [2] = {BBTools::pawnBackward  <Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnBackward  <Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard isolated      [2] = {BBTools::pawnIsolated            (pawns[Co_White])                 , BBTools::pawnIsolated            (pawns[Co_Black])};
       const BitBoard doubled       [2] = {BBTools::pawnDoubled   <Co_White>(pawns[Co_White])                 , BBTools::pawnDoubled   <Co_Black>(pawns[Co_Black])};
       const BitBoard candidates    [2] = {BBTools::pawnCandidates<Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnCandidates<Co_Black>(pawns[Co_Black],pawns[Co_White])};
       const BitBoard semiOpenPawn  [2] = {BBTools::pawnSemiOpen  <Co_White>(pawns[Co_White],pawns[Co_Black]) , BBTools::pawnSemiOpen  <Co_Black>(pawns[Co_Black],pawns[Co_White])};
       pe.pawnTargets   [Co_White] = BBTools::pawnAttacks   <Co_White>(pawns[Co_White])                       ; pe.pawnTargets   [Co_Black] = BBTools::pawnAttacks   <Co_Black>(pawns[Co_Black]);
       pe.semiOpenFiles [Co_White] = BBTools::fillFile(pawns[Co_White]) & ~BBTools::fillFile(pawns[Co_Black]) ; pe.semiOpenFiles [Co_Black] = BBTools::fillFile(pawns[Co_Black]) & ~BBTools::fillFile(pawns[Co_White]); // semiOpen white means with white pawn, and without black pawn
       pe.passed        [Co_White] = BBTools::pawnPassed    <Co_White>(pawns[Co_White],pawns[Co_Black])       ; pe.passed        [Co_Black] = BBTools::pawnPassed    <Co_Black>(pawns[Co_Black],pawns[Co_White]);
       pe.holes         [Co_White] = BBTools::pawnHoles     <Co_White>(pawns[Co_White]) & holesZone[Co_White] ; pe.holes         [Co_Black] = BBTools::pawnHoles     <Co_Black>(pawns[Co_Black]) & holesZone[Co_White];
       pe.openFiles =  BBTools::openFiles(pawns[Co_White], pawns[Co_Black]);
   
       // PST, attack
       evalPawn<Co_White>(p.pieces<P_wp>(Co_White),pe.score);
       evalPawn<Co_Black>(p.pieces<P_wp>(Co_Black),pe.score);
       // danger in king zone
       pe.danger[Co_White] -= countBit(pe.pawnTargets[Co_White] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
       pe.danger[Co_White] += countBit(pe.pawnTargets[Co_Black] & kingZone[Co_White]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack] [0];
       pe.danger[Co_Black] -= countBit(pe.pawnTargets[Co_Black] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_defence][0];
       pe.danger[Co_Black] += countBit(pe.pawnTargets[Co_White] & kingZone[Co_Black]) * EvalConfig::kingAttWeight[EvalConfig::katt_attack] [0];
       // pawn passer
       evalPawnPasser<Co_White>(p,pe.passed[Co_White],pe.score);
       evalPawnPasser<Co_Black>(p,pe.passed[Co_Black],pe.score);
       // pawn protected
       evalPawnProtected<Co_White>(pe.passed[Co_White] & pe.pawnTargets[Co_White],pe.score);
       evalPawnProtected<Co_Black>(pe.passed[Co_Black] & pe.pawnTargets[Co_Black],pe.score);
       // pawn candidate
       evalPawnCandidate<Co_White>(candidates[Co_White],pe.score);
       evalPawnCandidate<Co_Black>(candidates[Co_Black],pe.score);
       // pawn backward
       pe.score -= EvalConfig::backwardPawnMalus[EvalConfig::Close]    * countBit(backward[Co_White] & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::backwardPawnMalus[EvalConfig::SemiOpen] * countBit(backward[Co_White] &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::backwardPawnMalus[EvalConfig::Close]    * countBit(backward[Co_Black] & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::backwardPawnMalus[EvalConfig::SemiOpen] * countBit(backward[Co_Black] &  semiOpenPawn[Co_Black]);
       // double pawn malus
       pe.score -= EvalConfig::doublePawnMalus[EvalConfig::Close]      * countBit(doubled[Co_White]  & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::doublePawnMalus[EvalConfig::SemiOpen]   * countBit(doubled[Co_White]  &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::doublePawnMalus[EvalConfig::Close]      * countBit(doubled[Co_Black]  & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::doublePawnMalus[EvalConfig::SemiOpen]   * countBit(doubled[Co_Black]  &  semiOpenPawn[Co_Black]);
       // isolated pawn malus
       pe.score -= EvalConfig::isolatedPawnMalus[EvalConfig::Close]    * countBit(isolated[Co_White] & ~semiOpenPawn[Co_White]);
       pe.score -= EvalConfig::isolatedPawnMalus[EvalConfig::SemiOpen] * countBit(isolated[Co_White] &  semiOpenPawn[Co_White]);
       pe.score += EvalConfig::isolatedPawnMalus[EvalConfig::Close]    * countBit(isolated[Co_Black] & ~semiOpenPawn[Co_Black]);
       pe.score += EvalConfig::isolatedPawnMalus[EvalConfig::SemiOpen] * countBit(isolated[Co_Black] &  semiOpenPawn[Co_Black]);
       // pawn shield (PST and king troppism alone is not enough)
       const int pawnShieldW = countBit(kingShield[Co_White] & pawns[Co_White]);
       const int pawnShieldB = countBit(kingShield[Co_Black] & pawns[Co_Black]);
       pe.score += EvalConfig::pawnShieldBonus * std::min(pawnShieldW*pawnShieldW,9);
       pe.score -= EvalConfig::pawnShieldBonus * std::min(pawnShieldB*pawnShieldB,9);
       // malus for king on a pawnless flank
       const File wkf = (File)SQFILE(p.king[Co_White]);
       const File bkf = (File)SQFILE(p.king[Co_Black]);
       if (!(pawns[Co_White] & kingFlank[wkf])) pe.score += EvalConfig::pawnlessFlank;
       if (!(pawns[Co_Black] & kingFlank[bkf])) pe.score -= EvalConfig::pawnlessFlank;
       // pawn storm
       pe.score -= EvalConfig::pawnStormMalus * countBit(kingFlank[wkf] & (rank3|rank4) & pawns[Co_Black]);
       pe.score += EvalConfig::pawnStormMalus * countBit(kingFlank[bkf] & (rank5|rank6) & pawns[Co_White]);
       // open file near king
       pe.danger[Co_White] += EvalConfig::kingAttOpenfile        * countBit(kingFlank[wkf] & pe.openFiles              )/8;
       pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_White])/8;
       pe.danger[Co_White] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[wkf] & pe.semiOpenFiles[Co_Black])/8;
       pe.danger[Co_Black] += EvalConfig::kingAttOpenfile        * countBit(kingFlank[bkf] & pe.openFiles              )/8;
       pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOpp * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_Black])/8;
       pe.danger[Co_Black] += EvalConfig::kingAttSemiOpenfileOur * countBit(kingFlank[bkf] & pe.semiOpenFiles[Co_White])/8;

       ++context.stats.counters[Stats::sid_ttPawnInsert];
       pe.h = Hash64to32(computePHash(p)); // set the pawn entry
    }
    assert(pePtr);
    const ThreadContext::PawnEntry & pe = *pePtr;
    score[sc_PawnTT] += pe.score;
    // update global things with pawn entry stuff
    kdanger[Co_White] += pe.danger[Co_White];
    kdanger[Co_Black] += pe.danger[Co_Black];
    checkers[Co_White][0] = BBTools::pawnAttacks<Co_Black>(p.pieces<P_wk>(Co_Black)) & pawns[Co_White];
    checkers[Co_Black][0] = BBTools::pawnAttacks<Co_White>(p.pieces<P_wk>(Co_White)) & pawns[Co_Black];
    att2[Co_White] |= att[Co_White] & pe.pawnTargets[Co_White];
    att2[Co_Black] |= att[Co_Black] & pe.pawnTargets[Co_Black];
    att[Co_White]  |= pe.pawnTargets[Co_White];
    att[Co_Black]  |= pe.pawnTargets[Co_Black];
    
    /*
    // lazy evaluation 2
    lazyScore = score.Score(p,data.gp); // scale both phase and 50 moves rule
    if ( std::abs(lazyScore) > 700){ // winning / losing position
        ScoreType ret = (white2Play?+1:-1)*lazyScore;
        STOP_AND_SUM_TIMER(Eval)
        return ret;
    }
    */

    const BitBoard nonPawnMat[2]               = {p.allPieces[Co_White] & ~pawns[Co_White] , p.allPieces[Co_Black] & ~pawns[Co_Black]};
    //const BitBoard attackedOrNotDefended[2]    = {att[Co_White]  | ~att[Co_Black]  , att[Co_Black]  | ~att[Co_White] };
    const BitBoard attackedAndNotDefended[2]   = {att[Co_White]  & ~att[Co_Black]  , att[Co_Black]  & ~att[Co_White] };
    const BitBoard attacked2AndNotDefended2[2] = {att2[Co_White] & ~att2[Co_Black] , att2[Co_Black] & ~att2[Co_White]};

    const BitBoard weakSquare[2]       = {att[Co_Black] & ~att2[Co_White] & (~att[Co_White] | attFromPiece[Co_White][P_wk-1] | attFromPiece[Co_White][P_wq-1]) , att[Co_White] & ~att2[Co_Black] & (~att[Co_Black] | attFromPiece[Co_Black][P_wk-1] | attFromPiece[Co_Black][P_wq-1])};
    const BitBoard safeSquare[2]       = {~att[Co_Black] | ( weakSquare[Co_Black] & att2[Co_White] ) , ~att[Co_White] | ( weakSquare[Co_White] & att2[Co_Black] ) };
    const BitBoard protectedSquare[2]  = {pe.pawnTargets[Co_White] | attackedAndNotDefended[Co_White] | attacked2AndNotDefended2[Co_White] , pe.pawnTargets[Co_Black] | attackedAndNotDefended[Co_Black] | attacked2AndNotDefended2[Co_Black] };

    // own piece in front of pawn
    score[sc_PieceBlockPawn] += EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_White>(pawns[Co_White]) & nonPawnMat[Co_White] ); 
    score[sc_PieceBlockPawn] -= EvalConfig::pieceFrontPawn * countBit( BBTools::shiftN<Co_Black>(pawns[Co_Black]) & nonPawnMat[Co_Black] ); 

    // pawn hole, unprotected
    score[sc_Holes] += EvalConfig::holesMalus * countBit(pe.holes[Co_White] & ~protectedSquare[Co_White]);
    score[sc_Holes] -= EvalConfig::holesMalus * countBit(pe.holes[Co_Black] & ~protectedSquare[Co_Black]);

    // free passer bonus
    evalPawnFreePasser<Co_White>(p,pe.passed[Co_White], score[sc_FreePasser]);
    evalPawnFreePasser<Co_Black>(p,pe.passed[Co_Black], score[sc_FreePasser]);

    // rook behind passed
    score[sc_RookBehindPassed] += EvalConfig::rookBehindPassed * (countBit(p.pieces<P_wr>(Co_White) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])) - countBit(p.pieces<P_wr>(Co_Black) & BBTools::rearSpan<Co_White>(pe.passed[Co_White])));
    score[sc_RookBehindPassed] -= EvalConfig::rookBehindPassed * (countBit(p.pieces<P_wr>(Co_Black) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])) - countBit(p.pieces<P_wr>(Co_White) & BBTools::rearSpan<Co_Black>(pe.passed[Co_Black])));

    // protected minor blocking openfile
    score[sc_MinorOnOpenFile] += EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.whiteBishop()|p.whiteKnight()) & pe.pawnTargets[Co_White]);
    score[sc_MinorOnOpenFile] -= EvalConfig::minorOnOpenFile * countBit(pe.openFiles & (p.blackBishop()|p.blackKnight()) & pe.pawnTargets[Co_Black]);
    
    // knight on opponent hole, protected
    score[sc_Outpost] += EvalConfig::outpost * countBit(pe.holes[Co_Black] & p.whiteKnight() & pe.pawnTargets[Co_White]);
    score[sc_Outpost] -= EvalConfig::outpost * countBit(pe.holes[Co_White] & p.blackKnight() & pe.pawnTargets[Co_Black]);

    // reward safe checks
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        kdanger[Co_White] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_Black][pp-1] & safeSquare[Co_White] );
        kdanger[Co_Black] += EvalConfig::kingAttSafeCheck[pp-1] * countBit( checkers[Co_White][pp-1] & safeSquare[Co_Black] );
    }

    // danger : use king danger score. **DO NOT** apply this in end-game
    score[sc_ATT][MG] -=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_White]/32),ScoreType(0)),ScoreType(63))];
    score[sc_ATT][MG] +=  EvalConfig::kingAttTable[std::min(std::max(ScoreType(kdanger[Co_Black]/32),ScoreType(0)),ScoreType(63))];
    data.danger[Co_White] = kdanger[Co_White];
    data.danger[Co_Black] = kdanger[Co_Black];

    // number of hanging pieces (complexity ...)
    const BitBoard hanging[2] = {nonPawnMat[Co_White] & weakSquare[Co_White] , nonPawnMat[Co_Black] & weakSquare[Co_Black] };
    score[sc_Hanging] += EvalConfig::hangingPieceMalus * (countBit(hanging[Co_White]) - countBit(hanging[Co_Black]));

    // threats by minor
    BitBoard targetThreat = (nonPawnMat[Co_White] | (pawns[Co_White] & weakSquare[Co_White]) ) & (attFromPiece[Co_Black][P_wn-1] | attFromPiece[Co_Black][P_wb-1]);
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByMinor[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    targetThreat = (nonPawnMat[Co_Black] | (pawns[Co_Black] & weakSquare[Co_Black]) ) & (attFromPiece[Co_White][P_wn-1] | attFromPiece[Co_White][P_wb-1]);
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByMinor[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    // threats by rook
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wr-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByRook[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wr-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByRook[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    // threats by queen
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wq-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByQueen[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wq-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByQueen[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    // threats by king
    targetThreat = p.allPieces[Co_White] & weakSquare[Co_White] & attFromPiece[Co_Black][P_wk-1];
    while (targetThreat) score[sc_Threat] += EvalConfig::threatByKing[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];
    targetThreat = p.allPieces[Co_Black] & weakSquare[Co_Black] & attFromPiece[Co_White][P_wk-1];
    while (targetThreat) score[sc_Threat] -= EvalConfig::threatByKing[PieceTools::getPieceType(p, BBTools::popBit(targetThreat))-1];

    // threat by safe pawn
    const BitBoard safePawnAtt[2]  = {nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(pawns[Co_White] & safeSquare[Co_White]), nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(pawns[Co_Black] & safeSquare[Co_Black])};
    score[sc_PwnSafeAtt] += EvalConfig::pawnSafeAtt * (countBit(safePawnAtt[Co_White]) - countBit(safePawnAtt[Co_Black]));

    // safe pawn push (protected once or not attacked)
    const BitBoard safePawnPush[2]  = {BBTools::shiftN<Co_White>(pawns[Co_White]) & ~p.occupancy & safeSquare[Co_White], BBTools::shiftN<Co_Black>(pawns[Co_Black]) & ~p.occupancy & safeSquare[Co_Black]};
    score[sc_PwnPush] += EvalConfig::pawnMobility * (countBit(safePawnPush[Co_White]) - countBit(safePawnPush[Co_Black]));

    // threat by safe pawn push
    score[sc_PwnPushAtt] += EvalConfig::pawnSafePushAtt * (countBit(nonPawnMat[Co_Black] & BBTools::pawnAttacks<Co_White>(safePawnPush[Co_White])) - countBit(nonPawnMat[Co_White] & BBTools::pawnAttacks<Co_Black>(safePawnPush[Co_Black])));

    // pieces mobility
    evalMob <P_wn,Co_White>(p,p.pieces<P_wn>(Co_White),score,safeSquare[Co_White]);
    evalMob <P_wb,Co_White>(p,p.pieces<P_wb>(Co_White),score,safeSquare[Co_White]);
    evalMob <P_wr,Co_White>(p,p.pieces<P_wr>(Co_White),score,safeSquare[Co_White]);
    evalMobQ<     Co_White>(p,p.pieces<P_wq>(Co_White),score,safeSquare[Co_White]);
    evalMobK<     Co_White>(p,p.pieces<P_wk>(Co_White),score,~att[Co_Black]);
    evalMob <P_wn,Co_Black>(p,p.pieces<P_wn>(Co_Black),score,safeSquare[Co_Black]);
    evalMob <P_wb,Co_Black>(p,p.pieces<P_wb>(Co_Black),score,safeSquare[Co_Black]);
    evalMob <P_wr,Co_Black>(p,p.pieces<P_wr>(Co_Black),score,safeSquare[Co_Black]);
    evalMobQ<     Co_Black>(p,p.pieces<P_wq>(Co_Black),score,safeSquare[Co_Black]);
    evalMobK<     Co_Black>(p,p.pieces<P_wk>(Co_Black),score,~att[Co_White]);

    // rook on open file
    score[sc_OpenFile] += EvalConfig::rookOnOpenFile         * countBit(p.whiteRook() & pe.openFiles);
    score[sc_OpenFile] += EvalConfig::rookOnOpenSemiFileOur  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_White]);
    score[sc_OpenFile] += EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.whiteRook() & pe.semiOpenFiles[Co_Black]);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenFile         * countBit(p.blackRook() & pe.openFiles);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenSemiFileOur  * countBit(p.blackRook() & pe.semiOpenFiles[Co_Black]);
    score[sc_OpenFile] -= EvalConfig::rookOnOpenSemiFileOpp  * countBit(p.blackRook() & pe.semiOpenFiles[Co_White]);

    // enemy rook facing king
    score[sc_RookFrontKing] += EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteKing()) & p.blackRook());
    score[sc_RookFrontKing] -= EvalConfig::rookFrontKingMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackKing()) & p.whiteRook());

    // enemy rook facing queen
    score[sc_RookFrontQueen] += EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_White>(p.whiteQueen()) & p.blackRook());
    score[sc_RookFrontQueen] -= EvalConfig::rookFrontQueenMalus * countBit(BBTools::frontSpan<Co_Black>(p.blackQueen()) & p.whiteRook());

    // queen aligned with own rook
    score[sc_RookQueenSameFile] += EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.whiteQueen()) & p.whiteRook());
    score[sc_RookQueenSameFile] -= EvalConfig::rookQueenSameFile * countBit(BBTools::fillFile(p.blackQueen()) & p.blackRook());

    const Square whiteQueenSquare = p.whiteQueen() ? BBTools::SquareFromBitBoard(p.whiteQueen()) : INVALIDSQUARE;
    const Square blackQueenSquare = p.blackQueen() ? BBTools::SquareFromBitBoard(p.blackQueen()) : INVALIDSQUARE;

    // pins on king and queen
    const BitBoard pinnedK [2] = { getPinned<Co_White>(p,p.king[Co_White]), getPinned<Co_Black>(p,p.king[Co_Black]) };
    const BitBoard pinnedQ [2] = { getPinned<Co_White>(p,whiteQueenSquare), getPinned<Co_Black>(p,blackQueenSquare) };
    for (Piece pp = P_wp ; pp < P_wk ; ++pp) {
        if (p.pieces(Co_White, pp)) {
            if (pinnedK[Co_White] & p.pieces(Co_White, pp)) score[sc_PinsK] -= EvalConfig::pinnedKing[pp - 1] * countBit(pinnedK[Co_White] & p.pieces(Co_White, pp));
            if (pinnedQ[Co_White] & p.pieces(Co_White, pp)) score[sc_PinsQ] -= EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_White] & p.pieces(Co_White, pp));
        }
        if (p.pieces(Co_Black, pp)) {
            if (pinnedK[Co_Black] & p.pieces(Co_Black, pp)) score[sc_PinsK] += EvalConfig::pinnedKing[pp - 1] * countBit(pinnedK[Co_Black] & p.pieces(Co_Black, pp));
            if (pinnedQ[Co_Black] & p.pieces(Co_Black, pp)) score[sc_PinsQ] += EvalConfig::pinnedQueen[pp - 1] * countBit(pinnedQ[Co_Black] & p.pieces(Co_Black, pp));
        }
    }

    // attack : queen distance to opponent king (wrong if multiple queens ...)
    if ( blackQueenSquare != INVALIDSQUARE ) score[sc_QueenNearKing] -= EvalConfig::queenNearKing * (7 - minDistance(p.king[Co_White], blackQueenSquare) );
    if ( whiteQueenSquare != INVALIDSQUARE ) score[sc_QueenNearKing] += EvalConfig::queenNearKing * (7 - minDistance(p.king[Co_Black], whiteQueenSquare) );

    // number of pawn and piece type value
    score[sc_Adjust] += EvalConfig::adjRook  [p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_r]);
    score[sc_Adjust] -= EvalConfig::adjRook  [p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_r]);
    score[sc_Adjust] += EvalConfig::adjKnight[p.mat[Co_White][M_p]] * ScoreType(p.mat[Co_White][M_n]);
    score[sc_Adjust] -= EvalConfig::adjKnight[p.mat[Co_Black][M_p]] * ScoreType(p.mat[Co_Black][M_n]);

    // bad bishop
    if (p.whiteBishop() & whiteSquare) score[sc_Adjust] -= EvalConfig::badBishop[countBit(pawns[Co_White] & whiteSquare)];
    if (p.whiteBishop() & blackSquare) score[sc_Adjust] -= EvalConfig::badBishop[countBit(pawns[Co_White] & blackSquare)];
    if (p.blackBishop() & whiteSquare) score[sc_Adjust] += EvalConfig::badBishop[countBit(pawns[Co_Black] & whiteSquare)];
    if (p.blackBishop() & blackSquare) score[sc_Adjust] += EvalConfig::badBishop[countBit(pawns[Co_Black] & blackSquare)];

    // adjust piece pair score
    score[sc_Adjust] += ( (p.mat[Co_White][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_White][M_p]] : 0)-(p.mat[Co_Black][M_b] > 1 ? EvalConfig::bishopPairBonus[p.mat[Co_Black][M_p]] : 0) );
    score[sc_Adjust] += ( (p.mat[Co_White][M_n] > 1 ? EvalConfig::knightPairMalus : 0)-(p.mat[Co_Black][M_n] > 1 ? EvalConfig::knightPairMalus : 0) );
    score[sc_Adjust] += ( (p.mat[Co_White][M_r] > 1 ? EvalConfig::rookPairMalus   : 0)-(p.mat[Co_Black][M_r] > 1 ? EvalConfig::rookPairMalus   : 0) );

    // initiative
    const EvalScore initiativeBonus = EvalConfig::initiative[0] * countBit(allPawns) + EvalConfig::initiative[1] * ((allPawns & queenSide) && (allPawns & kingSide)) + EvalConfig::initiative[2] * (countBit(p.occupancy & ~allPawns) == 2) - EvalConfig::initiative[3];
    score[sc_initiative][MG] += sgn(score.score[MG]) * std::max(initiativeBonus[MG], ScoreType(-std::abs(score.score[MG])));
    score[sc_initiative][EG] += sgn(score.score[EG]) * std::max(initiativeBonus[EG], ScoreType(-std::abs(score.score[EG])));

    // tempo
    score[sc_Tempo] += EvalConfig::tempo*(white2Play?+1:-1);

    if ( display ) score.Display(p,data.gp);
    ScoreType ret = (white2Play?+1:-1)*score.Score(p,data.gp); // scale both phase and 50 moves rule
    STOP_AND_SUM_TIMER(Eval)
    return ret;
}

ScoreType createHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score += ply;
  else if (isMatedScore (score)) score -= ply;
  return score;
}

ScoreType adjustHashScore(ScoreType score, DepthType ply){
  if      (isMatingScore(score)) score -= ply;
  else if (isMatedScore (score)) score += ply;
  return score;
}

#ifdef DEBUG_KING_CAP
inline void debug_king_cap(const Position & p){
    if ( !p.whiteKing()||!p.blackKing()){
       std::cout << "no more king" << std::endl;
       std::cout << ToString(p) << std::endl;
       std::cout << ToString(p.lastMove) << std::endl;
       exit(1);
    }
}
#else
inline void debug_king_cap(const Position &){;}
#endif

#ifdef WITH_SYZYGY
extern "C" {
#include "tbprobe.h"
}
namespace SyzygyTb { // more or less copy/paste from Arasan
const ScoreType TB_CURSED_SCORE = 1;
const ScoreType TB_WIN_SCORE = 2000;
const ScoreType valueMap[5]     = {-TB_WIN_SCORE, -TB_CURSED_SCORE, 0, TB_CURSED_SCORE, TB_WIN_SCORE};
const ScoreType valueMapNo50[5] = {-TB_WIN_SCORE, -TB_WIN_SCORE,    0, TB_WIN_SCORE,    TB_WIN_SCORE};
int MAX_TB_MEN = -1;

static MType getMoveType(const Position &p, unsigned res){
    const bool isCap = (p.b[TB_GET_TO(res)] != P_none);
    switch (TB_GET_PROMOTES(res)) {
       case TB_PROMOTES_QUEEN:  return isCap? T_cappromq:T_promq;
       case TB_PROMOTES_ROOK:   return isCap? T_cappromr:T_promr;
       case TB_PROMOTES_BISHOP: return isCap? T_cappromb:T_promb;
       case TB_PROMOTES_KNIGHT: return isCap? T_cappromn:T_promn;
       default:                 return isCap? T_capture :T_std;
    }
}

Move getMove(const Position &p, unsigned res) { return ToMove(TB_GET_FROM(res), TB_GET_TO(res), TB_GET_EP(res) ? T_ep : getMoveType(p,res)); } // Note: castling not possible

bool initTB(const std::string &path){
   Logging::LogIt(Logging::logInfo) << "Init tb from path " << path;
   bool ok = tb_init(path.c_str());
   if (!ok) MAX_TB_MEN = 0;
   else     MAX_TB_MEN = TB_LARGEST;
   Logging::LogIt(Logging::logInfo) << "MAX_TB_MEN: " << MAX_TB_MEN;
   return ok;
}

int probe_root(ThreadContext & context, const Position &p, ScoreType &score, MoveList &rootMoves){
   if ( MAX_TB_MEN <= 0 ) return -1;
   score = 0;
   unsigned results[TB_MAX_MOVES];
   debug_king_cap(p);
   unsigned result = tb_probe_root(p.allPieces[Co_White],p.allPieces[Co_Black],p.whiteKing()|p.blackKing(),p.whiteQueen()|p.blackQueen(),p.whiteRook()|p.blackRook(),p.whiteBishop()|p.blackBishop(),p.whiteKnight()|p.blackKnight(),p.whitePawn()|p.blackPawn(),p.fifty,p.castling != C_none,p.ep == INVALIDSQUARE ? 0 : p.ep,p.c == Co_White,results);
   if (result == TB_RESULT_FAILED) return -1;
   const unsigned wdl = TB_GET_WDL(result);
   assert(wdl<5);
   score = valueMap[wdl];
   if (context.isRep(p,false)) rootMoves.push_back(getMove(p,result));
   else {
      unsigned res;
      for (int i = 0; (res = results[i]) != TB_RESULT_FAILED; i++) { if (TB_GET_WDL(res) >= wdl) rootMoves.push_back(getMove(p,res)); }
   }
   return TB_GET_DTZ(result);
}

int probe_wdl(const Position &p, ScoreType &score, bool use50MoveRule){
   if ( MAX_TB_MEN <= 0 ) return -1;
   score = 0;
   debug_king_cap(p);
   unsigned result = tb_probe_wdl(p.allPieces[Co_White],p.allPieces[Co_Black],p.whiteKing()|p.blackKing(),p.whiteQueen()|p.blackQueen(),p.whiteRook()|p.blackRook(),p.whiteBishop()|p.blackBishop(),p.whiteKnight()|p.blackKnight(),p.whitePawn()|p.blackPawn(),p.fifty,p.castling != C_none,p.ep == INVALIDSQUARE ? 0 : p.ep,p.c == Co_White);
   if (result == TB_RESULT_FAILED) return 0;
   unsigned wdl = TB_GET_WDL(result);
   assert(wdl<5);
   if (use50MoveRule) score = valueMap[wdl];
   else               score = valueMapNo50[wdl];
   return 1;
}

} // SyzygyTb
#endif

ScoreType ThreadContext::qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    EvalData data;
    ++stats.counters[Stats::sid_qnodes];
    const ScoreType evalScore = eval(p,data,*this);

    if ( evalScore >= beta ) return evalScore;
    if ( evalScore > alpha ) alpha = evalScore;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    ScoreType bestScore = isInCheck?-MATE+ply:evalScore;

    ThreadContext::CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    MoveList moves;
    MoveGen::generate<MoveGen::GP_cap>(p,moves);
    MoveSorter::sort(*this,moves,p,data.gp,ply,cmhPtr,true);

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        const ScoreType score = -qsearchNoPruning(-beta,-alpha,p2,ply+1,seldepth);
        if ( score > bestScore){
           bestScore = score;
           if ( score > alpha ){
              if ( score >= beta ) return score;
              alpha = score;
           }
        }
    }
    return bestScore;
}

template < bool qRoot, bool pvnode >
ScoreType ThreadContext::qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth){
    if (stopFlag) return STOPSCORE; // no time verification in qsearch, too slow
    ++stats.counters[Stats::sid_qnodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta , (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    if ((int)ply > seldepth) seldepth = ply;

    EvalData data;
    if (ply >= MAX_DEPTH - 1) return eval(p, data, *this);
    Move bestMove = INVALIDMOVE;

    const bool isInCheck = isAttacked(p, kingSquare(p));
    const bool specialQSearch = isInCheck || qRoot;
    DepthType hashDepth = specialQSearch ? 0 : -1;

    debug_king_cap(p);

    ThreadContext::CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    TT::Entry e;
    if (TT::getEntry(*this, p, computeHash(p), hashDepth, e)) {
        if (!pvnode && e.h != 0 && ((e.b == TT::B_alpha && e.s <= alpha) || (e.b == TT::B_beta  && e.s >= beta) || (e.b == TT::B_exact))) { return adjustHashScore(e.s, ply); }
        if ( e.m!=INVALIDMINIMOVE && (isInCheck || isCapture(e.m)) ) bestMove = e.m;
    }
    if ( qRoot && interiorNodeRecognizer<true,false,true>(p) == MaterialHash::Ter_Draw) return drawScore(); ///@todo is that gain elo ???

    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here
    else{
        if (e.h != 0){
            ++stats.counters[Stats::sid_ttschits];
            evalScore = e.e;
            /*
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            if ( matHash ){
               ++stats.counters[Stats::sid_materialTableHits];
               const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
               data.gp = MEntry.gp;
            }
            else{
               ScoreType matScoreW = 0;
               ScoreType matScoreB = 0;
               data.gp = gamePhase(p,matScoreW,matScoreB);
               ++stats.counters[Stats::sid_materialTableMiss];
            }
            */
        }
        else {
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    bool evalScoreIsHashScore = false;
    // use tt score if possible and not in check
    if ( !isInCheck && e.h != 0 && ((e.b == TT::B_alpha && e.s <= evalScore) || (e.b == TT::B_beta && e.s >= evalScore) || (e.b == TT::B_exact)) ) evalScore = e.s, evalScoreIsHashScore = true;

    TT::Bound b = TT::B_alpha;
    if ( evalScore >= beta ) return evalScore;
    if ( /*pvnode &&*/ evalScore > alpha) alpha = evalScore; ///@todo ??

    ScoreType bestScore = evalScore;

    MoveList moves;
    if ( isInCheck ) MoveGen::generate<MoveGen::GP_all>(p,moves); ///@odo generate only evasion !
    else             MoveGen::generate<MoveGen::GP_cap>(p,moves);
    MoveSorter::sort(*this,moves,p,data.gp,ply,cmhPtr,isInCheck,isInCheck,&e); ///@todo warning gp = 0 here !

    const ScoreType alphaInit = alpha;

    for(auto it = moves.begin() ; it != moves.end() ; ++it){
        if (!isInCheck) {
            if (!SEE_GE(p,*it,0)) continue;
            if (SearchConfig::doQFutility && evalScore + SearchConfig::qfutilityMargin[evalScoreIsHashScore] + (Move2Type(*it)==T_ep ? Values[P_wp+PieceShift] : PieceTools::getAbsValue(p, Move2To(*it))) <= alphaInit) continue;
        }
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        TT::prefetch(computeHash(p2));
        const ScoreType score = -qsearch<false,false>(-beta,-alpha,p2,ply+1,seldepth);
        if ( score > bestScore){
           bestMove = *it;
           bestScore = score;
           if ( score > alpha ){
               if (score >= beta) {
                   b = TT::B_beta;
                   break;
               }
               b = TT::B_exact;
               alpha = score;
           }
        }
    }
    TT::setEntry(*this,computeHash(p),bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),b,hashDepth);
    return bestScore;
}

inline void updatePV(PVList & pv, const Move & m, const PVList & childPV) {
    pv.clear();
    pv.push_back(m);
    std::copy(childPV.begin(), childPV.end(), std::back_inserter(pv));
}

inline void updateTables(ThreadContext & context, const Position & p, DepthType depth, DepthType ply, const Move m, TT::Bound bound, ThreadContext::CMHPtrArray & cmhPtr) {
    if (bound == TT::B_beta) {
        context.killerT.update(m, ply);
        context.counterT.update(m, p);
        context.historyT.update<1>(depth, m, p, cmhPtr);
    }
    else if (bound == TT::B_alpha) context.historyT.update<-1>(depth, m, p, cmhPtr);
}

ScoreType randomMover(const Position & p, PVList & pv, bool isInCheck) {
    MoveList moves;
    MoveGen::generate<MoveGen::GP_all>(p, moves, false);
    if (moves.empty()) return isInCheck ? -MATE : 0;
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(moves.begin(), moves.end(),g);
    for (auto it = moves.begin(); it != moves.end(); ++it) {
        Position p2 = p;
        if (!apply(p2, *it)) continue;
        PVList childPV;
        updatePV(pv, *it, childPV);
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE + 1;
        return 0;
    }
    return isInCheck ? -MATE : 0;
}

#ifdef DEBUG_PSEUDO_LEGAL
bool isPseudoLegal2(const Position & p, Move m); // forward decl
bool isPseudoLegal(const Position & p, Move m) {
    const bool b = isPseudoLegal2(p,m);
    if (b) {
        MoveList moves;
        MoveGen::generate<MoveGen::GP_all>(p, moves);
        bool found = false;
        for (auto it = moves.begin(); it != moves.end() && !found; ++it) if (sameMove(*it, m)) found = true;
        if (!found){
            std::cout << ToString(p) << "\n"  << ToString(m) << "\t" << m << std::endl;
            std::cout << SquareNames[Move2From(m)] << std::endl;
            std::cout << SquareNames[Move2To(m)] << std::endl;
            std::cout << (int)Move2Type(m) << std::endl;
            std::cout << Move2Score(m) << std::endl;
            std::cout << int(m & 0x0000FFFF) << std::endl;
            for (auto it = moves.begin(); it != moves.end(); ++it) std::cout << ToString(*it) <<"\t" << *it << "\t";
            std::cout << std::endl;
            std::cout << "Not a generated move !" << std::endl;
        }
    }
    return b;
}
bool isPseudoLegal2(const Position & p, Move m) { // validate TT move
#else
 bool isPseudoLegal(const Position & p, Move m) { // validate TT move
#endif
    if (!VALIDMOVE(m)) { return false; }
    const Square from = Move2From(m);
    const Piece fromP = p.b[from];
    if (fromP == P_none || (fromP > 0 && p.c == Co_Black) || (fromP < 0 && p.c == Co_White)) { return false; }
    const Square to = Move2To(m);
    const Piece toP = p.b[to];
    if ((toP > 0 && p.c == Co_White) || (toP < 0 && p.c == Co_Black)) { return false; }
    if ((Piece)std::abs(toP) == P_wk) { return false; }
    const Piece fromPieceType = (Piece)std::abs(fromP);
    const MType t = Move2Type(m);
    if ( t == T_reserved ) {return false;}
    if (toP == P_none && (isCapture(t) && t!=T_ep)) { return false; }
    if (toP != P_none && !isCapture(t)) { return false; }
    if (t == T_ep && (p.ep == INVALIDSQUARE || fromPieceType != P_wp)) { return false;}
    if (t == T_ep && p.b[p.ep + (p.c==Co_White?-8:+8)] != (p.c==Co_White?P_bp:P_wp)) { return false;}
    if (isPromotion(m) && fromPieceType != P_wp) { return false; }
    if (isCastling(m)) {
        if (p.c == Co_White) {
            if (t == T_wqs && (p.castling & C_wqs) && from == p.kingInit[Co_White] && fromP == P_wk && to == Sq_c1 && toP == P_none
                && (((BBTools::mask[p.king[Co_White]].between[Sq_c1] | BBTools::mask[p.rooksInit[Co_White][CT_OOO]].between[Sq_d1]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_c1] | SquareToBitboard(p.king[Co_White]))) return true;
            if (t == T_wks && (p.castling & C_wks) && from == p.kingInit[Co_White] && fromP == P_wk && to == Sq_g1 && toP == P_none
                && (((BBTools::mask[p.king[Co_White]].between[Sq_g1] | BBTools::mask[p.rooksInit[Co_White][CT_OO]].between[Sq_f1]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_White]].between[Sq_g1] | SquareToBitboard(p.king[Co_White]))) return true;
            return false;
        }
        else {
            if (t == T_bqs && (p.castling & C_bqs) && from == p.kingInit[Co_Black] && fromP == P_bk && to == Sq_c8 && toP == P_none
                && (((BBTools::mask[p.king[Co_Black]].between[Sq_c8] | BBTools::mask[p.rooksInit[Co_Black][CT_OOO]].between[Sq_d8]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_c8] | SquareToBitboard(p.king[Co_Black]))) return true;
            if (t == T_bks && (p.castling & C_bks) && from == p.kingInit[Co_Black] && fromP == P_bk && to == Sq_g8 && toP == P_none
                && (((BBTools::mask[p.king[Co_Black]].between[Sq_g8] | BBTools::mask[p.rooksInit[Co_Black][CT_OO]].between[Sq_f8]) & p.occupancy) == empty)
                && !isAttacked(p, BBTools::mask[p.king[Co_Black]].between[Sq_g8] | SquareToBitboard(p.king[Co_Black]))) return true;
            return false;
        }
    }
    if (fromPieceType == P_wp) {
        if (t == T_ep && to != p.ep) { return false; }
        if (t != T_ep && p.ep != INVALIDSQUARE && to == p.ep) { return false; }
        if (!isPromotion(m) && SQRANK(to) == PromRank[p.c]) { return false; }
        if (isPromotion(m) && SQRANK(to) != PromRank[p.c]) { return false; }
        BitBoard validPush = BBTools::mask[from].push[p.c] & ~p.occupancy;
        if ((BBTools::mask[from].push[p.c] & p.occupancy) == empty) validPush |= BBTools::mask[from].dpush[p.c] & ~p.occupancy;
        if (validPush & SquareToBitboard(to)) return true;
        const BitBoard validCap = BBTools::mask[from].pawnAttack[p.c] & ~p.allPieces[p.c];
        if ((validCap & SquareToBitboard(to)) && (( t != T_ep && toP != P_none) || (t == T_ep && to == p.ep))) return true;
        return false;
    }
    if (fromPieceType != P_wk) {
        if ((BBTools::pfCoverage[fromPieceType - 1](from, p.occupancy, p.c) & SquareToBitboard(to)) != empty) { return true; }
        return false;
    }
    if ((BBTools::mask[p.king[p.c]].kingZone & SquareToBitboard(to)) != empty) { return true; } // only king is not verified yet
    return false;
}

// pvs inspired by Xiphos
template< bool pvnode, bool canPrune>
ScoreType ThreadContext::pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, bool cutNode, const Move skipMove){
    if (stopFlag) return STOPSCORE;
    //if ( TimeMan::maxKNodes>0 && (stats.counters[Stats::sid_nodes] + stats.counters[Stats::sid_qnodes])/1000 > TimeMan::maxKNodes) { stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopFlag triggered (nodes limits) in thread " << id(); } ///@todo
    if ( (TimeType)std::max(1, (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()) > getCurrentMoveMs() ){ stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopFlag triggered in thread " << id(); }

    EvalData data;
    if (ply >= MAX_DEPTH - 1 || depth >= MAX_DEPTH - 1) return eval(p, data, *this);

    if ((int)ply > seldepth) seldepth = ply;

    if ( depth <= 0 ) return qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);

    debug_king_cap(p);

    ++stats.counters[Stats::sid_nodes];

    alpha = std::max(alpha, (ScoreType)(-MATE + ply));
    beta  = std::min(beta,  (ScoreType)( MATE - ply + 1));
    if (alpha >= beta) return alpha;

    const bool rootnode = ply == 0;

    if (!rootnode && interiorNodeRecognizer<true, pvnode, true>(p) == MaterialHash::Ter_Draw) return drawScore();

    ThreadContext::CMHPtrArray cmhPtr;
    getCMHPtr(p.halfmoves,cmhPtr);

    const bool withoutSkipMove = skipMove == INVALIDMOVE;
    const Hash pHash = computeHash(p) ^ (withoutSkipMove ? 0 : skipMove);
    bool validTTmove = false;
    TT::Entry e;
    if ( TT::getEntry(*this, p, pHash, depth, e)) {
        if ( e.h != 0 && !rootnode && !pvnode && ( (e.b == TT::B_alpha && e.s <= alpha) || (e.b == TT::B_beta  && e.s >= beta) || (e.b == TT::B_exact) ) ) {
            if (!isInCheck && e.m != INVALIDMINIMOVE && Move2Type(e.m) == T_std ) updateTables(*this, p, depth, ply, e.m, e.b, cmhPtr);
            return adjustHashScore(e.s, ply);
        }
    }
    validTTmove = e.h != 0 && e.m != INVALIDMINIMOVE;

#ifdef WITH_SYZYGY
    ScoreType tbScore = 0;
    if ( !rootnode && withoutSkipMove && (countBit(p.allPieces[Co_White]|p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN && SyzygyTb::probe_wdl(p, tbScore, false) > 0){
       ++stats.counters[Stats::sid_tbHit1];
       if ( abs(tbScore) == SyzygyTb::TB_WIN_SCORE) tbScore += eval(p, data, *this);
       TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(tbScore,ply),createHashScore(tbScore,ply),TT::B_exact,DepthType(127));
       return tbScore;
    }
#endif

    ScoreType evalScore;
    if (isInCheck) evalScore = -MATE + ply;
    else if ( p.lastMove == NULLMOVE && ply > 0 ) evalScore = ScaleScore(EvalConfig::tempo,stack[p.halfmoves-1].data.gp) - stack[p.halfmoves-1].eval; // skip eval if nullmove just applied ///@todo wrong ! gp is 0 here
    else{
        if (e.h != 0){
            ++stats.counters[Stats::sid_ttschits];
            evalScore = e.e;
            const Hash matHash = MaterialHash::getMaterialHash(p.mat);
            if ( matHash ){
               ++stats.counters[Stats::sid_materialTableHits];
               const MaterialHash::MaterialHashEntry & MEntry = MaterialHash::materialHashTable[matHash];
               data.gp = MEntry.gp;
            }
            else{
               ScoreType matScoreW = 0;
               ScoreType matScoreB = 0;
               data.gp = gamePhase(p,matScoreW,matScoreB);
               ++stats.counters[Stats::sid_materialTableMiss];
            }
            ///@todo data.danger is not filled here !!
        }
        else {
            ++stats.counters[Stats::sid_ttscmiss];
            evalScore = eval(p, data, *this);
        }
    }
    stack[p.halfmoves].eval = evalScore; // insert only static eval, never hash score !
    stack[p.halfmoves].data = data;
    bool evalScoreIsHashScore = false;
    if ( !validTTmove) TT::setEntry(*this,pHash,INVALIDMOVE,createHashScore(evalScore,ply),createHashScore(evalScore,ply),TT::B_none,-2); // already insert an eval here in case of pruning ...
    if ( (e.h != 0 && !isInCheck) && ((e.b == TT::B_alpha && e.s < evalScore) || (e.b == TT::B_beta && e.s > evalScore) || (e.b == TT::B_exact)) ) evalScore = adjustHashScore(e.s,ply), evalScoreIsHashScore=true;

    ScoreType bestScore = -MATE + ply;
    MoveList moves;
    bool moveGenerated = false;
    bool capMoveGenerated = false;
    bool futility = false, lmp = false, /*mateThreat = false,*/ historyPruning = false, CMHPruning = false;
    const bool isNotEndGame = p.mat[p.c][M_t]> 0; ///@todo better ?
    const bool improving = (!isInCheck && ply > 1 && stack[p.halfmoves].eval >= stack[p.halfmoves-2].eval);
    DepthType marginDepth = std::max(1,depth-(evalScoreIsHashScore?e.d:0));

    Move refutation = INVALIDMOVE;

    // prunings
    if ( !DynamicConfig::mateFinder && canPrune && !isInCheck /*&& !isMateScore(beta)*/ && !pvnode){ ///@todo removing the !isMateScore(beta) is not losing that much elo and allow for better check mate finding ...
        // static null move
        if (SearchConfig::doStaticNullMove && !isMateScore(evalScore) && isNotEndGame && depth <= SearchConfig::staticNullMoveMaxDepth[evalScoreIsHashScore] && evalScore >= beta + SearchConfig::staticNullMoveDepthInit[evalScoreIsHashScore] + (SearchConfig::staticNullMoveDepthCoeff[evalScoreIsHashScore]) * depth) return ++stats.counters[Stats::sid_staticNullMove], evalScore;

        // razoring
        ScoreType rAlpha = alpha - SearchConfig::razoringMarginDepthInit[evalScoreIsHashScore] - SearchConfig::razoringMarginDepthCoeff[evalScoreIsHashScore]*marginDepth;
        if ( SearchConfig::doRazoring && depth <= SearchConfig::razoringMaxDepth[evalScoreIsHashScore] && evalScore <= rAlpha ){
            ++stats.counters[Stats::sid_razoringTry];
            const ScoreType qScore = qsearch<true,pvnode>(alpha,beta,p,ply,seldepth);
            if ( stopFlag ) return STOPSCORE;
            if ( qScore <= alpha || (depth < 2 && evalScoreIsHashScore) ) return ++stats.counters[Stats::sid_razoring],qScore;
        }

        // null move
        if (isNotEndGame && withoutSkipMove && evalScore >= stack[p.halfmoves].eval && SearchConfig::doNullMove && ply >= (unsigned int)nullMoveMinPly && depth >= SearchConfig::nullMoveMinDepth) {
            PVList nullPV;
            ++stats.counters[Stats::sid_nullMoveTry];
            const DepthType R = depth / 4 + 3 + std::min((evalScore - beta) / 80, 3); // adaptative
            const ScoreType nullIIDScore = evalScore; // pvs<false, false>(beta - 1, beta, p, std::max(depth/4,1), ply, nullPV, seldepth, isInCheck, !cutNode);
            if (nullIIDScore >= beta /*&& stack[p.halfmoves].eval >= beta + 10 * (depth-improving)*/ ) { ///@todo try to minimize sid_nullMoveTry2 versus sid_nullMove
                TT::Entry nullE;
                const DepthType nullDepth = depth-R;
                TT::getEntry(*this, p, pHash, nullDepth, nullE);
                if (nullE.h == nullHash || nullE.s >= beta ) { // avoid null move search if TT gives a score < beta for the same depth
                    ++stats.counters[Stats::sid_nullMoveTry2];
                    Position pN = p;
                    applyNull(*this,pN);
                    stack[pN.halfmoves].h = pN.h;
                    stack[pN.halfmoves].p = pN;
                    ScoreType nullscore = -pvs<false, false>(-beta, -beta + 1, pN, nullDepth, ply + 1, nullPV, seldepth, isInCheck, !cutNode);
                    if (stopFlag) return STOPSCORE;
                    TT::Entry nullEThreat;
                    TT::getEntry(*this, pN, computeHash(pN), 0, nullEThreat);
                    if ( nullEThreat.h != nullHash ) refutation = nullEThreat.m;
                    //if (isMatedScore(nullscore)) mateThreat = true;
                    if (nullscore >= beta){
                       if (depth <= SearchConfig::nullMoveVerifDepth || nullMoveMinPly>0) return ++stats.counters[Stats::sid_nullMove], isMateScore(nullscore) ? beta : nullscore;
                       ++stats.counters[Stats::sid_nullMoveTry3];
                       nullMoveMinPly = ply + 3*nullDepth/4;
                       nullscore = pvs<false, false>(beta - 1, beta, p, nullDepth, ply+1, nullPV, seldepth, isInCheck, !cutNode);
                       nullMoveMinPly = 0;
                       if (stopFlag) return STOPSCORE;
                       if (nullscore >= beta ) return ++stats.counters[Stats::sid_nullMove2], nullscore;
                    }
                }
            }
        }

        // ProbCut
        if ( SearchConfig::doProbcut && depth >= SearchConfig::probCutMinDepth && !isMateScore(beta)){
          ++stats.counters[Stats::sid_probcutTry];
          int probCutCount = 0;
          const ScoreType betaPC = beta + SearchConfig::probCutMargin;
          MoveGen::generate<MoveGen::GP_cap>(p,moves);
          MoveSorter::sort(*this,moves,p,data.gp,ply,cmhPtr,true,isInCheck,&e);
          capMoveGenerated = true;
          for (auto it = moves.begin() ; it != moves.end() && probCutCount < SearchConfig::probCutMaxMoves /*+ 2*cutNode*/; ++it){
            if ( (validTTmove && sameMove(e.m, *it)) || isBadCap(*it) ) continue; // skip TT move if quiet or bad captures
            Position p2 = p;
            if ( ! apply(p2,*it) ) continue;
            ++probCutCount;
            ScoreType scorePC = -qsearch<true,pvnode>(-betaPC, -betaPC + 1, p2, ply + 1, seldepth);
            PVList pcPV;
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) ++stats.counters[Stats::sid_probcutTry2], scorePC = -pvs<false,true>(-betaPC,-betaPC+1,p2,depth-SearchConfig::probCutMinDepth+1,ply+1,pcPV,seldepth, isAttacked(p2, kingSquare(p2)), !cutNode);
            if (stopFlag) return STOPSCORE;
            if (scorePC >= betaPC) return ++stats.counters[Stats::sid_probcut], scorePC;
          }
        }
    }

    // IID
    if ( (e.h == nullHash /*|| e.d < depth/4*/) && ((pvnode && depth >= SearchConfig::iidMinDepth) || (cutNode && depth >= SearchConfig::iidMinDepth2)) ){ ///@todo try with cutNode only ?
        ++stats.counters[Stats::sid_iid];
        PVList iidPV;
        pvs<pvnode,false>(alpha,beta,p,/*pvnode?depth-2:*/depth/2,ply,iidPV,seldepth,isInCheck,cutNode,skipMove);
        if (stopFlag) return STOPSCORE;
        validTTmove = TT::getEntry(*this, p, pHash, depth, e) && e.h != 0 && e.m != INVALIDMINIMOVE;
    }

    killerT.killers[ply+1][0] = killerT.killers[ply+1][1] = 0;

    // LMP
    if (!rootnode && SearchConfig::doLMP && depth <= SearchConfig::lmpMaxDepth) lmp = true;
    // futility
    const ScoreType futilityScore = alpha - SearchConfig::futilityDepthInit[evalScoreIsHashScore] - SearchConfig::futilityDepthCoeff[evalScoreIsHashScore]*depth;
    if (!rootnode && SearchConfig::doFutility && depth <= SearchConfig::futilityMaxDepth[evalScoreIsHashScore] && evalScore <= futilityScore) futility = true;
    // history pruning
    if (!rootnode && SearchConfig::doHistoryPruning && isNotEndGame && depth < SearchConfig::historyPruningMaxDepth) historyPruning = true;
    // CMH pruning
    if (!rootnode && SearchConfig::doCMHPruning && isNotEndGame && depth < SearchConfig::CMHMaxDepth) CMHPruning = true;

    int validMoveCount = 0;
    Move bestMove = INVALIDMOVE;
    TT::Bound hashBound = TT::B_alpha;
    bool ttMoveIsCapture = false;
    //bool ttMoveSingularExt = false;

    stack[p.halfmoves].threat = refutation;

    // try the tt move before move generation (if not skipped move)
    if ( e.h != 0 && validTTmove && !sameMove(e.m,skipMove)) { // should be the case thanks to iid at pvnode
        bestMove = e.m; // in order to preserve tt move for alpha bound entry
        Position p2 = p;
        if ( apply(p2, e.m)) {
            TT::prefetch(computeHash(p2));
            const Square to = Move2To(e.m);
            validMoveCount++;
            PVList childPV;
            stack[p2.halfmoves].h = p2.h;
            stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
            const bool isCheck = isAttacked(p2, kingSquare(p2));
            if ( isCapture(e.m) ) ttMoveIsCapture = true;
            const bool isQuiet = Move2Type(e.m) == T_std;
            const bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(e.m)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
            // extensions
            DepthType extension = 0;
            if ( DynamicConfig::level>80){
               if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension;
               if (!extension && isCastling(e.m) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
               if (!extension && ply > 1 && VALIDMOVE(stack[p.halfmoves].threat) && VALIDMOVE(stack[p.halfmoves - 2].threat) && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
               //if (!extension && mateThreat) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
               //if (!extension && VALIDMOVE(p.lastMove) && Move2Type(p.lastMove) == T_capture && Move2To(e.m) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; // recapture
               //if (!extension && isCheck ) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
               /*
               if (!extension && isQuiet) {
               const int pp = (p.b[Move2From(e.m)] + PieceShift) * 64 + Move2To(e.m);
               if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= MAX_HISTORY / 2 && cmhPtr[1][pp] >= MAX_HISTORY / 2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
               }
               */
               if (!extension && isAdvancedPawnPush ) {
                   const BitBoard pawns[2] = { p2.pieces<P_wp>(Co_White), p2.pieces<P_wp>(Co_Black) };
                   const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
                   if ( SquareToBitboard(to) & passed[p.c] ) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
               }
               if (!extension && pvnode && (p.pieces<P_wq>(p.c) && isQuiet && PieceTools::getPieceType(p, Move2From(e.m)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces<P_wq>(p.c)))) && SEE_GE(p, e.m, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
               if (!extension && withoutSkipMove && depth >= SearchConfig::singularExtensionDepth && !rootnode && !isMateScore(e.s) && e.b == TT::B_beta && e.d >= depth - 3){
                   const ScoreType betaC = e.s - 2*depth;
                   PVList sePV;
                   DepthType seSeldetph = 0;
                   const ScoreType score = pvs<false,false>(betaC - 1, betaC, p, depth/2, ply, sePV, seSeldetph, isInCheck, cutNode, e.m);
                   if (stopFlag) return STOPSCORE;
                   if (score < betaC) {
                       ++stats.counters[Stats::sid_singularExtension],++extension;
                       if ( score < betaC - std::min(4*depth,36)) ++stats.counters[Stats::sid_singularExtension2],/*ttMoveSingularExt=true,*/++extension;
                   }
                   else if ( score >= beta && betaC >= beta) return ++stats.counters[Stats::sid_singularExtension3],score;
               }
            }
            const ScoreType ttScore = -pvs<pvnode,true>(-beta, -alpha, p2, depth - 1 + extension, ply + 1, childPV, seldepth, isCheck, !cutNode);
            if (stopFlag) return STOPSCORE;
            if (rootnode ) previousBest = e.m;
            if ( ttScore > bestScore ){
                bestScore = ttScore;
                bestMove = e.m;
                if (ttScore > alpha) {
                    hashBound = TT::B_exact;
                    if (pvnode) updatePV(pv, e.m, childPV);
                    if (ttScore >= beta) {
                        ++stats.counters[Stats::sid_ttbeta];
                        if ( !isInCheck && isQuiet ) updateTables(*this, p, depth + (ttScore > (beta+80)), ply, e.m, TT::B_beta, cmhPtr);
                        TT::setEntry(*this,pHash,e.m,createHashScore(ttScore,ply),createHashScore(evalScore,ply),TT::B_beta,depth);
                        return ttScore;
                    }
                    ++stats.counters[Stats::sid_ttalpha];
                    alpha = ttScore;
                }
            }
        }
    }

#ifdef WITH_SYZYGY
    if (rootnode && withoutSkipMove && (countBit(p.allPieces[Co_White] | p.allPieces[Co_Black])) <= SyzygyTb::MAX_TB_MEN) {
        ScoreType tbScore = 0;
        if (SyzygyTb::probe_root(*this, p, tbScore, moves) < 0) { // only good moves if TB success
            if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
            else                  MoveGen::generate<MoveGen::GP_all>  (p, moves, false);
        }
        else ++stats.counters[Stats::sid_tbHit2];
        moveGenerated = true;
    }
#endif

    ScoreType score = -MATE + ply;

    if (!moveGenerated) {
        if (capMoveGenerated) MoveGen::generate<MoveGen::GP_quiet>(p, moves, true);
        else                  MoveGen::generate<MoveGen::GP_all>  (p, moves, false);
    }
    if (moves.empty()) return isInCheck ? -MATE + ply : 0;
    MoveSorter::sort(*this, moves, p, data.gp, ply, cmhPtr, true, isInCheck, &e, refutation != INVALIDMOVE && isCapture(Move2Type(refutation)) ? refutation : INVALIDMOVE);

    for(auto it = moves.begin() ; it != moves.end() && !stopFlag ; ++it){
        if (sameMove(skipMove, *it)) continue; // skipmove
        if (validTTmove && sameMove(e.m, *it)) continue; // already tried
        Position p2 = p;
        if ( ! apply(p2,*it) ) continue;
        TT::prefetch(computeHash(p2));
        const Square to = Move2To(*it);
        if (p.c == Co_White && to == p.king[Co_Black]) return MATE - ply + 1;
        if (p.c == Co_Black && to == p.king[Co_White]) return MATE - ply + 1;
        validMoveCount++;
        const bool firstMove = validMoveCount == 1;
        PVList childPV;
        stack[p2.halfmoves].h = p2.h;
        stack[p2.halfmoves].p = p2; ///@todo another expensive copy !!!!
        const bool isCheck = isAttacked(p2, kingSquare(p2));
        bool isAdvancedPawnPush = PieceTools::getPieceType(p,Move2From(*it)) == P_wp && (SQRANK(to) > 5 || SQRANK(to) < 2);
        // extensions
        DepthType extension = 0;
        const bool isQuiet = Move2Type(*it) == T_std;
        if ( DynamicConfig::level>80){
           if (!extension && pvnode && isInCheck) ++stats.counters[Stats::sid_checkExtension],++extension; // we are in check (extension)
           if (!extension && isCastling(*it) ) ++stats.counters[Stats::sid_castlingExtension],++extension;
           if (!extension && ply > 1 && stack[p.halfmoves].threat != INVALIDMOVE && stack[p.halfmoves - 2].threat != INVALIDMOVE && (sameMove(stack[p.halfmoves].threat, stack[p.halfmoves - 2].threat) || (Move2To(stack[p.halfmoves].threat) == Move2To(stack[p.halfmoves - 2].threat) && isCapture(stack[p.halfmoves].threat)))) ++stats.counters[Stats::sid_BMExtension], ++extension;
           //if (!extension && mateThreat && depth <= 4) ++stats.counters[Stats::sid_mateThreatExtension],++extension;
           //if (!extension && VALIDMOVE(p.lastMove) && !isBadCap(*it) && Move2Type(p.lastMove) == T_capture && Move2To(*it) == Move2To(p.lastMove)) ++stats.counters[Stats::sid_recaptureExtension],++extension; //recapture
           //if (!extension && isCheck && !isBadCap(*it)) ++stats.counters[Stats::sid_checkExtension2],++extension; // we give check with a non risky move
           if (!extension && !firstMove && isQuiet) {
               const int pp = (p.b[Move2From(*it)] + PieceShift) * 64 + Move2To(*it);
               if (cmhPtr[0] && cmhPtr[1] && cmhPtr[0][pp] >= MAX_HISTORY / 2 && cmhPtr[1][pp] >= MAX_HISTORY / 2) ++stats.counters[Stats::sid_CMHExtension], ++extension;
           }
           if (!extension && isAdvancedPawnPush /*&& (killerT.isKiller(*it, ply) || !isBadCap(*it))*/) {
               const BitBoard pawns[2] = { p2.pieces<P_wp>(Co_White), p2.pieces<P_wp>(Co_Black) };
               const BitBoard passed[2] = { BBTools::pawnPassed<Co_White>(pawns[Co_White], pawns[Co_Black]), BBTools::pawnPassed<Co_Black>(pawns[Co_Black], pawns[Co_White]) };
               isAdvancedPawnPush = SquareToBitboard(to) & passed[p.c];
               if (isAdvancedPawnPush) ++stats.counters[Stats::sid_pawnPushExtension], ++extension;
           }
           if (!extension && pvnode && firstMove && (p.pieces<P_wq>(p.c) && isQuiet && Move2Type(*it) == T_std && PieceTools::getPieceType(p, Move2From(*it)) == P_wq && isAttacked(p, BBTools::SquareFromBitBoard(p.pieces<P_wq>(p.c)))) && SEE_GE(p, *it, 0)) ++stats.counters[Stats::sid_queenThreatExtension], ++extension;
        }
        // pvs
        if (validMoveCount < (2/*+2*rootnode*/) || !SearchConfig::doPVS ) score = -pvs<pvnode,true>(-beta,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode);
        else{
            // reductions & prunings
            DepthType reduction = 0;
            const bool isPrunable           = /*isNotEndGame &&*/ !isAdvancedPawnPush && !isMateScore(alpha) && !DynamicConfig::mateFinder && !killerT.isKiller(*it,ply);
            const bool isReductible         = /*isNotEndGame &&*/ !isAdvancedPawnPush && !DynamicConfig::mateFinder;
            const bool noCheck              = !isInCheck && !isCheck;
            const bool isPrunableStd        = isPrunable && isQuiet;
            const bool isPrunableStdNoCheck = isPrunableStd && noCheck;
            const bool isPrunableCap        = isPrunable && Move2Type(*it) == T_capture && isBadCap(*it) && noCheck ;
            const bool isDangerPrune        = data.danger[p.c] > SearchConfig::dangerLimitPruning[0] || data.danger[~p.c] > SearchConfig::dangerLimitPruning[1];
            const bool isDangerRed          = data.danger[p.c] > SearchConfig::dangerLimitReduction[0] || data.danger[~p.c] > SearchConfig::dangerLimitReduction[1];
            const float dangerPruneFactor   = ((1.f+data.danger[p.c])/SearchConfig::dangerLimitPruning[0] + (1.f+data.danger[~p.c])/SearchConfig::dangerLimitPruning[1])/2;
            if ( isDangerPrune) ++stats.counters[Stats::sid_dangerPrune];
            if ( isDangerRed)   ++stats.counters[Stats::sid_dangerReduce];
            // futility
            if (futility && isPrunableStdNoCheck) {++stats.counters[Stats::sid_futility]; continue;}
            // LMP
            if (lmp && isPrunableStdNoCheck && validMoveCount > (1/*+dangerPruneFactor*/)*SearchConfig::lmpLimit[improving][depth] ) {++stats.counters[Stats::sid_lmp]; continue;}
            // History pruning (with CMH)
            if (historyPruning && isPrunableStdNoCheck && Move2Score(*it) < SearchConfig::historyPruningThresholdInit + depth*SearchConfig::historyPruningThresholdDepth) {++stats.counters[Stats::sid_historyPruning]; continue;}
            // CMH pruning alone
            if (CMHPruning && isPrunableStdNoCheck){
              const int pp = (p.b[Move2From(*it)]+PieceShift) * 64 + Move2To(*it);
              if ((!cmhPtr[0] || cmhPtr[0][pp] < 0) && (!cmhPtr[1] || cmhPtr[1][pp] < 0)) { ++stats.counters[Stats::sid_CMHPruning]; continue;}
            }
            // SEE (capture)
            if (isPrunableCap){
               if (futility) {++stats.counters[Stats::sid_see]; continue;}
               else if ( !rootnode && badCapScore(*it) < -(1+dangerPruneFactor*dangerPruneFactor)*100*depth /*!SEE_GE(p,*it,-100*depth)*/) {++stats.counters[Stats::sid_see2]; continue;} ///@todo already known in current move score
            }
            // LMR
            if (SearchConfig::doLMR && (isReductible && isQuiet ) && depth >= SearchConfig::lmrMinDepth ){
                ++stats.counters[Stats::sid_lmr];
                reduction = SearchConfig::lmrReduction[std::min((int)depth,MAX_DEPTH-1)][std::min(validMoveCount,MAX_DEPTH)];
                reduction += !improving;
                reduction += ttMoveIsCapture/*&&isPrunableStd*/;
                //reduction += cutNode&&isPrunableStd;
                reduction -= (2 * Move2Score(*it)) / MAX_HISTORY; //history reduction/extension (beware killers and counter are socred above history max, so reduced less
                if ( reduction > 0){
                    if      ( pvnode           ) --reduction;
                    else if ( isDangerRed      ) --reduction;
                    else if ( !noCheck         ) --reduction;
                    //else if ( ttMoveSingularExt) --reduction;
                }
                if ( extension - reduction > 0 ) reduction = extension;
                if ( reduction >= depth - 1 + extension ) reduction = depth - 1 + extension - 1;
            }
            const DepthType nextDepth = depth-1-reduction+extension;
            // SEE (quiet)
            if ( isPrunableStdNoCheck && /*!rootnode &&*/ !SEE_GE(p,*it,-15*(1/*+isDangerPrune*/)*nextDepth*nextDepth)) {++stats.counters[Stats::sid_seeQuiet]; continue;}
            // PVS
            score = -pvs<false,true>(-alpha-1,-alpha,p2,nextDepth,ply+1,childPV,seldepth,isCheck,true);
            if ( reduction > 0 && score > alpha )                       { ++stats.counters[Stats::sid_lmrFail]; childPV.clear(); score = -pvs<false,true>(-alpha-1,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,!cutNode); }
            if ( pvnode && score > alpha && (rootnode || score < beta) ){ ++stats.counters[Stats::sid_pvsFail]; childPV.clear(); score = -pvs<true ,true>(-beta   ,-alpha,p2,depth-1+extension,ply+1,childPV,seldepth,isCheck,false); } // potential new pv node
        }
        if (stopFlag) return STOPSCORE;
        if (rootnode) previousBest = *it;
        if ( score > bestScore ){
            bestScore = score;
            bestMove = *it;
            //bestScoreUpdated = true;
            if ( score > alpha ){
                if (pvnode) updatePV(pv, *it, childPV);
                //alphaUpdated = true;
                alpha = score;
                hashBound = TT::B_exact;
                if ( score >= beta ){
                    if ( !isInCheck && isQuiet ){
                        updateTables(*this, p, depth + (score>beta+80), ply, *it, TT::B_beta, cmhPtr);
                        for(auto it2 = moves.begin() ; it2 != moves.end() && !sameMove(*it2,*it); ++it2) if ( Move2Type(*it2) == T_std ) historyT.update<-1>(depth + (score > (beta + 80)), *it2, p, cmhPtr);
                    }
                    hashBound = TT::B_beta;
                    break;
                }
            }
        }
    }

    if ( validMoveCount==0 ) return (isInCheck || !withoutSkipMove)?-MATE + ply : 0;
    TT::setEntry(*this,pHash,bestMove,createHashScore(bestScore,ply),createHashScore(evalScore,ply),hashBound,depth);
    return bestScore;
}

void ThreadContext::displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, const std::string & mark){
    static unsigned char count = 0;
    count++; // overflow is ok
    const auto now = Clock::now();
    const TimeType ms = std::max(1,(int)std::chrono::duration_cast<std::chrono::milliseconds>(now - TimeMan::startTime).count());
    std::stringstream str;
    Counter nodeCount = ThreadPool::instance().counter(Stats::sid_nodes) + ThreadPool::instance().counter(Stats::sid_qnodes);
    if (Logging::ct == Logging::CT_xboard) {
        str << int(depth) << " " << bestScore << " " << ms / 10 << " " << nodeCount << " ";
        if (DynamicConfig::fullXboardOutput) str << (int)seldepth << " " << int(nodeCount / (ms / 1000.f) / 1000.) << " " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        str << "\t" << ToString(pv);
        if ( !mark.empty() ) str << mark;
    }
    else if (Logging::ct == Logging::CT_uci) {
        str << "info depth " << int(depth) << " score cp " << bestScore << " time " << ms << " nodes " << nodeCount << " nps " << int(nodeCount / (ms / 1000.f)) << " seldepth " << (int)seldepth << " pv " << ToString(pv) << " tbhits " << ThreadPool::instance().counter(Stats::sid_tbHit1) + ThreadPool::instance().counter(Stats::sid_tbHit2);
        static auto lastHashFull = Clock::now();
        if (  (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHashFull).count() > 500
              && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(now - TimeMan::startTime).count()*2)) < getCurrentMoveMs()
              && !stopFlag){
            lastHashFull = now;
            str << " hashfull " << TT::hashFull();
        }
    }
    Logging::LogIt(Logging::logGUI) << str.str();
}

PVList ThreadContext::search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth){
    d=std::max((signed short int)1,DynamicConfig::level==SearchConfig::nlevel?d:std::min(d,SearchConfig::levelDepthMax[DynamicConfig::level/10]));
    if ( isMainThread() ){
        TimeMan::startTime = Clock::now(); ///@todo put this before ?
        Logging::LogIt(Logging::logInfo) << "Search params :" ;
        Logging::LogIt(Logging::logInfo) << "requested time  " << getCurrentMoveMs() ;
        Logging::LogIt(Logging::logInfo) << "requested depth " << (int) d ;
        stopFlag = false;
        moveDifficulty = MoveDifficultyUtil::MD_std;
        //TT::clearTT(); // to be used for reproductible results
        TT::age();
    }
    else{
        Logging::LogIt(Logging::logInfo) << "helper thread waiting ... " << id() ;
        while(startLock.load()){;}
        Logging::LogIt(Logging::logInfo) << "... go for id " << id() ;
    }
    stats.init();
    //clearPawnTT(); ///@todo loop context
    killerT.initKillers();
    historyT.initHistory();
    counterT.initCounter();

    stack[p.halfmoves].h = p.h;

    DepthType reachedDepth = 0;
    PVList pv;
    ScoreType bestScore = 0;
    m = INVALIDMOVE;

    if ( isMainThread() ){
       const Move bookMove = SanitizeCastling(p,Book::Get(computeHash(p)));
       if ( bookMove != INVALIDMOVE){
           if ( isMainThread() ) startLock.store(false);
           pv.push_back(bookMove);
           m = pv[0];
           d = 0;
           sc = 0;
           seldepth = 0;
           displayGUI(d,d,sc,pv);
           return pv;
       }
    }

    ScoreType depthScores[MAX_DEPTH] = { 0 };
    const bool isInCheck = isAttacked(p, kingSquare(p));
    const DepthType easyMoveDetectionDepth = 5;

    DepthType startDepth = 1;//std::min(d,easyMoveDetectionDepth);

    if ( isMainThread() && d > easyMoveDetectionDepth+5 && ThreadContext::currentMoveMs < INFINITETIME && TimeMan::msecUntilNextTC > 0){
       struct RootScores { Move m; ScoreType s; };
       std::vector<RootScores> rootScores;
       // easy move detection (small open window search)
       ScoreType easyScore = pvs<true,false>(-MATE, MATE, p, easyMoveDetectionDepth, 0, pv, seldepth, isInCheck,false,INVALIDMOVE);
       std::sort(rootScores.begin(), rootScores.end(), [](const RootScores& r1, const RootScores & r2) {return r1.s > r2.s; });
       if (stopFlag) { bestScore = easyScore; goto pvsout; }
       if (rootScores.size() == 1) moveDifficulty = MoveDifficultyUtil::MD_forced; // only one : check evasion or zugzwang
       else if (rootScores.size() > 1 && rootScores[0].s > rootScores[1].s + MoveDifficultyUtil::easyMoveMargin) moveDifficulty = MoveDifficultyUtil::MD_easy;
    }

    if ( DynamicConfig::level == 0 ){ // random mover
       bestScore = randomMover(p,pv,isInCheck);
       goto pvsout;
    }

    // ID loop
    for(DepthType depth = startDepth ; depth <= std::min(d,DepthType(MAX_DEPTH-6)) && !stopFlag ; ++depth ){ // -6 so that draw can be found for sure ///@todo I don't understand this -6 anymore ..
        if (!isMainThread()){ // stockfish like thread management
            const int i = (id()-1)%threadSkipSize;
            if (((depth + ThreadPool::skipPhase[i]) / ThreadPool::skipSize[i]) % 2) continue;
        }
        else{ if ( depth > 1) startLock.store(false);} // delayed other thread start
        Logging::LogIt(Logging::logInfo) << "Thread " << id() << " searching depth " << (int)depth;
        PVList pvLoc;
        ScoreType delta = (SearchConfig::doWindow && depth>4)?8:MATE; // MATE not INFSCORE in order to enter the loop below once
        ScoreType alpha = std::max(ScoreType(bestScore - delta), ScoreType (-MATE));
        ScoreType beta  = std::min(ScoreType(bestScore + delta), MATE);
        ScoreType score = 0;
        while( true ){
            pvLoc.clear();
            stack[p.halfmoves].h = p.h;
            score = pvs<true,false>(alpha,beta,p,depth,0,pvLoc,seldepth,isInCheck,false);
            if ( stopFlag ) break;
            delta += 2 + delta/2; // from xiphos ...
            if (alpha > -MATE && score <= alpha) {
                beta  = std::min(MATE,ScoreType((alpha + beta)/2));
                alpha = std::max(ScoreType(score - delta), ScoreType(-MATE) );
                Logging::LogIt(Logging::logInfo) << "Increase window alpha " << alpha << ".." << beta;
                if ( isMainThread() ){
                    PVList pv2;
                    TT::getPV(p, *this, pv2);
                    displayGUI(depth,seldepth,score,pv2,"!");
                }
            }
            else if (beta < MATE && score >= beta ) {
                //alpha = std::max(ScoreType(-MATE),ScoreType((alpha + beta)/2));
                beta  = std::min(ScoreType(score + delta), ScoreType( MATE) );
                Logging::LogIt(Logging::logInfo) << "Increase window beta "  << alpha << ".." << beta;
                if ( isMainThread() ){
                    PVList pv2;
                    TT::getPV(p, *this, pv2);
                    displayGUI(depth,seldepth,score,pv2,"?");
                }
            }
            else break;
        }
        if (stopFlag) break;
        pv = pvLoc;
        reachedDepth = depth;
        bestScore    = score;
        if ( isMainThread() ){
            displayGUI(depth,seldepth,bestScore,pv);
            if (TimeMan::isDynamic && depth > MoveDifficultyUtil::emergencyMinDepth && bestScore < depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin) { moveDifficulty = MoveDifficultyUtil::MD_hardDefense; Logging::LogIt(Logging::logInfo) << "Emergency mode activated : " << bestScore << " < " << depthScores[depth - 1] - MoveDifficultyUtil::emergencyMargin; }
            if (TimeMan::isDynamic && (TimeType)std::max(1, int(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - TimeMan::startTime).count()*1.8)) > getCurrentMoveMs()) { stopFlag = true; Logging::LogIt(Logging::logInfo) << "stopflag triggered, not enough time for next depth"; break; } // not enought time
            depthScores[depth] = bestScore;
        }
    }
pvsout:
    if ( isMainThread() ) startLock.store(false);
    if (pv.empty()){
        m = INVALIDMOVE;
        Logging::LogIt(Logging::logWarn) << "Empty pv" ;
    } else m = pv[0];
    d = reachedDepth;
    sc = bestScore;
    if (isMainThread()) ThreadPool::instance().DisplayStats();
    return pv;
}

namespace Options {
    nlohmann::json json;
    std::vector<std::string> args;
    ///@todo use std::variant ? and std::optinal ?? c++17
    enum KeyType : unsigned char { k_bad = 0, k_bool, k_depth, k_int, k_score, k_ull, k_string};
    enum WidgetType : unsigned char { w_check = 0, w_string, w_spin, w_combo, w_button, w_max};
    const std::string widgetXboardNames[w_max] = {"check","string","spin","combo","button"};
    struct KeyBase {
      template < typename T > KeyBase(KeyType t, WidgetType w, const std::string & k, T * v, const std::function<void(void)> & cb = []{} ) :type(t), wtype(w), key(k), value((void*)v) {callBack = cb;}
      template < typename T > KeyBase(KeyType t, WidgetType w, const std::string & k, T * v, const T & vmin, const T & vmax, const std::function<void(void)> & cb = []{} ) :type(t), wtype(w), key(k), value((void*)v), vmin(vmin), vmax(vmax) {callBack = cb;}
      KeyType     type;
      WidgetType  wtype;
      std::string key;
      void*       value;
      int         vmin = 0, vmax = 0; // assume int type is covering all the case (string excluded ...)
      bool        hotPlug = false;
      std::function<void(void)> callBack;
    };
    std::vector<KeyBase> _keys;
    KeyBase & GetKey(const std::string & key) {
        bool keyFound = false;
        static int dummy = 0;
        static KeyBase badKey(k_bad,w_button,"bad_default_key",&dummy,0,0);
        KeyBase * keyRef = &badKey;
        for (size_t k = 0; k < _keys.size(); ++k) { if (key == _keys[k].key) { keyRef = &_keys[k]; keyFound = true; break;} }
        if ( !keyFound) Logging::LogIt(Logging::logWarn) << "Key not found " << key;
        return *keyRef;
    }
    template< KeyType T > struct OptionTypeHelper{};
    template<> struct OptionTypeHelper<k_bool>  { typedef bool _type;};
    template<> struct OptionTypeHelper<k_depth> { typedef DepthType _type;};
    template<> struct OptionTypeHelper<k_int>   { typedef int _type;};
    template<> struct OptionTypeHelper<k_score> { typedef ScoreType _type;};
    template<> struct OptionTypeHelper<k_ull>   { typedef unsigned long long _type;};
    template<> struct OptionTypeHelper<k_string>{ typedef std::string _type;};
    const int GetValue(const std::string & key){ // assume we can convert to int safely (not valid for string of course !)
        const KeyBase & k = GetKey(key);
        switch (k.type) {
        case k_bool:   return (int)*static_cast<bool*>(k.value);
        case k_depth:  return (int)*static_cast<DepthType*>(k.value);
        case k_int:    return (int)*static_cast<int*>(k.value);
        case k_score:  return (int)*static_cast<ScoreType*>(k.value);
        case k_ull:    return (int)*static_cast<unsigned long long int*>(k.value);
        case k_string:
        case k_bad:
        default:       Logging::LogIt(Logging::logError) << "Bad key type"; return false;
        }
    }
    const std::string GetValueString(const std::string & key){ // the one for string
        const KeyBase & k = GetKey(key);
        if (k.type != k_string) Logging::LogIt(Logging::logError) << "Bad key type";
        return *static_cast<std::string*>(k.value);
    }
    void displayOptionsDebug(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key) << "\"";
            else if (it->type==k_bool )   Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (GetValue(it->key)?"true":"false") << "\"";
            else                          Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (int)GetValue(it->key)  <<  " " << it->vmin << " " << it->vmax << "\"";
    }
    void displayOptionsXBoard(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key) << "\"";
            else if (it->type==k_bool )   Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << bool(GetValue(it->key)) << "\"";
            else                          Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (int)GetValue(it->key)  <<  " " << it->vmin << " " << it->vmax << "\"";
    }
    void displayOptionsUCI(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << GetValueString(it->key);
            else if (it->type==k_bool )   Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << (GetValue(it->key)?"true":"false");
            else                          Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << (int)GetValue(it->key)  <<  " min " << it->vmin << " max " << it->vmax;
    }
#define SETVALUE(TYPEIN,TYPEOUT) {TYPEIN v; str >> std::boolalpha >> v; *static_cast<TYPEOUT*>(keyRef.value) = (TYPEOUT)v;} break;
    bool SetValue(const std::string & key, const std::string & value){
        KeyBase & keyRef = GetKey(key);
        if ( !keyRef.hotPlug && ThreadPool::instance().main().searching() ){
            Logging::LogIt(Logging::logError) << "Cannot change " << key << " during a search";
            return false;
        }
        std::stringstream str(value);
        switch (keyRef.type) {
        case k_bool:   SETVALUE(bool,bool)
        case k_depth:  SETVALUE(int,DepthType)
        case k_int:    SETVALUE(int,int)
        case k_score:  SETVALUE(int,ScoreType)
        case k_ull:    SETVALUE(int,unsigned long long)
        case k_string: SETVALUE(std::string,std::string)
        case k_bad:
        default: Logging::LogIt(Logging::logError) << "Bad key type"; return false;
        }
        if ( keyRef.callBack ) keyRef.callBack();
        Logging::LogIt(Logging::logInfo) << "Option set " << key << "=" << value;
        displayOptionsDebug();
        return true;
    }
    void registerCOMOptions(){ // options exposed to GUI
       _keys.push_back(KeyBase(k_int,   w_spin, "Level"                       , &DynamicConfig::level                          , (unsigned int)0  , (unsigned int)SearchConfig::nlevel     ));
       _keys.push_back(KeyBase(k_int,   w_spin, "Hash"                        , &DynamicConfig::ttSizeMb                       , (unsigned int)1  , (unsigned int)256000, &TT::initTable));
       _keys.push_back(KeyBase(k_int,   w_spin, "Threads"                     , &DynamicConfig::threads                        , (unsigned int)1  , (unsigned int)256   , std::bind(&ThreadPool::setup, &ThreadPool::instance())));
       _keys.push_back(KeyBase(k_bool,  w_check, "UCI_Chess960"               , &DynamicConfig::FRC                            , false            , true ));
       _keys.push_back(KeyBase(k_bool,  w_check, "Ponder"                     , &DynamicConfig::UCIPonder                      , false            , true ));

#ifdef WITH_CLOP_SEARCH
       _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin0"            , &SearchConfig::qfutilityMargin[0]              , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin1"            , &SearchConfig::qfutilityMargin[1]              , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "staticNullMoveMaxDepth0"     , &SearchConfig::staticNullMoveMaxDepth[0]       , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "staticNullMoveMaxDepth1"     , &SearchConfig::staticNullMoveMaxDepth[1]       , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthCoeff0"   , &SearchConfig::staticNullMoveDepthCoeff[0]     , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthCoeff1"   , &SearchConfig::staticNullMoveDepthCoeff[1]     , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthInit0"    , &SearchConfig::staticNullMoveDepthInit[0]      , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthInit1"    , &SearchConfig::staticNullMoveDepthInit[1]      , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthCoeff0"   , &SearchConfig::razoringMarginDepthCoeff[0]     , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthCoeff1"   , &SearchConfig::razoringMarginDepthCoeff[1]     , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthInit0"    , &SearchConfig::razoringMarginDepthInit[0]      , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthInit1"    , &SearchConfig::razoringMarginDepthInit[1]      , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "razoringMaxDepth0"           , &SearchConfig::razoringMaxDepth[0]             , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "razoringMaxDepth1"           , &SearchConfig::razoringMaxDepth[1]             , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "nullMoveMinDepth"            , &SearchConfig::nullMoveMinDepth                , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "historyPruningMaxDepth"      , &SearchConfig::historyPruningMaxDepth          , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "historyPruningThresholdInit" , &SearchConfig::historyPruningThresholdInit     , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "historyPruningThresholdDepth", &SearchConfig::historyPruningThresholdDepth    , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "futilityMaxDepth0"           , &SearchConfig::futilityMaxDepth[0]             , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "futilityMaxDepth1"           , &SearchConfig::futilityMaxDepth[1]             , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthCoeff0"         , &SearchConfig::futilityDepthCoeff[0]           , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthCoeff1"         , &SearchConfig::futilityDepthCoeff[1]           , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthInit0"          , &SearchConfig::futilityDepthInit[0]            , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthInit1"          , &SearchConfig::futilityDepthInit[1]            , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth"                 , &SearchConfig::iidMinDepth                     , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth2"                , &SearchConfig::iidMinDepth2                    , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "probCutMinDepth"             , &SearchConfig::probCutMinDepth                 , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_int  , w_spin, "probCutMaxMoves"             , &SearchConfig::probCutMaxMoves                 , 0               , 30                  ));
       _keys.push_back(KeyBase(k_score, w_spin, "probCutMargin"               , &SearchConfig::probCutMargin                   , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "lmrMinDepth"                 , &SearchConfig::lmrMinDepth                     , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "singularExtensionDepth"      , &SearchConfig::singularExtensionDepth          , DepthType(0)    , DepthType(30)       ));

       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitPruning0"         , &SearchConfig::dangerLimitPruning[0]           , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitPruning1"         , &SearchConfig::dangerLimitPruning[1]           , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitReduction0"       , &SearchConfig::dangerLimitReduction[0]         , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitReduction1"       , &SearchConfig::dangerLimitReduction[1]         , ScoreType(0)    , ScoreType(1500)     ));

       ///@todo more ...
#endif
       
    }
    void readOptions(int argc, char ** argv) { // load json config and command line args in memory
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
        std::ifstream str("minic.json");
        if (!str.is_open()) Logging::LogIt(Logging::logWarn) << "I was not able to open minic.json";
        else {
            str >> json;
            if (!json.is_object()) Logging::LogIt(Logging::logError) << "Something wrong in minic.json";
        }
    }
    // from argv (override json)
    template<typename T> bool getOptionCLI(T & value, const std::string & key) {
        auto it = std::find(args.begin(), args.end(), std::string("-") + key);
        if (it == args.end()) { Logging::LogIt(Logging::logWarn) << "ARG key not given, " << key; return false; }
        std::stringstream str;
        ++it;
        if (it == args.end()) { Logging::LogIt(Logging::logError) << "ARG value not given, " << key; return false; }
        str << *it;
        str >> value;
        Logging::LogIt(Logging::logInfo) << "From ARG, " << key << " : " << value;
        return true;
    }
    // from json
    template<typename T> bool getOption(T & value, const std::string & key) {
        if (getOptionCLI(value, key)) return true;
        auto it = json.find(key);
        if (it == json.end()) { Logging::LogIt(Logging::logWarn) << "JSON key not given, " << key; return false; }
        value = it.value();
        Logging::LogIt(Logging::logInfo) << "From config file, " << it.key() << " : " << value;
        return true;
    }

    void initOptions(int argc, char ** argv){
#define GETOPT(name,type) Options::getOption<type>(DynamicConfig::name,#name);
       registerCOMOptions();
       readOptions(argc,argv);
       GETOPT(quiet,            bool)  // first to be read
       GETOPT(debugMode,        bool)
       GETOPT(debugFile,        std::string)
       GETOPT(book,             bool)
       GETOPT(bookFile,         std::string)
       GETOPT(ttSizeMb,         unsigned int)
       GETOPT(threads,          unsigned int)
       GETOPT(mateFinder,       bool)
       GETOPT(fullXboardOutput, bool)
       GETOPT(level,            unsigned int)
#ifdef WITH_SYZYGY
       GETOPT(syzygyPath,       std::string)
#endif
   }
}

namespace COM {
    enum State : unsigned char { st_pondering = 0, st_analyzing, st_searching, st_none };
    State state; // this is redundant with Mode & Ponder...
    enum Ponder : unsigned char { p_off = 0, p_on = 1 };
    Ponder ponder;
    std::string command;
    Position position;
    Move move = INVALIDMOVE, ponderMove = INVALIDMOVE;
    DepthType depth;
    enum Mode : unsigned char { m_play_white = 0, m_play_black = 1, m_force = 2, m_analyze = 3 };
    Mode mode;
    enum SideToMove : unsigned char { stm_white = 0, stm_black = 1 };
    SideToMove stm; ///@todo isn't this redundant with position.c ??
    Position initialPos;
    std::vector<Move> moves;
    std::future<void> f;

    void newgame() {
        mode = m_force;
        stm = stm_white;
        readFEN(startPosition, COM::position);
        TT::clearTT();
        //clearPawnTT(); ///@todo loop context
    }

    void init() {
        Logging::LogIt(Logging::logInfo) << "Init COM";
        ponder = p_off;
        state = st_none;
        depth = -1;
        newgame();
    }

    void readLine() {
        command.clear();
        std::getline(std::cin, command);
        Logging::LogIt(Logging::logInfo) << "Received command : " << command;
    }

    SideToMove opponent(SideToMove & s) { return s == stm_white ? stm_black : stm_white; }

    bool sideToMoveFromFEN(const std::string & fen) {
        const bool b = readFEN(fen, COM::position,true);
        stm = COM::position.c == Co_White ? stm_white : stm_black;
        if (!b) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
        return b;
    }

    Move thinkUntilTimeUp(TimeType forcedMs = -1) { // think and when threads stop searching, return best move
        Logging::LogIt(Logging::logInfo) << "Thinking... (state " << COM::state << ")";
        ScoreType score = 0;
        Move m = INVALIDMOVE;
        if (depth < 0) depth = MAX_DEPTH;
        Logging::LogIt(Logging::logInfo) << "depth          " << (int)depth;
        ThreadContext::currentMoveMs = forcedMs <= 0 ? TimeMan::GetNextMSecPerMove(position) : forcedMs;
        Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << ThreadContext::currentMoveMs;
        Logging::LogIt(Logging::logInfo) << ToString(position);
        DepthType seldepth = 0;
        PVList pv;
        const ThreadData d = { depth,seldepth/*dummy*/,score/*dummy*/,position,m/*dummy*/,pv/*dummy*/ };
        ThreadPool::instance().search(d);
        m = ThreadPool::instance().main().getData().best; // here output results
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(m) << " (state " << COM::state << ")";;
        return m;
    }

    bool makeMove(Move m, bool disp, std::string tag, Move ponder = INVALIDMOVE) {
        bool b = apply(position, m, true);
        if (disp && m != INVALIDMOVE) Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m) << (Logging::ct==Logging::CT_uci && VALIDMOVE(ponder) ? (" ponder " + ToString(ponder)) : "");
        Logging::LogIt(Logging::logInfo) << ToString(position);
        return b;
    }

    void stop() {
        Logging::LogIt(Logging::logInfo) << "stopping previous search";
        ThreadContext::stopFlag = true;
        if ( f.valid() ){
           Logging::LogIt(Logging::logInfo) << "wait for future to land ...";
           f.wait(); // synchronous wait of current future
           Logging::LogIt(Logging::logInfo) << "...ok future is terminated";
        }
    }

    void stopPonder() {
        if (state == st_pondering) { stop(); }
    }

    void thinkAsync(State st, TimeType forcedMs = -1) { // fork a future that runs a synchorous search, if needed send returned move to GUI
        f = std::async(std::launch::async, [st,forcedMs] {
            COM::move = COM::thinkUntilTimeUp(forcedMs);
            const PVList & pv = ThreadPool::instance().main().getData().pv;
            COM::ponderMove = pv.size()>1?pv[1]:INVALIDMOVE;
            Logging::LogIt(Logging::logInfo) << "search async done (state " << st << ")";
            if (st == st_searching) {
                Logging::LogIt(Logging::logInfo) << "sending move to GUI " << ToString(COM::move);
                if (COM::move == INVALIDMOVE) { COM::mode = COM::m_force; } // game ends
                else {
                    if (!COM::makeMove(COM::move, true, Logging::ct == Logging::CT_uci ? "bestmove" : "move", COM::ponderMove)) {
                        Logging::LogIt(Logging::logGUI) << "info string Bad computer move !";
                        Logging::LogIt(Logging::logInfo) << ToString(COM::position);
                        COM::mode = COM::m_force;
                    }
                    else{
                        COM::stm = COM::opponent(COM::stm);
                        COM::moves.push_back(COM::move);
                    }
                }
            }
            Logging::LogIt(Logging::logInfo) << "Putting state to none (state " << st << ")";
            state = st_none;
        });
    }

    Move moveFromCOM(std::string mstr) { // copy string on purpose
        Square from = INVALIDSQUARE;
        Square to   = INVALIDSQUARE;
        MType mtype = T_std;
        if (!readMove(COM::position, trim(mstr), from, to, mtype)) return INVALIDMOVE;
        return ToMove(from, to, mtype);
    }
}

#ifdef WITH_UCI
#include "Add-On/uci.cc"
#endif
#ifdef WITH_XBOARD
#include "Add-On/xboard.cc"
#endif

#ifdef DEBUG_TOOL
#include "Add-On/cli.cc"
#endif

#if defined(WITH_TEXEL_TUNING) || defined(WITH_TEST_SUITE) || defined(WITH_PGN_PARSER)
#include "Add-On/extendedPosition.cc"
#endif

#ifdef WITH_PGN_PARSER
#include "Add-On/pgnparser.cc"
#endif

#ifdef WITH_TEXEL_TUNING
#include "Add-On/texelTuning.cc"
#endif

#ifdef WITH_TEST_SUITE
#include "Add-On/testSuite.cc"
#endif

void init(int argc, char ** argv) {
    Logging::hellooo();
    Options::initOptions(argc, argv);
    Logging::init(); // after reading options
    Zobrist::initHash();
    TT::initTable();
    SearchConfig::initLMR();
    SearchConfig::initMvvLva();
    BBTools::initMask();
#ifdef WITH_MAGIC
    BBTools::MagicBB::initMagic();
#endif
    MaterialHash::KPK::init();
    MaterialHash::MaterialHashInitializer::init();
    EvalConfig::initEval();
    ThreadPool::instance().setup();
    Book::initBook();
#ifdef WITH_SYZYGY
    SyzygyTb::initTB(DynamicConfig::syzygyPath);
#endif
}

int main(int argc, char ** argv) {
    START_TIMER
    init(argc, argv);
#ifdef WITH_TEST_SUITE
    if (argc > 1 && test(argv[1])) return EXIT_SUCCESS;
#endif
#ifdef WITH_TEXEL_TUNING
    if (argc > 1 && std::string(argv[1]) == "-texel") { TexelTuning(argv[2]); return EXIT_SUCCESS; }
#endif
#ifdef WITH_PGN_PARSER
    if (argc > 1 && std::string(argv[1]) == "-pgn") { return PGNParse(argv[2]); }
#endif
#ifdef DEBUG_TOOL
    std::string firstOption;
    if (argc < 2) {
        Logging::LogIt(Logging::logInfo) << "Hint: You can use -xboard command line option to enter xboard mode";
        firstOption="-uci"; // default is uci
    }
    else firstOption=argv[1];
    int ret = cliManagement(firstOption,argc,argv);
    STOP_AND_SUM_TIMER(Total)
    #ifdef WITH_TIMER
        Timers::Display();
    #endif
    return ret;
#else
    TimeMan::init();
#ifdef WITH_XBOARD
    XBoard::init();
    XBoard::xboard();
#else
#ifdef WITH_UCI
    UCI::init();
    UCI::uci();
#endif
#endif
    STOP_AND_SUM_TIMER(Total)
#ifdef WITH_TIMER
    Timers::Display();
#endif
    return EXIT_SUCCESS;
#endif
}
