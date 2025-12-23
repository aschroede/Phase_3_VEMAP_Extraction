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


std::pair<size_t,BigInt> calculateEliminationWidth(dai::FactorGraph fg, vector<size_t> elimOrder){

    // Get list of factors in this form ([A], [B], [A, B, C], [D], [E, B], [F, C, E, D])


    // keep track of the largest factor constructed
    std::uint16_t maxVars = 0;
    dai::BigInt maxStates = 0;

    // Perform Variable Elimination
    std::vector<dai::Factor> factors = fg.factors();

    std::vector<dai::VarSet> factorList;

    for(int j=0; j<factors.size(); j++){

        dai::VarSet vars = (factors[j].vars());

        factorList.push_back(vars);
    }

    for (int i=0; i < elimOrder.size(); i++){

        std::cout << "Eliminate: " << elimOrder[i] << endl;

        // Find all factors that contain the variable to be eliminated

        std::vector<dai::VarSet> toMultiply;
        for(int j=0; j<factorList.size(); j++){

            dai::VarSet vars = factorList[j];
            for (auto it = vars.begin(); it != vars.end(); ++it){

                if(it->label() == elimOrder[i]){

                    toMultiply.push_back(factorList[j]);
                }
            }
        }

        // "Multiply" the factors by taking the union of the variables in toMultiply
        dai::VarSet newFactor = toMultiply[0];
        if(toMultiply.size() > 1){

            for (int i = 1; i<toMultiply.size(); i++){
                newFactor.operator|=(toMultiply[i]);
            }
        }

        // Check the size of the new factor that was created by multiplying all the other factors
        if(newFactor.nrStates() > maxStates){
            maxStates = newFactor.nrStates();
        }
        if(newFactor.size() > maxVars){
            maxVars = newFactor.size();
        }



        // "Sum out"/"Maximise Out" the variable to be eliminated by doing set subtraction
        // The operator/=() takes a Var as an argument, not an index
        // How can I get the elimination order in terms of variables rather than indices?
        // Is there an index to var function somewhere?
        // There is the indices to var function
        // Need to remove the variable to be eliminated specified by elimOrder[i]. But elimOrder[i] is just a number
        // and operator/=(const Var &t) takes a variable. Not sure how to get a variable from the variable number

        dai::Var varToRemove = fg.var(elimOrder[i]);
        newFactor.operator/=(varToRemove);

        // Now put the newFactor in the list of factors and remove the old factors that were multiplied together.

        for (auto it = toMultiply.begin(); it != toMultiply.end(); ++it){
                factorList.erase(std::find_if(factorList.begin(), factorList.end(), [&](VarSet const& f){ return f == *it; }));
            }

        factorList.push_back(newFactor);

    }

    // After all variables have been eliminated, then multiply remaining factors together

    // Multiply remaining factors
    dai::VarSet newFactor = factorList[0];
    if(factorList.size() > 1){

        for (int i = 1; i<factorList.size(); i++){
            newFactor.operator|=(factorList[i]);
        }
    }

    // Check the size of the new factor that was created by multiplying all the other factors
    if(newFactor.nrStates() > maxStates){
        maxStates = newFactor.nrStates();
    }
    if(newFactor.size() > maxVars){
        maxVars = newFactor.size();
    }

    return make_pair(maxVars, maxStates);
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
 * @param f The elimination heuristic object.
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

template<class EliminationChoice>
vector<size_t> getConstrainedElimOrder(const FactorGraph &fg, EliminationChoice f, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars  ){

    // Create cluster graph from factor graph
    ClusterGraph cl( fg, true );

    // Now get constrained tree width
    std::set<size_t> nonMapVarindices;
    std::set<size_t> MapVarindices;

    for( size_t i = 0; i < cl.vars().size(); ++i ){

        auto it = std::find(evidence_vars.begin(), evidence_vars.end(), i);

        // // Only add non-evidence variables
        if(it == evidence_vars.end()){

            auto it = std::find(map_vars.begin(), map_vars.end(), i);

            // If not in map variables add it
            if (it == map_vars.end()) {
                nonMapVarindices.insert( i );
            }
            else{
                MapVarindices.insert( i );
            }
        }
    }

    vector<size_t> elimOrder;

    // Load up non map vars first
    while( !nonMapVarindices.empty() ) {
        size_t i = f( cl, nonMapVarindices );
        VarSet Di = cl.elimVar( i );
        elimOrder.push_back(i);
        nonMapVarindices.erase( i );
    }

    // Then load map vars
    while( !MapVarindices.empty() ) {
        size_t i = f( cl, MapVarindices );
        VarSet Di = cl.elimVar( i );
        elimOrder.push_back(i);
        MapVarindices.erase( i );
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

std::vector<unsigned long int> get_map_ve(dai::FactorGraph fg, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars,
                       std::vector<unsigned int> evidence_values, bool mapList, LibLogger &logger)
{

    // Clamp evidence
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < evidence_vars.size(); i++){
        fg.clampReduce(evidence_vars[i], evidence_values[i], false);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Clamping evidence " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

    double num_states = 1;
    double num_hyp_states = 1;
    for (Var var : fg.vars()){

        if(std::find(evidence_vars.begin(), evidence_vars.end(), var.label()) == evidence_vars.end()){

            num_states *= var.states();
        }

        if(std::find(map_vars.begin(), map_vars.end(), var.label()) != map_vars.end()){

            num_hyp_states *= var.states();
        }

    }

    // Generate constrained variable elimination order. Don't include evidence variables
    greedyVariableElimination::eliminationCostFunction ec = eliminationCost_MinFill;

    // Find name of heuristic
    for (const auto& entry: functionNames){
        if (entry.first == ec){
            logger.log(LogLevel::INFO, "[MAP] Heuristic Used: " + entry.second);
            break;
        }
    }

    // Get constrained elimination order using heuristic
    vector<size_t> constrainedElimOrder = getConstrainedElimOrder(fg, greedyVariableElimination( ec ), map_vars, evidence_vars);
    logger.log(LogLevel::INFO, "[MAP] Elimination Order: " + vecToString(constrainedElimOrder));

    //boundTreewidth1(fg, eliminationCost_MinFill, 0);
    std::pair<size_t,BigInt> data = calculateEliminationWidth(fg, constrainedElimOrder);
    logger.log(LogLevel::INFO, "Treewidth: " + std::to_string(data.first));
    logger.log(LogLevel::INFO, "Maximum States in a single cluster: " + data.second.get_str());

    int eliminationCount = 0;
    // Perform Variable Elimination
    std::vector<dai::Factor> factors = fg.factors();
    // Make a stack of factors to keep around so that we can backtrack and find the MAP instantiation
    std::stack<dai::Factor> backtrack_factors;

	for (Factor factor : factors){
		logger.log(LogLevel::DEBUG, "[MAP] Factor: " + factor.toStringNice());
	}

    for (int i=0; i < constrainedElimOrder.size(); i++){

        std::cout << "Eliminate: " << constrainedElimOrder[i] << endl;
		logger.log(LogLevel::DEBUG, "Eliminating: " + constrainedElimOrder[i]);

        // Find all factors fk that mention variable pi[i]
        // f <- Then multiply those factors together

        // Could also use findFactor and findVars in factorgraph
        std::vector<dai::Factor> toMultiply;

        for(int j=0; j<factors.size(); j++){

            dai::VarSet vars = (factors[j].vars());

            for (auto it = vars.begin(); it != vars.end(); ++it){

                if(it->label() == constrainedElimOrder[i]){

                    toMultiply.push_back(factors[j]);
                }
            }
        }

		for (Factor factor : toMultiply){
			logger.log(LogLevel::DEBUG, "Factors to multiply: " + factor.toStringNice());
		}

        // If more than one factor found with the variable to be eliminated,
        // then multiply together the factors
        dai::Factor newFactor = toMultiply[0];
        if(toMultiply.size() > 1){

            for (int i = 1; i<toMultiply.size(); i++){

                newFactor *= toMultiply[i];
            }
        }

        // Check if variable to eliminate is a MAP variable
        // If it is make sure to keep the final factor for this variable prior to maximization available
        // for backtracking to find assignment later.
        if (std::find(map_vars.begin(), map_vars.end(), constrainedElimOrder[i]) != map_vars.end()){

            // Store this factor for backtracking
            backtrack_factors.push(newFactor);

            // If variable pi(i) is a map variable then
            // fi <- max out pi(i) from f
            dai::VarSet vars = newFactor.vars();

            dai::VarSet varsToKeep;
            for (auto it = vars.begin(); it != vars.end(); ++it){

                if(it->label() == constrainedElimOrder[i]){
                    continue;
                }
                else{
                    varsToKeep.insert(*it);
                }
            }
            newFactor = newFactor.maxMarginal(varsToKeep,  false);
        }

        //Else fi <- sum out pi(i) from f
        else{

            dai::VarSet vars = newFactor.vars();

            dai::VarSet varsToKeep;
            for (auto it = vars.begin(); it != vars.end(); ++it){

                if(it->label() == constrainedElimOrder[i]){
                    continue;
                }
                else{
                    varsToKeep.insert(*it);
                }
            }
            newFactor = newFactor.marginal(varsToKeep, false);
        }

        // Replace all factors fk in the set of factor S by factor fi
        // Remove factors to multiply and replace with newFactor
        for (auto it = toMultiply.begin(); it != toMultiply.end(); ++it){
            factors.erase(std::find_if(factors.begin(), factors.end(), [&](Factor const& f){ return f == *it; }));
        }
        factors.push_back(newFactor);

        std::cout << "Eliminated " << ++eliminationCount << "/" << constrainedElimOrder.size() << endl;

    }

    // Now backtrack to get the instantiation
    std::map<size_t, size_t> assignmentByLabel; // map from Var label -> chosen value
    while (!backtrack_factors.empty()){
        dai::Factor factor = backtrack_factors.top();
        backtrack_factors.pop();

		logger.log(LogLevel::DEBUG, "Backtrack factor: " + factor.toStringNice());

        // Find which MAP variable in this factor still needs assignment
        size_t targetVarLabel = std::numeric_limits<size_t>::max();
        for (auto it = factor.vars().begin(); it != factor.vars().end(); ++it) {
            size_t lbl = it->label();
            if (std::find(map_vars.begin(), map_vars.end(), lbl) != map_vars.end() && assignmentByLabel.find(lbl) == assignmentByLabel.end()) {
                targetVarLabel = lbl;
                break;
            }
        }

        // If we couldn't find an unassigned MAP var in this factor, skip it
        if (targetVarLabel == std::numeric_limits<size_t>::max()){
            logger.log(LogLevel::ERROR, "[MAP] Variable label does not exist");
            continue;
        }

        // Iterate over all entries in the factor and pick the value for targetVarLabel
        // that maximizes the factor's probability while being consistent with already chosen assignments
		// for variables that are present in the factor.
        double bestP = -2.0;
        size_t bestVal = 0;

        for (size_t idx = 0; idx < factor.nrStates(); ++idx) {
            // calcState returns a map<Var, size_t>
            std::map<dai::Var, size_t> stateMap = dai::calcState(factor.vars(), idx);

            // Check consistency with already chosen assignments
            bool consistent = true;
			// a is each of the assignments made already
			// should iterate through each of the variables in the factor itself and check against their assignments
			// in assignmentByLabel

            for (const auto& v : factor.vars()) {
                size_t lbl = v.label();

                // Only check variables that are already assigned
                auto itAssign = assignmentByLabel.find(lbl);
                if (itAssign == assignmentByLabel.end())
                    continue;

                // Look up this variable in the stateMap
                auto itState = std::find_if(
                    stateMap.begin(), stateMap.end(),
                    [&](const std::pair<dai::Var, size_t>& p) {
                        return p.first.label() == lbl;
                    });

                if (itState == stateMap.end() || itState->second != itAssign->second) {
                    consistent = false;
                    break;
                }
            }

			if (!consistent)
                continue;

            // get value for target var in this state
            auto itTarget = std::find_if(stateMap.begin(), stateMap.end(), [&](const std::pair<dai::Var, size_t> &p) {
                return p.first.label() == targetVarLabel;
            });
            if (itTarget == stateMap.end()){
                logger.log(LogLevel::ERROR, "[MAP] Variable label does not exist");
                continue; // shouldn't happen
            }

            double p = factor.p()[idx];
            if (p > bestP) {
                bestP = p;
                bestVal = itTarget->second;
            }
        }

        // Record chosen value (if nothing matched, default to 0)
        assignmentByLabel[targetVarLabel] = bestVal;
    }

    // Assemble final MAP instantiation in the order of map_vars
    std::vector<unsigned long int> mapInstantiation;
    for (size_t lbl : map_vars) {
        auto it = assignmentByLabel.find(lbl);
        if (it != assignmentByLabel.end())
            mapInstantiation.push_back(it->second);
        else
            mapInstantiation.push_back(0); // fallback if not assigned
    }

    logger.log(LogLevel::INFO, "[MAP-BT] Final instantiation: " + vecToString(mapInstantiation));
    return mapInstantiation;
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