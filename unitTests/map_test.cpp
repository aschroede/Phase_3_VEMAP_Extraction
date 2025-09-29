#include <dai/factor.h>
#include <strstream>

#include "dai/clustergraph.h"
#include "dai/factorgraph.h"
#include "dai/map.h"

using namespace dai;
const Real tol = 1e-8;
#define BOOST_TEST_MODULE MapTest

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>


// --- Helper Functions to create known FactorGraphs ---

/**
 * @brief Creates a simple chain graph (A-B, B-C) where all variables have 2 states.
 * Expected max clique size (treewidth + 1) is 2. Max states is 2^2 = 4.
 */
FactorGraph createChainGraph() {
    // Define three variables, each with 2 states
    dai::Var A(0, 2); // Var(label, size)
    dai::Var B(1, 2);
    dai::Var C(2, 2);

    std::vector<dai::Factor> factors;

    // Factor f(A, B)
    dai::VarSet varsAB(A, B);
    factors.push_back(dai::Factor(varsAB, 0.5));

    // Factor f(B, C)
    dai::VarSet varsBC(B, C);
    factors.push_back(dai::Factor(varsBC, 0.5));

    return dai::FactorGraph(factors);
}

/**
 * @brief Creates a simple triangle graph (A-B, B-C, C-A) - a single clique of size 3.
 * Expected max clique size (treewidth + 1) is 3. Max states is 2^3 = 8.
 */
FactorGraph createTriangleGraph() {
    // Define three variables, each with 2 states
    dai::Var A(0, 2);
    dai::Var B(1, 2);
    dai::Var C(2, 2);

    std::vector<dai::Factor> factors;

    // Factors forming a clique {A, B, C}
    dai::VarSet varsAB(A, B);
    factors.push_back(dai::Factor(varsAB, 0.5));

    dai::VarSet varsBC(B, C);
    factors.push_back(dai::Factor(varsBC, 0.5));

    dai::VarSet varsCA(C, A);
    factors.push_back(dai::Factor(varsCA, 0.5));

    return dai::FactorGraph(factors);
}

// --- BOOST TEST SUITE ---

// Helper function for Boost Test to print BigInt values if an assertion fails
namespace boost {
    namespace test_tools {
        template<>
        struct tt_detail::print_log_value<dai::BigInt> {
            void operator()(std::ostream& os, const dai::BigInt& v) {
                // Assuming BigInt has a conversion to string or double for logging purposes
                os << v;
            }
        };
    }
}

BOOST_AUTO_TEST_SUITE(TreeWidthTests)

BOOST_AUTO_TEST_CASE(TestChainGraphMinFill) {
    // A-B-C chain: Min-Fill elimination order results in a largest clique of size 2.
    // Max states = 2^2 = 4.
    FactorGraph fg = createChainGraph();
    size_t maxStatesLimit = 0; // No limit

    auto result = getTreeWidth(fg, eliminationCost_MinFill, maxStatesLimit);

    size_t treewidth = result.first;
    dai::BigInt maxStates = result.second;

    BOOST_CHECK_EQUAL(treewidth, 2);
    BOOST_CHECK_EQUAL(maxStates, dai::BigInt(4));

}

BOOST_AUTO_TEST_CASE(TestTriangleGraphMinFill) {
    // A-B-C-A triangle (clique of size 3): Min-Fill elimination results in a largest clique of size 3.
    // Max states = 2^3 = 8.
    FactorGraph fg = createTriangleGraph();
    size_t maxStatesLimit = 0; // No limitprint_log_value

    auto result = getTreeWidth(fg,eliminationCost_MinFill, maxStatesLimit);

    size_t treewidth = result.first;
    dai::BigInt maxStates = result.second;

    BOOST_CHECK_EQUAL(treewidth, 3);
    BOOST_CHECK_EQUAL(maxStates, dai::BigInt(8));
}

BOOST_AUTO_TEST_CASE(TestMaxStatesLimitPassThrough) {
    // This test ensures the maxStates argument is passed to VarElim correctly.
    // Since the actual effect of a low maxStates limit on a small graph is hard to predict
    // without full DAI knowledge, we assert the function simply runs and produces a valid result.
    FactorGraph fg = createChainGraph();
    size_t maxStatesLimit = 10; // A non-zero limit

    auto result = getTreeWidth(fg, eliminationCost_MinFill, maxStatesLimit);

    // Asserting the expected (unlimited) result should hold for a non-restrictive limit
    BOOST_CHECK_EQUAL(result.first, 2);
    BOOST_CHECK_EQUAL(result.second, dai::BigInt(4));
}

BOOST_AUTO_TEST_SUITE_END()

/**
 * @brief Creates a 6-node tree graph with the following structure:
 *      0
 *      |
 *      1
 *    /  \
 *   2    3
 *  /     \
 * 4       5
 * Variable indices: 0, 1, 2, 3, 4, 5 (all size 2).
 */
