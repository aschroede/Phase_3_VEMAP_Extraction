/*
 * This file is part of libDAI - http://www.libdai.org/
 *
 * Copyright (c) 2006-2011, The libDAI authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
 */

#include <iostream>
#include <chrono>
#include <stack>
#include <statsutil.h>
#include <dai/factor.h>
#include <dai/factorgraph.h>
#include <dai/map.h>
#include <dai/clustergraph.h>
#include <dai/logger.h>
#include <dai/jtree.h>
#include <dai/properties.h>

// comment for production mode, uncomment for debug messages
#define DEBUGMODE

#ifdef DEBUGMODE
#define DEBUG(a) a;
#else
#define DEBUG(a) ;
#endif

namespace dai {
    class VarSet;

    using namespace std;

/**
 * @brief Logs the details of a vector of factors.
 *
 * This function iterates through a vector of `dai::Factor` objects and logs a string representation
 * of each factor to a provided logger at the `LogLevel::DEBUG` level. This is useful for debugging and
 * tracing the state of factors throughout a computation.
 *
 * @param factors A reference to the vector of `dai::Factor` objects to be logged.
 * @param logger A reference to the `dai::LibLogger` instance used for logging.
 */
void LogFactors(std::vector<dai::Factor>& factors, dai::LibLogger& logger) {
    for (dai::Factor& factor : factors) {
        logger.log(LogLevel::DEBUG, factor.toStringNice());
    }
}

/**
 * @brief Generates an unconstrained variable elimination order for a factor graph.
 *
 * This template function determines a variable elimination order that is "unconstrained". For the MAP algorithm
 * we must first sum out all non-MAP variables and then maximise over the MAP variables, thus the elimination order
 * is constrained by the fact that the MAP variables must be eliminated last. This restriction means that the best
 * unconstrained elimination order may not be possible in the constrained case.
 *
 * @tparam EliminationChoice A class representing the elimination heuristic (e.g., `greedyVariableElimination`).
 * @param fg The input factor graph.
 * @param f The elimination heuristic object. TODO: what is this and what options are there?
 * @param query_vars A vector of indices for the query variables.
 * @param evidence_vars A vector of indices for the evidence variables.
 * @return A vector of `size_t` representing the variable elimination order.
 */
template <class EliminationChoice>
vector<size_t> getUnconstrainedElimOrder(const FactorGraph& fg, EliminationChoice f, std::vector<unsigned int> query_vars, std::vector<unsigned int> evidence_vars) {
    // Create cluster graph from factor graph
    ClusterGraph cl(fg, true);

    // Get unconstrained elimination order by finding non-query and non-evidence variables
    std::set<size_t> nonQueryVarindices;
    for (size_t i = 0; i < cl.vars().size(); ++i) {
        bool isEvidence = (std::find(evidence_vars.begin(), evidence_vars.end(), i) != evidence_vars.end());
        bool isQuery = (std::find(query_vars.begin(), query_vars.end(), i) != query_vars.end());

        // Add variables that are neither evidence nor query
        if (!isEvidence && !isQuery) {
            nonQueryVarindices.insert(i);
        }
    }

    vector<size_t> elimOrder;
    // Load up non-query/non-evidence variables first based on the heuristic
    while (!nonQueryVarindices.empty()) {
        size_t i = f(cl, nonQueryVarindices);
        cl.elimVar(i);
        elimOrder.push_back(i);
        nonQueryVarindices.erase(i);
    }

    return elimOrder;
}

/**
 * @brief Generates a constrained variable elimination order for a factor graph.
 *
 * This template function generates a variable elimination order that is "constrained" by a set of
 * MAP (Maximum a Posteriori) variables. It prioritizes eliminating non-MAP evidence variables
 * first, then eliminates the MAP variables at the end. This is a common strategy for solving MAP
 * problems using variable elimination.
 *
 * @tparam EliminationChoice A class representing the elimination heuristic (e.g., `greedyVariableElimination`).
 * @param fg The input factor graph.
 * @param f The elimination heuristic object.
 * @param map_vars A vector of indices for the MAP variables.
 * @param evidence_vars A vector of indices for the evidence variables.
 * @return A vector of `size_t` representing the variable elimination order.
 */
template <class EliminationChoice>
vector<size_t> getConstrainedElimOrder(const FactorGraph& fg, EliminationChoice f, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars) {
    // Create cluster graph from factor graph
    ClusterGraph cl(fg, true);

    // Separate variables into non-MAP and MAP sets
    std::set<size_t> nonMapVarindices;
    std::set<size_t> MapVarindices;

    // Put variables in appropriate lists
    for (size_t i = 0; i < cl.vars().size(); ++i) {
        bool isEvidence = (std::find(evidence_vars.begin(), evidence_vars.end(), i) != evidence_vars.end());
        if (!isEvidence) {
            bool isMap = (std::find(map_vars.begin(), map_vars.end(), i) != map_vars.end());
            if (isMap) {
                MapVarindices.insert(i);
            } else {
                nonMapVarindices.insert(i);
            }
        }
    }

    vector<size_t> elimOrder;

    // Eliminate non-MAP variables first
    while (!nonMapVarindices.empty()) {
        size_t i = f(cl, nonMapVarindices);
        cl.elimVar(i);
        elimOrder.push_back(i);
        nonMapVarindices.erase(i);
    }

    return elimOrder;
}

/**
 * @brief Calculates the treewidth and max states for a given elimination order.
 *
 * This function simulates the variable elimination process on a factor graph for a given elimination
 * order. It does not perform the actual calculations on factor values, but rather tracks the `VarSet`
 * of each factor to determine the size and number of states of the largest factor created. This
 * provides an estimate of the memory and computational complexity without a full run.
 *
 * This allows us to see the treewidth and number of states in the constrained elimination order. Useful for diagnosis
 *
 * @param fg The input factor graph.
 * @param elimOrder The variable elimination order as a vector of variable indices.
 * @return A pair containing the treewidth (size_t, number of variables) and the maximum number of states (BigInt).
 */
std::pair<size_t, BigInt> getElimOrderTreeWidth(dai::FactorGraph fg, vector<size_t> elimOrder) {
    std::uint16_t treeWidth = 0;
    dai::BigInt maxStates = 0;

    std::vector<dai::VarSet> factorVarSets;
    for (const auto& factor : fg.factors()) {
        factorVarSets.push_back(factor.vars());
    }

    for (size_t varIndex : elimOrder) {
        // Find all factor variable sets that contain the variable to be eliminated
        std::vector<dai::VarSet> toMultiply;
        for (const auto& vars : factorVarSets) {
            if (vars.contains(fg.var(varIndex))) {
                toMultiply.push_back(vars);
            }
        }

        // Combine the variable sets by taking their union
        if (toMultiply.empty()) {
            continue;
        }

        dai::VarSet newFactorVarSet = toMultiply[0];
        for (size_t i = 1; i < toMultiply.size(); ++i) {
            newFactorVarSet |= toMultiply[i];
        }

        // Update maximum treewidth and states
        if (newFactorVarSet.nrStates() > maxStates) {
            maxStates = newFactorVarSet.nrStates();
        }
        if (newFactorVarSet.size() > treeWidth) {
            treeWidth = newFactorVarSet.size();
        }

        // "Sum out" the variable by removing it from the new factor's variable set
        newFactorVarSet.erase(fg.var(varIndex));

        // Remove old factor variable sets and add the new one
        for (const auto& vars : toMultiply) {
            factorVarSets.erase(std::find(factorVarSets.begin(), factorVarSets.end(), vars));
        }
        factorVarSets.push_back(newFactorVarSet);
    }

    // Handle remaining factors after elimination
    if (!factorVarSets.empty()) {
        dai::VarSet newFactorVarSet = factorVarSets[0];
        for (size_t i = 1; i < factorVarSets.size(); ++i) {
            newFactorVarSet |= factorVarSets[i];
        }

        if (newFactorVarSet.nrStates() > maxStates) {
            maxStates = newFactorVarSet.nrStates();
        }
        if (newFactorVarSet.size() > treeWidth) {
            treeWidth = newFactorVarSet.size();
        }
    }

    return make_pair(treeWidth, maxStates);
}


dai::Factor elim_vars(dai::FactorGraph fg, LibLogger& logger, vector<size_t> elimOrder)
{
    std::vector<dai::Factor> factors = fg.factors();
    logger.log(LogLevel::DEBUG, "Initial set of factors:");
    LogFactors(factors, logger);

    // Perform variable elimination
    for (size_t varIndex : elimOrder) {
        logger.log(LogLevel::DEBUG, "Variable to Eliminate: " + std::to_string(varIndex));

        // Find and multiply factors that contain the variable to be eliminated
        std::vector<dai::Factor> toMultiply;
        for (const auto& factor : factors) {
            if (factor.vars().contains(fg.var(varIndex))) {
                toMultiply.push_back(factor);
            }
        }
        logger.log(LogLevel::DEBUG, "Factors to multiply: ");
        LogFactors(toMultiply, logger);

        if (toMultiply.empty()) {
            continue; // Already eliminated or not present
        }

        dai::Factor newFactor = toMultiply[0];
        for (size_t i = 1; i < toMultiply.size(); ++i) {
            newFactor *= toMultiply[i];
        }
        logger.log(LogLevel::DEBUG, "Multiplication Result: ");
        logger.log(LogLevel::DEBUG, newFactor.toStringNice());

        // Marginalize out the variable
        dai::Var varToRemove = fg.var(varIndex);
        newFactor = newFactor.marginal(newFactor.vars() / varToRemove, false);
        logger.log(LogLevel::DEBUG, "After marginalising out " + std::to_string(varIndex));
        logger.log(LogLevel::DEBUG, newFactor.toStringNice());

        // Update the list of factors
        for (const auto& oldFactor : toMultiply) {
            factors.erase(std::find(factors.begin(), factors.end(), oldFactor));
        }
        factors.push_back(newFactor);

        logger.log(LogLevel::DEBUG, "New List of Factors:");
        LogFactors(factors, logger);
    }

    logger.log(LogLevel::DEBUG, "Completed marginalisation. Multiplying remaining factors...");

    // Multiply remaining factors to get the final joint marginal
    dai::Factor finalFactor = factors[0];
    for (size_t i = 1; i < factors.size(); ++i) {
        finalFactor *= factors[i];
    }
    finalFactor.normalize();

    // logger.log(LogLevel::INFO, "Final Result (normalized): ");
    // logger.log(LogLevel::INFO, "**** Variable Elimination complete ****");
    return finalFactor;
}

/**
 * @brief Performs variable elimination to compute marginal probabilities.
 *
 * This function performs the Variable Elimination (VE) algorithm to compute the marginal distribution
 * over a set of query variables. It first clamps the evidence variables, then determines an
 * elimination order, and iteratively multiplies and marginalizes factors to reduce the graph.
 *
 * @param fg The input factor graph.
 * @param query_vars A vector of indices for the variables whose marginal distribution is to be calculated.
 * @param evidence_vars A vector of indices for the evidence variables.
 * @param evidence_values A vector of values corresponding to the evidence variables.
 * @param logger A reference to the `dai::LibLogger` for logging progress and results.
 * @return A `dai::Factor` representing the joint marginal probability distribution over the query variables.
 */
dai::Factor variableElimination(dai::FactorGraph fg, std::vector<unsigned int> query_vars, std::vector<unsigned int> evidence_vars,
    std::vector<unsigned int> evidence_values, LibLogger& logger) {

    logger.log(LogLevel::DEBUG, "**** Starting Variable Elimination ****");

    // Generate a suitable variable elimination order
    greedyVariableElimination::eliminationCostFunction ec = eliminationCost_MinFill;
    logger.log(LogLevel::DEBUG, "Heuristic Used: " + functionNames.at(ec));

    vector<size_t> elimOrder = getUnconstrainedElimOrder(fg, greedyVariableElimination(ec), query_vars, evidence_vars);
    logger.log(LogLevel::DEBUG, "Elimination Order: " + vecToString(elimOrder));

    // Calculate and log treewidth for the given elimination order
    std::pair<size_t, BigInt> data = getElimOrderTreeWidth(fg, elimOrder);
    logger.log(LogLevel::DEBUG, "Treewidth: " + std::to_string(data.first));
    logger.log(LogLevel::DEBUG, "Maximum States in a single cluster: " + data.second.get_str());

    // Clamp evidence variables
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < evidence_vars.size(); ++i) {
        fg.clampReduce(evidence_vars[i], evidence_values[i], false);
    }
    auto end = std::chrono::steady_clock::now();
    logger.log(LogLevel::DEBUG, "Clamping evidence: " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) + "ns");

    return elim_vars(fg, logger, elimOrder);
}

