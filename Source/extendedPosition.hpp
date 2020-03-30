#pragma once

#include "definition.hpp"

#include "bitboardTools.hpp"
#include "position.hpp"

/* Those things are used for test suite to work when reading edp file
 */

struct ExtendedPosition : Position{
    ExtendedPosition(const std::string & s, bool withMoveCount = true);
    bool shallFindBest();
    bool shallAvoidBad();
    bool meaStyle();
    std::vector<std::string> bestMoves();
    std::vector<std::string> badMoves();
    std::vector<std::string> comment0();
    std::string id();

    static bool readEPDFile(const std::string & fileName, std::vector<std::string> & positions);

    static void test(const std::vector<std::string> & positions,
                     const std::vector<int> &         timeControls,
                     bool                             breakAtFirstSuccess,
                     const std::vector<int> &         scores,
                     std::function< int(int) >        eloF,
                     bool                             withMoveCount = true);

    static void testStatic(const std::vector<std::string> & positions,
                           int chunck = 4,
                           bool withMoveCount = false);

    std::map<std::string,std::vector<std::string> > _extendedParams;
};
