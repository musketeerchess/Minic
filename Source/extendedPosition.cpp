#include "extendedPosition.hpp"

#include "logging.hpp"
#include "moveGen.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"

#include <cctype>

BitBoard getPieceBitboard(const Position & p, Piece t){
    return p.allB[t+PieceShift];
}

uint64_t numberOf(const Position & p, Piece t){ return countBit(getPieceBitboard(p,t));}

std::string showAlgAbr(Move m, const Position & p) {
    Square from  = Move2From(m);
    Square to    = Move2To(m);
    MType  mtype = Move2Type(m);
    if ( m == INVALIDMOVE) return "xx";

    bool isCheck = false;
    bool isNotLegal = false;
    Position p2 = p;
    if (apply(p2,m)){
        if ( isAttacked(p2, kingSquare(p2)) ) isCheck = true;
    }
    else{ isNotLegal = true; }

    if ( mtype == T_wks || mtype == T_bks) return std::string("0-0")   + (isCheck?"+":"") + (isNotLegal?"~":"");
    if ( mtype == T_wqs || mtype == T_bqs) return std::string("0-0-0") + (isCheck?"+":"") + (isNotLegal?"~":"");

    std::string s;
    Piece t = p.b[from];

    // add piece type if not pawn
    s+= PieceNames[PieceShift + std::abs(t)];
    if ( t==P_wp || t==P_bp ) s.clear(); // no piece symbol for pawn

    // ensure move is not ambiguous
    bool isSamePiece= false;
    bool isSameFile = false;
    bool isSameRank = false;
    if ( numberOf(p,t)>1 ){
        std::vector<Square> v;
        BitBoard b = getPieceBitboard(p,t);
        while (b) v.push_back(popBit(b));
        for(auto it = v.begin() ; it != v.end() ; ++it){
            if ( *it == from ) continue; // to not compare to myself ...
            MoveList l;
            MoveGen::generateSquare<MoveGen::GP_all>(p,l,*it);
            for(auto mit = l.begin() ; mit != l.end() ; ++mit){
                if ( *mit == m ) continue; // to not compare to myself ... should no happend thanks to previous verification
                Position p3 = p;
                if (apply(p3,*mit)){ // only if move is legal
                   if ( Move2To(*mit) == to && (t == p.b[Move2From(*mit)]) ){ // another move is landing on the same square with the same piece type
                      isSamePiece = true;
                      if ( SQFILE(Move2From(*mit)) == SQFILE(from)){ isSameFile = true; }
                      if ( SQRANK(Move2From(*mit)) == SQRANK(from)){ isSameRank = true; }
                   }
                }
            }
        }
    }

    if (((t==P_wp || t==P_bp) && isCapture(m)) ) s+= FileNames[SQFILE(from)];
    else if ( isSamePiece ){
        if ( !isSameFile )                     s+= FileNames[SQFILE(from)];
        else if ( isSameFile && !isSameRank )  s+= RankNames[SQRANK(from)];
        else if ( isSameFile && isSameRank )  {s+= FileNames[SQFILE(from)]; s+= RankNames[SQRANK(from)];}
    }

    // add 'x' if capture
    if ( isCapture(m)){
        s+="x";
    }

    // add landing position
    s+= SquareNames[to];

    // and promotion to
    if (isPromotion(m)){
        switch(mtype){
        case T_cappromq:
        case T_promq:
            s += "=";
            s += "Q";
            break;
        case T_cappromr:
        case T_promr:
            s += "=";
            s += "R";
            break;
        case T_cappromb:
        case T_promb:
            s += "=";
            s += "B";
            break;
        case T_cappromn:
        case T_promn:
            s += "=";
            s += "N";
            break;
        default:
            break;
        }
    }

    if ( isCheck )    s += "+";
    if ( isNotLegal ) s += "~";

    return s;
}

void split( std::vector<std::string> & v, const std::string & str, const std::string & sep){
    size_t start = 0, end = 0;
    while ( end != std::string::npos){
        end = str.find( sep, start);
        v.push_back( str.substr( start,(end == std::string::npos) ? std::string::npos : end - start));
        start = (   ( end > (std::string::npos - sep.size()) ) ?  std::string::npos  :  end + sep.size());
    }
}

std::vector<std::string> split2(const std::string & line, char sep, char delim){
   std::vector<std::string> v;
   const char * c = line.c_str();       // prepare to parse the line
   bool inSep{false};
   for (const char* p = c; *p ; ++p) {  // iterate through the string
       if (*p == delim) {               // toggle flag if we're btw delim
           inSep = !inSep;
       }
       else if (*p == sep && !inSep) {  // if sep OUTSIDE delim
           std::string str(c,p-c);
           str.erase(std::remove(str.begin(), str.end(), ':'), str.end());
           v.push_back(str);            // keep the field
           c = p+1;                     // and start parsing next one
       }
   }
   v.push_back(std::string(c));         // last field delimited by end of line instead of sep
   return v;
}

std::string ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    return s;
}

