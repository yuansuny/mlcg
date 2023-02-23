#include "instance.h"
#include "solver.h"
#include "reduce.h"
#include "training.h"
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <iomanip>
#include <sstream>

string ToString(int value, int digitsCount){
    ostringstream os;
    os << setfill('0') << setw(digitsCount) << value;
    return os.str();
}

double get_cpu_time(){
    return (double)clock() / CLOCKS_PER_SEC;
}

double get_wall_time(){
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

int main(int argc, char* argv[]) {
    using namespace GCP;
    using namespace std;

    const string input_dir = "../MATILDA_Graphs/";
    const string output_dir = "../results/";

    double cutoff = 3600;
    int runs = 25;
    int d;
    sscanf(argv[1], "%d", &d);

    string input_file_name = "g" + ToString(d, 4);
    cout << input_file_name << endl;

    int retrain = 0; // set retrain = 1 to retrain the ML model
    const int num_training_files = 100;
    vector<string> training_file_name;
    for (int i = 1; i <= num_training_files; ++i){
        string opt_file_name = input_dir + "g" + ToString(i, 4) + ".allsol";
        ifstream opt_file(opt_file_name);
//        if (opt_file && i != 14 && i != 65 && i != 66 && i != 73 && i != 76 && i != 85 && i != 90 && i != 94){
        if (opt_file){
            training_file_name.push_back("g" + ToString(i, 4));
        }
    }

    if (retrain == 1){
        auto training = Training(training_file_name, "../../MATILDA_Graphs/");
//        training.generate_training_model_svm();
        training.generate_training_model_svm_linear();
    }

    string output_obj_filename, output_sol_filename, output_time_filename;
    output_obj_filename = output_dir + input_file_name + "_res_obj.csv";
    output_sol_filename = input_dir + input_file_name + ".allsol";
    output_time_filename = output_dir + input_file_name + "_res_time.csv";
    ofstream output_file_obj (output_obj_filename, ios::trunc);
    ofstream output_file_time (output_time_filename, ios::trunc);


    if (output_file_obj.is_open()){
        output_file_obj << "|V|, " << "Y_s, " << "|V_s|, " << "UB, " << "LB" << endl;
    } else{
        cout << "Cannot open the output file " + output_obj_filename << endl;
        return 0;
    }
    output_file_obj.close();

    if (output_file_time.is_open()){
        output_file_time << "t_read, " << "t_sample, " << "t_reduce, " << "t_total" << endl;
    } else{
        cout << "Cannot open the output file " + output_time_filename << endl;
        return 0;
    }
    output_file_time.close();

    for (int i = 0; i < runs; ++i){
        double w0 = get_wall_time();
        const auto instance = Instance(input_file_name, input_dir);
        auto num_variables = instance.get_n_sets();
        cout << "Original number of mis sets is " << num_variables << endl;
        output_file_obj.open(output_obj_filename, ios::app);
        output_file_obj << num_variables << ", ";

        double w1 = get_wall_time();
        double c1 = get_cpu_time();
        cout << "Reading time is " << w1 - w0 << "s" << endl;
        output_file_time.open(output_time_filename, ios::app);
        output_file_time << w1 - w0 <<", ";

        auto reduce = Reduce(instance);
        double w2 = get_wall_time();
        double c2 = get_cpu_time();
        cout << "Sampling wall time is  " << w2 - w1 << "s" << endl;
        cout << "Sampling CPU time is  " << c2 - c1 << "s" << endl;
        output_file_time << w2 - w1 <<", ";

        reduce.ml_evolving_mis();

        double w3 = get_wall_time();
        double c3 = get_cpu_time();
        cout << "evolving variables wall time is  " << w3 - w2 << "s" << endl;
        cout << "evolving variables CPU time is  " << c3 - c2 << "s" << endl;

        double w4 = get_wall_time();
        output_file_obj << reduce.get_objective_value_sampling() <<", ";
        cout << "Best objective value found from sampling is  " << reduce.get_objective_value_sampling() << endl;

        output_file_obj << reduce.get_num_mis_selected() <<", ";
        cout << "Number of variables left is " << reduce.get_num_mis_selected() << endl;

        double w5 = get_wall_time();
        cout << "Reduction time is  " << w5 - w1 << "s" << endl;
        output_file_time << w5 - w1 <<", ";

        auto solver = Solver(instance, reduce, cutoff-(w5-w0));
        solver.solve_gcp_gurobi();

        output_file_obj << solver.get_objective_value() << ", " << solver.get_objective_bound() << endl;
        cout << "Best objective value found is  " << solver.get_objective_value() << endl;
        cout << "Best objective bound found is  " << solver.get_objective_bound() << endl;
        output_file_obj.close();

        auto w6 = get_wall_time();
        cout << "Total time is  " << w6 - w0 << "s" << endl;
        output_file_time << w6 - w0 << endl;
        output_file_time.close();

    }

    return 0;
}
