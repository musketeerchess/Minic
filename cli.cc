struct PerftAccumulator{
    PerftAccumulator(): pseudoNodes(0), validNodes(0), captureNodes(0), epNodes(0), checkNode(0), checkMateNode(0){}
    Counter pseudoNodes,validNodes,captureNodes,epNodes,checkNode,checkMateNode;

    void Display(){
        LogIt(logInfo) << "pseudoNodes   " << pseudoNodes   ;
        LogIt(logInfo) << "validNodes    " << validNodes    ;
        LogIt(logInfo) << "captureNodes  " << captureNodes  ;
        LogIt(logInfo) << "epNodes       " << epNodes       ;
        LogIt(logInfo) << "checkNode     " << checkNode     ;
        LogIt(logInfo) << "checkMateNode " << checkMateNode ;
    }
};

void pstMean(){
    for (int i = 0 ; i < 6; ++i){
        int divisor = (i==0)?48:64;
        ScoreType sum = 0;
        for(int k = 0 ; k < 64 ; ++k){
           sum += PST[i][k];
        }
        LogIt(logInfo) << Names[i+1+PieceShift] << " " << sum/divisor;
        for(int k = ((i==0)?8:0) ; k < ((i==0)?56:64) ; ++k){
            if (k%8==0) std::cout << std::endl;
            std::cout << PST[i][k]-sum/divisor << ",";
        }
        std::cout << std::endl;
        sum = 0;
        for(int k = 0 ; k < 64 ; ++k){
           sum += PSTEG[i][k];
        }
        LogIt(logInfo) << Names[i+1+PieceShift] << "EG " << sum/divisor;
        for(int k = ((i==0)?8:0) ; k < ((i==0)?56:64) ; ++k){
            if (k%8==0) std::cout << std::endl;
            std::cout << PSTEG[i][k]-sum/divisor << ",";
        }
        std::cout << std::endl;
    }
}

Counter perft(const Position & p, DepthType depth, PerftAccumulator & acc, bool divide = false){
    if ( depth == 0) return 0;
    static TT::Entry e;
    MoveList moves;
    generate(p,moves);
    int validMoves = 0;
    int allMoves = 0;
    for (auto it = moves.MLbegin() ; it != moves.MLend(); ++it){
        const Move m = *it;
        ++allMoves;
        Position p2 = p;
        if ( ! apply(p2,m) ) continue;
        ++validMoves;
        if ( divide && depth == 2 ) LogIt(logInfo) << ToString(p2) ;
        Counter nNodes = perft(p2,depth-1,acc,divide);
        if ( divide && depth == 2 ) LogIt(logInfo) << "=> after " << ToString(m) << " " << nNodes ;
        if ( divide && depth == 1 ) LogIt(logInfo) << (int)depth << " " <<  ToString(m) ;
    }
    if ( depth == 1 ) { acc.pseudoNodes += allMoves; acc.validNodes += validMoves; }
    if ( divide && depth == 2 ) LogIt(logInfo) << "********************" ;
    return acc.validNodes;
}

void perft_test(const std::string & fen, DepthType d, unsigned long long int expected) {
    Position p;
    readFEN(fen, p);
    LogIt(logInfo) << ToString(p) ;
    PerftAccumulator acc;
    if (perft(p, d, acc, false) != expected) LogIt(logInfo) << "Error !! " << fen << " " << expected ;
    acc.Display();
    LogIt(logInfo) << "#########################" ;
}

