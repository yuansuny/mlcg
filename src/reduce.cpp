#include "reduce.h"
#include <cmath>
#include <limits>

namespace GCP {
    Reduce::Reduce(const Instance& g) : g{g} {
        initializing_parameters();
    }

    void Reduce::initializing_parameters(){
        num_thread = omp_get_max_threads();
        cout << "threads used: " << num_thread << endl;
        num_mis = mis_factor * g.size();
        cout << "number of MISs generated is " <<num_mis << endl;
        mis_sets.resize(num_mis);
        adj_list = g.get_adj_list();
        adj_matrix = vector<vector<bool>>(g.size(), vector<bool>(g.size(), 0));
        for (int i = 0; i < g.size(); ++i){
            for (int j = 0; j < adj_list[i].size(); ++j){
                adj_matrix[i][adj_list[i][j]] = 1;
                adj_matrix[adj_list[i][j]][i] = 1;
            }
        }
        sample_size = num_mis * sample_factor;
        cout << "sample size is " << sample_size << endl;
        samples.resize(sample_size);
        objs = vector<int>(sample_size, 0);
        rank = vector<int>(sample_size);
        vertex_mis_mapping.resize(g.size());

        set_size = vector<int>(num_mis);
        max_set_degree = vector<float>(num_mis);
        ave_set_degree = vector<float>(num_mis);
        min_set_degree = vector<float>(num_mis);
        std_set_degree = vector<float>(num_mis);
        max_rel_set_size = vector<float>(num_mis);
        ave_rel_set_size = vector<float>(num_mis);
        min_rel_set_size = vector<float>(num_mis);
        std_rel_set_size = vector<float>(num_mis);
        vertex_max_mis_size = vector<int>(g.size(), 0);
        best_obj_sampling = g.size();

        ranking_scores = vector<float>(num_mis);
        corr_xr = vector<float>(num_mis);
        objective_scores = vector<float>(num_mis);
        corr_xy = vector<float>(num_mis);
        ml_score = vector<float>(num_mis);
        predicted_value = vector<bool>(num_mis);
        mis_sorted = vector<bool>(num_mis, 0);
        lp_solution.resize(num_mis);
        reduced_cost.resize(num_mis);
    }

