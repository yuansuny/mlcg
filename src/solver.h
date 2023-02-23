#ifndef SOLVER_H
#define SOLVER_H
#include "instance.h"
#include "reduce.h"
#include <iostream>
#include <vector>
#include <omp.h>
#include "gurobi_c++.h"

namespace GCP {
  class Solver {
    const Instance& ins;
    const Reduce& r;
    const double cutoff;
    vector<vector<int>> opt_sol;
    vector<int> opt_sets;
    vector<bool> all_opt_sets_binary;
    double objVal;
    double objBound;
    
  public:
    
    // Builds a solver for graph g.
    explicit Solver(const Instance& ins,  const Reduce& r, const float cutoff) : ins{ins}, r{r}, cutoff{cutoff} {}

    void solve_gcp_gurobi();
    void computing_all_optimal_solutions();
    vector<vector<int>> get_optimal_solution() const { return opt_sol; }
    vector<int> get_optimal_sets() const { return opt_sets; }
    vector<bool> get_all_opt_sets_binary() const { return all_opt_sets_binary; }
    double get_objective_value() const { return objVal; }
    double get_objective_bound() const { return objBound; }

  };
}

#endif