int cliManagement(std::string cli, int argc, char ** argv){

    if ( cli == "-xboard" ){
        XBoard::init();
        TimeMan::init();
        XBoard::xboard();
        return 0;
    }

    LogIt(logInfo) << "You can use -xboard command line option to enter xboard mode";

    if ( cli == "-perft_test" ){
        perft_test(startPosition, 5, 4865609);
        perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 4, 4085603);
        perft_test("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 6, 11030083);
        perft_test("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292);
    }

    if ( argc < 3 ) return 1;

#ifdef IMPORTBOOK
    if ( cli == "-buildBook"){
        Book::readBook(argv[2]);
        return 0;
    }
#endif

    std::string fen = argv[2];

    if ( fen == "start")  fen = startPosition;
    if ( fen == "fine70") fen = fine70;
    if ( fen == "shirov") fen = shirov;

    Position p;
    if ( ! readFEN(fen,p) ){
        LogIt(logInfo) << "Error reading fen" ;
        return 1;
    }

    LogIt(logInfo) << ToString(p) ;

    if (cli == "-qsearch"){
        DepthType seldepth = 0;
        double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,p,1,seldepth);
        LogIt(logInfo) << "Score " << s;
        return 1;
    }

    if (cli == "-seeval") {
        // K3r3/4r3/k2n1nb1/8/R1q1P2R/8/2QN1N2/1B6 b - - 0 1
        /*
        #
        # Info  2018-12-31 16:43:00-624: Capture from d2
        # Info  2018-12-31 16:43:00-624: Capture from d6
        # Info  2018-12-31 16:43:00-624: Capture from f2
        # Info  2018-12-31 16:43:00-624: Capture from f6
        # Info  2018-12-31 16:43:00-624: Capture from h4
        # Info  2018-12-31 16:43:00-624: Capture from g6
        # Info  2018-12-31 16:43:00-624: Capture from a4
        # Info  2018-12-31 16:43:00-624: Capture from e7
        # Info  2018-12-31 16:43:00-624: Capture from c2
        # Info  2018-12-31 16:43:00-624: Capture from e8
        # Info  2018-12-31 16:43:00-624: Capture from b1
        # Info  2018-12-31 16:43:00-624: see value : c4e4 -606
        */
        Move m = ToMove(Sq_c4, Sq_e4, T_capture);
        LogIt(logInfo) << "see value : " << ToString(m) << " " << ThreadPool::instance().main().SEEVal<true>(p,m);
        return 0;
    }

    if (cli == "-attacked") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        LogIt(logInfo) << showBitBoard(BB::isAttackedBB(p, k, p.c));
        return 0;
    }

    if (cli == "-cov") {
        Square k = Sq_e4;
        if (argc >= 3) k = atoi(argv[3]);
        switch (p.b[k]) {
        case P_wp:
            LogIt(logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c]) & ~p.whitePiece);
            break;
        case P_wn:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wb:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wr:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wq:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_wk:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.whitePiece);
            break;
        case P_bk:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wk>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bq:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wq>(k, p.occupancy, p.c )& ~p.blackPiece);
            break;
        case P_br:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wr>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bb:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wb>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bn:
            LogIt(logInfo) << showBitBoard(BB::coverage<P_wn>(k, p.occupancy, p.c) & ~p.blackPiece);
            break;
        case P_bp:
            LogIt(logInfo) << showBitBoard((BB::coverage<P_wp>(k, p.occupancy, p.c) + BB::mask[k].push[p.c])& ~p.blackPiece);
            break;
        default:
            LogIt(logInfo) << showBitBoard(0ull);
        }
        return 0;
    }

    if ( cli == "-eval" ){
        float gp = 0;
        int score = eval(p,gp);
        LogIt(logInfo) << "eval " << score << " phase " << gp ;
        return 0;
    }

    if ( cli == "-gen" ){
        MoveList moves;
        generate(p,moves);
        sort(ThreadPool::instance().main(),moves,p,0);
        LogIt(logInfo) << "nb moves : " << moves.size() ;
        for(auto it = moves.MLbegin(); it != moves.MLend(); ++it){
            LogIt(logInfo) << ToString(*it,true) ;
        }
        return 0;
    }

    if ( cli == "-testmove" ){
        Move m = ToMove(8,16,T_std);
        Position p2 = p;
        apply(p2,m);
        LogIt(logInfo) << ToString(p2) ;
        return 0;
    }

    if ( cli == "-perft" ){
        DepthType d = 5;
        if ( argc >= 3 ) d = atoi(argv[3]);
        PerftAccumulator acc;
        perft(p,d,acc,false);
        acc.Display();
        return 0;
    }

    if ( cli == "-analyze" ){
        DepthType depth = 5;
        if ( argc >= 3 ) depth = atoi(argv[3]);
        Move bestMove = INVALIDMOVE;
        ScoreType s = 0;
        TimeMan::isDynamic       = false;
        TimeMan::nbMoveInTC      = -1;
        TimeMan::msecPerMove     = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecInTC        = -1;
        TimeMan::msecInc         = -1;
        TimeMan::msecUntilNextTC = -1;
        currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        std::vector<Move> pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        LogIt(logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    if ( cli == "-mateFinder" ){
        DynamicConfig::mateFinder = true;
        DepthType depth = 5;
        if ( argc >= 3 ) depth = atoi(argv[3]);
        Move bestMove = INVALIDMOVE;
        ScoreType s = 0;
        TimeMan::isDynamic       = false;
        TimeMan::nbMoveInTC      = -1;
        TimeMan::msecPerMove     = 60*60*1000*24; // 1 day == infinity ...
        TimeMan::msecInTC        = -1;
        TimeMan::msecInc         = -1;
        TimeMan::msecUntilNextTC = -1;
        currentMoveMs = TimeMan::GetNextMSecPerMove(p);
        DepthType seldepth = 0;
        std::vector<Move> pv;
        ThreadData d = {depth,seldepth/*dummy*/,s/*dummy*/,p,bestMove/*dummy*/,pv/*dummy*/}; // only input coef
        ThreadPool::instance().searchSync(d);
        bestMove = ThreadPool::instance().main().getData().best; // here output results
        s = ThreadPool::instance().main().getData().sc; // here output results
        pv = ThreadPool::instance().main().getData().pv; // here output results
        LogIt(logInfo) << "Best move is " << ToString(bestMove) << " " << (int)depth << " " << s << " pv : " << ToString(pv) ;
        return 0;
    }

    LogIt(logInfo) << "Error : unknown command line" ;
    return 1;
}
