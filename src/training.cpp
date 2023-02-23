#include "training.h"

namespace GCP {
    Training::Training(std::vector<std::string> training_files, std::string input_dir) : training_files{training_files}, input_dir{input_dir}{
        std::cout << "number of training graphs is: " << training_files.size() << "\n";
        construct_training_set();
    }

    void Training::construct_training_set(){
        std::string train_s = train_data_dir  + train_file_name;
        char train_data[train_s.size()+1];
        strcpy(train_data, train_s.c_str());

        std::ofstream train_file(train_data, std::ios::trunc);
        if (! train_file.is_open()){
            std::cout << "Cannot open the output file " <<  train_data << "\n";
            return;
        }
        train_file.close();

        std::uint64_t num0 = 0;
        std::uint64_t num1 = 0;
        for (int d = 0; d < training_files.size(); ++d){
            auto ins = Instance(training_files[d], input_dir);

            if (ins.get_optimal_ratio() > 0.5){
                cout << training_files[d] << " is not used to construct training the set" << endl;
                continue;
            }

            auto reduce = Reduce(ins);
            reduce.compute_correlation_based_measure();
            reduce.compute_ranking_based_measure();
            reduce.compute_rank_correlation_measure();
            reduce.compute_objective_based_measure();
            reduce.compute_lp_solution();
            train_file.open(train_data, std::ios::app);
            for (int i = 0; i < ins.get_n_sets(); ++i){
                if (ins.get_optimal_value(i) > 0.5){
                    num1++;
//                }else if (reduce.get_rbm_value(i) > 0){
                }else if (i % 1 == 0){
                    num0++;
                }else{
                    continue;
                }
                train_file << ins.get_optimal_value(i) << ", ";
                train_file << reduce.get_cbm_value(i) << ", ";
                train_file << reduce.get_rbm_value(i) / reduce.get_max_rbm() << ", ";
                train_file << reduce.get_rcm_value(i) << ", ";
                train_file << reduce.get_obm_value(i) / reduce.get_max_obm() << ", ";
                train_file << ins.get_max_rel_set_size(i) << ", ";
                train_file << ins.get_ave_rel_set_size(i) << ", ";
                train_file << ins.get_min_rel_set_size(i) << ", ";
                train_file << ins.get_std_rel_set_size(i) << ", ";
                train_file << (double) ins.get_set_size(i) / (double) ins.get_max_set_size() << ", ";
                train_file << ins.get_max_set_degree(i) / ins.get_max_node_degree() << ", ";
                train_file << ins.get_ave_set_degree(i) / ins.get_max_node_degree() << ", ";
                train_file << ins.get_min_set_degree(i) / ins.get_max_node_degree() << ", ";
                train_file << ins.get_std_set_degree(i) / ins.get_max_node_degree() << ", ";
                train_file << reduce.get_lp_solution(i) << " " << "\n";
            }

            train_file.close();
        }
        std::cout << "num0 is " << num0 << "; " << "num1 is " << num1 <<  std::endl;
        weight = (double)alpha * num0/num1;
    }

    void Training::generate_training_model_svm(){
        std::string train_s = train_data_dir  + train_file_name;
        std::string model_s = train_data_dir + train_model_name;
        char train_data[train_s.size()+1];
        char model_file[model_s.size()+1];
        strcpy(train_data, train_s.c_str());
        strcpy(model_file, model_s.c_str());
        std::cout << train_data << std::endl;
        std::cout << model_file << std::endl;
        std::cout << weight << std::endl;
        std::cout << kernel_type << std::endl;
        std::cout << prob << std::endl;
        svm_train_model(train_data, model_file, weight, kernel_type, prob);

//        const int rem_result = remove(train_data);
//        if(rem_result == 0){
//            std::cout << "Successfully remove training data file" << std::endl;
//        } else {
//            std::cout << "No such training data file " << std::endl;
//        }
    }

    void Training::generate_training_model_svm_linear(){
        std::string train_s = train_data_dir  + train_file_name;
        std::string model_s = train_data_dir + train_model_name;
        char train_data[train_s.size()+1];
        char model_file[model_s.size()+1];
        strcpy(train_data, train_s.c_str());
        strcpy(model_file, model_s.c_str());
        linear_svm_train_model(train_data, model_file, weight);

//        const int rem_result = remove(train_data);
//        if(rem_result == 0){
//            std::cout << "Successfully remove training data file" << std::endl;
//        } else {
//            std::cout << "No such training data file " << std::endl;
//        }
    }
}
