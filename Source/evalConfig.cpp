#include "evalConfig.hpp"

namespace EvalConfig {

CONST_TEXEL_TUNING EvalScore imbalance_mines[5][5] = {
    // pawn knight bishop rook queen
    {  { -4, 90}                                                      }, // Pawn
    {  {115,291}, {-132,-263}                                         }, // Knight
    {  {256,248}, {-239,-245}, {-199,-288}                            }, // Bishop
    {  {313,512}, {-208,-206}, {-232,-430},{ -178, -440}              }, // Rook
    {  {543,568}, {-482,-354}, {-701,-524},{-1207,-1130}, {-413,-412} }  // Queen
};

CONST_TEXEL_TUNING EvalScore imbalance_theirs[5][5] = {
    // pawn knight bishop rook queen
    { {-163,-227}                                                  }, // Pawn
    { { 269, 323}, {   1,-38}                                      }, // Knight
    { { 190, 463}, {  26,-74},  {   7, -42}                        }, // Bishop
    { { 211, 602}, { 106,-67},  { -17,-203}, { -33,-180}           }, // Rook
    { { 725, 873}, { 446,347},  { 481, 300}, { 383, 331},  {36,14} }  // Queen
};

CONST_TEXEL_TUNING EvalScore PST[6][64] = {
    {
       {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},
       {  98,  72},{ 133,  70},{  60,  78},{  95,  59},{  68,  91},{ 126,  65},{  34, 129},{ -11, 134},
       {  -4,  67},{   7,  44},{  23,  22},{  28, -10},{  65, -24},{  56,   5},{  25,  30},{ -20,  47},
       {   5,  40},{   8,  23},{  10,   6},{  29, -19},{  26,  -3},{  17,   9},{  15,  18},{  -2,  22},
       { -10,  30},{ -17,  29},{   2,   6},{  26, -12},{  31, -13},{  32, -11},{  10,   3},{  -5,   6},
       {  -5,  15},{ -18,  19},{   9,  -3},{   4,  -5},{  17,   0},{  29,  -7},{  43, -10},{  12,  -8},
       { -19,  25},{ -10,  16},{  -8,  16},{  -5,  17},{  -1,  12},{  44,  -2},{  46,  -8},{  -4, -13},
       {   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0},{   0,   0}
    },
    {
       {-167, -58},{ -89, -38},{ -34, -13},{ -49, -28},{  61, -31},{ -97, -27},{ -15, -63},{-107, -99},
       { -73, -79},{ -42, -57},{  71, -77},{  36, -39},{  23, -57},{  62, -83},{   7, -59},{ -17, -57},
       { -47, -33},{  60, -48},{  33, -15},{  61, -23},{  84, -30},{ 129, -33},{  73, -41},{  44, -46},
       {  -9, -20},{  19,  -5},{   1,   9},{  46,  10},{  32,  11},{  63,  -1},{  29,  -1},{  22, -19},
       { -13, -19},{   4,  -8},{  11,   4},{   9,  16},{  25,   8},{  25,   3},{  20,   3},{  -5, -20},
       { -16, -25},{  -4,  -8},{  18, -23},{  13,   6},{  26,   2},{  29, -22},{  36, -31},{ -11, -24},
       { -29, -42},{ -53, -20},{  -7, -15},{   9, -23},{  13,  -9},{  20, -22},{ -14, -23},{ -20, -44},
       {-105, -29},{ -22, -45},{ -58, -23},{ -33, -15},{ -13, -17},{ -29, -18},{ -22, -48},{ -23, -64}
    },
    {
       { -29, -14},{   4, -21},{ -82, -11},{ -37,  -8},{ -25,  -7},{ -42,  -9},{   7, -17},{  -8, -24},
       { -26,  -6},{  14,  -5},{ -18,   6},{ -13, -11},{  30,  -3},{  59, -15},{  18,  -5},{ -48, -14},
       { -12,   1},{  37,  -7},{  42,  -5},{  40,  -4},{  35,  -1},{  50,   5},{  37,   2},{   6,   4},
       {  -4,   5},{   3,   9},{  19,  11},{  45,   4},{  26,   6},{  37,   6},{  10,  -1},{   9,   3},
       {  -6,  -4},{  16,   7},{  26,   6},{  21,  17},{  38,  -3},{  20,  10},{  18,  -5},{   4,  -9},
       {  16, -11},{  41, -11},{  35,   9},{  28,   7},{  31,  14},{  56, -11},{  39,  -9},{  27, -12},
       {   4, -14},{  37, -23},{  38, -14},{  22,   1},{  33,  -1},{  29,  -1},{  61, -26},{   1, -27},
       { -33, -22},{  -3,  -9},{   9,  -5},{  -6,   4},{  -5,   7},{   1,   3},{ -39,  -5},{ -21, -18}
    },
    {
       {  32,  12},{  42,   8},{  32,  12},{  51,   8},{  63,  10},{   9,  16},{  31,  13},{  43,   7},
       {  28,   3},{  30,   4},{  58,  -8},{  62,  -7},{  79, -16},{  67,  -2},{  26,  13},{  44,   4},
       {  -6,  10},{  19,   5},{  25,  -8},{  36,  -4},{  16,  -3},{  45,  -9},{  61,  -5},{  16,  -6},
       { -25,  11},{ -12,   9},{   6,  12},{  25,   0},{  25,   3},{  35,   4},{  -8,   8},{ -19,  10},
       { -34,  14},{ -25,  19},{ -14,  16},{  -3,   7},{   8,   1},{  -6,   1},{   6,  -3},{ -24,  -5},
       { -31,   2},{ -19,  13},{ -16,   1},{ -16,   3},{   5,  -7},{   6,  -4},{  -3,   3},{ -25,  -5},
       { -27,  -1},{ -10,   4},{ -17,   6},{  -6,   6},{   6,   1},{  23, -10},{  -3,  -8},{ -51,  -1},
       {   8,  -4},{   6,   6},{  17,   1},{  27, -10},{  31,  -9},{  34,  -2},{ -13,   6},{   9, -29}
    },
    {
       { -28, -10},{   0,  23},{  29,  22},{  11,  22},{  59,  27},{  44,  19},{  43,  10},{  45,  20},
       { -28, -15},{ -54,  20},{ -13,  29},{   1,  42},{ -19,  58},{  52,  21},{  28,  30},{  53,   0},
       {   0, -20},{ -16,   5},{  -8,   3},{   0,  48},{  25,  43},{  56,  32},{  44,  19},{  49,   5},
       { -17,   7},{ -28,  25},{ -23,  19},{ -33,  44},{ -18,  56},{   7,  38},{   2,  57},{   7,  39},
       {  -3, -12},{ -27,  29},{  -5,  16},{ -19,  44},{   2,  29},{  -3,  34},{  16,  39},{   6,  28},
       {   0, -16},{  27, -32},{  -9,  13},{  13,  -4},{  10,   5},{  22,  15},{  32,  13},{  20,   8},
       { -12, -15},{  -1, -16},{  26, -31},{  27, -30},{  41, -26},{  37, -24},{   4, -35},{   1, -32},
       {  11, -33},{  14, -29},{  22, -22},{  34, -37},{  18,  -2},{  -2, -26},{ -31, -20},{ -50, -41}
    },
    {
       { -65, -74},{  23, -35},{  16, -17},{ -15,  -4},{ -56, -12},{ -34,  20},{   2,   4},{  13, -17},
       {  29, -12},{  -1,  17},{ -20,  14},{  -7,  31},{  -8,  20},{  -4,  38},{ -38,  23},{ -29,  11},
       {  -9,  10},{  24,  28},{   2,  29},{ -16,  15},{ -20,  21},{   6,  44},{  22,  46},{ -22,  10},
       { -17,  -8},{ -20,  22},{ -12,  34},{ -27,  33},{ -30,  34},{ -25,  35},{ -14,  30},{ -36,   0},
       { -49, -18},{  -1,  -2},{ -27,  28},{ -39,  27},{ -47,  35},{ -43,  20},{ -34,   5},{ -51, -12},
       { -14, -19},{ -14,  -3},{ -24,  21},{ -47,  28},{ -44,  30},{ -30,  18},{ -14,  13},{ -28,  -6},
       {   1, -30},{   7,  -6},{  -8,   9},{ -65,  22},{ -53,  30},{ -28,  17},{  22,  -4},{  14, -20},
       { -15, -55},{  64, -57},{  31, -29},{ -66,  -4},{  20, -37},{ -26, -11},{  49, -39},{  40, -65}
    }
};

CONST_TEXEL_TUNING EvalScore   pawnShieldBonus       = {4, -1};
CONST_TEXEL_TUNING EvalScore   passerBonus[8]        = { { 0, 0 }, {16, -32} , { 0, -10}, {-8, 14}, {6, 47}, {41, 54}, {52, 66}, {0, 0}};
CONST_TEXEL_TUNING EvalScore   rookBehindPassed      = {-12,52};
CONST_TEXEL_TUNING EvalScore   kingNearPassedPawn    = { -9,15};
///@todo make this depends on file! (/rank?)
CONST_TEXEL_TUNING EvalScore   doublePawnMalus[2]    = {{ 23, 13 },{ 11, 17 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   isolatedPawnMalus[2]  = {{  9,  7 },{ 17, 17 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   backwardPawnMalus[2]  = {{  2, -3 },{ 23, -3 }}; // close semiopenfile
CONST_TEXEL_TUNING EvalScore   holesMalus            = { -4, 2};
CONST_TEXEL_TUNING EvalScore   pieceFrontPawn        = {-14,15};
CONST_TEXEL_TUNING EvalScore   outpost               = { 15,19};
CONST_TEXEL_TUNING EvalScore   centerControl         = {  4, 0};
CONST_TEXEL_TUNING EvalScore   candidate[8]          = { {0, 0}, { -7,  0}, {-17,  8}, {  3, 25}, { 23, 76}, { 34, 64}, { 34, 64}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   protectedPasserBonus[8]={ {0, 0}, { 24, -8}, { 14, -9}, { 26, -7}, {  1, 14}, { 36, 47}, {  7,  9}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   freePasserBonus[8]    = { {0, 0}, { 14,  6}, { -6,  6}, { -7, 21}, { -9, 39}, {  6,122}, { 50,150}, { 0, 0} };
CONST_TEXEL_TUNING EvalScore   pawnMobility          = { -5, 17};
CONST_TEXEL_TUNING EvalScore   pawnSafeAtt           = { 59, 23};
CONST_TEXEL_TUNING EvalScore   pawnSafePushAtt       = { 16,  8};
CONST_TEXEL_TUNING EvalScore   pawnlessFlank         = {-18,-20};
CONST_TEXEL_TUNING EvalScore   pawnStormMalus        = { 14,-20};
CONST_TEXEL_TUNING EvalScore   rookOnOpenFile        = { 57,  9};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOur = { 17,  0};
CONST_TEXEL_TUNING EvalScore   rookOnOpenSemiFileOpp = { 37,  0};

CONST_TEXEL_TUNING EvalScore   rookQueenSameFile     = {  8,  3};
CONST_TEXEL_TUNING EvalScore   rookFrontQueenMalus   = { -3,-29};
CONST_TEXEL_TUNING EvalScore   rookFrontKingMalus    = {-14,  8};
CONST_TEXEL_TUNING EvalScore   minorOnOpenFile       = {  8,  4};

CONST_TEXEL_TUNING EvalScore   pinnedKing [5]        = { { -4,-11}, {  8, 64}, {-12, 69}, {-14, 67}, {-14, 13} };
CONST_TEXEL_TUNING EvalScore   pinnedQueen[5]        = { {  5, -6}, {-27,  7}, { -2, 13}, { -3,  6}, { 32, 27} };

CONST_TEXEL_TUNING EvalScore   hangingPieceMalus     = {-36,-10};

CONST_TEXEL_TUNING EvalScore   threatByMinor[6]      = { { -16,-23 },{ -20,-43 },{ -36, -32 },{ -58,-13 },{ -57, -7 },{ -62, -25 } };
CONST_TEXEL_TUNING EvalScore   threatByRook[6]       = { {  -1,-27 },{ -27, -9 },{  -5, -32 },{ -16, 15 },{ -74, -2 },{ -18, -38 } };
CONST_TEXEL_TUNING EvalScore   threatByQueen[6]      = { {  15, 11 },{   0, 17 },{  21, -13 },{  31, -4 },{  39, 24 },{ -29, -47 } };
CONST_TEXEL_TUNING EvalScore   threatByKing[6]       = { {  19,-52 },{  -9,-21 },{  49, -60 },{  15,-53 },{   0,  0 },{   0,   0 } };

CONST_TEXEL_TUNING EvalScore   adjKnight[9]          = { {-26,-20}, { -10,-3}, {  1, -2}, {  6,  3}, { -4, 16}, {  1, 23}, { 11, 42}, { 25, 56}, { 44, 29} };
CONST_TEXEL_TUNING EvalScore   adjRook[9]            = { { 24,-10}, {  15, 3}, {-23, 11}, {-46, 13}, {-47,  4}, {-42, -2}, {-44, -6}, {-42, -7}, {-44, 36} };
CONST_TEXEL_TUNING EvalScore   badBishop[9]          = { { -6, -5}, {  -7,15}, { -8, 29}, {  0, 35}, {  6, 39}, { 13, 45}, { 19, 40}, { 21, 50}, { 39, 65} };
CONST_TEXEL_TUNING EvalScore   bishopPairBonus[9]    = { { 29, 56}, { 28, 57}, { 30, 71}, { 16, 82}, { 34, 66}, { 29, 74}, { 30, 86}, { 40, 85}, { 54, 68} };
CONST_TEXEL_TUNING EvalScore   knightPairMalus       = {17,  1};
CONST_TEXEL_TUNING EvalScore   rookPairMalus         = {11,-14};
CONST_TEXEL_TUNING EvalScore   queenNearKing         = { 4, -2};

//N B R QB QR K
CONST_TEXEL_TUNING EvalScore MOB[6][15] = { { { 13,-42},{ 22,  0},{ 28, 10},{ 31, 19},{ 36, 20},{ 39, 24},{ 42, 28},{ 35, 46},{ 29, 47},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} },
                                            { {-15,-24},{  0,  0},{  7, 12},{ 14, 19},{ 17, 23},{ 16, 29},{ 19, 28},{ 21, 30},{ 41, 25},{ 45, 36},{ 55, 37},{ 71, 65},{ 71, 56},{120, 95},{  0,  0} },
                                            { { 14,-40},{ 16, 16},{ 18, 36},{ 20, 44},{ 19, 50},{ 25, 53},{ 28, 58},{ 36, 60},{ 37, 65},{ 45, 69},{ 57, 71},{ 56, 73},{ 48, 76},{ 57, 69},{ 72, 56} },
                                            { {-12,-40},{ -5,-22},{  1,-17},{  3, -3},{  9,  0},{  8, 11},{  8, 22},{ 17, 41},{ 13, 13},{ 23, 47},{ 28, 50},{ 34, 24},{ 16, 32},{ 24, 87},{  0,  0} },
                                            { { -3,-64},{ -5,-25},{ -2,-12},{ -2,-12},{  2, -8},{  0,  2},{  2, 13},{  7, 14},{ 13, 15},{ 21, 26},{  8, 37},{ 21, 43},{ 25, 48},{ 19, 45},{ 24, 54} },
                                            { { -3,-16},{-15, 31},{-19, 41},{-21, 51},{-26, 50},{-28, 41},{-29, 44},{-30, 38},{-40, 19},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0},{  0,  0} }};

CONST_TEXEL_TUNING EvalScore initiative[4] = {{0,5}, {59,40}, {115,65}, {71,88}};

CONST_TEXEL_TUNING ScoreType kingAttMax    = 428;
CONST_TEXEL_TUNING ScoreType kingAttTrans  = 49;
CONST_TEXEL_TUNING ScoreType kingAttScale  = 11;
CONST_TEXEL_TUNING ScoreType kingAttOffset =  9;
CONST_TEXEL_TUNING ScoreType kingAttWeight[2][6]    = { { 134, 230, 246, 206, 335, -33}, { 220, 156, 138, 8, -22, 0} };
CONST_TEXEL_TUNING ScoreType kingAttSafeCheck[6]    = {   128, 1184, 1152, 1056, 1024, 0};
CONST_TEXEL_TUNING ScoreType kingAttOpenfile        = 118;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOpp = 80;
CONST_TEXEL_TUNING ScoreType kingAttSemiOpenfileOur = 108;
ScoreType kingAttTable[64]       = {0};

CONST_TEXEL_TUNING EvalScore tempo = {0,0}; //{37, 34};

} // EvalConfig