    void Reduce::initializing_mis() {
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum+1) / num_thread;
            random_device rd;
            mt19937 gen(rd() + threadnum);
            srand (time(NULL) + threadnum);
            int v, idx, num;
            vector<int> candidates(g.size());
            int nb_candidates;
            for(int i = low; i < high; ++i) {
                nb_candidates = g.size();
                for (int j = 0; j < nb_candidates; ++j){
                    candidates[j] = j;
                }
                while (nb_candidates > 0){
                    if (nb_candidates == g.size()){
                        idx = i % g.size();
                    } else{
                        idx = rand() % nb_candidates;
                    }
                    v = candidates[idx];
                    mis_sets[i].push_back(v);
                    num = 0;
                    for (int j = 0; j < nb_candidates; ++j){
                        if (adj_matrix[v][candidates[j]] == 0 && j != idx){
                            candidates[num] = candidates[j];
                            num++;
                        }
                    }
                    nb_candidates = num;
                }
            }
        }
    }


    void Reduce::computing_graph_features() {
        for (int i = 0; i < num_mis; ++i){
            max_set_degree[i] = 0.0;
            ave_set_degree[i] = 0.0;
            min_set_degree[i] = 1.0;
            std_set_degree[i] = 0.0;
            max_rel_set_size[i] = 0.0;
            ave_rel_set_size[i] = 0.0;
            min_rel_set_size[i] = 1.0;
            std_rel_set_size[i] = 0.0;
        }

        max_set_size = 0;
        for (int i = 0; i < g.size(); ++i){
            vertex_mis_mapping[i].clear();
            vertex_max_mis_size[i] = 0;
        }

        int v;
        for (int i = 0; i < num_mis; ++i){
            set_size[i] = mis_sets[i].size();
            if (max_set_size < set_size[i]){
                max_set_size = set_size[i];
            }
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                vertex_mis_mapping[v].push_back(i);
                if (vertex_max_mis_size[v] < set_size[i]){
                    vertex_max_mis_size[v] = set_size[i];
                }

                ave_set_degree[i] += g.get_degree(v) / set_size[i];
                if (max_set_degree[i] < g.get_degree(v)){
                    max_set_degree[i] = g.get_degree(v);
                }
                if (min_set_degree[i] > g.get_degree(v)){
                    min_set_degree[i] = g.get_degree(v);
                }
            }
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                std_set_degree[i] += pow(g.get_degree(v) - ave_set_degree[i], 2) / set_size[i];
            }
            std_set_degree[i] = sqrt(std_set_degree[i]);
        }

        double relative_size;
        for (int i = 0; i < num_mis; ++i){
            for (int j = 0; j < set_size[i]; ++j){
                v = mis_sets[i][j];
                relative_size = (double) set_size[i] / vertex_max_mis_size[v];
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
                relative_size = (double) set_size[i] / vertex_max_mis_size[v];
                std_rel_set_size[i] += pow(relative_size - ave_rel_set_size[i], 2) / set_size[i];
            }
            std_rel_set_size[i] = sqrt(std_rel_set_size[i]);
        }
    }

    void Reduce::multi_thread_sampling() {
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = sample_size * threadnum / num_thread;
            int high = sample_size * (threadnum+1) / num_thread;
            random_device rd;
            mt19937 gen(rd() + threadnum);
            srand (time(NULL) + threadnum);
            vector<bool> covered_vertex(g.size());
            vector<int> vertices(g.size());
            iota(vertices.begin(), vertices.end(), 0);
            int v, idx, s;
            for(int i = low; i < high; ++i) {
                samples[i].clear();
                shuffle(vertices.begin(), vertices.end(), gen);
                for (int j = 0; j < g.size(); ++j){
                    covered_vertex[j] = 0;
                }
                for (int j = 0; j < g.size(); ++j){
                    v = vertices[j];
                    if (covered_vertex[v] == 0){
                        idx = rand() % vertex_mis_mapping[v].size();
                        s = vertex_mis_mapping[v][idx];
                        samples[i].push_back(s);
                        for (int k = 0; k < mis_sets[s].size(); ++k){
                            covered_vertex[mis_sets[s][k]] = 1;
                        }
                    }
                }
                objs[i] = samples[i].size();
            }
        }
    }

    void Reduce::compute_objective_rankings(){
        vector<int> sort_idx(sample_size);
        iota(sort_idx.begin(), sort_idx.end(), 0);
        vector<int> objs_copy(objs);
        sort(sort_idx.begin(), sort_idx.end(), [&objs_copy](int i1, int i2) {return objs_copy[i1] < objs_copy[i2];});
        for (int i = 0; i < sample_size; ++i){
            rank[sort_idx[i]] = i+1;
        }

        best_sol_sampling.clear();
        best_sol_sampling = samples[sort_idx[0]];
        best_obj_sampling = objs[sort_idx[0]];

//        if (best_obj_sampling > objs[sort_idx[0]]){
//            best_sol_sampling.clear();
//            best_sol_sampling = samples[sort_idx[0]];
//            best_obj_sampling = objs[sort_idx[0]];
//        }
        cout << "best obj in sampling is " << best_obj_sampling << endl;
    }

    void Reduce::removing_variables_rbm() {
        cout << "problem reduction using ranking-based measure with threshold " << threshold_r << endl;
        compute_ranking_based_measure();
        predicted_value = vector<bool>(num_mis, 0);
        for (int i = 0; i < num_mis; ++i){
            if (ranking_scores[i] > threshold_r){
                predicted_value[i] = 1;
            } else{
                predicted_value[i] = 0;
            }
        }
        repair();
        compute_reduced_problem_size();
    }

    void Reduce::compute_ranking_based_measure() {
//        cout << "computing ranking based measure " << endl;
        for (int i = 0; i < num_mis; ++i){
            ranking_scores[i] = 0.0;
        }
        #pragma omp parallel
        {
            vector<float> ranking_scores_local = vector<float>(num_mis, 0.0);
            int threadnum = omp_get_thread_num();
            int low = sample_size * threadnum / num_thread;
            int high = sample_size * (threadnum+1) / num_thread;
            int s;
            for (int i = low; i < high; ++i){
                for (int j = 0; j < samples[i].size(); ++j){
                    s = samples[i][j];
                    ranking_scores_local[s] += 1.0/rank[i];
                }
            }
            #pragma omp critical
            for (int i = 0; i < num_mis; ++i){
                ranking_scores[i] += ranking_scores_local[i];
            }
        }
        max_rbm = 0.0;
        for (int i = 0; i < num_mis; ++i){
            if (ranking_scores[i] > max_rbm){
                max_rbm = ranking_scores[i];
            }
        }
    }

    void Reduce::removing_variables_rcm(){
//        cout << "problem reduction using rank correlation measure with threshold " << threshold_c << endl;
        compute_rank_correlation_measure();
        predicted_value = vector<bool>(num_mis, 0);
        for (int i = 0; i < num_mis; ++i){
            if (corr_xr[i] < threshold_c){
                predicted_value[i] = 1;
            } else{
                predicted_value[i] = 0;
            }
        }
        repair();
        compute_reduced_problem_size();
    }

    void Reduce::compute_rank_correlation_measure(){
//        cout << "computing rank correlation measure " << endl;
        double mean_y = 0.0;
        for (int i = 0; i < sample_size; ++i){
            mean_y += (double)rank[i]/sample_size;
        }
        vector<double> diff_y(sample_size);
        double variance_y = 0.0, sum_diff_y = 0.0;
        for (int i = 0; i < sample_size; ++i){
            diff_y[i] = (double)rank[i] - mean_y;
            variance_y += diff_y[i]*diff_y[i];
            sum_diff_y += diff_y[i];
        }

        vector<double> mean_x = vector<double>(num_mis, 0.0);
        vector<double> S1 = vector<double>(num_mis, 0.0);

        #pragma omp parallel
        {
            vector<double> mean_x_local = vector<double>(num_mis, 0.0);
            vector<double> S1_local = vector<double>(num_mis, 0.0);
            int threadnum = omp_get_thread_num();
            int low = sample_size * threadnum / num_thread;
            int high = sample_size * (threadnum+1) / num_thread;
            double ratio = 1.0/sample_size;
            int s;
            for (int i = low; i < high; ++i){
                for (int j = 0; j < samples[i].size(); ++j){
                    s = samples[i][j];
                    mean_x_local[s] += ratio;
                    S1_local[s] += diff_y[i];
                }
            }
            #pragma omp critical
            for (int i = 0; i < num_mis; ++i){
                mean_x[i] += mean_x_local[i];
                S1[i] += S1_local[i];
            }
        }

        vector<double> variance_x(num_mis, 0.0);
        vector<double> variance_xy(num_mis, 0.0);

        min_rcm = 1.0;
        for (int i = 0; i < num_mis; ++i){
            variance_x[i] = mean_x[i]*(1-mean_x[i])*sample_size;
            variance_xy[i] = (1-mean_x[i])*S1[i] - mean_x[i]*(sum_diff_y - S1[i]);
            if (variance_x[i] > 0){
                corr_xr[i] = variance_xy[i]/sqrt(variance_x[i]*variance_y);
            }else if (mean_x[i] == 0) {
                corr_xr[i] = 1.0;
            } else{
                corr_xr[i] = - 1.0;
            }
            if (corr_xr[i] < min_rcm){
                min_rcm = corr_xr[i];
            }
        }
    }

    void Reduce::removing_variables_obm(){
        cout << "problem reduction using objective-based measure with threshold " << threshold_o << endl;
        compute_objective_based_measure();
        predicted_value = vector<bool>(num_mis, 0);
        for (int i = 0; i < num_mis; ++i){
            if (objective_scores[i] > threshold_o){
                predicted_value[i] = 1;
            } else{
                predicted_value[i] = 0;
            }
        }
        repair();
        compute_reduced_problem_size();
    }


    void Reduce::compute_objective_based_measure() {
//        cout << "computing objective based measure " << endl;
        for (int i = 0; i < num_mis; ++i){
            objective_scores[i] = 0.0;
        }
        #pragma omp parallel
        {
            vector<float> objective_scores_local = vector<float>(num_mis, 0.0);
            int threadnum = omp_get_thread_num();
            int low = sample_size * threadnum / num_thread;
            int high = sample_size * (threadnum+1) / num_thread;
            int s;
            for (int i = low; i < high; ++i){
                for (int j = 0; j < samples[i].size(); ++j){
                    s = samples[i][j];
                    objective_scores_local[s] += 1.0/(objs[i]);
                }
            }
            #pragma omp critical
            for (int i = 0; i < num_mis; ++i){
                objective_scores[i] += objective_scores_local[i];
            }
        }
        max_obm = 0.0;
        for (int i = 0; i < num_mis; ++i){
            if (objective_scores[i] > max_obm){
                max_obm = objective_scores[i];
            }
        }
    }


    void Reduce::removing_variables_cbm(){
        cout << "problem reduction using correlation-based measure with threshold " << threshold_c << endl;
        compute_correlation_based_measure();
        predicted_value = vector<bool>(num_mis, 0);
        for (int i = 0; i < num_mis; ++i){
            if (corr_xy[i] < threshold_c){
                predicted_value[i] = 1;
            } else{
                predicted_value[i] = 0;
            }
        }
        repair();
        compute_reduced_problem_size();
    }

    void Reduce::compute_correlation_based_measure(){
//        cout << "computing correlation based measure " << endl;
        double mean_y = 0.0;
        for (int i = 0; i < sample_size; ++i){
            mean_y += (double)objs[i]/sample_size;
        }
        vector<double> diff_y = vector<double>(sample_size);
        double variance_y = 0.0, sum_diff_y = 0.0;
        for (int i = 0; i < sample_size; ++i){
            diff_y[i] = objs[i] - mean_y;
            variance_y += diff_y[i]*diff_y[i];
            sum_diff_y += diff_y[i];
        }

        vector<double> mean_x = vector<double>(num_mis, 0.0);
        vector<double> S1 = vector<double>(num_mis, 0.0);

        #pragma omp parallel
        {
            vector<double> mean_x_local = vector<double>(num_mis, 0.0);
            vector<double> S1_local = vector<double>(num_mis, 0.0);

            int threadnum = omp_get_thread_num();
            int low = sample_size * threadnum / num_thread;
            int high = sample_size * (threadnum+1) / num_thread;
            double ratio = 1.0/sample_size;
            int s;
            for (int i = low; i < high; ++i){
                for (int j = 0; j < samples[i].size(); ++j){
                    s = samples[i][j];
                    mean_x_local[s] += ratio;
                    S1_local[s] += diff_y[i];
                }
            }
            #pragma omp critical
            for (int i = 0; i < num_mis; ++i){
                mean_x[i] += mean_x_local[i];
                S1[i] += S1_local[i];
            }
        }

        vector<double> variance_x(num_mis, 0.0);
        vector<double> variance_xy(num_mis, 0.0);
        min_cbm = 1.0;
        for (int i = 0; i < num_mis; ++i){
            variance_x[i] = mean_x[i]*(1-mean_x[i])*sample_size;
            variance_xy[i] = (1-mean_x[i])*S1[i] - mean_x[i]*(sum_diff_y - S1[i]);
            if (variance_x[i] > 0){
                corr_xy[i] = variance_xy[i]/sqrt(variance_x[i]*variance_y);
            }else if (mean_x[i] == 0) {
                corr_xy[i] = 1.0;
            } else{
                corr_xy[i] = -1.0;
            }
            if (corr_xy[i] < min_cbm){
                min_cbm = corr_xy[i];
            }
        }
    }

    void Reduce::removing_variables_none() {
        cout << "no problem reduction" << endl;
        num_mis = g.get_n_sets();
        predicted_value = vector<bool>(num_mis, 1);
        compute_reduced_problem_size();
    }


    void Reduce::compute_lp_solution() {
//        cout << "computing lp relaxation " << endl;
//        vector<vector<int>> mis_sets = g.get_mis_sets();
        vector<vector<bool>> mis_sets_binary(num_mis, vector<bool>(g.size(), 0));
        int v;
        for (int i = 0; i < num_mis; ++i){
            for (int j = 0; j < mis_sets[i].size(); ++j){
                v = mis_sets[i][j];
                mis_sets_binary[i][v] = 1;
            }
        }
        // setup the model now
        GRBEnv *env;
        vector<GRBVar> x;
        env = new GRBEnv();
        GRBModel model = GRBModel(*env);
        model.getEnv().set(GRB_IntParam_OutputFlag, 0);
        model.set(GRB_StringAttr_ModelName, "LP_GCP");
        // Create variables and set them to be binary
        x.resize(num_mis);
        for (int i = 0; i < num_mis; ++i){
            x[i] = model.addVar(0,1,0,GRB_CONTINUOUS);
        }
        model.update();
        // each vertex is covered by at least one set
        for (int j = 0; j < g.size(); ++j){
            GRBLinExpr rtot = 0;
            for (int i = 0; i < num_mis; ++i){
                rtot += mis_sets_binary[i][j] * x[i];
            }
            model.addConstr(rtot >= 1, "");
        }
        model.update();
        model.set(GRB_IntParam_Threads, omp_get_max_threads());
        model.set(GRB_IntParam_Presolve, 0);
        // the objective
        GRBLinExpr tot=0;
        for(int i = 0; i < num_mis; ++i){
            tot += x[i];
        }
        model.setObjective(tot,GRB_MINIMIZE);
        model.update();
        model.optimize();
        lp_bound = model.get(GRB_DoubleAttr_ObjVal);
//        cout << "root node lp bound is " << lp_bound << endl;
        for (int i = 0; i < num_mis; ++i){
            lp_solution[i] = x[i].get(GRB_DoubleAttr_X);
            reduced_cost[i] = x[i].get(GRB_DoubleAttr_RC);
        }
        delete env;
    }


    void Reduce::solve_submip() {
        vector<vector<bool>> mis_sets_binary(num_mis_selected, vector<bool>(g.size(), 0));
        int v;
        for (int i = 0; i < num_mis_selected; ++i){
            for (int j = 0; j < selected_mis_sets[i].size(); ++j){
                v = selected_mis_sets[i][j];
                mis_sets_binary[i][v] = 1;
            }
        }
        cout << "number of mis sets used is " << num_mis_selected << endl;

        // setup the model now
        GRBEnv *env;
        vector<GRBVar> x;
        env = new GRBEnv();
        GRBModel model = GRBModel(*env);
        model.getEnv().set(GRB_IntParam_OutputFlag, 0);
        model.set(GRB_StringAttr_ModelName, "LP_GCP");
        // Create variables and set them to be binary
        x.resize(num_mis_selected);
        for (int i = 0; i < num_mis_selected; ++i){
            x[i] = model.addVar(0,1,0,GRB_BINARY);
        }
        model.update();
        // each vertex is covered by at least one set
        for (int j = 0; j < g.size(); ++j){
            GRBLinExpr rtot = 0;
            for (int i = 0; i < num_mis_selected; ++i){
                rtot += mis_sets_binary[i][j] * x[i];
            }
            model.addConstr(rtot >= 1, "");
        }
        model.update();
        model.set(GRB_IntParam_Threads, omp_get_max_threads());
        model.set(GRB_IntParam_Presolve, 0);
        // the objective
        GRBLinExpr tot=0;
        for(int i = 0; i < num_mis_selected; ++i){
            tot += x[i];
        }
        model.setObjective(tot,GRB_MINIMIZE);
        model.update();
        model.optimize();
        double objVal = model.get(GRB_DoubleAttr_ObjVal);
        cout << "best objective value is " << objVal << endl;
        delete env;
    }

    void Reduce::constructing_test_data(){
        compute_correlation_based_measure();
        compute_ranking_based_measure();
        compute_rank_correlation_measure();
        compute_objective_based_measure();
        compute_lp_solution();

        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            string test_s = test_data_dir + g.get_file_name() + test_data_name + to_string(threadnum) + ".csv";

            char test_data[test_s.size()+1];
            strcpy(test_data, test_s.c_str());
            ofstream test_file(test_data, ios::trunc);
            if (! test_file.is_open()){
                cout << "Cannot open the output file " <<  test_data << "\n";
            }

            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum + 1) / num_thread;

            for (int i = low; i < high; ++i){
                test_file << 0 << " ";
//                test_file << "1:" << fixed << setprecision(6) << corr_xy[i] / min_cbm << " ";
                test_file << "1:" << fixed << setprecision(6) << corr_xy[i] << " ";
                test_file << "2:" << fixed << setprecision(6) << ranking_scores[i] / max_rbm << " ";
//                test_file << "3:" << fixed << setprecision(6) << corr_xr[i] / min_rcm << " ";
                test_file << "3:" << fixed << setprecision(6) << corr_xr[i] << " ";
                test_file << "4:" << fixed << setprecision(6) << objective_scores[i] / max_obm << " ";
                test_file << "5:" << fixed << setprecision(6) << g.get_max_rel_set_size(i) << " ";
                test_file << "6:" << fixed << setprecision(6) << g.get_ave_rel_set_size(i) << " ";
                test_file << "7:" << fixed << setprecision(6) << g.get_min_rel_set_size(i) << " ";
                test_file << "8:" << fixed << setprecision(6) << g.get_std_rel_set_size(i) << " ";
                test_file << "9:" << fixed << setprecision(6) << (double) g.get_set_size(i) / (double) g.get_max_set_size() << " ";
                test_file << "10:" << fixed << setprecision(6) << g.get_max_set_degree(i) / g.get_max_node_degree() << " ";
                test_file << "11:" << fixed << setprecision(6) << g.get_ave_set_degree(i) / g.get_max_node_degree() << " ";
                test_file << "12:" << fixed << setprecision(6) << g.get_min_set_degree(i) / g.get_max_node_degree() << " ";
                test_file << "13:" << fixed << setprecision(6) << g.get_std_set_degree(i) / g.get_max_node_degree() << " ";
                test_file << "14:" << fixed << setprecision(16) << lp_solution[i] << " " << "\n";
            }
            test_file.close();
        }
    }

    void Reduce::loading_output_data(){
        predicted_value = vector<bool>(num_mis, 0);
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            string output_s = test_data_dir + g.get_file_name() + output_file_name + to_string(threadnum) + ".csv";
            char output_file[output_s.size()+1];
            strcpy(output_file, output_s.c_str());
            ifstream predicted_file(output_file);
            if (! predicted_file){
                cout << "fail to read the predicted file \n";
            }
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum + 1) / num_thread;
            bool value;
            for (int i = low; i < high; ++i){
                predicted_file >> value;
                predicted_value[i] = value;
            }
            predicted_file.close();
            remove(output_file);