/**
 * @brief Finds the state with the maximum probability in a factor.
 *
 * This function iterates through all possible states of a `dai::Factor` and finds the one with the
 * highest probability. It then returns the corresponding variable assignments. This is useful for
 * finding the Maximum a Posteriori (MAP) assignment from a joint probability distribution factor.
 *
 * @param factor The `dai::Factor` to be searched.
 * @param logger A reference to the `dai::LibLogger` for logging the result.
 * @return A vector of `unsigned long int` representing the values of the variables for the MAP assignment.
 */
tuple<map<Var, unsigned long>, double> extractMax(dai::Factor factor, LibLogger& logger) {

    auto start = std::chrono::steady_clock::now();
    double max = 0.0;
    int entry = 0;
    for (int i = 0; i < factor.nrStates(); i++) {
        if (factor.p()[i] > max) {
            max = factor.p()[i];
            entry = i;
        }
    }

    // Transform index to a map of <Var, value> pairs
    std::map<dai::Var, size_t> mapValues = dai::calcState(factor.vars(), entry);
    auto end = std::chrono::steady_clock::now();



    // Log result and print to terminal
    //logger.log(LogLevel::INFO, "MAP Assignment: ");

    std::string result = "Map Assignment: ";
    for (const auto &entry : mapValues)
    {
        //logger.log(LogLevel::INFO, "X" + std::to_string(entry.first.label()) + "=" + std::to_string(entry.second) + ", ");
        result += "X" + std::to_string(entry.first.label()) + "=" + std::to_string(entry.second) + ", ";
    }
    result += " with probability " + std::to_string(max);
    logger.log(LogLevel::INFO, result);
    //logger.log(LogLevel::INFO, " with probability " + std::to_string(max));


    // Should return the vector of variable assignments and max
    return tuple(mapValues, max);
}

