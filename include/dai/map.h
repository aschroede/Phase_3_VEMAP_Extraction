#ifndef __defined_libdai_map_h
#define __defined_libdai_map_h


#include <set>
#include <vector>
#include <dai/factorgraph.h>
#include <dai/logger.h>

namespace dai {

using namespace std;

template<class EliminationChoice>
vector<size_t> getConstrainedElimOrder(const FactorGraph &fg, EliminationChoice f, std::vector<unsigned int> map_vars);

dai::Factor get_map_ve(dai::FactorGraph fg, std::vector<unsigned int> map_vars, std::vector<unsigned int> evidence_vars,
        std::vector<unsigned int> evidence_values, bool mapList, LibLogger &logger);

std::vector<unsigned long int> get_map_jt(dai::FactorGraph fg, std::vector<unsigned int> hypothesis_vars, std::vector<unsigned int> evidence_vars,
	std::vector<unsigned int> evidence_values, bool mapList, LibLogger &logger);

dai::Factor variableElimination(dai::FactorGraph fg, std::vector<unsigned int> query_vars, std::vector<unsigned int> evidence_vars,
        std::vector<unsigned int> evidence_values, LibLogger &logger);

std::vector<unsigned long int> extractMax(dai::Factor factor, LibLogger &logger);
}



#endif