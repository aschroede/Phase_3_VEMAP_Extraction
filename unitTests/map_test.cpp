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


std::string outputfile = "UnitTestLog";
LogLevel logLevel = LogLevel::DEBUG;
dai::LibLogger logger = dai::LibLogger(outputfile, logLevel);


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

/**
 * @brief Creates a 6-node tree graph with simple CPTs for testing variable elimination.
 * V0--V1, V1--V2, V1--V3, V2--V4, V3--V5 (All vars size 2).
 * V_i = 0 is first state, V_i = 1 is second state.
 * CPTs are highly biased/deterministic for easy manual verification.
 */
FactorGraph createSixNodeNetworkWithCPTs() {
    dai::Var V0(0, 2);
    dai::Var V1(1, 2);
    dai::Var V2(2, 2);
    dai::Var V3(3, 2);
    dai::Var V4(4, 2);
    dai::Var V5(5, 2);

    std::vector<dai::Factor> factors;

    // Use the constructor TFactor(const VarSet& vars, const std::vector<S> &x)

    // 1. P(V0): Uniform prior
    // P(V0=0)=0.5, P(V0=1)=0.5
    factors.push_back(dai::Factor(dai::VarSet({V0}), std::vector<double>{0.5, 0.5}));

    // 2. P(V1|V0): Strong correlation (V1=V0 is 0.8 likely)
    // Values stored in order: P(V1, V0). (V0=0, V1=0), (V0=1, V1=0), (V0=0, V1=1), (V0=1, V1=1)
    // 00=0.8, 10=0.2, 01=0.2, 11=0.8
    factors.push_back(dai::Factor(dai::VarSet(V0, V1), std::vector<double>{0.8, 0.2, 0.2, 0.8}));

    // 3. P(V2|V1): Deterministic V2 = V1
    // Values stored in order: P(V2, V1). (V1=0, V2=0), (V1=1, V2=0), (V1=0, V2=1), (V1=1, V2=1)
    // 00=1.0, 10=0.0, 01=0.0, 11=1.0
    factors.push_back(dai::Factor(dai::VarSet(V1, V2), std::vector<double>{1.0, 0.0, 0.0, 1.0}));

    // 4. P(V3|V1): Deterministic V3 = V1
    // Values stored in order: P(V3, V1). (V1=0, V3=0), (V1=1, V3=0), (V1=0, V3=1), (V1=1, V3=1)
    // 00=1.0, 10=0.0, 01=0.0, 11=1.0
    factors.push_back(dai::Factor(dai::VarSet(V1, V3), std::vector<double>{1.0, 0.0, 0.0, 1.0}));
    // 5. P(V4|V2): Deterministic V4 = V2
    // Values stored in order: P(V4, V2). (V2=0, V4=0), (V2=1, V4=0), (V2=0, V4=1), (V2=1, V4=1)
    // 00=1.0, 10=0.0, 01=0.0, 11=1.0
    factors.push_back(dai::Factor(dai::VarSet(V2, V4), std::vector<double>{1.0, 0.0, 0.0, 1.0}));

    // 6. P(V5|V3): Deterministic V5 = V3
    // Values stored in order: P(V5, V3). (V3=0, V5=0), (V3=1, V5=0), (V3=0, V5=1), (V3=1, V5=1)
    // 00=1.0, 10=0.0, 01=0.0, 11=1.0
    factors.push_back(dai::Factor(dai::VarSet(V3, V5), std::vector<double>{1.0, 0.0, 0.0, 1.0}));

    return dai::FactorGraph(factors);
}



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
    // Note we do not append the query variables since they are not eliminated
    std::vector<size_t> expectedOrder = {0, 4, 5, 3};

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
    std::vector<size_t> expectedOrder = {4, 3};

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
    std::vector<size_t> expectedOrder = {};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 5: Standard MAP Case (MAP last)
