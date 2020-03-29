#pragma once

#include "evalDef.hpp"
#include "material.hpp"
#include "score.hpp"
#include "smp.hpp"
#include "stats.hpp"
#include "tables.hpp"

/* Searcher struct store all the information needed by a search thread
 * Implements main search function (driver, pvs, qsearch, see, display to GUI, ...)
 * This was inspired from the former thread Stockfish style management
 *
 * Many things are templates here, so other hpp file are included at the bottom of this one.
 */
struct Searcher{
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

    // used for easy move detection
    struct RootScores { Move m; ScoreType s; };
    std::vector<RootScores> rootScores;

    // used for move ordering
    Move previousBest;

    KillerT killerT;
    HistoryT historyT;
    CounterT counterT;
    DepthType nullMoveMinPly = 0;

    void getCMHPtr(DepthType ply, CMHPtrArray & cmhPtr);
    ScoreType getCMHScore(const Position & p, const Square from, const Square to, DepthType ply, const CMHPtrArray & cmhPtr)const;

    ScoreType drawScore();

    template <bool pvnode, bool canPrune = true> ScoreType pvs(ScoreType alpha, ScoreType beta, const Position & p, DepthType depth, unsigned int ply, PVList & pv, DepthType & seldepth, bool isInCheck, bool cutNode, const std::vector<MiniMove> * skipMoves = nullptr);
    template <bool qRoot, bool pvnode> ScoreType qsearch(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    ScoreType qsearchNoPruning(ScoreType alpha, ScoreType beta, const Position & p, unsigned int ply, DepthType & seldepth);
    bool SEE_GE(const Position & p, const Move & m, ScoreType threshold)const;
    ScoreType SEE(const Position & p, const Move & m)const;
    PVList search(const Position & p, Move & m, DepthType & d, ScoreType & sc, DepthType & seldepth);
    template< bool withRep = true, bool isPv = true, bool INR = true> MaterialHash::Terminaison interiorNodeRecognizer(const Position & p)const;
    bool isRep(const Position & p, bool isPv)const;
    static void displayGUI(DepthType depth, DepthType seldepth, ScoreType bestScore, const PVList & pv, int multipv, const std::string & mark = "");

    void idleLoop();

    void start();

    void wait();

    void search();

    size_t id()const;
    bool   isMainThread()const;

    Searcher(size_t n);

    ~Searcher();

    void setData(const ThreadData & d);
    const ThreadData & getData()const;

    static std::atomic<bool> startLock;

    bool searching()const;

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
        inline void reset(){
            score[MG] = 0;   score[EG] = 0;
            danger[0] = 0;   danger[1] = 0;
        }
    };
    #pragma pack(pop)

    static const unsigned long long int ttSizePawn;
    std::unique_ptr<PawnEntry[]> tablePawn = 0;

    void initPawnTable();

    void clearPawnTT();

    bool getPawnEntry(Hash h, PawnEntry *& pe);

    void prefetchPawn(Hash h);

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

#include "searcherDraw.hpp"
#include "searcherPVS.hpp"
#include "searcherQSearch.hpp"