ExtendedPosition::ExtendedPosition(const std::string & extFEN, bool withMoveCount) : Position(extFEN, withMoveCount){
    if (!withMoveCount) {
        halfmoves = 0; moves = 1; fifty = 0;
    }
    //Logging::LogIt(Logging::logInfo) << ToString(*this);
    std::vector<std::string> strList;
    std::stringstream iss(extFEN);
    std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(strList));
    if ( strList.size() < (withMoveCount?7u:5u)) Logging::LogIt(Logging::logFatal) << "Not an extended position";
    std::vector<std::string>(strList.begin()+(withMoveCount?6:4), strList.end()).swap(strList);
    const std::string extendedParamsStr = std::accumulate(strList.begin(), strList.end(), std::string(""),[](const std::string & a, const std::string & b) {return a + ' ' + b;});
    //LogIt(logInfo) << "extended parameters : " << extendedParamsStr;
    std::vector<std::string> strParamList;
    split(strParamList,extendedParamsStr,";");
    for(size_t k = 0 ; k < strParamList.size() ; ++k){
        //LogIt(logInfo) << "extended parameters : " << k << " " << strParamList[k];
        strParamList[k] = ltrim(strParamList[k]);
        if ( strParamList[k].empty()) continue;
        std::vector<std::string> pair;
        split(pair,strParamList[k]," ");
        if ( pair.size() < 2 ) Logging::LogIt(Logging::logFatal) << "Not un extended parameter pair";
        std::vector<std::string> values = pair;
        values.erase(values.begin());
        _extendedParams[pair[0]] = values;
        //LogIt(logInfo) << "extended parameters pair : " << pair[0] << " => " << values[0];
    }
}

bool ExtendedPosition::shallFindBest(){ return _extendedParams.find("bm") != _extendedParams.end();}

bool ExtendedPosition::shallAvoidBad(){ return _extendedParams.find("am") != _extendedParams.end();}

std::vector<std::string> ExtendedPosition::bestMoves(){ return _extendedParams["bm"];}

std::vector<std::string> ExtendedPosition::badMoves(){ return _extendedParams["am"];}

std::vector<std::string> ExtendedPosition::comment0(){ return _extendedParams["c0"];}

std::string ExtendedPosition::id(){
    if ( _extendedParams.find("id") != _extendedParams.end() ) return _extendedParams["id"][0];
    else return "";
}

bool ExtendedPosition::readEPDFile(const std::string & fileName, std::vector<std::string> & positions){
    Logging::LogIt(Logging::logInfo) << "Loading EPD file : " << fileName;
    std::ifstream str(fileName);
    if (str) {
        std::string line;
        while (std::getline(str, line)) positions.push_back(line);
        return true;
    }
    else {
        Logging::LogIt(Logging::logError) << "Cannot open EPD file " << fileName;
        return false;
    }
}

#include <algorithm>
#include <cctype>
#include <locale>
#include <functional>

template<typename T>
std::ostream & operator<<(std::ostream & os, const std::vector<T> &v){
    for(auto it = v.begin() ; it != v.end() ; ++it){
        os << *it << " ";
    }
    return os;
}