/**
 * @brief Computes the Maximum a Posteriori (MAP) assignment using Variable Elimination.
 *
 * This function calculates the MAP assignment for a set of hypothesis (MAP) variables given a set of
 * evidence variables. It uses a constrained variable elimination approach where non-MAP variables are
 * summed out, while MAP variables are maximized out.
 *
 * @param fg The input factor graph.
 * @param map_vars A vector of indices for the MAP variables.
 * @param evidence_vars A vector of indices for the evidence variables.
 * @param evidence_values A vector of values for the evidence variables.
 * @param mapList A boolean indicating whether to list all map probabilities during computation (for debugging).
 * @param logger A reference to the `dai::LibLogger` for logging progress and results.
 * @return A `dai::Factor` representing the factor of the MAP variables. Call extractMax() on this factor to extract MAP.
 */

tuple<map<Var, unsigned long>, double> get_map_ve(dai::FactorGraph fg, std::vector<unsigned int> map_vars,
                                                  std::vector<unsigned int> evidence_vars,
                                                  std::vector<unsigned int> evidence_values, bool mapList,
                                                  LibLogger& logger) {

    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < evidence_vars.size(); ++i) {
        fg.clampReduce(evidence_vars[i], evidence_values[i], false);
    }
    auto end = std::chrono::steady_clock::now();

    // Get constrained elimination order using a heuristic
    greedyVariableElimination::eliminationCostFunction ec = eliminationCost_MinFill;
    logger.log(LogLevel::INFO, "Heuristic Used: " + functionNames.at(ec));

    vector<size_t> constrainedElimOrder = getConstrainedElimOrder(fg, greedyVariableElimination(ec), map_vars, evidence_vars);
    logger.log(LogLevel::INFO, "Elimination Order: " + vecToString(constrainedElimOrder));

    // Calculate treewidth for logging purposes
    std::pair<size_t, BigInt> data = getElimOrderTreeWidth(fg, constrainedElimOrder);
    logger.log(LogLevel::INFO, "Treewidth: " + std::to_string(data.first));
    logger.log(LogLevel::INFO, "Maximum States in a single cluster: " + data.second.get_str());

    auto map_factor = elim_vars(fg, logger, constrainedElimOrder);


    return extractMax(map_factor, logger);
}