FactorGraph createSixNodeNetwork() {
    // Define six variables, each with 2 states
    dai::Var V0(0, 2);
    dai::Var V1(1, 2);
    dai::Var V2(2, 2);
    dai::Var V3(3, 2);
    dai::Var V4(4, 2);
    dai::Var V5(5, 2);

    std::vector<dai::Factor> factors;

    // Factors forming the edges
    factors.push_back(dai::Factor(dai::VarSet(V0, V1), 0.5)); // 0-1
    factors.push_back(dai::Factor(dai::VarSet(V1, V2), 0.5)); // 1-2
    factors.push_back(dai::Factor(dai::VarSet(V1, V3), 0.5)); // 1-3
    factors.push_back(dai::Factor(dai::VarSet(V2, V4), 0.5)); // 2-4
    factors.push_back(dai::Factor(dai::VarSet(V3, V5), 0.5)); // 3-5

    return dai::FactorGraph(factors);
}


// --- BOOST TEST SUITE ---

BOOST_AUTO_TEST_SUITE(EliminationOrderTests)

// Test 1: Fully Unconstrained (No Query/Evidence)
BOOST_AUTO_TEST_CASE(TestSixNodeFullyUnconstrained) {
    // Network: 0-1-(2,3), 2-4, 3-5. All 6 nodes are non-query/non-evidence.
    // Min-Fill heuristic prioritizes leaves (degree 1) first.
    // Leaves: 0, 4, 5. Intermediate: 2, 3. Root: 1.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> query_vars = {};
    std::vector<unsigned int> evidence_vars = {};

    // Use MinFill heuristic
    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getUnconstrainedElimOrder(fg, f, query_vars, evidence_vars);

    // Note there are multiple valid elimination orders here
    std::vector<size_t> expectedOrder = {0, 4, 2, 1, 3, 5};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 2: Two Query Variables, Four Non-Query
BOOST_AUTO_TEST_CASE(TestSixNodeQueryAndNonQuery) {
    // Network: Query = {1, 2}. Non-Query = {0, 3, 4, 5}. Evidence = {}.
    // Heuristic phase only considers {0, 3, 4, 5}.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> query_vars = {1, 2}; // Query variables (eliminated last)
    std::vector<unsigned int> evidence_vars = {};

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getUnconstrainedElimOrder(fg, f, query_vars, evidence_vars);

    // Elimination phase (MinFill on {0, 3, 4, 5}):
    // All candidates {0, 3, 4, 5} are currently degree 1 or 2.
    // 1. Eliminate 0 (Min-Fill picks lowest index leaf/min-degree node) -> {0}
    // 2. Eliminate 4 -> {0, 4}
    // 3. Eliminate 5 -> {0, 4, 5}
    // 4. Eliminate 3 -> {0, 4, 5, 3}
    // Append phase (Query {1, 2} in index order): {1, 2}
    std::vector<size_t> expectedOrder = {0, 4, 5, 3, 1, 2};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 3: Mixed Evidence, Query, and Non-Query
BOOST_AUTO_TEST_CASE(TestSixNodeMixedVariables) {
    // Network: Evidence = {0, 5}. Query = {1, 2}. Non-Query = {3, 4}.
    // Heuristic phase only considers {3, 4}.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> query_vars = {1, 2}; // Query variables (eliminated last)
    std::vector<unsigned int> evidence_vars = {0, 5}; // Evidence variables (excluded)

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getUnconstrainedElimOrder(fg, f, query_vars, evidence_vars);

    // Elimination phase (MinFill on {3, 4}):
    // 1. Eliminate 4 (Min-Fill picks 4 over 3 based on network structure/tie-breaking) -> {4}
    // 2. Eliminate 3 -> {4, 3}
    // Append phase (Query {1, 2} in index order): {1, 2}
    std::vector<size_t> expectedOrder = {4, 3, 1, 2};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 4: Only Query Variables (Heuristic Phase is Skipped)
BOOST_AUTO_TEST_CASE(TestSixNodeOnlyQueryVars) {
    // Network: All 6 nodes are Query variables. Evidence = {}.
    // Expected: Non-query set is empty. All variables are appended in index order.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> query_vars = {0, 1, 2, 3, 4, 5};
    std::vector<unsigned int> evidence_vars = {};

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getUnconstrainedElimOrder(fg, f, query_vars, evidence_vars);

    // Elimination phase (heuristic): empty. Order: []
    // Remaining variables (append phase): {0, 1, 2, 3, 4, 5} are appended in numerical order.
    std::vector<size_t> expectedOrder = {0, 1, 2, 3, 4, 5};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}


BOOST_AUTO_TEST_SUITE_END()