void ExtendedPosition::test(const std::vector<std::string> & positions,
                            const std::vector<int> &         timeControls,
                            bool                             breakAtFirstSuccess,
                            const std::vector<int> &         scores,
                            std::function< int(int) >        eloF,
                            bool                             withMoveCount){
    struct Results{
        Results():k(0),t(0),score(0){}
        int k;
        int t;
        std::string name;
        std::vector<std::string> bm;
        std::vector<std::string> am;
        std::vector<std::pair<std::string, int > > mea;
        std::string computerMove;
        int score;
    };

    if (scores.size() != timeControls.size()){
        Logging::LogIt(Logging::logFatal) << "Wrong timeControl versus score vector size";
    }

    Results ** results = new Results*[positions.size()];

    // run the test and fill results table
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        Logging::LogIt(Logging::logInfo) << "Test #" << k << " " << positions[k];
        results[k] = new Results[timeControls.size()];
        ExtendedPosition extP(positions[k],withMoveCount);
        for(size_t t = 0 ; t < timeControls.size() ; ++t){
            Logging::LogIt(Logging::logInfo) << " " << t;
            Searcher::currentMoveMs = timeControls[t];
            DepthType seldepth = 0;
            DepthType depth = 64;
            ScoreType s = 0;
            Move bestMove = INVALIDMOVE;
            PVList pv;
            ThreadData d = {depth,seldepth,s,extP,bestMove,pv}; // only input coef
            ThreadPool::instance().search(d);
            bestMove = ThreadPool::instance().main().getData().best; // here output results

            results[k][t].name = extP.id();
            results[k][t].k = (int)k;
            results[k][t].t = timeControls[t];

            results[k][t].computerMove = showAlgAbr(bestMove,extP);
            Logging::LogIt(Logging::logInfo) << "Best move found is  " << results[k][t].computerMove;

            if ( extP.shallFindBest()){
                Logging::LogIt(Logging::logInfo) << "Best move should be " << extP.bestMoves()[0];
                results[k][t].bm = extP.bestMoves();
                results[k][t].score = 0;
                bool success = false;
                for(size_t i = 0 ; i < results[k][t].bm.size() ; ++i){
                    if ( results[k][t].computerMove == results[k][t].bm[i]){
                        results[k][t].score = scores[t];
                        success = true;
                        break;
                    }
                }
                if ( breakAtFirstSuccess && success ) break;
            }
            else if( extP.shallAvoidBad()){
                Logging::LogIt(Logging::logInfo) << "Bad move was " << extP.badMoves()[0];
                results[k][t].am = extP.badMoves();
                results[k][t].score = scores[t];
                bool success = true;
                for(size_t i = 0 ; i < results[k][t].am.size() ; ++i){
                    if ( results[k][t].computerMove == results[k][t].am[i]){
                        results[k][t].score = 0;
                        success = false;
                        break;
                    }
                }
                if ( breakAtFirstSuccess && success ) break;
            }
            else { // look into c0 section ...
                Logging::LogIt(Logging::logInfo) << "Mea style " << extP.comment0()[0];
                std::vector<std::string> tokens = extP.comment0();
                for (size_t s = 0 ; s < tokens.size() ; ++s){
                    std::string tmp = tokens[s];
                    tmp.erase(std::remove(tmp.begin(), tmp.end(), '"'), tmp.end());
                    tmp.erase(std::remove(tmp.begin(), tmp.end(), ','), tmp.end());
                    std::cout << tmp << std::endl;
                    std::vector<std::string> keyval;
                    tokenize(tmp,keyval,"=");
                    results[k][t].mea.push_back(std::make_pair(keyval[0], std::stoi( keyval[1] )));
                }
                results[k][t].score = 0;
                bool success = false;
                for(size_t i = 0 ; i < results[k][t].mea.size() ; ++i){
                    if ( results[k][t].computerMove == results[k][t].mea[i].first){
                        results[k][t].score = results[k][t].mea[i].second;
                        success = true;
                        break;
                    }
                }
                if ( breakAtFirstSuccess && success ) break;
            }
        }
    }

    // display results
    int totalScore = 0;
    std::cout << std::setw(25) << "Test"
              << std::setw(14) << "Move";
    for(size_t j = 0 ; j < timeControls.size() ; ++j){
        std::cout << std::setw(8) << timeControls[j];
    }
    std::cout << std::setw(6) << "score" << std::endl;
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        int score = 0;
        for(size_t t = 0 ; t < timeControls.size() ; ++t){
            score += results[k][t].score;
        }
        totalScore += score;
        std::stringstream str;
        const std::vector<std::string> v = (results[k][0].bm.empty()?results[k][0].am:results[k][0].bm);
        std::ostream_iterator<std::string> strIt(str," ");
        std::copy(v.begin(), v.end(), strIt);
        std::cout << std::setw(25) << results[k][0].name
                  << std::setw(14) << (results[k][0].bm.empty()?std::string("!")+str.str():str.str());
        for(size_t j = 0 ; j < timeControls.size() ; ++j){
            std::cout << std::setw(8) << results[k][j].computerMove;
        }
        std::cout << std::setw(6) << score << std::endl;
    }

    if ( eloF(100) != 0) {
        Logging::LogIt(Logging::logInfo) << "Total score " << totalScore << " => ELO " << eloF(totalScore);
    }

    // clear results table
    for (size_t k = 0 ; k < positions.size() ; ++k ){
        delete[] results[k];
    }
    delete[] results;
}

void ExtendedPosition::testStatic(const std::vector<std::string> & positions,
                                  int                              chunck,
                                  bool                             withMoveCount) {
    struct Results {
        Results() :k(0), t(0), score(0){}
        int k;
        int t;
        ScoreType score;
        std::string name;
    };

    Results * results = new Results[positions.size()];

    // run the test and fill results table
    for (size_t k = 0; k < positions.size(); ++k) {
        std::cout << "Test #" << k << " " << positions[k] << std::endl;
        ExtendedPosition extP(positions[k], withMoveCount);
        //std::cout << " " << t << std::endl;
        EvalData data;
        ScoreType ret = eval<true>(extP,data,ThreadPool::instance().main());

        results[k].name = extP.id();
        results[k].k = (int)k;
        results[k].score = ret;

        std::cout << "score is  " << ret << std::endl;

    }

    // display results
    std::cout << std::setw(25) << "Test" << std::setw(14) << "score" << std::endl;
    for (size_t k = 0; k < positions.size()-2; k+=4) {
        std::cout << std::setw(25) << results[k].name << std::setw(14) << results[k].score << std::endl;
        // only compare unsigned score ...
        if ( std::abs( std::abs(results[k].score) - std::abs(results[k+2].score) ) > 0 ){
            Logging::LogIt(Logging::logWarn) << "Score differ !";
        }
        std::cout << std::setw(25) << results[k+1].name << std::setw(14) << results[k+1].score << std::endl;
        // only compare unsigned score ...
        if ( std::abs( std::abs(results[k+1].score) - std::abs(results[k+3].score) ) > 0 ){
            Logging::LogIt(Logging::logWarn) << "Score differ !";
        }
    }

    // clear results table
    delete[] results;
}