/**
 * @brief Computes the Maximum a Posteriori (MAP) assignment using the Junction Tree algorithm.
 *
 * This function uses the Junction Tree (JT) algorithm to compute the MAP assignment for a set of
 * hypothesis variables. It first clamps evidence and then uses a `JTree` object to perform
 * sum-product inference. The marginal distribution over the hypothesis variables is then calculated,
 * and the state with the maximum probability is extracted to find the MAP assignment.
 *
 * @param fg The input factor graph.
 * @param hypothesis_vars A vector of indices for the hypothesis (MAP) variables.
 * @param evidence_vars A vector of indices for the evidence variables.
 * @param evidence_values A vector of values for the evidence variables.
 * @param mapList A boolean indicating whether to list all map probabilities during computation (for debugging).
 * @param logger A reference to the `dai::LibLogger` for logging progress and results.
 * @return A vector of `unsigned long int` representing the values of the variables for the MAP assignment.
 */
std::vector<unsigned long int> get_map_jt(dai::FactorGraph fg, std::vector<unsigned int> hypothesis_vars, std::vector<unsigned int> evidence_vars,
    std::vector<unsigned int> evidence_values, bool mapList, dai::LibLogger& logger) {
    std::vector<unsigned long int> map;
    std::vector<unsigned long int> h_vars(begin(hypothesis_vars), end(hypothesis_vars));

    dai::VarSet hypSet = fg.inds2vars(h_vars);

    // Clamp evidence
    auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < evidence_vars.size(); ++i) {
        fg.clamp(evidence_vars[i], evidence_values[i], false);
    }
    auto end = std::chrono::steady_clock::now();
    DEBUG(std::cout << "Clamping evidence " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;)

    // Initialize and run the Junction Tree algorithm
    start = std::chrono::steady_clock::now();
    dai::PropertySet opts;
    dai::JTree jt = dai::JTree(fg, opts("updates", std::string("HUGIN"))("inference", std::string("SUMPROD")));

    logger.log(LogLevel::INFO, "[MAP] Heuristic Used: " + jt.Heuristic);
    logger.log(LogLevel::INFO, "[MAP] Elimination Order: " + vecToString(jt.ElimOrder));
    logger.log(LogLevel::INFO, "MAP] Treewidth: " + std::to_string(jt.MaxCluster));
    logger.log(LogLevel::INFO, "[MAP] Maximum States in a single cluster: " + jt.MaxStates.get_str());

    jt.init();
    jt.run();
    end = std::chrono::steady_clock::now();
    auto jtInitRunTime = end - start;
    logger.log(LogLevel::INFO, "[MAP] JT run " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtInitRunTime).count()) + " ns");

    // Calculate marginal distribution over hypothesis variables
    start = std::chrono::steady_clock::now();
    dai::Factor hypFact = jt.calcMarginal(hypSet);
    end = std::chrono::steady_clock::now();
    auto jtMarginaliseTime = end - start;
    logger.log(LogLevel::INFO, "[MAP] Marginal time " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtMarginaliseTime).count()) + " ns");

    // Find the state with the maximum probability
    double max = 0.0;
    int entry = 0;
    for (int i = 0; i < hypFact.nrStates(); ++i) {
        if (hypFact.p()[i] > max) {
            max = hypFact.p()[i];
            entry = i;
        }
    }

    // Transform index to <Var, value> pairs
    std::map<dai::Var, size_t> mapValues = dai::calcState(hypFact.vars(), entry);
    end = std::chrono::steady_clock::now();
    auto jtMaximiseTime = end - start;

    logger.log(LogLevel::INFO, "[MAP] MAP time " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtMaximiseTime).count()) + " ns");
    auto totalTime = jtInitRunTime + jtMarginaliseTime + jtMaximiseTime;
    logger.log(LogLevel::INFO, "[MAP] Total JT time: " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(totalTime).count()) + " ns");

    // Convert the map to a vector of values
    for (auto const& i : mapValues) {
        map.push_back(i.second);
    }
    logger.log(LogLevel::INFO, "[MAP] Instantiation: " + vecToString(map) + " has probability " + std::to_string(max));

    return map;
}

} // namespace dai