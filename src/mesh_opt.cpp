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

// the size of vars should be nvars, if there is no auxiliary variables, nvars = vnbr*3.
// The auxiliary vars are located from aux_start_loc to ninner*3+aux_start_loc
void lsTools::calculate_mesh_opt_expanded_function_values(const Eigen::VectorXd&  Loc_ActInner, Eigen::VectorXd &vars,
    const std::vector<CGMesh::HalfedgeHandle>& heh0, const std::vector<CGMesh::HalfedgeHandle>& heh1,
    const std::vector<double> &t1s, const std::vector<double> &t2s,
    const std::vector<double>& angle_degree,
    const bool first_compute, const int aux_start_loc,  std::vector<Trip> &tripletes, Eigen::VectorXd &MTenergy) {
    
    double cos_angle;
    if (angle_degree.size() == 1) {
        double angle_radian = angle_degree[0] * LSC_PI / 180.; // the angle in radian
        cos_angle = cos(angle_radian);
    }
    
    int ninner = Loc_ActInner.size();
    int vnbr = V.rows();

    //PeWeight = Eigen::VectorXd::Zero(ninner);
    //lens = Eigen::VectorXd::Zero(ninner);
    tripletes.clear();
    tripletes.reserve(ninner * 30);// the number of rows is ninner*4, the number of cols is aux_start_loc + ninner * 3 (all the vertices and auxiliary vars)
	MTenergy = Eigen::VectorXd::Zero(ninner * 4); // mesh total energy values

    for (int i = 0; i < ninner; i++)
    {
        if (Loc_ActInner[i] == false) {
            continue;
        }
        int vm = IVids[i];
        if (angle_degree.size() == vnbr) {
            double angle_radian = angle_degree[vm] * LSC_PI / 180.; // the angle in radian
            cos_angle = cos(angle_radian);
        }
        CGMesh::HalfedgeHandle inhd = heh0[i], outhd = heh1[i];
        int v1 = lsmesh.from_vertex_handle(inhd).idx();
        int v2 = lsmesh.to_vertex_handle(inhd).idx();
        int v3 = lsmesh.from_vertex_handle(outhd).idx();
        int v4 = lsmesh.to_vertex_handle(outhd).idx();
        double t1 = t1s[i];
        double t2 = t2s[i];
        Eigen::Vector3d ver0 = V.row(v1) + (V.row(v2) - V.row(v1)) * t1;
        Eigen::Vector3d ver1 = V.row(vm);
        Eigen::Vector3d ver2 = V.row(v3) + (V.row(v4) - V.row(v3)) * t2;
        // the locations
        int lrx = i + aux_start_loc;
        int lry = i + aux_start_loc + ninner;
        int lrz = i + aux_start_loc + ninner * 2;
        if (first_compute) {
            Eigen::Vector3d real_r = (ver1 - ver0).cross(ver2 - ver1);
            real_r = real_r.normalized();
            vars[lrx] = real_r[0];
            vars[lry] = real_r[1];
            vars[lrz] = real_r[2];
        }

        
        int lfx = v1;
        int lfy = v1 + vnbr;
        int lfz = v1 + vnbr * 2;
        int ltx = v2;
        int lty = v2 + vnbr;
        int ltz = v2 + vnbr * 2;
        int lmx = vm;
        int lmy = vm + vnbr;
        int lmz = vm + vnbr * 2;
        // r dot (vm+(t1-1)*vf-t1*vt)
        // vf = v1, vt = v2 
		tripletes.push_back(Trip(i, lrx, vars[lmx] + (t1 - 1) * vars[lfx] - t1 * vars[ltx]));
		tripletes.push_back(Trip(i, lry, vars[lmy] + (t1 - 1) * vars[lfy] - t1 * vars[lty]));
		tripletes.push_back(Trip(i, lrz, vars[lmz] + (t1 - 1) * vars[lfz] - t1 * vars[ltz]));

		tripletes.push_back(Trip(i, lmx, vars[lrx]));
		tripletes.push_back(Trip(i, lmy, vars[lry]));
		tripletes.push_back(Trip(i, lmz, vars[lrz]));

		tripletes.push_back(Trip(i, lfx, (t1 - 1) * vars[lrx]));
		tripletes.push_back(Trip(i, lfy, (t1 - 1) * vars[lry]));
		tripletes.push_back(Trip(i, lfz, (t1 - 1) * vars[lrz]));

		tripletes.push_back(Trip(i, ltx, -t1 * vars[lrx]));
		tripletes.push_back(Trip(i, lty, -t1 * vars[lry]));
		tripletes.push_back(Trip(i, ltz, -t1 * vars[lrz]));
        Eigen::Vector3d r = Eigen::Vector3d(vars[lrx], vars[lry], vars[lrz]);
        MTenergy[i] = r.dot(ver1 - ver0);

        // vf = v3, vt = v4
		lfx = v3;
		lfy = v3 + vnbr;
		lfz = v3 + vnbr * 2;
		ltx = v4;
		lty = v4 + vnbr;
		ltz = v4 + vnbr * 2;

		tripletes.push_back(Trip(i + ninner, lrx, vars[lmx] + (t2 - 1) * vars[lfx] - t2 * vars[ltx]));
		tripletes.push_back(Trip(i + ninner, lry, vars[lmy] + (t2 - 1) * vars[lfy] - t2 * vars[lty]));
		tripletes.push_back(Trip(i + ninner, lrz, vars[lmz] + (t2 - 1) * vars[lfz] - t2 * vars[ltz]));

		tripletes.push_back(Trip(i + ninner, lmx, vars[lrx]));
		tripletes.push_back(Trip(i + ninner, lmy, vars[lry]));
		tripletes.push_back(Trip(i + ninner, lmz, vars[lrz]));

		tripletes.push_back(Trip(i + ninner, lfx, (t2 - 1) * vars[lrx]));
		tripletes.push_back(Trip(i + ninner, lfy, (t2 - 1) * vars[lry]));
		tripletes.push_back(Trip(i + ninner, lfz, (t2 - 1) * vars[lrz]));

		tripletes.push_back(Trip(i + ninner, ltx, -t2 * vars[lrx]));
		tripletes.push_back(Trip(i + ninner, lty, -t2 * vars[lry]));
		tripletes.push_back(Trip(i + ninner, ltz, -t2 * vars[lrz]));

        MTenergy[i + ninner] = r.dot(ver1 - ver2);

		// r*r=1
		tripletes.push_back(Trip(i + ninner * 2, lrx, 2 * vars(lrx)));
		tripletes.push_back(Trip(i + ninner * 2, lry, 2 * vars(lry)));
		tripletes.push_back(Trip(i + ninner * 2, lrz, 2 * vars(lrz)));

		MTenergy[i + ninner * 2] = r.dot(r) - 1;

		// r*norm - cos = 0
		Eigen::Vector3d norm = norm_v.row(vm);
		tripletes.push_back(Trip(i + ninner * 3, lrx, norm(0)));
		tripletes.push_back(Trip(i + ninner * 3, lry, norm(1)));
		tripletes.push_back(Trip(i + ninner * 3, lrz, norm(2)));
        
		MTenergy[i + ninner * 3] = norm.dot(r)-cos_angle;
    }
}
spMat Jacobian_transpose_mesh_opt_on_ver( const std::array<spMat,3> &JC, 
const Eigen::Vector3d& norm, const spMat &SMfvalues){

    spMat result = (JC[0] * norm[0] + JC[1] * norm[1] + JC[2] * norm[2]) * SMfvalues;

    return result;
}
void lsTools::assemble_solver_mesh_opt_part(const Eigen::VectorXd& Loc_ActInner, Eigen::VectorXd& vars,
    const std::vector<CGMesh::HalfedgeHandle>& heh0, const std::vector<CGMesh::HalfedgeHandle>& heh1,
    const std::vector<double>& t1s, const std::vector<double>& t2s, 
    const std::vector<double>& angle_degrees, const bool first_compute, const int aux_start_loc, spMat& JTJ, Eigen::VectorXd& B, Eigen::VectorXd& MTEnergy){
	std::vector<Trip> tripletes;
    int vsize = V.rows();
    int ninner = Loc_ActInner.size();
	calculate_mesh_opt_expanded_function_values(Loc_ActInner, vars, heh0, heh1,
		t1s, t2s, angle_degrees, first_compute, aux_start_loc, tripletes, MTEnergy);
        
    int nvars = vars.size();
    int ncondi = ninner * 4;
    spMat J;
    J.resize(ncondi, nvars);
    J.setFromTriplets(tripletes.begin(), tripletes.end());
    JTJ = J.transpose() * J;
    B = -J.transpose() * MTEnergy;
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

// the mat must be a symmetric matrix
void spmat_on_corner(const spMat& mat, spMat& target, int nmat, int ntarget) {
    spMat tmp;
    tmp.resize(nmat, ntarget);
    tmp.leftCols(nmat) = mat;
    target.leftCols(nmat) = tmp.transpose();
}
// they must be symmetric matrices
spMat sum_uneven_spMats(const spMat& mat_small, const spMat& mat_large) {
    int snbr = mat_small.rows();
    int lnbr = mat_large.rows();
    if (snbr == lnbr) {
        return mat_small + mat_large;
    }
    spMat tmp;
    tmp.resize(snbr, lnbr);
    tmp.leftCols(snbr) = mat_small;
    spMat result;
    result.resize(lnbr, lnbr);
    result.leftCols(snbr) = tmp.transpose();
	return result + mat_large;
}
Eigen::VectorXd sum_uneven_vectors(const Eigen::VectorXd& vsmall, const Eigen::VectorXd& vlarge) {
    int snbr = vsmall.size();
    int lnbr = vlarge.size();
    if (snbr == lnbr) {
        return vsmall + vlarge;
    }
    Eigen::VectorXd tmp=Eigen::VectorXd::Zero(lnbr);
    tmp.topRows(snbr) = vsmall;
    return tmp + vlarge;

}
void lsTools::Run_Mesh_Opt(){
    igl::Timer tmsolver;
    double ts = 0, tpg = 0, tinit = 0, tel=0, tsolve=0, teval=0;
    tmsolver.start();
    bool first_compute = false; // if we need initialize auxiliary vars
    if(!Last_Opt_Mesh){
        first_compute = true;// if last time opt levelset, we re-compute the auxiliary vars
        get_gradient_hessian_values();
        calculate_gradient_partial_parts_ver_based();
        initialize_mesh_optimization();// get the properties associated with level set but not vertices positions
    }
    int ninner = Local_ActInner.size();
    int vnbr=V.rows();
   
    Eigen::VectorXd vars;// the variables feed to the energies without auxiliary variables
    vars.resize(vnbr * 3);
    if (Glob_Vars.size() == 0) {
        std::cout << "Initializing Global Variable For Mesh Opt ... " << std::endl;
        Glob_Vars=Eigen::VectorXd::Zero(vnbr * 3 + ninner * 3);// We change the size if opt more than 1 level set
        Glob_Vars.segment(0, vnbr) = V.col(0);
        Glob_Vars.segment(vnbr, vnbr) = V.col(1);
		Glob_Vars.segment(vnbr * 2, vnbr) = V.col(2);
    }
    vars.topRows(vnbr) = V.col(0);
    vars.middleRows(vnbr, vnbr) = V.col(1);
    vars.bottomRows(vnbr) = V.col(2);
    
    
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
    spMat Hel;
    Eigen::VectorXd Bel;
    assemble_solver_mesh_edge_length_part(vars, Hel, Bel);
    H += weight_Mesh_edgelength * Hel;
    B += weight_Mesh_edgelength * Bel;
    tel = tmsolver.getElapsedTimeInSec();
    
    /*double dmax = get_mat_max_diag(H);
    if (dmax == 0)
    {
        dmax = 1;
    }*/
    tmsolver.start();
    spMat Hpg;
    Eigen::VectorXd Bpg;
    std::vector < double > angle_degrees(1);
    angle_degrees[0] = pseudo_geodesic_target_angle_degree;
    int aux_start_loc = vnbr * 3;// the first levelset the auxiliary vars start from vnbr*3
    Eigen::VectorXd MTEnergy;
    assemble_solver_mesh_opt_part(Local_ActInner, Glob_Vars,
		Vheh0, Vheh1, Mt1, Mt2, angle_degrees, first_compute, aux_start_loc, Hpg, Bpg, MTEnergy);
    spMat Htotal;
    Eigen::VectorXd Btotal = Eigen::VectorXd::Zero(vnbr * 3 + ninner * 3);
    //Htmp.resize(vnbr * 3, vnbr * 3 + ninner * 3);
    Htotal.resize(vnbr * 3 + ninner * 3, vnbr * 3 + ninner * 3);
    spmat_on_corner(H, Htotal, vnbr * 3, vnbr * 3 + ninner * 3);
    Htotal += weight_Mesh_pesudo_geodesic * Hpg;
    Btotal.topRows(vnbr * 3) = B;
    Btotal += weight_Mesh_pesudo_geodesic * Bpg;
    tpg = tmsolver.getElapsedTimeInSec();
    
	Htotal += weight_mass * 1e-6 * Eigen::VectorXd::Ones(vnbr * 3 + ninner * 3).asDiagonal();
    
    if(vector_contains_NAN(Btotal)){
        std::cout<<"energy value wrong"<<std::endl;
    }
    tmsolver.start();
    Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> solver(Htotal);
    // assert(solver.info() == Eigen::Success);
    if (solver.info() != Eigen::Success)
    {
        // solving failed
        std::cout << "solver fail" << std::endl;
        return;
    }
    
    
    Eigen::VectorXd dx = solver.solve(Btotal).eval();
    tsolve = tmsolver.getElapsedTimeInSec();
    tmsolver.start();
    dx *= 0.75;
    double mesh_opt_step_length = dx.norm();
    // double inf_norm=dx.cwiseAbs().maxCoeff();
    if (mesh_opt_step_length > Mesh_opt_max_step_length)
    {
        dx *= Mesh_opt_max_step_length / mesh_opt_step_length;
    }
    vars+=dx.topRows(vnbr*3);
    Glob_Vars += dx;
    V.col(0)=vars.topRows(vnbr);
    V.col(1)=vars.middleRows(vnbr,vnbr);
    V.col(2)=vars.bottomRows(vnbr);
    if (vars != Glob_Vars.topRows(vnbr * 3)) {
        std::cout << "vars diff from glob vars" << std::endl;
    }
    double energy_smooth=(QcH*V).norm();
    /*double energy_mvl = (MVLap * vars).norm();*/
    std::cout<<"Mesh Opt: smooth, "<< energy_smooth <<", ";
    double energy_ls= MTEnergy.norm();
    double max_energy_ls = MTEnergy.lpNorm<Eigen::Infinity>();
    std::cout<<"pg, "<<energy_ls<<", MaxEnergy, "<< max_energy_ls<<", ";
    double max_ls_angle_energy = MTEnergy.bottomRows(ninner).norm();
    std::cout << "total angle energy, " << max_ls_angle_energy << ", ";
    double energy_el = ElEnergy.norm();
    std::cout << "el, " << energy_el << ", ";
    step_length=dx.norm();
    std::cout<<"step "<<step_length<<std::endl;
    update_mesh_properties();
    Last_Opt_Mesh=true;
    teval = tmsolver.getElapsedTimeInSec();
    //std::cout << "ts " << ts << " tpg " << tpg  << " tinit " << tinit << " tel " << tel << " tsolve " << tsolve << " teval " << teval << std::endl<<std::endl;
}
