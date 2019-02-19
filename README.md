# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I'll try to keep it under 3000sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.

## Strength
Quite poor for a chess engine (around 2400) but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   0 Minic                          66       7    6405   59.4%   30.8%
   1 fruit_21                      150      27     534   70.3%   23.8%
   2 drosophila-win64               33      25     534   54.8%   26.0%
   3 Minic 0.34                    -43      19     534   43.9%   56.7%
   4 myrddin                       -45      26     533   43.6%   20.5%
   5 Minic 0.32                    -73      21     533   39.7%   49.7%
   6 MadChess.Engine               -88      26     534   37.5%   23.8%
   7 Minic 0.29                   -122      23     534   33.1%   39.3%
   8 Bitfoot                      -122      28     534   33.1%   15.7%
   9 sungorus64                   -126      28     533   32.6%   19.1%
  10 Horizon_4_4                  -145      29     534   30.2%   14.8%
  11 Minic 0.28                   -183      26     534   25.8%   31.1%
```

Current WAC score is 284 (10sec per position on an old i7-2600K).

## What about SMP ?  
```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 Minic 3 threads                60      34     200   58.5%   49.0%
   2 Minic 2 threads               -16      33     200   47.8%   52.5%
   3 Minic                         -44      32     200   43.8%   54.5%
```

## How to compile
* Linux : use the given build script (or make your own ...)
* Windows : use the given VS2017 project (or make your own ...)

## How to run
add the command line option "-xboard" to go to xboard/winboard mode.

Other available options are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -mateFinder <"fen"> depth : same as analysis but without prunings in search.

## Options

Minic now comes with both a json configuration file where some parameters can be set
```
{
   "threads": 1,
   "mateFinder": false,
   "ttSizeMb": 1024,
   "book": true
}
```
and a command line interface, using the same key word. For instance, forcing mateFinder mode can be done by adding "-mateFinder 1" when calling Minic.

## History

* 0.1 : first commit with the initial code done in less than 2 days
* 0.2-0.5 : many bug fix
* 0.6 : IID
* 0.7 : try TT move first hoping for cut-off
* 0.8 : SEE and LMP
* 0.9 : little internal book
* 0.10 : singular extension for TT move
* 0.11 : on the road to bitboard (only attack), use of "Dumb" HQBB code.
* 0.12 : better use of bitboard, in generation and eval also
* 0.13 : some draw issue fix
* 0.14 : better lmp (use of "improving bool")
* 0.15 : keep track of material count directly in position
* 0.16 : use bitboard for pawn move generation also
* 0.17 : Minic goes SMP ! 
* 0.18 : configuration file (json) and threads management
* 0.19 : option from command line and fix a TT issue  
* 0.20 : clean up code, especially draw management  
* 0.21 : add a little more evaluation (basic material bonus/malus)
* 0.22 : not much ...
* 0.23 : try to texel tune pieces value, not sure it is a success 
* 0.24 : passer eval
* 0.25 : more aggressive pruning
* 0.26 : fix a TC bug
* 0.27 : smaller window, better singular extension
* 0.28 : xboard time command is used now
* 0.29 : refactoring, very bad TT bug fix, better timeman, better gamephase, work on SEE ... huuuuuge progress. Also preparing for a better eval (pawn structure)...
* 0.30 : add a simple mobility term inside evaluation
* 0.31 : not much
* 0.32 : fix bug in eval
* 0.33 : more aggressive SEE, better move sorting, better time control. Preparing for Syzygy endgame table and end-game helpers KQK, KRK and KBNK.
* 0.34 : just another TC management improvement
* 0.35 : work on material draw
* 0.36 : little optimisations
* 0.37 : MoveList optimisations, fuse eval and score TT, refactoring a lot, bug fixes
* 0.38 : build a bigger book (not used yet), implement the so caleed easy-move
* 0.39 : fix a bad mate score TT issue, speed up pawn move generation
* 0.40 : pruning in endgame also
