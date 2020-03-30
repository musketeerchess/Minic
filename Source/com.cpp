#include "com.hpp"

#include "logging.hpp"
#include "searcher.hpp"
#include "transposition.hpp"

namespace COM {

    State state; // this is redundant with Mode & Ponder...
    Ponder ponder;
    std::string command;
    Position position;
    Move move, ponderMove;
    DepthType depth;
    Mode mode;
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
        ponderMove = INVALIDMOVE;
        move = INVALIDMOVE;
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

    SideToMove opponent(SideToMove & s) {
        return s == stm_white ? stm_black : stm_white;
    }

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
        Searcher::currentMoveMs = forcedMs <= 0 ? TimeMan::GetNextMSecPerMove(position) : forcedMs;
        Logging::LogIt(Logging::logInfo) << "currentMoveMs  " << Searcher::currentMoveMs;
        Logging::LogIt(Logging::logInfo) << ToString(position);
        DepthType seldepth = 0;
        PVList pv;
        const ThreadData d = { depth,seldepth/*dummy*/,score/*dummy*/,position,m/*dummy*/,pv/*dummy*/ };
        ThreadPool::instance().search(d);
        m = ThreadPool::instance().main().getData().best; // here output results
        Logging::LogIt(Logging::logInfo) << "...done returning move " << ToString(m) << " (state " << COM::state << ")";;
        return m;
    }

    bool makeMove(Move m, bool disp, std::string tag, Move ponder) {
        bool b = apply(position, m, true);
        if (disp && m != INVALIDMOVE) Logging::LogIt(Logging::logGUI) << tag << " " << ToString(m) << (Logging::ct==Logging::CT_uci && VALIDMOVE(ponder) ? (" ponder " + ToString(ponder)) : "");
        Logging::LogIt(Logging::logInfo) << ToString(position);
        return b;
    }

    void stop() {
        Logging::LogIt(Logging::logInfo) << "stopping previous search";
        Searcher::stopFlag = true;
        if ( f.valid() ){
           Logging::LogIt(Logging::logInfo) << "wait for future to land ...";
           f.wait(); // synchronous wait of current future
           Logging::LogIt(Logging::logInfo) << "...ok future is terminated";
        }
    }

    void stopPonder() {
        if (state == st_pondering) {
            stop();
        }
    }

    void thinkAsync(State st, TimeType forcedMs) { // fork a future that runs a synchorous search, if needed send returned move to GUI
        f = std::async(std::launch::async, [st,forcedMs] {
            COM::move = COM::thinkUntilTimeUp(forcedMs);
            const PVList & pv = ThreadPool::instance().main().getData().pv;
            COM::ponderMove = INVALIDMOVE;
            if ( pv.size() > 1) {
               Position p2 = COM::position;
               if ( apply(p2,pv[0]) && isPseudoLegal(p2,pv[1])) COM::ponderMove = pv[1];
            }
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