//            const int rem_result = remove(output_file);
//            if(rem_result == 0){
//                cout << "Successfully remove predicted data file from thread " << threadnum << endl;
//            } else {
//                cout << "No such predicted data file from thread " << threadnum << endl;
//            }
        }
    }

    // Removing variables using SVM
    void Reduce::removing_variables_svm(){
        cout << "problem reduction using SVM" << endl;
        constructing_test_data();
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            string test_s = test_data_dir + g.get_file_name() + test_data_name + to_string(threadnum) + ".csv";
            string output_s = test_data_dir + g.get_file_name() + output_file_name + to_string(threadnum) + ".csv";
            string model_s = training_data_dir + training_model_name;
            char test_data[test_s.size()+1];
            char output_file[output_s.size()+1];
            char model_file[model_s.size()+1];
            strcpy(test_data, test_s.c_str());
            strcpy(output_file, output_s.c_str());
            strcpy(model_file, model_s.c_str());
            #pragma omp critical
            svm_predict_model(test_data, model_file, output_file, probability);
        }
        loading_output_data();
        repair();
        compute_reduced_problem_size();
        cout << "problem reduction using SVM done" << endl;
    }

    // Removing variables using Linear SVM (fast)
    void Reduce::removing_variables_svm_linear(){
        cout << "problem reduction using linear SVM" << endl;
        constructing_test_data();
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            string test_s = test_data_dir + g.get_file_name() + test_data_name + to_string(threadnum) + ".csv";
            string output_s = test_data_dir + g.get_file_name() + output_file_name + to_string(threadnum) + ".csv";
            string model_s = training_data_dir + training_model_name;
            char test_data[test_s.size()+1];
            char output_file[output_s.size()+1];
            char model_file[model_s.size()+1];
            strcpy(test_data, test_s.c_str());
            strcpy(output_file, output_s.c_str());
            strcpy(model_file, model_s.c_str());
            #pragma omp critical
            linear_svm_predict_model(test_data, model_file, output_file, probability);
            remove(test_data);
//            const int rem_result = remove(test_data);
//            if(rem_result == 0){
//                cout << "Successfully remove test data file from thread " << threadnum << endl;
//            } else {
//                cout << "No such test data file from thread " << threadnum << endl;
//            }
        }

        loading_output_data();
        repair();
        compute_reduced_problem_size();
        cout << "Linear SVM prediction done" << endl;
    }

    //adding the best solution found in sampling to make sure the reduced space contains at least one feasible solution
    void Reduce::repair(){
        int s;
        for (int j = 0; j < best_sol_sampling.size(); ++j){
            s = best_sol_sampling[j];
            predicted_value[s] = 1;
        }
    }

    void Reduce::compute_reduced_problem_size(){
        num_mis_selected = 0;
        for (int i = 0; i < num_mis; ++i){
            if(predicted_value[i] > 0.5){
                num_mis_selected += 1;
                mis_sets_left.push_back(i);
            }
        }
        cout << "number of maximal independent sets left is " << num_mis_selected << endl;
        cout << "proportion of remaining problem size is " << (double)num_mis_selected/num_mis << endl;
    }


    void Reduce::evaluating_mis_linear_svm(){
        computing_graph_features();
        multi_thread_sampling();
        compute_objective_rankings();
        compute_correlation_based_measure();
        compute_ranking_based_measure();
        compute_rank_correlation_measure();
        compute_objective_based_measure();
        compute_lp_solution();

//        vector<float> svm_weights = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -2.19015};

         vector<float> svm_weights = { -2.7936029882134346, \
                                       -0.48863480788453095, \
                                       1.0901254158966691, \
                                       1.6857535044752539, \
                                       1.1620206926205059 , \
                                       0.25617391459177563 , \
                                       -0.90103647298282441 , \
                                       -2.7003551414741618 , \
                                       -0.016885971759520835 , \
                                       -2.225856383509373 , \
                                       -0.050537943061425283 , \
                                       2.7940134193840604 , \
                                       2.9415760263759179 , \
                                       0.90346492175190574 , \
                                       -2.1901594737815828 };

        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum + 1) / num_thread;
            for (int i = low; i < high; ++i){
                ml_score[i] = svm_weights[0] * corr_xy[i] + \
                              svm_weights[1] * ranking_scores[i] / max_rbm + \
                              svm_weights[2] * corr_xr[i] + \
                              svm_weights[3] * objective_scores[i] / max_obm + \
                              svm_weights[4] * max_rel_set_size[i] + \
                              svm_weights[5] * ave_rel_set_size[i] + \
                              svm_weights[6] * min_rel_set_size[i] + \
                              svm_weights[7] * std_rel_set_size[i] + \
                              svm_weights[8] * set_size[i] / max_set_size + \
                              svm_weights[9] * max_set_degree[i] / g.get_max_node_degree() + \
                              svm_weights[10] * ave_set_degree[i] / g.get_max_node_degree() + \
                              svm_weights[11] * min_set_degree[i] / g.get_max_node_degree() + \
                              svm_weights[12] * std_set_degree[i] / g.get_max_node_degree() + \
                              svm_weights[13] * lp_solution[i] + \
                              svm_weights[14] * reduced_cost[i] ;
            }
        }


