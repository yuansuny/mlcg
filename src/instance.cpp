#include "instance.h"
#include <cmath>
#include <random>
#include <iterator>
#include <algorithm>

namespace GCP {
    Instance::Instance(string file_name, string input_dir) :  file_name{file_name}, input_dir{input_dir} {
        cout << file_name << endl;
        read_graph();
//        list_maximal_independent_sets();
//        read_optimal_solution();
//        read_all_optimal_solution();
//        compute_problem_specific_features();
    }

    void Instance::read_all_optimal_solution(){
        string opt_file_name = input_dir + file_name + ".allsol";
        ifstream opt_file(opt_file_name);
        if (!opt_file){
            cout << "optimal solution is not provided" << endl;
            return;
        }
        string line;
        bool READ_Point = 0;
        int node;
        int n0 = 0;
        int n1 = 1;
        while(!opt_file.eof()) {
            getline(opt_file, line);
            if (line == "EOF" || line == "-1") {
                break;
            }
            if (READ_Point){
                std::stringstream stream(line);
                while (stream >> node) {
                    optimal_value.push_back(node);
                    if (node == 0){
                        n0++;
                    }
                    if (node == 1){
                        n1++;
                    }
                }
            }
            if (line == "SET_INDEX"){
                READ_Point = 1;
            }
        }
        optimal_ratio = (float) n1 / (n0+n1);
        cout << "n0 is " << n0 << ", n1 is " << n1 << endl;

    }

    void Instance::read_optimal_solution(){
        string opt_file_name = input_dir + file_name + ".sol";
        ifstream opt_file(opt_file_name);
        if (!opt_file){
            cout << "optimal solution is not provided" << endl;
            return;
        }
        string line;
        vector<int> opt_sets;
        bool READ_Point = 0;
        int node;
        while(!opt_file.eof()) {
            getline(opt_file, line);
            if (line == "EOF" || line == "-1") {
                break;
            }
            if (READ_Point){
                std::stringstream stream(line);
                while (stream >> node) {
                    opt_sets.push_back(node);
                }
            }
            if (line == "SET_INDEX"){
                READ_Point = 1;
            }
        }
        int opt_obj = opt_sets.size();
        optimal_value = vector<bool>(n_sets, 0);
        int s;
        for (int i = 0; i < opt_sets.size(); ++i){
            s = opt_sets[i];
            optimal_value[s] = 1;
        }

        // verify if the solution is optimal
        vector<bool> covered_vertex(n_nodes, 0);
        int num_vertex_covered = 0;
        int v;
        for (int i = 0; i < opt_sets.size(); ++i){
            s = opt_sets[i];
            for (int j = 0; j < mis_sets[s].size(); ++j){
                v = mis_sets[s][j];
                if (covered_vertex[v] == 0){
                    covered_vertex[v] = 1;
                    num_vertex_covered++;
                }
            }
        }
        if (num_vertex_covered == n_nodes){
            cout << "optimal objective value is " << opt_obj << endl;
        } else{
            cout << "the mis sets provided are not feasible" << endl;
            assert(num_vertex_covered == n_nodes);
        }
    }

    void Instance::read_graph(){
        string input_file = input_dir  + file_name + ".col";
        ifstream file(input_file);
        string line, s1, s2;
        int v1, v2, ne = 0;
        int idx;

        while(!file.eof()) {
            getline(file, line);
//            cout << line << "\n";
            if (line[0] == 'p') {
                stringstream stream(line);
                stream >> s1 >> s2 >> n_nodes >> n_edges;
                adj_list.resize(n_nodes);
                cout << "number of nodes is " << n_nodes << "; number of edges is " << n_edges << endl;
            }
            if (line[0] == 'e'){
                stringstream stream(line);
                stream >> s1 >> v1 >> v2;
                for (idx = 0; idx < adj_list[v1-1].size(); ++idx){
                    if (adj_list[v1-1][idx] == v2 - 1){
                        break;
                    }
                }
                if (idx == adj_list[v1-1].size()){
                    adj_list[v1-1].push_back(v2-1);
                    adj_list[v2-1].push_back(v1-1);
                    ne++;
                }
            }
        }

        degree = vector<double>(n_nodes);
        max_node_degree = 0.0;
        for (int i = 0; i < n_nodes; ++i){
            degree[i] = (double) adj_list[i].size()/n_nodes;
            if (max_node_degree < degree[i]){
                max_node_degree = degree[i];
            }
        }
//        cout << "Computing degree done" << endl;
        n_edges = ne;
        cout << "number of undirected edges is " << n_edges << endl;
    }


