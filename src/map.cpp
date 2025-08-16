/*  This file is part of libDAI - http://www.libdai.org/
 *
 *  Copyright (c) 2006-2011, The libDAI authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
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

using namespace std;

void LogFactors(std::vector<dai::Factor> &factors, dai::LibLogger &logger)
{
    for (dai::Factor &factor : factors)
    {
        logger.log(LogLevel::DEBUG, factor.toStringNice());
    }
}

// TODO: need to figure out what this does...
std::pair<size_t,BigInt> getTreeWidth( const FactorGraph &fg, greedyVariableElimination::eliminationCostFunction fn, size_t maxStates ) {
    // Create cluster graph from factor graph
    ClusterGraph _cg( fg, true );

    ClusterGraph ElimVec;
    std::vector<size_t> ElimOrder;

    // Obtain elimination sequence
    tie(ElimVec, ElimOrder) = _cg.VarElim( greedyVariableElimination( fn ), maxStates );
    std::vector<dai::VarSet> ElimCliques = ElimVec.eraseNonMaximal().clusters();

    // Calculate treewidth
    size_t treewidth = 0;
    BigInt nrstates = 0.0;
    for( size_t i = 0; i < ElimCliques.size(); i++ ) {
        if( ElimCliques[i].size() > treewidth )
            treewidth = ElimCliques[i].size();
        BigInt s = ElimCliques[i].nrStates();
        if( s > nrstates )
            nrstates = s;
    }

    std::cout << "Unconstrained tree width (BoundTreeWidth): " << treewidth << std::endl;
    std::cout << "Unconstrained state number (BoundTreeWidth): " << nrstates << std::endl;

    return make_pair(treewidth, nrstates);
}

template<class EliminationChoice>
vector<size_t> getUnconstrainedElimOrder(const FactorGraph &fg, EliminationChoice f, std::vector<unsigned int> query_vars, std::vector<unsigned int> evidence_vars  ){

    // Create cluster graph from factor graph
    ClusterGraph cl( fg, true );


    // Now get unconstrained tree width
    std::set<size_t> nonQueryVarindices;

    for( size_t i = 0; i < cl.vars().size(); ++i ){

        auto it = std::find(evidence_vars.begin(), evidence_vars.end(), i);

        // // Only add non-evidence variables
        if(it == evidence_vars.end()){

            auto it = std::find(query_vars.begin(), query_vars.end(), i);

            // If not in query variables add it
            if (it == query_vars.end()) {
                nonQueryVarindices.insert( i );
            }
        }
    }

    vector<size_t> elimOrder;
 
    // Load up non map vars first
    while( !nonQueryVarindices.empty() ) {
        size_t i = f( cl, nonQueryVarindices );
        VarSet Di = cl.elimVar( i );
        elimOrder.push_back(i);
        nonQueryVarindices.erase( i );
    }

    return elimOrder;

}

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

std::pair<size_t,BigInt> simulateVariableElim(dai::FactorGraph fg, vector<size_t> elimOrder){

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

dai::Factor variableElimination(dai::FactorGraph fg, std::vector<unsigned int> query_vars, std::vector<unsigned int> evidence_vars,
        std::vector<unsigned int> evidence_values, LibLogger &logger){

    // Generate constrained variable elimination order. Don't include evidence variables
    greedyVariableElimination::eliminationCostFunction ec = eliminationCost_MinFill;

    // Find name of heuristic to log it
    for (const auto& entry: functionNames){
        if (entry.first == ec){
            logger.log(LogLevel::INFO, "Heuristic Used: " + entry.second);
            break;
        }
    }

    // Get constrained elimination order using heuristic
    vector<size_t> constrainedElimOrder = getUnconstrainedElimOrder(fg, greedyVariableElimination( ec ), query_vars, evidence_vars);
    logger.log(LogLevel::INFO, "Elimination Order: " + vecToString(constrainedElimOrder));

    // Calculate treewidth of elim order
    std::pair<size_t,BigInt> data = simulateVariableElim(fg, constrainedElimOrder);
    logger.log(LogLevel::INFO, "Treewidth: " + std::to_string(data.first));
    logger.log(LogLevel::INFO, "Maximum States in a single cluster: " + data.second.get_str());
    
    std::vector<dai::Factor> factors = fg.factors();
    logger.log(LogLevel::DEBUG, "Initial set of factors:");
    LogFactors(factors, logger);

    // Clamp evidence
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < evidence_vars.size(); i++){
        fg.clampReduce(evidence_vars[i], evidence_values[i], false);
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Clamping evidence " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

    // Log reduced factors
    factors = fg.factors();
    logger.log(LogLevel::DEBUG, "Factors after applying evidence:");
    LogFactors(factors, logger);


    // Perform Variable Elimination
    int eliminationCount = 0;
    for (int i=0; i < constrainedElimOrder.size(); i++){

        logger.log(LogLevel::DEBUG, "Variable to Eliminate: " + std::to_string(constrainedElimOrder[i]));

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

        logger.log(LogLevel::DEBUG, "Factors to multiply: ");
        LogFactors(toMultiply, logger);

        // If more than one factor found with the variable to be eliminated,
        // then multiply together the factors
        dai::Factor newFactor = toMultiply[0];
        if(toMultiply.size() > 1){

            for (int i = 1; i<toMultiply.size(); i++){

                newFactor *= toMultiply[i];
            }
        }

        logger.log(LogLevel::DEBUG, "Multiplication Result: ");
        logger.log(LogLevel::DEBUG, newFactor.toStringNice());

        // sum out pi(i) from f
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

        logger.log(LogLevel::DEBUG, "After marginalising out " + std::to_string(constrainedElimOrder[i]));
        logger.log(LogLevel::DEBUG, newFactor.toStringNice());

        // Replace all factors fk in the set of factor S by factor fi
        // Remove factors to multiply and replace with newFactor
        for (auto it = toMultiply.begin(); it != toMultiply.end(); ++it){
            factors.erase(std::find_if(factors.begin(), factors.end(), [&](Factor const& f){ return f == *it; }));
        }
        factors.push_back(newFactor);

        logger.log(LogLevel::DEBUG, "New List of Factors:");
        LogFactors(factors, logger);
    }

    logger.log(LogLevel::DEBUG, "Completed marginalisation. Multiplying remaining factors...");

    // Multiply remaining factors
    dai::Factor newFactor = factors[0];
    if(factors.size() > 1){
        for (int i = 1; i<factors.size(); i++){
            newFactor *= factors[i];
        }
    }

    logger.log(LogLevel::DEBUG, newFactor.toStringNice());
    newFactor.normalize();
    logger.log(LogLevel::DEBUG, "Normalized Result: ");
    logger.log(LogLevel::INFO, "\n" + newFactor.toStringNice());

    return newFactor;
}


std::vector<unsigned long int> extractMax(dai::Factor factor, LibLogger &logger){

    std::vector<unsigned long int> map;
	auto start = std::chrono::steady_clock::now();
	double max = 0.0;
	int entry = 0; 
    for (int i = 0; i < factor.nrStates(); i++)
    {
        // if (mapList)
        // {
        //     std::cout << "entry ";
        //     for (auto const& j: dai::calcState(hypFact.vars(), i))
        //         std::cout << j.second;
        //     std::cout << " has probability " << hypFact.p()[i] << std::endl;
        // }
		if (factor.p()[i] > max)
		{
    	    max = factor.p()[i];
			entry = i;
		}
    }

	// transform index to map of <Var, value> pairs
	std::map<dai::Var, size_t> mapValues = dai::calcState(factor.vars(), entry);
	auto end = std::chrono::steady_clock::now();
    auto jtMaximiseTime = end-start;
	// ofs << "[MAP] MAP time " << std::chrono::duration_cast<std::chrono::nanoseconds>(jtMaximiseTime).count() << " ns" << std::endl;

    //auto totalTime = jtInitRunTime+jtMarginaliseTime+jtMaximiseTime;
    //ofs << "[MAP] Total JT time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(totalTime).count() << " ns" << std::endl;

	// now set map accordingly to the values in hypothesis_vars
	for (auto const& i: mapValues)
	{
		map.push_back(i.second);
	}
    logger.log(LogLevel::INFO, "Map instantiation " + vecToString(map) + " has probability " + std::to_string(max));

	return map;
}


dai::Factor get_map_ve(dai::FactorGraph fg, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars,
                       std::vector<unsigned int> evidence_values, bool mapList, LibLogger &logger)
{

    try{

        // TODO: PruneNetwork

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
        std::pair<size_t,BigInt> data = simulateVariableElim(fg, constrainedElimOrder);
        logger.log(LogLevel::INFO, "Treewidth: " + std::to_string(data.first));
        logger.log(LogLevel::INFO, "Maximum States in a single cluster: " + data.second.get_str());
        
        int eliminationCount = 0;
        // Perform Variable Elimination
        std::vector<dai::Factor> factors = fg.factors();

        // for (dai::Factor& factor : factors) {
        //     std::cout << factor.p().size() << " " << factor.i().size() << endl;
        // }

        for (int i=0; i < constrainedElimOrder.size(); i++){

            std::cout << "Eliminate: " << constrainedElimOrder[i] << endl;

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

            // If more than one factor found with the variable to be eliminated,
            // then multiply together the factors
            dai::Factor newFactor = toMultiply[0];
            if(toMultiply.size() > 1){

                for (int i = 1; i<toMultiply.size(); i++){

                    newFactor *= toMultiply[i];
                }
            }

            // Check if variable to eliminate is a MAP variable
            if (std::find(map_vars.begin(), map_vars.end(), constrainedElimOrder[i]) != map_vars.end()){
                
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
                newFactor = newFactor.maxMarginalTransparent(varsToKeep,  false);
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

            // Replace all factors  fk in the set of factor S by factor fi
            // Remove factors to multiply and replace with newFactor
            for (auto it = toMultiply.begin(); it != toMultiply.end(); ++it){
                factors.erase(std::find_if(factors.begin(), factors.end(), [&](Factor const& f){ return f == *it; }));
            }

            //printAllMemStats();

            factors.push_back(newFactor);

            std::cout << "Eliminated " << ++eliminationCount << "/" << constrainedElimOrder.size() << endl;

            std::cout << sizeof(factors) << std::endl;
            

        }

        // Multiply remaining factors
        dai::Factor newFactor = factors[0];
        if(factors.size() > 1){
            for (int i = 1; i<factors.size(); i++){

                newFactor *= factors[i];
            }
        }
        std::cout << "Returning last factor" << std::endl;
        return newFactor;
    }
    
    catch( Exception &e ) {
        // Save a copy of /proc/self/maps to a file
        std::ofstream mapsFile("proc_self_maps_copy.txt");
        if (mapsFile.is_open()) {
            std::ifstream maps("/proc/self/maps");
            if (maps.is_open()) {
                mapsFile << maps.rdbuf();
                maps.close();
            } else {
                std::cerr << "Failed to open /proc/self/maps for reading." << std::endl;
            }

            mapsFile.close();
        } else {
            std::cerr << "Failed to open proc_self_maps_copy.txt for writing." << std::endl;
        }
    }
}

std::vector<unsigned long int> get_map_jt(dai::FactorGraph fg, std::vector<unsigned int> hypothesis_vars, std::vector<unsigned int> evidence_vars,
                                          std::vector<unsigned int> evidence_values, bool mapList, dai::LibLogger& logger)
{
	// returns the map, the joint value assignment to the hypothesis vars that has maximum posterior probability given the evidence
	// while marginalizing over the (relevant) intermediate variables. As libDAI has no MAP function we just compute the distribution
	// over the MAP variables and select the state with maximum value from the posterior, which is the MAP assignment

	// when used in MFE function, the evidence is the actual 'real' evidence plus the sampled irrelevant intermediate nodes

    std::vector<unsigned long int> map;
    std::vector<unsigned long int> h_vars(begin(hypothesis_vars), end(hypothesis_vars));    // needs cast to long

	dai::VarSet hypSet = fg.inds2vars(h_vars);

	auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < evidence_vars.size(); i++)
    {
        fg.clamp(evidence_vars[i], evidence_values[i], false);
    }
	auto end = std::chrono::steady_clock::now();
	DEBUG(std::cout << "Clamping evidence " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;)

	start = std::chrono::steady_clock::now();
    dai::PropertySet opts;
    dai::JTree jt = dai::JTree(fg, opts("updates",std::string("HUGIN"))("inference",std::string("SUMPROD")));

    logger.log(LogLevel::INFO, "[MAP] Heuristic Used: " + jt.Heuristic);
    logger.log(LogLevel::INFO, "[MAP] Elimination Order: " + vecToString(jt.ElimOrder));
    logger.log(LogLevel::INFO, "MAP] Treewidth: " + jt.MaxCluster);
    logger.log(LogLevel::INFO, "[MAP] Maximum States in a single cluster: " + jt.MaxStates.get_str());


    jt.init();
    jt.run();
	end = std::chrono::steady_clock::now();
    auto jtInitRunTime = end-start;

    logger.log(LogLevel::INFO, "[MAP] JT run " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtInitRunTime).count()) + " ns");


	start = std::chrono::steady_clock::now();
	dai::Factor hypFact = jt.calcMarginal(hypSet);
	end = std::chrono::steady_clock::now();
    auto jtMarginaliseTime = end-start;
    logger.log(LogLevel::INFO, "[MAP] Marginal time " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtMarginaliseTime).count()) + " ns");

	// find element with maximum value ( = MAP explanation)
	start = std::chrono::steady_clock::now();
	double max = 0.0;
	int entry = 0; 
    for (int i = 0; i < hypFact.nrStates(); i++)
    {
        if (mapList)
        {
            std::cout << "entry ";
            for (auto const& j: dai::calcState(hypFact.vars(), i))
                std::cout << j.second;
            std::cout << " has probability " << hypFact.p()[i] << std::endl;
        }
		if (hypFact.p()[i] > max)
		{
    	    max = hypFact.p()[i];
			entry = i;
		}
    }

	// transform index to map of <Var, value> pairs
	std::map<dai::Var, size_t> mapValues = dai::calcState(hypFact.vars(), entry);
	end = std::chrono::steady_clock::now();
    auto jtMaximiseTime = end-start;

    logger.log(LogLevel::INFO, "[MAP] MAP time " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(jtMaximiseTime).count()) + " ns");

    auto totalTime = jtInitRunTime+jtMarginaliseTime+jtMaximiseTime;
    logger.log(LogLevel::INFO, "[MAP] Total JT time: " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(totalTime).count()) + " ns");


	// now set map accordingly to the values in hypothesis_vars
	for (auto const& i: mapValues)
	{
		map.push_back(i.second);
	}
    logger.log(LogLevel::INFO, "[MAP] Instantiation: " + vecToString(map) + " has probability " + std::to_string(max));

	return map;
}

}
