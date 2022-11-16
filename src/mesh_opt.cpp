#include<lsc/mesh_opt.h>
#include <lsc/tools.h>
#include <igl/grad.h>
#include <igl/hessian.h>
#include <igl/curved_hessian_energy.h>
#include <igl/repdiag.h>
#include <igl/Timer.h>


bool vector_contains_NAN(Eigen::VectorXd& B){
    for(int i=0;i<B.size();i++){
        if(isnan(B[i])){
            return true;
        }
    }
    return false;
}
// the variables are sorted as v0x, v1x, ...vnx, v0y, v1y,...,vny, v0z, v1z, ..., vnz
// xyz is 0, 1, or 2
void location_in_sparse_matrix(const int vnbr, const int vderivate, const int dxyz, const int vcoff, const int cxyz, int& row, int &col){
    int dlocation = vderivate + dxyz * vnbr;
    int clocation = vcoff + cxyz * vnbr;
    row=dlocation;
    col=clocation;
}
void location_in_sparse_matrix(const int vnbr, const int vderivate, const int dxyz, int &col){
    col = vderivate + dxyz * vnbr;
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
    assert(ninner>0);
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
            std::cout<<"ERROR in Mesh Opt: Using Wrong Triangle For Finding Level Set, "<<t1[i]<<" "<<t2[i]<<std::endl;
            assert(false);
        }
        // rare special cases
        if(v2==v3){
            //TODO
            std::cout << "bad mesh !" << std::endl;
        }
        if(v1==v4){
            //TODO
            std::cout << "bad mesh !" << std::endl;
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
    MEnergy = Eigen::VectorXd::Zero(ninner);
    PeWeight = Eigen::VectorXd::Zero(ninner);
    lens = Eigen::VectorXd::Zero(ninner);

    for (int i = 0; i < ninner; i++)
    {
        if(ActInner[i]==false){
            PeWeight[i]=0;
            MEnergy[i]=0;
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
        double lg1=(ver1-ver0).norm();
        double lg2=(ver2-ver1).norm();
        
        if(lg1<1e-16 || lg2<1e-16){
            PeWeight[i]=0;// it means the g1xg2 will not be accurate
        }
        else{
            Eigen::Vector3d norm = norm_v.row(vm);
            double cos_real = cross.normalized().dot(norm);
            double cos_diff = fabs(cos_real-cos_angle)/2;
            PeWeight[i] = cos_diff;
        }
        lens[i]=cross.norm();
        if(lens[i]<1e-16){// if it is colinear, we prefer to fix this point first
            lens[i]=1e6;
        }
        Eigen::Vector3d norm=norm_v.row(vm);
        double value=cross.dot(norm)/lens[i]-cos_angle;
        MEnergy[i]=value;
        
    }

}
void lsTools::calculate_mesh_opt_expanded_function_values(const Eigen::VectorXd &vars,  const double angle_degree,const bool first_compute, Eigen::VectorXd& lens) {
    
    double angle_radian = angle_degree * LSC_PI / 180.; // the angle in radian
    double cos_angle = cos(angle_radian);
    int ninner = ActInner.size();
    int vnbr = V.rows();
    MEnergy = Eigen::VectorXd::Zero(ninner);
    PeWeight = Eigen::VectorXd::Zero(ninner);
    lens = Eigen::VectorXd::Zero(ninner);
    std::vector<Trip> tripletes;
    tripletes.reserve(ninner * 12 * 4);// the number of rows is ninner*8, the number of cols is vnbr * 3 + ninner * 3 (all the vertices and auxiliary vars)
	Eigen::VectorXd MTenergy = Eigen::VectorXd::Zero(ninner * 8); // mesh total energy
    

    for (int i = 0; i < ninner; i++)
    {
        if (ActInner[i] == false) {
            continue;
        }
        int vm = IVids[i];
        CGMesh::HalfedgeHandle inhd = Vheh0[i], outhd = Vheh1[i];
        int v1 = lsmesh.from_vertex_handle(inhd).idx();
        int v2 = lsmesh.to_vertex_handle(inhd).idx();
        int v3 = lsmesh.from_vertex_handle(outhd).idx();
        int v4 = lsmesh.to_vertex_handle(outhd).idx();
        Eigen::Vector3d ver0 = V.row(v1) + (V.row(v2) - V.row(v1)) * Mt1[i];
        Eigen::Vector3d ver1 = V.row(vm);
        Eigen::Vector3d ver2 = V.row(v3) + (V.row(v4) - V.row(v3)) * Mt2[i];
        double t1 = Mt1[i];
        double t2 = Mt2[i];
        // the locations
        int lrx = i + vnbr * 3;
        int lry = i + vnbr * 3 + ninner;
        int lrz = i + vnbr * 3 + ninner * 2;
        int lfx = v1;
        int lfy = v1 + vnbr;
        int lfz = v1 + vnbr * 2;
        int ltx = v2;
        int lty = v2 + vnbr;
        int ltz = v2 + vnbr * 2;
        int lmx = vm;
        int lmy = vm + vnbr;
        int lmz = vm + vnbr * 2;
        // r x (vm+(t1-1)*vf-t1*vt)
        // vf = v1, vt = v2 // TODO this is wrong, should be 
        tripletes.push_back(i, lrx, vars[lmy] - vars[lmz] + (t1 - 1) * (vars[lfy] - vars[lfz]) - t1 * (vars[lty] - vars[ltz]));
        tripletes.push_back(i, lry, vars[lmz] - vars[lmx] + (t1 - 1) * (vars[lfz] - vars[lfx]) - t1 * (vars[ltz] - vars[ltx]));
        tripletes.push_back(i, lrz, vars[lmx] - vars[lmy] + (t1 - 1) * (vars[lfx] - vars[lfy]) - t1 * (vars[ltx] - vars[lty]));
        MTenergy[i]=
		tripletes.push_back(i, lmx, vars[lrz] - vars[lry]);
        tripletes.push_back(i, lmy, vars[lrx] - vars[lrz]);
        tripletes.push_back(i, lmz, vars[lry] - vars[lrx]);

        tripletes.push_back(i, lfx, (t1-1)*(vars[lrz] - vars[lry]));
        tripletes.push_back(i, lfy, (t1-1)*(vars[lrx] - vars[lrz]));
        tripletes.push_back(i, lfz, (t1-1)*(vars[lry] - vars[lrx]));

        tripletes.push_back(i, ltx, -t1*(vars[lrz] - vars[lry]));
        tripletes.push_back(i, lty, -t1*(vars[lrx] - vars[lrz]));
        tripletes.push_back(i, ltz, -t1*(vars[lry] - vars[lrx]));

        // vf = v3, vt = v4
		lfx = v3;
		lfy = v3 + vnbr;
		lfz = v3 + vnbr * 2;
		ltx = v4;
		lty = v4 + vnbr;
		ltz = v4 + vnbr * 2;
		tripletes.push_back(i + ninner, lrx, vars[lmy] - vars[lmz] + (t2 - 1) * (vars[lfy] - vars[lfz]) - t2 * (vars[lty] - vars[ltz]));
		tripletes.push_back(i + ninner, lry, vars[lmz] - vars[lmx] + (t2 - 1) * (vars[lfz] - vars[lfx]) - t2 * (vars[ltz] - vars[ltx]));
		tripletes.push_back(i + ninner, lrz, vars[lmx] - vars[lmy] + (t2 - 1) * (vars[lfx] - vars[lfy]) - t2 * (vars[ltx] - vars[lty]));

        tripletes.push_back(i + ninner, lmx, vars[lrz] - vars[lry]);
        tripletes.push_back(i + ninner, lmy, vars[lrx] - vars[lrz]);
        tripletes.push_back(i + ninner, lmz, vars[lry] - vars[lrx]);

        tripletes.push_back(i + ninner, lfx, (t2 - 1) * (vars[lrz] - vars[lry]));
        tripletes.push_back(i + ninner, lfy, (t2 - 1) * (vars[lrx] - vars[lrz]));
        tripletes.push_back(i + ninner, lfz, (t2 - 1) * (vars[lry] - vars[lrx]));

        tripletes.push_back(i + ninner, ltx, -t2 * (vars[lrz] - vars[lry]));
        tripletes.push_back(i + ninner, lty, -t2 * (vars[lrx] - vars[lrz]));
        tripletes.push_back(i + ninner, ltz, -t2 * (vars[lry] - vars[lrx]));

        // r*r=1
        tripletes.push_back(i + ninner * 2, lrx, 2 * vars(lrx));
        tripletes.push_back(i + ninner * 2, lry, 2 * vars(lry));
        tripletes.push_back(i + ninner * 2, lrz, 2 * vars(lrz));

        // r*norm - cos = 0
        Eigen::Vector3d norm = norm_v.row(vm);
        tripletes.push_back(i + ninner * 3, lrx, norm(0));
        tripletes.push_back(i + ninner * 3, lry, norm(1));
        tripletes.push_back(i + ninner * 3, lrz, norm(2));
        
        Eigen::Vector3d cross = (ver1 - ver0).cross(ver2 - ver1);
        double lg1 = (ver1 - ver0).norm();
        double lg2 = (ver2 - ver1).norm();

        if (lg1 < 1e-16 || lg2 < 1e-16) {
            PeWeight[i] = 0;// it means the g1xg2 will not be accurate
        }
        else {
            
            double cos_real = cross.normalized().dot(norm);
            double cos_diff = fabs(cos_real - cos_angle) / 2;
            PeWeight[i] = cos_diff;
        }
        lens[i] = cross.norm();
        if (lens[i] < 1e-16) {// if it is colinear, we prefer to fix this point first
            lens[i] = 1e6;
        }
        Eigen::Vector3d norm = norm_v.row(vm);
        double value = cross.dot(norm) / lens[i] - cos_angle;
        MEnergy[i] = value;

    }
}
spMat Jacobian_transpose_mesh_opt_on_ver( const std::array<spMat,3> &JC, 
const Eigen::Vector3d& norm, const spMat &SMfvalues){

    spMat result = (JC[0] * norm[0] + JC[1] * norm[1] + JC[2] * norm[2]) * SMfvalues;

    return result;
}
void lsTools::assemble_solver_mesh_opt_part(spMat& H, Eigen::VectorXd &B){
    igl::Timer timer;
    double tcal = 0, tmat = 0;
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
    mJTF=Eigen::VectorXd::Zero(vsize*3);
    Eigen::VectorXd lens;
    timer.start();
    calculate_mesh_opt_function_values(pseudo_geodesic_target_angle_degree, lens);
    tcal = timer.getElapsedTimeInSec();
    timer.start();
    for (int i = 0; i < ninner; i++)
    {
        if (ActInner[i] == false)
        {
            continue;
        }
        int vid = IVids[i];
        Eigen::Vector3d norm = norm_v.row(vid);
        spMat JT = Jacobian_transpose_mesh_opt_on_ver(MJsimp[i], norm, SMfunc) / lens[i];
        JTJ += PeWeight[i] * JT * JT.transpose();
        mJTF += -PeWeight[i] * JT * MEnergy[i];
    }
    tmat= timer.getElapsedTimeInSec();
    H = JTJ;
    B = mJTF;
    std::cout << "tcal " << tcal << " tmat " << tmat << std::endl;
}
void lsTools::assemble_solver_mesh_smoothing(const Eigen::VectorXd &vars, spMat& H, Eigen::VectorXd &B){
    spMat JTJ=igl::repdiag(QcH,3);// the matrix size nx3 x nx3
    Eigen::VectorXd mJTF=-JTJ*vars;
    H=JTJ;
    B=mJTF;
}
void lsTools::update_mesh_properties(){
    // first update all the vertices in openmesh
    int nv = lsmesh.n_vertices();
	for (CGMesh::VertexIter v_it = lsmesh.vertices_begin(); v_it != lsmesh.vertices_end(); ++v_it)
	{
        int vid=v_it.handle().idx();
		lsmesh.point(*v_it)=CGMesh::Point(V(vid,0),V(vid,1),V(vid,2));
	}
    igl::cotmatrix(V, F, Dlps);
    igl::curved_hessian_energy(V, F, QcH);

    // 1
    get_mesh_normals_per_face();
    // 2
    get_mesh_angles();// only useful when computing gradient ver to F. and the vertex normals 
    // // 3 if use this, please don't use get_I_and_II_locally()
    get_mesh_normals_per_ver();
    // 4
    get_face_rotation_matices();// not useful, just for debug

    // 5
    get_rotated_edges_for_each_face();// not useful
    // 6
    get_function_gradient_vertex();

    // 7
    get_function_hessian_vertex();// not useful

    // get_I_and_II_locally(); // useful when we consider curvatures
    get_vertex_rotation_matices();// not useful
    get_all_the_edge_normals();// necessary. In case some one wants to trace again.
}

void solve_mean_value_laplacian_mat(CGMesh& lsmesh,const std::vector<int>& IVids, spMat& mat) {
    int nbr = lsmesh.n_vertices();
    std::vector<Trip> triplets;
    triplets.reserve(nbr * 7);
    spMat mat2;

    mat2.resize(IVids.size() * 3, nbr * 3);
    for (int i = 0; i < IVids.size(); i++) {
        int vid = IVids[i];
        assert(vid >= 0);
        CGMesh::VertexHandle vh = lsmesh.vertex_handle(vid);
        double valence = lsmesh.valence(vh);
        for (CGMesh::VertexVertexIter vv_it = lsmesh.vv_begin(vh); vv_it != lsmesh.vv_end(vh); ++vv_it)
        {
            int id = vv_it.handle().idx();
            int loc;
            location_in_sparse_matrix(nbr, id, 0, loc);
            triplets.push_back(Trip(i, loc, -1 / valence));//x

            location_in_sparse_matrix(nbr, id, 1, loc);
            triplets.push_back(Trip(i + IVids.size(), loc, -1 / valence));//y

            location_in_sparse_matrix(nbr, id, 2, loc);
            triplets.push_back(Trip(i + IVids.size() * 2, loc, -1 / valence));//z
		}
		int loc;
		location_in_sparse_matrix(nbr, vid, 0, loc);
		triplets.push_back(Trip(i, loc, 1));
		location_in_sparse_matrix(nbr, vid, 1, loc);
		triplets.push_back(Trip(i + IVids.size(), loc, 1));
		location_in_sparse_matrix(nbr, vid, 2, loc);
		triplets.push_back(Trip(i + IVids.size() * 2, loc, 1));
    }
    mat2.setFromTriplets(triplets.begin(), triplets.end());
    mat = mat2;

}
void lsTools::assemble_solver_mean_value_laplacian(const Eigen::VectorXd& vars, spMat& H, Eigen::VectorXd& B) {
    spMat JTJ = MVLap.transpose()*MVLap;
    Eigen::VectorXd mJTF = -JTJ * vars;
    H = JTJ;
    B = mJTF;
}

void lsTools::solve_edge_length_matrix(const Eigen::MatrixXd& V, const Eigen::MatrixXi& E, spMat& mat) {
    int enbr = E.rows();
    int nver = V.rows();
    std::vector<Trip> tripletes;
    tripletes.reserve(enbr*6);
    ElStored.resize(enbr * 3);
    for (int i = 0; i < enbr; i++) {
        int vid0 = E(i, 0);
		int vid1 = E(i, 1);
		tripletes.push_back(Trip(3 * i, vid0, 1));
		tripletes.push_back(Trip(3 * i, vid1, -1));
		tripletes.push_back(Trip(3 * i + 1, nver + vid0, 1));
		tripletes.push_back(Trip(3 * i + 1, nver + vid1, -1));
		tripletes.push_back(Trip(3 * i + 2, nver * 2 + vid0, 1));
		tripletes.push_back(Trip(3 * i + 2, nver * 2 + vid1, -1));
        ElStored[3 * i] = V(vid0, 0) - V(vid1, 0);
        ElStored[3 * i+1] = V(vid0, 1) - V(vid1, 1);
        ElStored[3 * i+2] = V(vid0, 2) - V(vid1, 2);
    }
    mat.resize(enbr * 3, nver * 3);
    mat.setFromTriplets(tripletes.begin(), tripletes.end());
}
void lsTools::assemble_solver_mesh_edge_length_part(const Eigen::VectorXd vars, spMat& H, Eigen::VectorXd& B) {
    H = Elmat.transpose() * Elmat;
    int enbr = E.rows();
    int nver = V.rows();
    B.resize(enbr * 3);
    for (int i = 0; i < enbr; i++) {
        int vid0 = E(i, 0);
        int vid1 = E(i, 1);
        B[i * 3] = vars[vid0] - vars[vid1] - ElStored[i * 3];
		B[i * 3 + 1] = vars[nver + vid0] - vars[nver + vid1] - ElStored[i * 3 + 1];
        B[i * 3 + 2] = vars[nver*2 + vid0] - vars[nver*2 + vid1] - ElStored[i * 3 + 2];
    }
    ElEnergy = B;
    B = -Elmat.transpose() * B;
}
void lsTools::Run_Mesh_Opt(){
    igl::Timer tmsolver;
    double ts = 0, tpg = 0, tinit = 0, tel=0, tsolve=0, teval=0;
    tmsolver.start();
    if(!Last_Opt_Mesh){
        get_gradient_hessian_values();
        calculate_gradient_partial_parts_ver_based();
        initialize_mesh_optimization();// get the properties associated with level set but not vertices positions
    }
    int vnbr=V.rows();

    Eigen::VectorXd vars;// the variables
    vars.resize(vnbr*3);
    vars.topRows(vnbr)=V.col(0);
    vars.middleRows(vnbr,vnbr)=V.col(1);
    vars.bottomRows(vnbr)=V.col(2);
    spMat H;
    Eigen::VectorXd B;
    H.resize(vnbr * 3, vnbr * 3);
    B = Eigen::VectorXd::Zero(vnbr * 3);
    tinit= tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    spMat Hsmooth;
    Eigen::VectorXd Bsmooth;
    assemble_solver_mesh_smoothing(vars, Hsmooth, Bsmooth);
    //assemble_solver_mean_value_laplacian(vars, Hsmooth, Bsmooth);
    H += weight_Mesh_smoothness * Hsmooth;
    B += weight_Mesh_smoothness * Bsmooth;
    ts = tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    spMat Hpg;
    Eigen::VectorXd Bpg;
    assemble_solver_mesh_opt_part(Hpg, Bpg);
    H += weight_Mesh_pesudo_geodesic * Hpg;
    B += weight_Mesh_pesudo_geodesic * Bpg;
    tpg = tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    spMat Hel;
    Eigen::VectorXd Bel;
    assemble_solver_mesh_edge_length_part(vars, Hel, Bel);
    H += weight_Mesh_edgelength * Hel;
    B += weight_Mesh_edgelength * Bel;
    tel = tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    double dmax = get_mat_max_diag(H);
    if (dmax == 0)
    {
        dmax = 1;
    }
    H += weight_mass* 1e-6 * dmax*dmax * Eigen::VectorXd::Ones(vnbr*3).asDiagonal();
    
    if(vector_contains_NAN(B)){
        std::cout<<"energy value wrong"<<std::endl;
    }
    Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> solver(H);
    // assert(solver.info() == Eigen::Success);
    if (solver.info() != Eigen::Success)
    {
        // solving failed
        std::cout << "solver fail" << std::endl;
        return;
    }
    tsolve = tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    Eigen::VectorXd dx = solver.solve(B).eval();
    dx *= 0.75;
    double mesh_opt_step_length = dx.norm();
    // double inf_norm=dx.cwiseAbs().maxCoeff();
    if (mesh_opt_step_length > Mesh_opt_max_step_length)
    {
        dx *= Mesh_opt_max_step_length / mesh_opt_step_length;
    }
    vars+=dx;
    V.col(0)=vars.topRows(vnbr);
    V.col(1)=vars.middleRows(vnbr,vnbr);
    V.col(2)=vars.bottomRows(vnbr);
    
    double energy_smooth=(QcH*V).norm();
    double energy_mvl = (MVLap * vars).norm();
    std::cout<<"Mesh Opt: smooth, "<< energy_smooth <<", ";
    double energy_ls=(spMat(PeWeight.asDiagonal()) * spMat(ActInner.asDiagonal()) * MEnergy).norm();
    std::cout<<"pg, "<<energy_ls<<", AngleDiffMax, "<<(PeWeight.asDiagonal()*ActInner).maxCoeff()<<", ";
    double energy_el = ElEnergy.norm();
    std::cout << "el, " << energy_el << ", ";
    step_length=dx.norm();
    std::cout<<"step "<<step_length<<std::endl;
    update_mesh_properties();
    Last_Opt_Mesh=true;
    teval = tmsolver.getElapsedTimeInSec();
    std::cout << "ts " << ts << " tpg " << tpg  << " tinit " << tinit << " tel " << tel << " tsolve " << tsolve << " teval " << teval << std::endl;
}
