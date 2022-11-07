#include<lsc/mesh_opt.h>
#include <lsc/tools.h>

// the variables are sorted as v0x, v1x, ...vnx, v0y, v1y,...,vny, v0z, v1z, ..., vnz
// xyz is 0, 1, or 2
void location_in_sparse_matrix(const int vnbr, const int vderivate, const int dxyz, const int vcoff, const int cxyz, int& row, int &col){
    int dlocation = vderivate + dxyz * vnbr;
    int clocation = vcoff + cxyz * vnbr;
    row=dlocation;
    col=clocation;
}

// the coefficient matrix of the cross pair.
// the results are 3 sparse matrices M0, M1, M2, where alpha* (nx*M0+ny*M1+nz*M2)*fvalues is the jacobian of 
// the term alpha*(V(vid0) X V(vid1)).dot(norm)
void get_derivate_coeff_cross_pair(const int vid0, const int vid1, const int vsize, const double alpha, std::array<spMat, 3> &cmats)
{
    cmats[0].resize(vsize*3, vsize*3);
    cmats[1].resize(vsize*3, vsize*3);
    cmats[2].resize(vsize*3, vsize*3);
    std::vector<Trip> triplets;
    int row, col;
    triplets.resize(4);

    // M0
    location_in_sparse_matrix(vsize,vid0,1,vid1,2,row,col);// y0z1
    triplets[0]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid1,2,vid0,1,row,col);// y0z1
    triplets[1]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid1,1,vid0,2,row,col);// -y1z0
    triplets[2]=Trip(row,col,-alpha);
    location_in_sparse_matrix(vsize,vid0,2,vid1,1,row,col);// -y1z0
    triplets[3]=Trip(row,col,-alpha);
    cmats[0].setFromTriplets(triplets.begin(),triplets.end());

     // M1
    location_in_sparse_matrix(vsize,vid1,0,vid0,2,row,col);// x1z0
    triplets[0]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid0,2,vid1,0,row,col);// x1z0
    triplets[1]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid0,0,vid1,2,row,col);// -x0z1
    triplets[2]=Trip(row,col,-alpha);
    location_in_sparse_matrix(vsize,vid1,2,vid0,0,row,col);// -x0z1
    triplets[3]=Trip(row,col,-alpha);
    cmats[1].setFromTriplets(triplets.begin(),triplets.end());

     // M2
    location_in_sparse_matrix(vsize,vid0,0,vid1,1,row,col);// x0y1
    triplets[0]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid1,1,vid0,0,row,col);// x0y1
    triplets[1]=Trip(row,col,alpha);
    location_in_sparse_matrix(vsize,vid1,0,vid0,1,row,col);// -x1y0
    triplets[2]=Trip(row,col,-alpha);
    location_in_sparse_matrix(vsize,vid0,1,vid1,0,row,col);// -x1y0
    triplets[3]=Trip(row,col,-alpha);
    cmats[2].setFromTriplets(triplets.begin(),triplets.end());
}

