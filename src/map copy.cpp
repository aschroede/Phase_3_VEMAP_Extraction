// /*  This file is part of libDAI - http://www.libdai.org/
//  *
//  *  Copyright (c) 2006-2011, The libDAI authors. All rights reserved.
//  *
//  *  Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
//  */

// #include <iostream>
// #include <map>
// #include <dai/alldai.h>  // Include main libDAI header file
// #include <chrono>
// #include <stack>

// // comment for production mode, uncomment for debug messages
// #define DEBUGMODE

// #ifdef DEBUGMODE
// 	#define DEBUG(a) a;
// #else
// 	#define DEBUG(a) ;
// #endif	

// namespace dai {

// using namespace std;

// template<class EliminationChoice>
// vector<size_t> getConstrainedElimOrder(const FactorGraph &fg, EliminationChoice f, std::vector<unsigned int> map_vars){

//     // Create cluster graph from factor graph
//     ClusterGraph cl( fg, true );

//     // Construct set of variable indices for non-Map vars
//     std::set<size_t> nonMapVarindices;
//     std::set<size_t> MapVarindices;

//     for( size_t i = 0; i < cl.vars().size(); ++i ){

//         auto it = std::find(map_vars.begin(), map_vars.end(), i);

//         // If not in map variables add it
//         if (it == map_vars.end()) {
//             nonMapVarindices.insert( i );
//         }
//         else{
//             MapVarindices.insert( i );
//         }
//     }

//     vector<size_t> elimOrder;

//     // Load up non map vars first
//     while( !nonMapVarindices.empty() ) {
//         size_t i = f( cl, nonMapVarindices );
//         elimOrder.push_back(i);
//         nonMapVarindices.erase( i );
//     }

//     // Then load map vars
//     while( !MapVarindices.empty() ) {
//         size_t i = f( cl, MapVarindices );
//         elimOrder.push_back(i);
//         MapVarindices.erase( i );
//     }

//     return elimOrder;

// }


// dai::Factor get_map(dai::FactorGraph fg, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars,
//         std::vector<unsigned int> evidence_values, bool mapList){
        
//         // TODO: PruneNetwork

//         std::vector<std::pair<Var, dai::Real>> _instantiation;

//         // Generate constrained variable elimination order (pi)
//         vector<size_t> constrainedElimOrder = getConstrainedElimOrder(fg, greedyVariableElimination( eliminationCost_MinFill), map_vars);

//         std::cout << "Elimination Order: " << constrainedElimOrder << endl;
//         std::cout << "Number of vars: " << constrainedElimOrder.size() << endl;
        
//         int eliminationCount = 0;

//         // Clamp evidence
//         auto start = std::chrono::steady_clock::now();
//         for (int i = 0; i < evidence_vars.size(); i++){
//             fg.clamp(evidence_vars[i], evidence_values[i], false);
//         }
//         auto end = std::chrono::steady_clock::now();
//         std::cout << "Clamping evidence " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;




//         // Perform Variable Elimination
//         std::vector<dai::Factor> factors = fg.factors();

//         for (dai::Factor& factor : factors) {
//             std::cout << factor.p().size() << " " << factor.i().size() << endl;
//         }

//         for (int i=0; i < constrainedElimOrder.size(); i++){

//             std::cout << "Eliminate: " << constrainedElimOrder[i] << endl;

//             // Find all factors fk that mention variable pi[i] 
//             // f <- Then multiply those factors together 

//             // Could also use findFactor and findVars in factorgraph
//             std::vector<dai::Factor> toMultiply;

//             for(int j=0; j<factors.size(); j++){
                
//                 dai::VarSet vars = (factors[j].vars());

//                 for (auto it = vars.begin(); it != vars.end(); ++it){

//                     if(it->label() == constrainedElimOrder[i]){
                        
//                         toMultiply.push_back(factors[j]);
//                     }
//                 }
//             }

//             // If more than one factor found with the variable to be eliminated,
//             // then multiply together the factors
//             dai::Factor newFactor = toMultiply[0];
//             if(toMultiply.size() > 1){