    void Instance::list_maximal_independent_sets(){
        int i, j, m;
        igraph_t graph;
        igraph_vector_t edges;
        igraph_vector_init(&edges, 2*n_edges);

        int count = 0;
        for (i = 0; i < n_nodes; ++i){
            for (j = 0; j < adj_list[i].size(); ++j){
                if (i < adj_list[i][j]){
                    VECTOR(edges)[2*count] = i;
                    VECTOR(edges)[2*count+1] = adj_list[i][j];
                    count++;
                }
            }
        }

        igraph_create(&graph, &edges, n_nodes, IGRAPH_UNDIRECTED);
        igraph_vector_destroy(&edges);

        igraph_vector_ptr_t res;
        igraph_vector_ptr_init(&res, 0);
        igraph_maximal_independent_vertex_sets(&graph, &res);

        igraph_destroy(&graph);
        n_sets = igraph_vector_ptr_size(&res);
        std::cout << "number of MIS is : " << n_sets << std::endl;

        mis_sets.resize(n_sets);
        node_to_sets.resize(n_nodes);
        for (i = 0; i < n_sets; i++) {
            igraph_vector_t *v = (igraph_vector_t *) VECTOR(res)[i];
            m = igraph_vector_size(v);
            for (j = 0; j < m; ++j){
                mis_sets[i].push_back(VECTOR(*v)[j]);
                node_to_sets[VECTOR(*v)[j]].push_back(i);
            }
            igraph_vector_destroy(v);
            igraph_free(v);
        }
        igraph_vector_ptr_destroy(&res);
    }


    void Instance::compute_problem_specific_features(){
        set_size = vector<int>(n_sets, 0.0);
        max_set_degree = vector<float>(n_sets, 0.0);
        ave_set_degree = vector<float>(n_sets, 0.0);
        min_set_degree = vector<float>(n_sets, 1.0);
        std_set_degree = vector<float>(n_sets, 0.0);
        max_set_size = 0;
        int v;
        for (int i = 0; i < n_sets; ++i){
            set_size[i] = mis_sets[i].size();
            if (max_set_size < set_size[i]){
                max_set_size = set_size[i];
            }
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                ave_set_degree[i] += degree[v] / set_size[i];
                if (max_set_degree[i] < degree[v]){
                    max_set_degree[i] = degree[v];
                }
                if (min_set_degree[i] > degree[v]){
                    min_set_degree[i] = degree[v];
                }
            }
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                std_set_degree[i] += pow(degree[v] - ave_set_degree[i], 2) / set_size[i];
            }
            std_set_degree[i] = sqrt(std_set_degree[i]);
        }


        vector<double> node_max_set_size(n_nodes, 0.0);
        vector<double> node_min_set_size(n_nodes, max_set_size);
        vector<double> node_ave_set_size(n_nodes, 0.0);
        int s;
        for (int i = 0; i < n_nodes; ++i){
            for (int j = 0; j < node_to_sets[i].size(); ++j){
                s = node_to_sets[i][j];
                node_ave_set_size[i] += (double) mis_sets[s].size() / node_to_sets[i].size();
                if (node_max_set_size[i] < mis_sets[s].size()){
                    node_max_set_size[i] = mis_sets[s].size();
                }
                if (node_min_set_size[i] > mis_sets[s].size()){
                    node_min_set_size[i] = mis_sets[s].size();
                }
            }
        }


        max_rel_set_size = vector<float>(n_sets, 0.0);
        ave_rel_set_size = vector<float>(n_sets, 0.0);
        min_rel_set_size = vector<float>(n_sets, 1.0);
        std_rel_set_size = vector<float>(n_sets, 0.0);
        double relative_size;
        for (int i = 0; i < n_sets; ++i){
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                relative_size = (double) set_size[i] / node_max_set_size[v];
                ave_rel_set_size[i] += relative_size / set_size[i];
                if (max_rel_set_size[i] < relative_size){
                    max_rel_set_size[i] = relative_size;
                }
                if (min_rel_set_size[i] > relative_size){
                    min_rel_set_size[i] = relative_size;
                }
            }
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                relative_size = (double) set_size[i] / node_max_set_size[v];
                std_rel_set_size[i] += pow(relative_size - ave_rel_set_size[i], 2) / set_size[i];
            }
            std_rel_set_size[i] = sqrt(std_rel_set_size[i]);
        }
    }

}