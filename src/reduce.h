#ifndef REDUCE_H
#define REDUCE_H
#include "instance.h"
#include "decision_tree.h"

extern "C" {
#include "svm_predict_model.h"
#include "linear_svm_predict_model.h"
}

#include <random>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <numeric>      // std::iota
#include <vector>
#include <cstring>
#include <string>
#include <time.h>
#include <sys/time.h>
#include <iomanip>
#include <omp.h>
#include "gurobi_c++.h"

using namespace std;

namespace GCP{
    class Reduce{
        const Instance& g;
        const float threshold_r = 0.05;
        const float threshold_o = 1;
        const float threshold_c = 0;
        const float threshold_ml = 0.2;
        const string test_data_dir = "../test_data/";
        const string training_data_dir = "../train_data/";
        const string training_model_name = "train_model";
        const string test_data_name = "_test_data_";
        const string output_file_name = "_predicted_value_";
        const int sample_factor = 1;
        const int mis_factor = 20;
        const int iterations = 10;
        const float prop = 0.5;
        int num_mis = 0;
        int sample_size = 0;
        int probability = 0;
        int num_thread = 0;
        double lp_bound = 0.0;
        vector<int> mis_sets_left;
        vector<vector<int>> samples;
        vector<vector<int>> mis_sets;
        vector<vector<int>> selected_mis_sets;
        vector<vector<int>> adj_list;
        vector<vector<bool>> adj_matrix;
        vector<vector<int>> vertex_mis_mapping;
        vector<int> objs;
        int best_obj_sampling;
        vector<int> best_sol_sampling;
        vector<int> rank;
        vector<bool> predicted_value;
        vector<bool> mis_sorted;
        vector<float> ranking_scores;
        vector<float> objective_scores;
        vector<float> corr_xy;
        vector<float> corr_xr;
        vector<double> lp_solution;
        vector<double> reduced_cost;
        vector<int> set_size;
        vector<float> max_set_degree;
        vector<float> ave_set_degree;
        vector<float> min_set_degree;
        vector<float> std_set_degree;
        vector<float> max_rel_set_size;
        vector<float> ave_rel_set_size;
        vector<float> min_rel_set_size;
        vector<float> std_rel_set_size;
        vector<int> vertex_max_mis_size;
        vector<float> ml_score;
        int max_set_size;
        float min_cbm, min_rcm, max_rbm, max_obm;
        int num_mis_selected = 0;
        void multi_thread_sampling();
        void compute_objective_rankings();
        void constructing_test_data();
        void loading_output_data();
        void initializing_parameters();
    public:
        explicit Reduce(const Instance& g);
        void initializing_mis();
        void removing_variables_rbm();
        void removing_variables_obm();
        void removing_variables_cbm();
        void removing_variables_rcm();
        void removing_variables_svm();
        void removing_variables_none();
        void removing_variables_svm_linear();
        void repair();
        int get_objective_value_sampling() const { return best_obj_sampling; }
        bool get_predicted_value(int i) const { return predicted_value[i]; }
        int get_mis_sets_left(int i) const { return mis_sets_left[i]; }
        float get_cbm_value(int i) const { return corr_xy[i]; }
        float get_rcm_value(int i) const { return corr_xr[i]; }
        float get_rbm_value(int i) const { return ranking_scores[i]; }
        float get_obm_value(int i) const { return objective_scores[i]; }
        double get_lp_solution(int i) const { return lp_solution[i]; }
        float get_min_cbm() const { return min_cbm; }
        float get_min_rcm() const { return min_rcm; }
        float get_max_rbm() const { return max_rbm; }
        float get_max_obm() const { return max_obm; }
        void compute_reduced_problem_size();
        void compute_correlation_based_measure();
        void compute_ranking_based_measure();
        void compute_rank_correlation_measure();
        void compute_objective_based_measure();
        void compute_lp_solution();
        void computing_graph_features();
        void evaluating_mis_linear_svm();
        void evaluating_mis_decision_tree();
        void ml_evolving_mis();
        void updating_mis_random();
        void updating_mis_crossover();
        void selecting_high_quality_mis();
        void compute_selected_mis_sets();
        void removing_duplicates();
        void solve_submip();
        int get_num_mis_selected() const { return num_mis_selected; }
        vector<int> get_best_sol_sampling() const { return best_sol_sampling; }
        vector<vector<int>> get_selected_mis_sets() const {return selected_mis_sets; }
    };
}

#endif