//        // print mis and features
//        for (int i = 0; i < num_mis; ++i){
//            cout << "MIS " << i << " : ";
//            for (int j = 0; j < mis_sets[i].size(); ++j){
//                cout << mis_sets[i][j] << ", ";
//            }
//            cout << endl;
//
//            cout << " features and scores : ";
//            cout << corr_xy[i] << ", ";
//            cout << ranking_scores[i] / max_rbm << ", ";
//            cout << corr_xr[i] << ", ";
//            cout << objective_scores[i] / max_obm << ", ";
//            cout << max_rel_set_size[i] << ", ";
//            cout << ave_rel_set_size[i] << ", ";
//            cout << min_rel_set_size[i] << ", ";
//            cout << std_rel_set_size[i] << ", ";
//            cout << (double) set_size[i] / max_set_size << ", ";
//            cout << max_set_degree[i] / g.get_max_node_degree() << ", ";
//            cout << ave_set_degree[i] / g.get_max_node_degree() << ", ";
//            cout << min_set_degree[i] / g.get_max_node_degree() << ", ";
//            cout << std_set_degree[i] / g.get_max_node_degree() << ", ";
//            cout << lp_solution[i] << ", ";
//            cout << reduced_cost[i] << ", ";
//            cout << ml_score[i] << endl;
//        }


    }


     void Reduce::evaluating_mis_decision_tree(){
        computing_graph_features();
        multi_thread_sampling();
        compute_objective_rankings();
        compute_correlation_based_measure();
        compute_ranking_based_measure();
        compute_rank_correlation_measure();
        compute_objective_based_measure();
        compute_lp_solution();

        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum + 1) / num_thread;
            vector<double> feature_vector(15);
            for (int i = low; i < high; ++i){
                // compute feature values for each mis
                feature_vector[0] = (double) set_size[i] / max_set_size;
                feature_vector[1] = max_rel_set_size[i];
                feature_vector[2] = min_rel_set_size[i];
                feature_vector[3] = ave_rel_set_size[i];
                feature_vector[4] = std_rel_set_size[i];
                feature_vector[5] = max_set_degree[i] / g.get_max_node_degree();
                feature_vector[6] = min_set_degree[i] / g.get_max_node_degree();
                feature_vector[7] = ave_set_degree[i] / g.get_max_node_degree();
                feature_vector[8] = std_set_degree[i] / g.get_max_node_degree();
                feature_vector[9] = objective_scores[i] / max_obm;
                feature_vector[10] = ranking_scores[i] / max_rbm;
                feature_vector[11] = corr_xy[i];
                feature_vector[12] = corr_xr[i];
                feature_vector[13] = lp_solution[i];
                feature_vector[14] = reduced_cost[i];
                // compute class label for each mis by decision tree
                #pragma omp critical
                ml_score[i] = decision_tree(feature_vector);
            }
        }
    }


    void Reduce::selecting_high_quality_mis(){
//        for (int i = 0; i < num_mis; ++i){
//            if (ml_score[i] > threshold_ml){
//                predicted_value[i] = 1;
//            }else{
//                predicted_value[i] = 0;
//            }
//        }

//        //linear svm
        for (int i = 0; i < num_mis; ++i){
            predicted_value[i] = 0;
        }
        vector<int> sort_idx(num_mis);
        iota(sort_idx.begin(), sort_idx.end(), 0);
        vector<float> score_copy(ml_score);
        sort(sort_idx.begin(), sort_idx.end(), [&score_copy](int i1, int i2) {return score_copy[i1] > score_copy[i2];});
        for (int i = 0; i < num_mis * prop; ++i){
            predicted_value[sort_idx[i]] = 1;
        }

        std::cout << "largest ml score is : " << ml_score[sort_idx[0]] << endl;

//        // random
//        for (int i = 0; i < num_mis; ++i){
//            if ((double) rand() / (RAND_MAX) < prop){
//                predicted_value[i] = 1;
//            }
//        }
    }

    void Reduce::updating_mis_crossover(){
        vector<int> selected_mis;
        vector<int> unselected_mis;
        for (int i = 0; i < num_mis; ++i){
            if (predicted_value[i] == 1){
                selected_mis.push_back(i);
            }else{
                unselected_mis.push_back(i);
                mis_sorted[i] = 0;
            }
        }
        cout << "number of selected MISs is : " << selected_mis.size() << endl;
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = unselected_mis.size() * threadnum / num_thread;
            int high = unselected_mis.size() * (threadnum+1) / num_thread;
            random_device rd;
            mt19937 gen(rd() + threadnum);
            srand (time(NULL) + threadnum);

            int s, s1, s2, idx, idx1, idx2, v;
            vector<int> candidates(g.size());
            int nb_candidates, num, k1, k2;
            for (int i = low; i < high; ++i){
                s = unselected_mis[i]; // mis to be updated
                mis_sets[s].clear();
                idx1 = rand() % selected_mis.size();
                idx2 = rand() % selected_mis.size();
                s1 = selected_mis[idx1];
                s2 = selected_mis[idx2];

                if (mis_sorted[s1] == 0){
                    sort(mis_sets[s1].begin(), mis_sets[s1].end());
                    mis_sorted[s1] = 1;
                }
                if (mis_sorted[s2] == 0){
                    sort(mis_sets[s2].begin(), mis_sets[s2].end());
                    mis_sorted[s2] = 1;
                }

                nb_candidates = g.size();
                for (int j = 0; j < nb_candidates; ++j){
                    candidates[j] = j;
                }
                k1 = 0;
                k2 = 0;
                while(k1 < mis_sets[s1].size() && k2 < mis_sets[s2].size()){
                    if (mis_sets[s1][k1] < mis_sets[s2][k2]){
                        k1++;
                    } else if (mis_sets[s1][k1] > mis_sets[s2][k2]){
                        k2++;
                    } else{
                        v = mis_sets[s1][k1];
                        mis_sets[s].push_back(v);
                        num = 0;
                        for (int k = 0; k < nb_candidates; ++k){
                            if (adj_matrix[v][candidates[k]] == 0 && candidates[k] != v){
                                candidates[num] = candidates[k];
                                num++;
                            }
                        }
                        nb_candidates = num;
                        k1++;
                        k2++;
                    }
                }

                while (nb_candidates > 0){
                    idx = rand() % nb_candidates;
                    v = candidates[idx];
                    mis_sets[s].push_back(v);
                    num = 0;
                    for (int j = 0; j < nb_candidates; ++j){
                        if (adj_matrix[v][candidates[j]] == 0 && candidates[j] != v){
                            candidates[num] = candidates[j];
                            num++;
                        }
                    }
                    nb_candidates = num;
                }
            }
        }
    }


    void Reduce::updating_mis_random(){
        vector<int> selected_mis;
        vector<int> unselected_mis;
        for (int i = 0; i < num_mis; ++i){
            if (predicted_value[i] == 1){
                selected_mis.push_back(i);
            }else{
                unselected_mis.push_back(i);
                mis_sorted[i] = 0;
            }
        }
        cout << "number of selected MISs is : " << selected_mis.size() << endl;
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = unselected_mis.size() * threadnum / num_thread;
            int high = unselected_mis.size() * (threadnum+1) / num_thread;
            random_device rd;
            mt19937 gen(rd() + threadnum);
            srand (time(NULL) + threadnum);

            int s, s1, s2, idx, idx1, idx2, v;
            vector<int> candidates(g.size());
            int nb_candidates, num, k1, k2;
            for (int i = low; i < high; ++i){
                s = unselected_mis[i]; // mis to be updated
                mis_sets[s].clear();
                idx1 = rand() % selected_mis.size();
                idx2 = rand() % selected_mis.size();
                s1 = selected_mis[idx1];
                s2 = selected_mis[idx2];

                nb_candidates = g.size();
                for (int j = 0; j < nb_candidates; ++j){
                    candidates[j] = j;
                }
                while (nb_candidates > 0){
                    if (nb_candidates == g.size()){
                        v = s % g.size();
                    } else{
                        idx = rand() % nb_candidates;
                        v = candidates[idx];
                    }

                    mis_sets[s].push_back(v);
                    num = 0;
                    for (int j = 0; j < nb_candidates; ++j){
                        if (adj_matrix[v][candidates[j]] == 0 && candidates[j] != v){
                            candidates[num] = candidates[j];
                            num++;
                        }
                    }
                    nb_candidates = num;
                }
            }
        }
    }


    void Reduce::compute_selected_mis_sets(){
        num_mis_selected = 0;
        for (int i = 0; i < num_mis; ++i){
            num_mis_selected += predicted_value[i];
        }
        selected_mis_sets.clear();
        selected_mis_sets.resize(num_mis_selected);
        int k = 0;
        for (int i = 0; i < num_mis; ++i){
            if (predicted_value[i] == 1){
                selected_mis_sets[k].clear();
                for (int j = 0; j < mis_sets[i].size(); ++j){
                    selected_mis_sets[k].push_back(mis_sets[i][j]);
                }
                k++;
            }
        }
    }


    void Reduce::removing_duplicates(){
        // sort selected MISs
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum+1) / num_thread;
            for (int i = low; i < high; ++i){
                if (predicted_value[i] == 1 && mis_sorted[i] == 0){
                    sort(mis_sets[i].begin(), mis_sets[i].end());
                    mis_sorted[i] = 1;
                }
            }
        }

        // remove duplicated MISs that are selected
        #pragma omp parallel
        {
            int threadnum = omp_get_thread_num();
            int low = num_mis * threadnum / num_thread;
            int high = num_mis * (threadnum+1) / num_thread;
            int k;
            for (int i = low; i < high; ++i){
                if (predicted_value[i] == 1){
                    for (int j = i+1; j < num_mis; ++j){
                        if (predicted_value[j] == 1){
                            if (mis_sets[i].size() == mis_sets[j].size()){
                                for (k = 0; k < mis_sets[i].size(); ++k){
                                    if (mis_sets[i][k] != mis_sets[j][k]){
                                        break;
                                    }
                                }
                                if (k == mis_sets[i].size()){
                                    #pragma omp critical
                                    predicted_value[j] = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    void Reduce::ml_evolving_mis(){
        initializing_mis();

        for (int i = 0; i < iterations; ++i){
            cout << "iter : " << i << endl;
            evaluating_mis_linear_svm();
//            evaluating_mis_decision_tree();
            selecting_high_quality_mis();
            repair();
            removing_duplicates();
            if (i < iterations - 1){
                updating_mis_random();
//                updating_mis_crossover();
            }
//            compute_selected_mis_sets();
//            solve_submip();
        }
//        for (int i = 0; i < num_mis; ++i){
//            predicted_value[i] = 1;
//        }
        compute_selected_mis_sets();
    }
}
