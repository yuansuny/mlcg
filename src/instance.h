#ifndef INSTANCE_H
#define INSTANCE_H
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <random>
#include <iterator>
#include <assert.h>

extern "C" {
#include <igraph.h>
}

using namespace std;

namespace GCP {
    class Instance {
        int n_nodes;
        int n_edges;
        int n_sets;
        vector<vector<int>> adj_list;
        vector<vector<int>> mis_sets;
        vector<vector<int>> node_to_sets;
        vector<double> degree;
        vector<bool> optimal_value;
        vector<int> set_size;
        vector<float> max_set_degree;
        vector<float> ave_set_degree;
        vector<float> min_set_degree;
        vector<float> std_set_degree;
        vector<float> max_rel_set_size;
        vector<float> ave_rel_set_size;
        vector<float> min_rel_set_size;
        vector<float> std_rel_set_size;

        int max_set_size;
        float max_node_degree;
        float optimal_ratio;
        const string file_name;
        const string input_dir;
        void read_graph();
        void list_maximal_independent_sets();
        void read_optimal_solution();
        void read_all_optimal_solution();
        void compute_problem_specific_features();
    
    public:

        // Created a new (random) graph with n_nodes nodes
        explicit Instance(string file_name, string input_dir);

        // Size of the graph (i.e. number of nodes).
        int size() const { return n_nodes; }
        int get_num_edges() const { return n_nodes; }
        int get_n_sets() const { return n_sets; }
        vector<vector<int>> get_adj_list() const {return adj_list; }
        vector<vector<int>> get_mis_sets() const {return mis_sets; }
        vector<vector<int>> get_node_to_sets() const {return node_to_sets; }
        double get_degree(int i) const { return degree[i]; }
        // Optimal solution of edge x[i][j]
        bool get_optimal_value(int i) const { return optimal_value[i]; }
        string get_file_name() const { return file_name; }

        int get_max_set_size() const { return max_set_size; }
        int get_set_size(int i) const { return set_size[i]; }
        float get_max_set_degree(int i) const { return max_set_degree[i]; }
        float get_min_set_degree(int i) const { return min_set_degree[i]; }
        float get_ave_set_degree(int i) const { return ave_set_degree[i]; }
        float get_std_set_degree(int i) const { return std_set_degree[i]; }
        float get_max_node_degree() const { return max_node_degree; }
        float get_max_rel_set_size(int i) const { return max_rel_set_size[i]; }
        float get_ave_rel_set_size(int i) const { return ave_rel_set_size[i]; }
        float get_min_rel_set_size(int i) const { return min_rel_set_size[i]; }
        float get_std_rel_set_size(int i) const { return std_rel_set_size[i]; }
        float get_optimal_ratio() const {return optimal_ratio; }
        // Prints out the cost matrix of the graph.
        friend ostream& operator<<(ostream& out, const Instance& g);
    };
}

#endif
