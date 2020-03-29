#include "moveSort.hpp"

#include "logging.hpp"
#include "searcher.hpp"

/* Moves are sorted this way
 * 1°) previous best (from current thread previousBest)
 * 2°) TT move
 * 3°) king evasion is in check and king moves
 * 4°) prom cap
 * 5°) good capture (not prom) based on SEE if not in check, based on MVV LVA if in check
 * 6°) prom
 * 7°) killer 0, then killer 1, then killer 0 from previous move, then counter
 * 8°) other quiet based on various history score (from/to, piece/to, CMH)
 * 9°) bad cap
 */

void MoveSorter::computeScore(Move & m)const{
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
                    if ( VALIDMOVE(p.lastMove) && isCapture(p.lastMove) && to == Move2To(p.lastMove) ) s += 400; // recapture bonus
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