//                 for (int i = 1; i<toMultiply.size(); i++){

//                     newFactor = newFactor*toMultiply[i];
//                 }
//             }

//             // Check if variable to eliminate is a MAP variable
//             if (std::find(map_vars.begin(), map_vars.end(), constrainedElimOrder[i]) != map_vars.end()){
                
//                 // If variable pi(i) is a map variable then
//                 // fi <- max out pi(i) from f
//                 dai::VarSet vars = newFactor.vars();

//                 dai::VarSet varsToKeep;
//                 for (auto it = vars.begin(); it != vars.end(); ++it){

//                     if(it->label() == constrainedElimOrder[i]){
//                         continue;
//                     }
//                     else{
//                         varsToKeep.insert(*it);
//                     }
//                 }
//                 newFactor = newFactor.maxMarginalTransparent(varsToKeep,  _instantiation,  false);
//             }

//             // Else fi <- sum out pi(i) from f
//             else{
                
//                 dai::VarSet vars = newFactor.vars();

//                 dai::VarSet varsToKeep;
//                 for (auto it = vars.begin(); it != vars.end(); ++it){

//                     if(it->label() == constrainedElimOrder[i]){
//                         continue;
//                     }
//                     else{
//                         varsToKeep.insert(*it);
//                     }
//                 }
//                 newFactor = newFactor.marginal(varsToKeep, false);
//             }

//             // Replace all factors  fk in the set of factor S by factor fi
//             // Remove factors to multiply and replace with newFactor
//             for (auto it = toMultiply.begin(); it != toMultiply.end(); ++it){
//                 factors.erase(std::find_if(factors.begin(), factors.end(), [&](Factor const& f){ return f == *it; }));
//             }

//             factors.push_back(newFactor);

//             std::cout << "Eliminated " << ++eliminationCount << "/" << constrainedElimOrder.size() << endl;

//         }

//         // Multiply remaining factors
//         dai::Factor newFactor = factors[0];
//         if(factors.size() > 1){
//             for (int i = 1; i<factors.size(); i++){

//                 newFactor = newFactor.operator*=(factors[i]);
//             }
//         }

//         return newFactor;
//     }

// int main( int argc, char *argv[] ) {


//     if ( argc != 2 && argc != 3 ) {
//         cout << "Usage: " << argv[0] << " <filename.fg> [maxstates]" << endl << endl;
//         cout << "Reads factor graph <filename.fg> and runs MAP on it." << endl;
//         return 1;

//     } else {
    
//         // Read FactorGraph from the file specified by the first command line argument
//         FactorGraph fg;
//         std::cout << "Factor graph path: " << argv[1] << std::endl;
//         fg.ReadFromFile(argv[1]);;


//         // Example from page 260 of Modeling and Reasoning with Bayesian Networks
//         // MAP variables = {I, J} = {0, 1}
//         // Evidence: O = true. O is variable number 4
//         // Constrained variable order = O, Y, X, I, J = 4, 2, 3, 0, 1

//         std::vector<unsigned int> ex_evidenceVars =        { 4 };
// 	    std::vector<unsigned int> ex_evidenceValues =      { 1};
// 	    std::vector<unsigned int> ex_mapVars =             { 0, 1};
//         std::vector<unsigned int> constrainedElimOrder =   { 4, 2, 3, 0, 1 };


//         dai::Factor MAP = get_map(fg, ex_mapVars, ex_evidenceVars, ex_evidenceValues, false);

//         cout << "Map probability: " << MAP.p() << endl;

//         cout << "Map instantiation: ";
//         for (const auto& myMap : MAP.i()) {
//             std::cout << myMap << endl;
//         }

//         //cout << "Map Instantiation: " << MAP.i() << endl;
//         // std::vector<std::pair<Var, dai::Real>> instantiation = MAP.getInstantiation();
        
//         // for (const auto& pair : instantiation){
//         //     std::cout << "Var: " << pair.first << " Value: " << pair.second << endl;
//         // }
//     }
// }

// }