// for each pair of cross product v1 x v2, alpha * v1 x v2.dot(norm) is the form of energy
void get_alpha_associate_with_cross_pairs(const double t1, const double t2, std::array<double,8> &result)
{
    result[0] = 1 - t1;
    result[1] = t1;
    result[2] = 1 - t2;
    result[3] = t2;
    result[4] = t1 * (t2 - 1);
    result[5] = -t1 * t2;
    result[6] = (t1 - 1) * (1 - t2);
    result[7] = t2 * (t1 - 1);
}
void lsTools::initialize_mesh_optimization()
{
    int ninner=ActInner.size();
    int vsize=V.rows();
    
    MCt.resize(ninner);
    std::vector<double> t1,t2;
    t1.resize(ninner);
    t2.resize(ninner);
    
    std::vector<std::array<std::array<spMat,3>,8>> MJCmats;
    MJCmats.resize(ninner);
    MJsimp.resize(ninner);
    
    for (int i = 0; i < ninner; i++)
    {
        if(ActInner[i]==false){
            continue;
        }
        int vm = IVids[i];
        CGMesh::HalfedgeHandle inhd = Vheh0[i], outhd=Vheh1[i];
        int v1=lsmesh.from_vertex_handle(inhd).idx();
        int v2=lsmesh.to_vertex_handle(inhd).idx();
        int v3=lsmesh.from_vertex_handle(outhd).idx();
        int v4=lsmesh.to_vertex_handle(outhd).idx();
        std::array<double,8> con_coeffs; // the coeffecients associate to each cross pair
        t1[i]=get_t_of_value(fvalues[vm], fvalues[v1],fvalues[v2]);
        t2[i]=get_t_of_value(fvalues[vm], fvalues[v3],fvalues[v4]);
        get_alpha_associate_with_cross_pairs(t1[i],t2[i],MCt[i]);
        if(t1[i]<-SCALAR_ZERO||t1[i]>1+SCALAR_ZERO||t2[i]<-SCALAR_ZERO||t2[i]>1+SCALAR_ZERO){
            std::cout<<"ERROR in Mesh Opt: Using Wrong Triangle For Finding Level Set"<<std::endl;
            assert(false);
        }
        // rare special cases
        if(v2==v3){
            //TODO
        }
        if(v1==v4){
            //TODO
        }
        std::array<spMat, 3> cmats; // the elementary mats for one pair of cross product.
        get_derivate_coeff_cross_pair(v1,vm,vsize,MCt[i][0],cmats);
        MJCmats[i][0]=cmats;
        get_derivate_coeff_cross_pair(v2,vm,vsize,MCt[i][1],cmats);
        MJCmats[i][1]=cmats;
        get_derivate_coeff_cross_pair(vm,v3,vsize,MCt[i][2],cmats);
        MJCmats[i][2]=cmats;
        get_derivate_coeff_cross_pair(vm,v4,vsize,MCt[i][3],cmats);
        MJCmats[i][3]=cmats;

        get_derivate_coeff_cross_pair(v2,v3,vsize,MCt[i][4],cmats);
        MJCmats[i][4]=cmats;
        get_derivate_coeff_cross_pair(v2,v4,vsize,MCt[i][5],cmats);
        MJCmats[i][5]=cmats;
        get_derivate_coeff_cross_pair(v1,v3,vsize,MCt[i][6],cmats);
        MJCmats[i][6]=cmats;
        get_derivate_coeff_cross_pair(v1,v4,vsize,MCt[i][7],cmats);
        MJCmats[i][7]=cmats;
        
        // simplify the results, the 3 matrices corresponds to normx, normy, normz.
        MJsimp[i][0].resize(vsize*3, vsize*3);
        MJsimp[i][1].resize(vsize*3, vsize*3);
        MJsimp[i][2].resize(vsize*3, vsize*3);
        for(int j=0;j<8;j++){
            MJsimp[i][0]+=MJCmats[i][j][0];
            MJsimp[i][1]+=MJCmats[i][j][1];
            MJsimp[i][2]+=MJCmats[i][j][2];
        }
    }
    Mt1=t1;
    Mt2=t2;
}
void lsTools::calculate_mesh_opt_function_values(const double angle_degree,Eigen::VectorXd& lens){
    double angle_radian = angle_degree * LSC_PI / 180.; // the angle in radian
    double cos_angle=cos(angle_radian);
    int ninner=ActInner.size();
    MEnergy.resize(ninner);
    lens.resize(ninner);

    for (int i = 0; i < ninner; i++)
    {
        if(ActInner[i]==false){
            continue;
        }
        int vm = IVids[i];
        CGMesh::HalfedgeHandle inhd = Vheh0[i], outhd=Vheh1[i];
        int v1=lsmesh.from_vertex_handle(inhd).idx();
        int v2=lsmesh.to_vertex_handle(inhd).idx();
        int v3=lsmesh.from_vertex_handle(outhd).idx();
        int v4=lsmesh.to_vertex_handle(outhd).idx();
        Eigen::Vector3d ver0=V.row(v1)+(V.row(v2)-V.row(v1))*Mt1[i];
        Eigen::Vector3d ver1=V.row(vm);
        Eigen::Vector3d ver2=V.row(v3)+(V.row(v4)-V.row(v3))*Mt2[i];
        Eigen::Vector3d cross=(ver1-ver0).cross(ver2-ver1);
        lens[i]=cross.norm();// TODO deal with colinear cases
        Eigen::Vector3d norm=norm_v.row(vm);
        double value=cross.dot(norm)/lens[i]-cos_angle;
        MEnergy[i]=value;
        
    }

}
spMat Jacobian_transpose_mesh_opt_on_ver( const std::array<spMat,3> &JC, 
const Eigen::Vector3d& norm, const spMat &SMfvalues){

    spMat result = (JC[0] * norm[0] + JC[1] * norm[1] + JC[2] * norm[2]) * SMfvalues;

    return result;
}
void lsTools::assemble_solver_mesh_opt_part(spMat& H, Eigen::VectorXd &B){
    int ninner=ActInner.size();
    int vsize=V.rows();
    Eigen::MatrixXd Mfunc(vsize*3, 1);
    Mfunc.topRows(vsize)=V.col(0);
    Mfunc.middleRows(vsize,vsize)=V.col(1);
    Mfunc.bottomRows(vsize)=V.col(2);
    spMat SMfunc = Mfunc.sparseView();// sparse function values
    spMat JTJ;
    JTJ.resize(vsize*3,vsize*3);
    Eigen::VectorXd mJTF;
    mJTF.resize(vsize*3);
    Eigen::VectorXd lens;
    calculate_mesh_opt_function_values(pseudo_geodesic_target_angle_degree,lens);
    for(int i=0;i<ninner;i++){
        if(ActInner[i]==false){
            continue;
        }
        int vid=IVids[i];
        Eigen::Vector3d norm=norm_v.row(vid);
        spMat JT=Jacobian_transpose_mesh_opt_on_ver(MJsimp[i],norm,SMfunc)/lens[i];
        JTJ+=JT*JT.transpose();
        mJTF+=-JT*MEnergy[i];
    }
    H=JTJ;
    B=mJTF;
}