BOOST_AUTO_TEST_CASE(TestConstrainedStandardMAP) {
    // Network: MAP={1, 2}. Non-MAP={0, 3, 4, 5}. Evidence={}.
    // Expected: Non-MAP eliminated first, then MAP.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> map_vars = {1, 2};
    std::vector<unsigned int> evidence_vars = {};

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getConstrainedElimOrder(fg, f, map_vars, evidence_vars);

    // Phase 1 (Non-MAP: {0, 3, 4, 5})
    // Min-Fill order on this set: (0, 4, 5, 3)

    std::vector<size_t> expectedOrder = {0, 4, 5, 3};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 6: MAP and Evidence Variables Present
BOOST_AUTO_TEST_CASE(TestConstrainedMapAndEvidence) {
    // Network: Evidence={0, 5}. MAP={2, 3}. Non-MAP={1, 4}.
    // Expected: Evidence {0, 5} excluded. Non-MAP {1, 4} eliminated first, then MAP {2, 3}.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> map_vars = {2, 3};
    std::vector<unsigned int> evidence_vars = {0, 5};

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getConstrainedElimOrder(fg, f, map_vars, evidence_vars);

    // Phase 1 (Non-MAP: {1, 4})
    // The graph is simplified because 0 and 5 are instantiated (removed).
    // Now 1 is connected to 2 (degree 1 in residual graph). 4 is connected to 2 (degree 1).
    // Min-Fill picks lowest index first: (1, 4)

    std::vector<size_t> expectedOrder = {4, 1};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

// Test 7: Only MAP Variables (Non-MAP phase is skipped)
BOOST_AUTO_TEST_CASE(TestConstrainedOnlyMAP) {
    // Network: MAP={0, 1, 2, 3, 4, 5}. Non-MAP={}. Evidence={}.
    // Expected: Non-MAP phase skipped. All MAP variables eliminated by heuristic.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<unsigned int> map_vars = {0, 1, 2, 3, 4, 5};
    std::vector<unsigned int> evidence_vars = {};

    greedyVariableElimination f(eliminationCost_MinFill);

    std::vector<size_t> actualOrder = getConstrainedElimOrder(fg, f, map_vars, evidence_vars);

    // Phase 1 (Non-MAP: {}) -> Empty

    // This is equivalent to the fully unconstrained case (Test 5), just limited to the MAP set.
    // Multiple correct answers here
    std::vector<size_t> expectedOrder = {};

    BOOST_CHECK_EQUAL_COLLECTIONS(actualOrder.begin(), actualOrder.end(),
                                  expectedOrder.begin(), expectedOrder.end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(getElimOrderTreeWidthTests)

// Test 1: Simple Chain Graph (0-1-2) with Optimal Order
BOOST_AUTO_TEST_CASE(TestChainTreeWidth) {
    // Optimal elimination order: 0, 2, 1
    // Max Clique Size is 2 ({0, 1} or {1, 2}).
    FactorGraph fg = createChainGraph();
    std::vector<size_t> order = {0, 2, 1}; // Eliminating leaves first

    std::pair<size_t, BigInt> result = getElimOrderTreeWidth(fg, order);

    // Expected: Max variables in a clique = 2 (e.g., {0, 1} before 0 is summed out)
    // Expected: Max states = 2^2 = 4
    BOOST_CHECK_EQUAL(result.first, 2);
    BOOST_CHECK_EQUAL(result.second, 4);
}

// Test 2: Triangle Graph (0-1-2-0) with any order
BOOST_AUTO_TEST_CASE(TestTriangleTreeWidth) {
    // This graph is a clique. Eliminating any variable forces the creation of a clique of size 3.
    FactorGraph fg = createTriangleGraph();
    std::vector<size_t> order = {0, 1, 2};

    std::pair<size_t, BigInt> result = getElimOrderTreeWidth(fg, order);

    // Expected: Max variables in a clique = 3 ({0, 1, 2} when 0 is summed out)
    // Expected: Max states = 2^3 = 8
    BOOST_CHECK_EQUAL(result.first, 3);
    BOOST_CHECK_EQUAL(result.second, 8);
}

// Test 3: 6-Node Network (0-4-5-2-3-1) - Optimal Order
BOOST_AUTO_TEST_CASE(TestSixNodeOptimalTreeWidth) {
    // The 6-node network is a tree, so the optimal treewidth is 2.
    // Optimal elimination order (Min-Fill): 0, 4, 5, 2, 3, 1
    FactorGraph fg = createSixNodeNetwork();
    std::vector<size_t> order = {0, 4, 5, 2, 3, 1};

    std::pair<size_t, BigInt> result = getElimOrderTreeWidth(fg, order);

    // Expected: Max variables in a clique = 2
    // (The maximum factor size remains 2 throughout the tree elimination)
    // Expected: Max states = 2^2 = 4
    BOOST_CHECK_EQUAL(result.first, 2);
    BOOST_CHECK_EQUAL(result.second, 4);
}

// Test 4: 6-Node Network (Suboptimal Order)
BOOST_AUTO_TEST_CASE(TestSixNodeSuboptimalTreeWidth) {
    // Suboptimal order: Eliminate the central node (1) first.
    // 1 is connected to {0, 2, 3}. Eliminating 1 forces the creation of a clique over {0, 2, 3}.
    FactorGraph fg = createSixNodeNetwork();
    std::vector<size_t> order = {1, 0, 2, 3, 4, 5};

    std::pair<size_t, BigInt> result = getElimOrderTreeWidth(fg, order);

    // Expected: Max variables in a clique = 4 ({0, 1, 2, 3} combined before 1 is summed out)
    // Expected: Max states = 2^4 = 16
    BOOST_CHECK_EQUAL(result.first, 4);
    BOOST_CHECK_EQUAL(result.second, 16);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(VariableEliminationTests)

// Test 6: Unconstrained Marginal Probability (P(V0))
BOOST_AUTO_TEST_CASE(TestSixNodeMarginalP_V0) {
    FactorGraph fg = createSixNodeNetworkWithCPTs();

    // Query: P(V0)
    std::vector<unsigned int> query_vars = {0};
    std::vector<unsigned int> evidence_vars = {};
    std::vector<unsigned int> evidence_values = {};

    dai::Factor resultFactor = variableElimination(fg, query_vars, evidence_vars, evidence_values, logger);

    // Expected: Since V0 is the root with a uniform prior, and all factors are conditional
    // on V0's children, eliminating the children should just normalize the initial prior.
    // P(V0) should be uniform: (0.5, 0.5)
    BOOST_CHECK_EQUAL(resultFactor.vars().size(), 1);
    BOOST_CHECK_EQUAL(resultFactor.vars().contains(dai::Var(0, 2)), true);

    // Check marginal values: P(V0=0) and P(V0=1)
    BOOST_CHECK_CLOSE(resultFactor.get(0), 0.5, 0.001); // P(V0=0)
    BOOST_CHECK_CLOSE(resultFactor.get(1), 0.5, 0.001); // P(V0=1)
}

// Test 7: Constrained Marginal Probability (P(V0 | V4=1))
BOOST_AUTO_TEST_CASE(TestSixNodeMarginalP_V0_Given_V4_Eq_1) {
    FactorGraph fg = createSixNodeNetworkWithCPTs();

    // Query: P(V0)
    std::vector<unsigned int> query_vars = {0};
    // Evidence: V4 = 1 (index 4, value 1)
    std::vector<unsigned int> evidence_vars = {4};
    std::vector<unsigned int> evidence_values = {1}; // State 1 (the second state)

    dai::Factor resultFactor = variableElimination(fg, query_vars, evidence_vars, evidence_values, logger);

    // Expected manual calculation:
    // Clamping V4=1 implies V2=1 => V1=1 => V0=1 is favored.
    // P(V0=0 | V4=1) propto P(V0=0) * P(V1=1|V0=0) = 0.5 * 0.2 = 0.1
    // P(V0=1 | V4=1) propto P(V0=1) * P(V1=1|V0=1) = 0.5 * 0.8 = 0.4
    // Normalized: P(V0=0)=0.1/0.5 = 0.2, P(V0=1)=0.4/0.5 = 0.8

    BOOST_CHECK_EQUAL(resultFactor.vars().size(), 1);

    // Check marginal values: P(V0=0) and P(V0=1)
    BOOST_CHECK_CLOSE(resultFactor.get(0), 0.2, 0.001); // P(V0=0)
    BOOST_CHECK_CLOSE(resultFactor.get(1), 0.8, 0.001); // P(V0=1)
}


// Test 8: Multiple Query Variables (P(V0, V5))
BOOST_AUTO_TEST_CASE(TestSixNodeJointMarginalP_V0_V5)
{
    FactorGraph fg = createSixNodeNetworkWithCPTs();

    // Query: P(V0, V5)
    std::vector<unsigned int> query_vars = {0, 5};
    std::vector<unsigned int> evidence_vars = {};
    std::vector<unsigned int> evidence_values = {};

    dai::Factor resultFactor = variableElimination(fg, query_vars, evidence_vars, evidence_values, logger);

    // P(VO, V5) = P(V0) * P(V1=V5|V0))

    BOOST_CHECK_EQUAL(resultFactor.vars().size(), 2);

    // Check joint marginal values:
    BOOST_CHECK_CLOSE(resultFactor.get(0), 0.4, 0.001); // P(V0=0, V5=0)
    BOOST_CHECK_CLOSE(resultFactor.get(1), 0.1, 0.001); // P(V0=1, V5=0)
    BOOST_CHECK_CLOSE(resultFactor.get(2), 0.1, 0.001); // P(V0=0, V5=1)
    BOOST_CHECK_CLOSE(resultFactor.get(3), 0.4, 0.001); // P(V0=1, V5=1)
}


BOOST_AUTO_TEST_SUITE_END()
//
//
BOOST_AUTO_TEST_SUITE(MapAndExtractMaxTests)
//
//// Test: extractMax should return the assignment for the highest-probability state
BOOST_AUTO_TEST_CASE(TestExtractMaxSimpleFactor)
{
    // Create a simple binary factor over variables {0,1} with a clear maximum at index 3 (1,1)
    dai::Var A(0, 2);
    dai::Var B(1, 2);
    dai::Factor f(dai::VarSet(A, B), std::vector<double>{0.1, 0.7, 0.2, 0.9});
    tuple<map<Var, unsigned long>, double> map = dai::extractMax(f, logger);


    BOOST_CHECK_EQUAL(std::get<0>(map).size(), 2);

    for (const auto& pair : std::get<0>(map)) {
        if (pair.first.label() == 0) { // Variable A
            BOOST_CHECK_EQUAL(pair.second, 1);
        } else if (pair.first.label() == 1) { // Variable B
            BOOST_CHECK_EQUAL(pair.second, 1);
        } else {
            BOOST_FAIL("Unexpected variable in map result");
        }
    }
}


//// Test: get_map_ve should compute MAP using variable elimination (test with evidence that forces a known MAP)
BOOST_AUTO_TEST_CASE(TestGetMapVE_SingleVarGivenEvidence)
{
    dai::FactorGraph fg = createSixNodeNetworkWithCPTs();
    // Hypothesis: V0 is the MAP variable. Evidence: V4 = 1 forces V0 = 1 in this CPT setup.
    std::vector<unsigned int> map_vars = {0};
    std::vector<unsigned int> evidence_vars = {4};
    std::vector<unsigned int> evidence_values = {1};
    tuple<map<Var, unsigned long>, double> map = dai::get_map_ve(fg, map_vars, evidence_vars, evidence_values, false, logger);

    BOOST_CHECK_EQUAL(std::get<0>(map).size(), 1);
    BOOST_CHECK_EQUAL(std::get<0>(map).begin()->second, 1);
}

//// Test: get_map_jt should compute MAP using the Junction Tree algorithm (same evidence-driven case)
BOOST_AUTO_TEST_CASE(TestGetMapJT_SingleVarGivenEvidence)
{
    dai::FactorGraph fg = createSixNodeNetworkWithCPTs();
    std::vector<unsigned int> hypothesis_vars = {0};
    std::vector<unsigned int> evidence_vars = {4};
    std::vector<unsigned int> evidence_values = {1};
    std::vector<unsigned long int> mapAssign = dai::get_map_jt(fg, hypothesis_vars, evidence_vars, evidence_values, false, logger);
    BOOST_CHECK_EQUAL(mapAssign.size(), 1);
    BOOST_CHECK_EQUAL(mapAssign[0], 1);
}

BOOST_AUTO_TEST_SUITE_END()