// This file deals with functional target angle values
#include <lsc/basic.h>
#include <lsc/tools.h>
#include <igl/exact_geodesic.h>
void assign_angles_based_on_funtion_values(const Eigen::VectorXd &fvalues, const double angle_large_function, const double angle_small_function,
                                           std::vector<double>& angle_degree)
{
    int vnbr=fvalues.size();
    angle_degree.resize(vnbr);
    
    double fmax=fvalues.maxCoeff();
    double fmin=fvalues.minCoeff();
    double favg=(fmax+fmin)/2;
    double itv=(angle_large_function-angle_small_function) /(fmax-fmin);
    for(int i=0;i<vnbr;i++){
        double value=fvalues[i];
        double ang=angle_small_function+(value-fmin)*itv;
        angle_degree[i]=ang;
    }

}

// IVids is a vector of size ninner, mapping inner ver ids to ver ids.
void get_geodesic_distance(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F, const std::vector<int> &IVids,
                           const std::vector<int> &idinner, Eigen::VectorXd &D)
{
    
    Eigen::VectorXi VS, FS, VT, FT;
    VS.resize(idinner.size());
    for (int i = 0; i < idinner.size(); i++)
    {
        int id = idinner[i]; // the id in inner vers
        int vm = IVids[id];
        VS[i] = vm;
    }
    // All vertices are the targets
    VT.setLinSpaced(V.rows(), 0, V.rows() - 1);
    igl::exact_geodesic(V, F, VS, FS, VT, FT, D);
}

// the strategy is: within the percentage % of the distance, the angle increase linearly.
void assign_pg_angle_based_on_geodesic_distance(const Eigen::VectorXd &D, const double percentage, Eigen::VectorXd &angle)
{
    double dismax = D.maxCoeff();
    double dthreads = dismax * percentage / 100.;
    angle.resize(D.size());
    // std::cout<<"dis max, "<<dismax<<", threadshold "<<dthreads<<", percentage, "<<percentage<<std::endl;
    for (int i = 0; i < angle.size(); i++)
    {
        double dis = D[i];
        double a;
        if (dis < dthreads)
        {
            a = dis / dthreads * 90;
        }
        else
        {
            a = 90;
        }
        angle[i] = a;
    }
}