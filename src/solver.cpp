#include "solver.h"
#include <cmath>
#include <limits>
////#include "Timer.h"
//#include "instance.h"
//#include <iostream>
//#include <vector>
//#include <omp.h>
//#include "gurobi_c++.h"

using namespace std;

namespace GCP {
    void Solver::solve_gcp_gurobi() {
        int n = r.get_num_mis_selected();
        int m = ins.size();
        vector<vector<bool>> mis_sets_binary(n, vector<bool>(m, 0));
        vector<vector<int>> mis_sets = r.get_selected_mis_sets();
        int v;
        for (int i = 0; i < n; ++i){
//            cout << "MIS  " << i << " : ";
            for (int j = 0; j < mis_sets[i].size(); ++j){
                v = mis_sets[i][j];
                mis_sets_binary[i][v] = 1;
//                cout << v << ", " ;
            }
//            cout << endl;
        }
        cout << "number of mis sets used is " << n << endl;

        // setup the model now
        GRBEnv *env;
        vector<GRBVar> x;
        try {
            env = new GRBEnv();
            GRBModel model = GRBModel(*env);
            model.getEnv().set(GRB_IntParam_OutputFlag, 1);
            model.set(GRB_StringAttr_ModelName, "MIP_GCP");
            // Create variables and set them to be binary
            x.resize(n);
            for (int i = 0; i < n; ++i){
                x[i] = model.addVar(0,1,0,GRB_BINARY);
            }
            model.update();

            // each vertex is covered by at least one set
            for (int j = 0; j < m; ++j){
                GRBLinExpr rtot = 0;
                for (int i = 0; i < n; ++i){
                    rtot += mis_sets_binary[i][j] * x[i];
                }
                model.addConstr(rtot >= 1, "");
            }
            model.update();

            // set time limit
            model.set(GRB_DoubleParam_TimeLimit, cutoff);
            // set number of thread
            model.set(GRB_IntParam_Threads, omp_get_max_threads());
            // set random seed
            srand (time(NULL));
            model.set(GRB_IntParam_Seed, rand()%10000);

            model.set(GRB_IntParam_Presolve, 1);
//            model.set(GRB_IntParam_MIPFocus, 1);
//            model.set(GRB_DoubleParam_ImproveStartNodes, 0.0);
//            model.set(GRB_DoubleParam_ImproveStartTime, 0.0);

            // the objective
            GRBLinExpr tot=0;
            for(int i = 0; i < n; ++i){
                tot += x[i];
            }
            model.setObjective(tot,GRB_MINIMIZE);
            model.update();
            model.optimize();
            objVal = model.get(GRB_DoubleAttr_ObjVal);
            objBound = model.get(GRB_DoubleAttr_ObjBound);
            opt_sol.resize((int) (objVal + 0.5));
            int k = 0;
            cout << "optimal solution is : " << endl;
            for (int i = 0; i < n; ++i){
                if (x[i].get(GRB_DoubleAttr_X) > 0.5){
                    cout << "color " << k << " : " ;
                    opt_sets.push_back(i);
                    for (int j = 0; j < mis_sets[i].size(); ++j){
                        opt_sol[k].push_back(mis_sets[i][j]);
                        cout <<  mis_sets[i][j] << ", " ;
                    }
                    cout << endl;
                    k++;
                }
            }
        }

        catch(GRBException e) {
            cout << "Error code = " << e.getErrorCode() << endl;
            cout << e.getMessage() << endl;
        }
        catch(...) {
            cout << "Exception during optimization" << endl;
        }

        delete env;
    }



    void Solver::computing_all_optimal_solutions() {
        vector<vector<int>> mis_sets = ins.get_mis_sets();
        int n = mis_sets.size();
        int m = ins.size();
        vector<vector<bool>> mis_sets_binary(n, vector<bool>(m, 0));
        int v;
        for (int i = 0; i < n; ++i){
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
        model.set(GRB_StringAttr_ModelName, "MIP_GCP");
        // Create variables and set them to be binary
        x.resize(n);
        for (int i = 0; i < n; ++i){
            x[i] = model.addVar(0,1,0,GRB_BINARY);
        }
        model.update();
        // each vertex is covered by at least one set
        for (int j = 0; j < m; ++j){
            GRBLinExpr rtot = 0;
            for (int i = 0; i < n; ++i){
                rtot += mis_sets_binary[i][j] * x[i];
            }
            model.addConstr(rtot >= 1, "");
        }
        model.update();
        model.set(GRB_IntParam_Threads, omp_get_max_threads());
        model.set(GRB_IntParam_Presolve, 1);
        // the objective
        GRBLinExpr tot=0;
        for(int i = 0; i < n; ++i){
            tot += x[i];
        }
        model.setObjective(tot,GRB_MINIMIZE);
        model.update();

        // computing all optimal solutions
        all_opt_sets_binary = vector<bool>(n, 0);
        for (int i = 0; i < opt_sets.size(); ++i){
            all_opt_sets_binary[opt_sets[i]] = 1;
        }

        int k = 0;
        for (int i = 0; i < n; ++i){
            if (all_opt_sets_binary[i] == 0){
                x[i].set(GRB_DoubleAttr_LB, 1);
                model.update();
                model.optimize();
                double obj = model.get(GRB_DoubleAttr_ObjVal);
                if (obj - objVal < 0.5){
                    for (int j = 0; j < n; ++j){
                        if (x[j].get(GRB_DoubleAttr_X) > 0.5){
                            all_opt_sets_binary[j] = 1;
                        }
                    }
                }
                k++;
                cout << k << "th problem" << "; objective: " << obj << "; optimal: " << objVal << endl;
                x[i].set(GRB_DoubleAttr_LB, 0);
                model.reset();
            }
        }
        delete env;
    }
}
