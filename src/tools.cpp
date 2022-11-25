#include <lsc/basic.h>
#include <lsc/tools.h>

bool triangles_coplanar(const Eigen::Vector3d &t0, const Eigen::Vector3d &t1, const Eigen::Vector3d &t2,
                        const Eigen::Vector3d &p0, const Eigen::Vector3d &p1, const Eigen::Vector3d &p2)
{
    Eigen::Vector3d norm1 = (t1 - t0).cross(t2 - t1).normalized();
    Eigen::Vector3d norm2 = (p1 - p0).cross(p2 - p1).normalized();
    Eigen::Vector3d cross = norm1.cross(norm2);

    if (cross.norm() < 1e-8)
    {
        return true;
    }
    return false;
}
bool triangles_coplanar(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F, const int fid1, const int fid2){
    Eigen::Vector3d t0, t1, t2, p0, p1, p2;
    t0 = V.row(F(fid1, 0));
    t1 = V.row(F(fid1, 1));
    t2 = V.row(F(fid1, 2));

    p0 = V.row(F(fid2, 0));
    p1 = V.row(F(fid2, 1));
    p2 = V.row(F(fid2, 2));
    return triangles_coplanar(t0,t1,t2,p0,p1,p2);  
}
bool segments_colinear(const Eigen::Vector3d& s1, const Eigen::Vector3d& s2){
    Eigen::Vector3d ss1=s1.normalized();
    Eigen::Vector3d ss2=s2.normalized();
    double dot=ss1.dot(ss2);
    if(fabs(dot)>1-1e-8){
        return true;
    }
    return false;
}
// -1: boundary edge
// 0 not coplanar
// 1 co-planar
int edge_is_coplanar(const CGMesh &lsmesh, const CGMesh::EdgeHandle &edge_middle, const Eigen::MatrixXd &V, const Eigen::MatrixXi &F)
{

    OpenMesh::HalfedgeHandle e1 = lsmesh.halfedge_handle(edge_middle, 0);
    OpenMesh::HalfedgeHandle e2 = lsmesh.halfedge_handle(edge_middle, 1);

    int f1 = lsmesh.face_handle(e1).idx();
    int f2 = lsmesh.face_handle(e2).idx();
    if (f1 < 0 || f2 < 0)
    {
        return -1;
    }
    Eigen::Vector3d t0 = V.row(F(f1, 0));
    Eigen::Vector3d t1 = V.row(F(f1, 1));
    Eigen::Vector3d t2 = V.row(F(f1, 2));
    Eigen::Vector3d p0 = V.row(F(f2, 0));
    Eigen::Vector3d p1 = V.row(F(f2, 1));
    Eigen::Vector3d p2 = V.row(F(f2, 2));
    return triangles_coplanar(t0, t1, t2, p0, p1, p2);
}

std::vector<Trip> to_triplets(spMat &M)
{
    std::vector<Trip> v;
    v.reserve(M.rows());
    for (int i = 0; i < M.outerSize(); i++)
    {
        for (spMat::InnerIterator it(M, i); it; ++it)
        {
            v.push_back(Trip(it.row(), it.col(), it.value()));
        }
    }

    return v;
}
spMat sparse_vec_to_sparse_maxrix(Efunc &vec)
{
    spMat mat;
    mat.resize(1, vec.size());
    std::vector<Trip> triplets;
    for (Efunc::InnerIterator it(vec); it; ++it)
    {
        triplets.push_back(Trip(0, it.index(), it.value()));
    }
    mat.setFromTriplets(triplets.begin(), triplets.end());
    // std::cout << "converted" << std::endl;
    return mat;
}
Efunc sparse_mat_col_to_sparse_vec(const spMat &mat, const int col)
{
    Efunc vec;
    vec.resize(mat.rows());
    assert(!mat.IsRowMajor);// it works for colomn major
    for (spMat::InnerIterator it(mat, col); it; ++it)
    {
        vec.coeffRef(it.index()) = it.value();
    }
    return vec;
}
Efunc dense_vec_to_sparse_vec(const Eigen::VectorXd &vec)
{
    Efunc result;
    result.resize(vec.size());
    for (int i = 0; i < vec.size(); i++)
    {
        if (vec[i] != 0)
        {
            result.coeffRef(i) = vec[i];
        }
    }
    return result;
}
void mat_col_to_triplets(const spMat &mat, const int col, const int ref, const bool inverse, std::vector<Trip> &triplets)
{
    assert(!mat.IsRowMajor);

    for (spMat::InnerIterator it(mat, col); it; ++it)
    {
        int id = it.index();
        double value = it.value();
        if (inverse)
        {
            triplets.push_back(Trip(ref, id, value));
        }
        else
        {
            triplets.push_back(Trip(id, ref, value));
        }
    }
}
void lsTools::debug_tool(int id, int id2, double value)
{
    trace_vers.resize(1);
    std::cout << "checking one pseudo geodesic" << std::endl;
    // debug init
    ver_dbg.resize(0, 0);
    ver_dbg1.resize(0, 0);
    flag_dbg = false;
    E0_dbg.resize(0, 0);
    direction_dbg.resize(0, 0);

    pnorm_dbg.resize(0, 0);
    pnorm_list_dbg.clear();
    flag_dbg = true;
    id_dbg = id2;

    // debug init end
    double start_point_para = 0.5;
    double start_angle_degree = 60;
    double target_angle = value;
    std::vector<Eigen::Vector3d> curve;
    std::vector<CGMesh::HalfedgeHandle> handles;
    trace_single_pseudo_geodesic_curve_pseudo_vertex_method(target_angle, Boundary_Edges[id], start_point_para, start_angle_degree,
                                                            curve, handles);
    flag_dbg = false;
    int nbr_midpts = curve.size() - 2;
    E0_dbg.resize(nbr_midpts, 3);
    direction_dbg.resize(nbr_midpts, 3);
    Eigen::MatrixXd binormals_real(nbr_midpts, 3);
    for (int i = 0; i < nbr_midpts; i++)
    {
        E0_dbg.row(i) = curve[i + 1];
        Eigen::Vector3d vec0 = (pseudo_vers_dbg[i + 1] - pseudo_vers_dbg[i]).normalized();
        Eigen::Vector3d vec1 = (curve[i + 2] - pseudo_vers_dbg[i + 1]).normalized();
        direction_dbg.row(i) = vec0.cross(vec1).normalized(); // bi normal of the pseudo-curve
        vec0 = (curve[i + 1] - curve[i]).normalized();
        vec1 = (curve[i + 2] - curve[i + 1]).normalized();
        binormals_real.row(i) = vec0.cross(vec1).normalized();
    }
    if (pnorm_list_dbg.size() != nbr_midpts)
    {
        std::cout << "dangerous size! " << pnorm_list_dbg.size() << ", " << nbr_midpts << std::endl;
    }
    if (target_angle > 90 - ANGLE_TOLERANCE && target_angle < 90 + ANGLE_TOLERANCE)
    {
        std::cout << "Traced geodesic" << std::endl;
    }
    else
    {
        assert(pnorm_list_dbg.size() == nbr_midpts);
        pnorm_dbg = vec_list_to_matrix(pnorm_list_dbg);
        // check the abs(consin)
        for (int i = 0; i < nbr_midpts; i++)
        {
            Eigen::Vector3d tmp_dir1 = direction_dbg.row(i);
            Eigen::Vector3d tmp_dir2 = pnorm_list_dbg[i].normalized();
            double cosin = tmp_dir1.dot(tmp_dir2);
            std::cout << i << "th cosin^2 " << cosin * cosin << std::endl;
            tmp_dir1 = binormals_real.row(i);
            cosin = tmp_dir1.dot(tmp_dir2);
            std::cout << i << "th cosin^2 " << cosin * cosin << std::endl;
        }
    }

    // TODO temporarily checking one trace line
    trace_vers[0] = curve;
    trace_hehs.push_back(handles);
}
void lsTools::debug_tool_v2(const std::vector<int> &ids, const std::vector<double> values)
{
    // cylinder_example(5, 10, 50, 30);
    // exit(0);
    int nbr_itv = ids[0]; // every nbr_itv boundary edges we shoot one curve
    if (nbr_itv < 1)
    {
        std::cout << "Please set up the parameter nbr_itv " << std::endl;
        return;
    }
    double target_angle = values[0];
    double start_angel = values[1];
    OpenMesh::HalfedgeHandle init_edge = Boundary_Edges[0];
    if (lsmesh.face_handle(init_edge).idx() >= 0)
    {
        init_edge = lsmesh.opposite_halfedge_handle(init_edge);
    }
    OpenMesh::HalfedgeHandle checking_edge = init_edge;
    int curve_id = 0;
    while (1)
    {
        ver_dbg.resize(0, 0);
        ver_dbg1.resize(0, 0);
        flag_dbg = false;
        E0_dbg.resize(0, 0);
        direction_dbg.resize(0, 0);
        pnorm_dbg.resize(0, 0);
        pnorm_list_dbg.clear();
        flag_dbg = true;
        id_dbg = ids[1];
        std::vector<Eigen::Vector3d> curve;
        std::vector<CGMesh::HalfedgeHandle> handles;
        trace_single_pseudo_geodesic_curve_pseudo_vertex_method(target_angle, checking_edge, 0.5, start_angel,
                                                                curve, handles);
        flag_dbg = false;
        curve_id++;
        if (curve_id == -1)
        {
            bool stop_flag = false;

            for (int i = 0; i < nbr_itv; i++)
            {
                checking_edge = lsmesh.next_halfedge_handle(checking_edge);
                if (checking_edge == init_edge)
                {
                    stop_flag = true;
                    break;
                }
            }
            continue;
        }
        trace_vers.push_back(curve);
        trace_hehs.push_back(handles);
        bool stop_flag = false;

        if (curve_id == -1)
        {
            break;
        }
        for (int i = 0; i < nbr_itv; i++)
        {
            checking_edge = lsmesh.next_halfedge_handle(checking_edge);
            if (checking_edge == init_edge)
            {
                stop_flag = true;
                break;
            }
        }
        if (stop_flag)
        {
            break;
        }
    }
}

void lsTools::debug_tool_v3(int id, int id2, double value)
{
    trace_vers.resize(1);
    std::cout << "checking one pseudo geodesic" << std::endl;
    // debug init
    ver_dbg.resize(0, 0);
    ver_dbg1.resize(0, 0);
    flag_dbg = false;
    E0_dbg.resize(0, 0);
    direction_dbg.resize(0, 0);
    trace_hehs.clear();
    pnorm_dbg.resize(0, 0);
    pnorm_list_dbg.clear();
    flag_dbg = true;
    id_dbg = id2;

    // debug init end
    double start_point_para = 0.5;
    double start_angle_degree = 60;
    double target_angle = value;
    std::vector<Eigen::Vector3d> curve;
    std::vector<CGMesh::HalfedgeHandle> handles;
    trace_single_pseudo_geodesic_curve(target_angle, Boundary_Edges[id], start_point_para, start_angle_degree,
                                       curve, handles);
    flag_dbg = false;
    int nbr_midpts = curve.size() - 2;
    E0_dbg.resize(nbr_midpts, 3);
    direction_dbg.resize(nbr_midpts, 3);
    Eigen::MatrixXd binormals_real(nbr_midpts, 3);
    for (int i = 0; i < nbr_midpts; i++)
    {
        E0_dbg.row(i) = curve[i + 1];
        Eigen::Vector3d vec0 = (curve[i + 1] - curve[i]).normalized();
        Eigen::Vector3d vec1 = (curve[i + 2] - curve[i + 1]).normalized();
        direction_dbg.row(i) = vec0.cross(vec1).normalized(); // bi normal of the pseudo-curve
    }

    if (target_angle > 90 - ANGLE_TOLERANCE && target_angle < 90 + ANGLE_TOLERANCE)
    {
        std::cout << "Traced geodesic" << std::endl;
    }
    else
    {

        for (int i = 0; i < nbr_midpts; i++)
        {
            Eigen::Vector3d tmp_dir1 = direction_dbg.row(i);
            Eigen::Vector3d tmp_dir2 = pnorm_list_dbg[i].normalized();
            double cosin = tmp_dir1.dot(tmp_dir2);
            std::cout << i << "th cosin^2 " << cosin * cosin << std::endl;
        }
    }

    // TODO temporarily checking one trace line
    trace_vers[0] = curve;
    trace_hehs.push_back(handles);
}
void lsTools::debug_tool_v4(const std::vector<int> &ids, const std::vector<double> values)
{
    // cylinder_example(5, 10, 50, 30);
    // exit(0);
    trace_vers.clear();
    trace_hehs.clear();
    int nbr_itv = ids[0]; // every nbr_itv boundary edges we shoot one curve
    if (nbr_itv < 1)
    {
        std::cout << "Please set up the parameter nbr_itv " << std::endl;
        return;
    }
    double target_angle = values[0];
    double start_angel = values[1];
    OpenMesh::HalfedgeHandle init_edge = Boundary_Edges[0];
    if (lsmesh.face_handle(init_edge).idx() >= 0)
    {
        init_edge = lsmesh.opposite_halfedge_handle(init_edge);
    }
    OpenMesh::HalfedgeHandle checking_edge = init_edge;
    int curve_id = 0;
    while (1)
    {
        ver_dbg.resize(0, 0);
        ver_dbg1.resize(0, 0);
        E0_dbg.resize(0, 0);
        direction_dbg.resize(0, 0);
        pnorm_dbg.resize(0, 0);
        pnorm_list_dbg.clear();
        flag_dbg = true;
        id_dbg = ids[1];
        std::vector<Eigen::Vector3d> curve;
        std::vector<CGMesh::HalfedgeHandle> handles;
        trace_single_pseudo_geodesic_curve(target_angle, checking_edge, 0.5, start_angel,
                                           curve, handles);
        flag_dbg = false;
        curve_id++;
        if (curve_id == -1)
        {
            bool stop_flag = false;

            for (int i = 0; i < nbr_itv; i++)
            {
                checking_edge = lsmesh.next_halfedge_handle(checking_edge);
                if (checking_edge == init_edge)
                {
                    stop_flag = true;
                    break;
                }
            }
            continue;
        }
        trace_vers.push_back(curve);
        trace_hehs.push_back(handles);
        bool stop_flag = false;

        if (curve_id == -1)
        {
            break;
        }
        for (int i = 0; i < nbr_itv; i++)
        {
            checking_edge = lsmesh.next_halfedge_handle(checking_edge);
            if (checking_edge == init_edge)
            {
                stop_flag = true;
                break;
            }
        }
        if (stop_flag)
        {
            break;
        }
    }
}

void extend_triplets_offset(std::vector<Trip> &triplets, const spMat &mat, int offrow, int offcol)
{
    for (int i = 0; i < mat.outerSize(); i++)
    {
        for (spMat::InnerIterator it(mat, i); it; ++it)
        {
            triplets.push_back(Trip(it.row() + offrow, it.col() + offcol, it.value()));
        }
    }
}
spMat dense_mat_list_as_sparse_diagnal(const std::vector<Eigen::MatrixXd> &mlist)
{
    spMat result;
    int outer = mlist.size();
    int inner_rows = mlist[0].rows();
    int inner_cols = mlist[0].cols();
    result.resize(outer * inner_rows, outer * inner_cols);
    std::vector<Trip> triplets;
    triplets.reserve(inner_cols * inner_rows * outer);
    for (int i = 0; i < outer; i++)
    {
        extend_triplets_offset(triplets, mlist[i].sparseView(), i * inner_rows, i * inner_cols);
    }
    result.setFromTriplets(triplets.begin(), triplets.end());
    return result;
}
//spMat rib_method_arrange_matrices_rows(const std::vector<spMat> &mats)
//{
//    int nvec = mats.size();
//    int inner_rows = mats[0].rows();
//    int inner_cols = mats[0].cols();
//    spMat result;
//    result.resize(nvec * inner_rows, inner_cols); // the rows get more, the column number remain the same.
//    std::vector<Trip> triplets;
//    triplets.reserve(inner_cols * inner_rows * nvec);
//    for (int i = 0; i < nvec; i++)
//    {
//        // TODO
//    }
//}

std::vector<double> polynomial_simplify(const std::vector<double> &poly)
{
    std::vector<double> result = poly;
    int size = poly.size();
    for (int i = 0; i < size - 1; i++)
    {
        if (result[size - i - 1] == 0)
        {
            result.pop_back();
        }
        else
        {
            break;
        }
    }
    return result;
}

std::vector<double> polynomial_add(const std::vector<double> &poly1, const std::vector<double> &poly2)
{
    int size = std::max(poly1.size(), poly2.size());
    std::vector<double> result(size);
    for (int i = 0; i < size; i++)
    {
        bool flag1 = i < poly1.size();
        bool flag2 = i < poly2.size();
        if (flag1 && flag2)
        {
            result[i] = poly1[i] + poly2[i];
        }
        else if (flag1)
        {
            result[i] = poly1[i];
        }
        else
        {
            result[i] = poly2[i];
        }
    }
    return polynomial_simplify(result);
}
std::vector<double> polynomial_times(const std::vector<double> &poly1, const std::vector<double> &poly2)
{
    int size = poly1.size() + poly2.size() - 1;
    std::vector<double> result(size);
    for (int i = 0; i < size; i++)
    { // initialize the result
        result[i] = 0;
    }

    for (int i = 0; i < poly1.size(); i++)
    {
        for (int j = 0; j < poly2.size(); j++)
        {
            result[i + j] += poly1[i] * poly2[j];
        }
    }
    return polynomial_simplify(result);
}
std::vector<double> polynomial_times(const std::vector<double> &poly1, const double &nbr)
{
    std::vector<double> result;
    if (nbr == 0)
    {
        result.resize(1);
        result[0] = 0;
        return result;
    }
    result = poly1;
    for (int i = 0; i < result.size(); i++)
    {
        result[i] *= nbr;
    }

    return polynomial_simplify(result);
}
double polynomial_value(const std::vector<double> &poly, const double para)
{
    double result = 0;
    for (int i = 0; i < poly.size(); i++)
    {
        result += poly[i] * std::pow(para, i);
    }
    return result;
}
std::vector<double> polynomial_integration(const std::vector<double> &poly)
{
    std::vector<double> result(poly.size() + 1);
    result[0] = 0;
    for (int i = 1; i < result.size(); i++)
    {
        result[i] = poly[i - 1] / i;
    }
    return polynomial_simplify(result);
}
double polynomial_integration(const std::vector<double> &poly, const double lower, const double upper)
{
    double up = polynomial_value(polynomial_integration(poly), upper);
    double lw = polynomial_value(polynomial_integration(poly), lower);
    return up - lw;
}

Eigen::MatrixXd vec_list_to_matrix(const std::vector<Eigen::Vector3d> &vec)
{
    Eigen::MatrixXd mat;
    mat.resize(vec.size(), 3);
    for (int i = 0; i < mat.rows(); i++)
    {
        mat.row(i) = vec[i];
    }
    return mat;
}

CGMesh::HalfedgeHandle boundary_halfedge(const CGMesh& lsmesh, const CGMesh::HalfedgeHandle& boundary_edge){
    assert(lsmesh.is_boundary(lsmesh.from_vertex_handle(boundary_edge)));
    assert(lsmesh.is_boundary(lsmesh.to_vertex_handle(boundary_edge)));
    CGMesh::HalfedgeHandle result=boundary_edge;
    if(lsmesh.face_handle(result).idx()>=0){
        result=lsmesh.opposite_halfedge_handle(result);
    }
    return result;
}
void split_mesh_boundary_by_corner_detection(CGMesh &lsmesh, const Eigen::MatrixXd& V, const double threadshold_angel_degree,
                                             const std::vector<CGMesh::HalfedgeHandle> &Boundary_Edges, 
                                             std::vector<std::vector<CGMesh::HalfedgeHandle>> &boundaries)
{
    boundaries.clear();
    int vnbr=V.rows();
    SpVeci bedges;// 0: not a boundary edge; 1: not checked boundary edge;
    SpVeci corner_vers;
    corner_vers.resize(vnbr);
    int enbr=lsmesh.n_edges();
    int bsize=Boundary_Edges.size();
    std::cout<<"boundary edges size "<<Boundary_Edges.size()<<std::endl;
    bedges.resize(enbr);
    for(int i=0;i<Boundary_Edges.size();i++){
        CGMesh::EdgeHandle eh=lsmesh.edge_handle(Boundary_Edges[i]);
        bedges.coeffRef(eh.idx())=1; // mark every boundary edges
    }
    
    
    int nbr_corners=0;
    for (int i = 0; i < bsize; i++)
    { // iterate all the boundary edges
        CGMesh::HalfedgeHandle start_he=boundary_halfedge(lsmesh, Boundary_Edges[i]);
        CGMesh::HalfedgeHandle next_he=lsmesh.next_halfedge_handle(start_he);
        // std::cout<<"this face id "<<lsmesh.face_handle(start_he).idx()<<" opposite "<<lsmesh.face_handle(lsmesh.opposite_halfedge_handle(start_he))<<std::endl;
        // std::cout<<"next face id "<<lsmesh.face_handle(next_he).idx()<<" opposite "<<lsmesh.face_handle(lsmesh.opposite_halfedge_handle(next_he))<<std::endl;
        assert(lsmesh.face_handle(next_he).idx()<0);
        Eigen::Vector3d p0=V.row(lsmesh.from_vertex_handle(start_he).idx());
        Eigen::Vector3d p1=V.row(lsmesh.to_vertex_handle(start_he).idx());
        Eigen::Vector3d p2=V.row(lsmesh.to_vertex_handle(next_he).idx());
        Eigen::Vector3d dir1=(p1-p0).normalized();
        Eigen::Vector3d dir2=(p2-p1).normalized();
        int id_middle_ver=lsmesh.to_vertex_handle(start_he).idx();
        double dot_product=dir1.dot(dir2);
        double real_angle=acos(dot_product) * 180 / LSC_PI;
        // std::cout<<"real angle "<<real_angle<<std::endl;
        if (real_angle < 180 - threadshold_angel_degree)
        {
            continue;
        }
        else{// current start_he and next_he are edges of a corner.
            corner_vers.coeffRef(id_middle_ver)=1;
            std::cout<<"find one corner"<<std::endl;
            nbr_corners++; 
        }
    }
    std::cout<<"nbr of corners "<<nbr_corners<<std::endl;
    if (nbr_corners == 0)
    {
        SpVeci recorded_edges;
        recorded_edges.resize(enbr);
        for (int i = 0; i < enbr; i++)
        {
            if (recorded_edges.coeffRef(i) == 0 && bedges.coeffRef(i) == 1)
            { // if this is a boundary edge, and not checked yet
                recorded_edges.coeffRef(i) = 1;
                CGMesh::HalfedgeHandle start_he = lsmesh.halfedge_handle(lsmesh.edge_handle(i), 0);
                start_he = boundary_halfedge(lsmesh, start_he);
                // CGMesh::HalfedgeHandle start_copy = start_he;
                std::vector<CGMesh::HalfedgeHandle> loop;
                loop.push_back(start_he);
                for (int j = 0; j < bsize; j++)
                {
                    CGMesh::HalfedgeHandle next_he = lsmesh.next_halfedge_handle(start_he);
                    int edge_id=lsmesh.edge_handle(next_he).idx();
                    if (recorded_edges.coeffRef(edge_id)==0)// if the next edge is not checked yet
                    {
                        recorded_edges.coeffRef(edge_id)=1;
                        loop.push_back(next_he);
                        start_he = next_he;
                    }
                    else
                    {
                        break;
                    }
                }
                boundaries.push_back(loop);
            }
        }
        return;
    }
    SpVeci startlist;// this list marks if this corner vertex is already used.
    startlist.resize(vnbr);
    // the number of corner points is the same as (or more than, in some extreme cases) the number of segments

    for(int i=0;i<bsize;i++){ 
        CGMesh::HalfedgeHandle start_he=boundary_halfedge(lsmesh, Boundary_Edges[i]);
        int id_start_ver=lsmesh.from_vertex_handle(start_he).idx();
        

        if(corner_vers.coeffRef(id_start_ver)==1&&startlist.coeffRef(id_start_ver)==0){//if this point is a corner and not used as a start point
            startlist.coeffRef(id_start_ver) = 1;// mark it as used point
            std::vector<CGMesh::HalfedgeHandle> loop;
            loop.push_back(start_he);
            for(int j=0;j<bsize;j++){
                CGMesh::HalfedgeHandle next_he=lsmesh.next_halfedge_handle(start_he);
                int id_end_ver=lsmesh.to_vertex_handle(next_he).idx();
                loop.push_back(next_he);
                
                if(corner_vers.coeffRef(id_end_ver)==1){// the end point is a vertex
                    break;
                }
                start_he=next_he;
            }
            boundaries.push_back(loop);
        }
       
    }
}


Eigen::Vector3d sphere_function(double r, double theta, double phi)
{
    double x = r * sin(theta) * cos(phi);
    double y = r * sin(theta) * sin(phi);
    double z = r * cos(theta);
    return Eigen::Vector3d(x, y, z);
}
// create a triangle mesh sphere.
#include <igl/write_triangle_mesh.h>
void sphere_example(double radius, double theta, double phi, int nt, int np)
{

    Eigen::MatrixXd ver;
    Eigen::MatrixXi faces;
    ver.resize(nt * np, 3);
    faces.resize(2 * (nt - 1) * (np - 1), 3);
    int verline = 0;
    double titv = 2 * theta / (nt - 1);
    double pitv = 2 * phi / (np - 1);
    for (int i = 0; i < nt; i++)
    {
        for (int j = 0; j < np; j++)
        {
            double upara = 0.5 * 3.1415926 - theta + i * titv;
            double vpara = -phi + j * pitv;
            ver.row(verline) = sphere_function(radius, upara, vpara);
            verline++;
        }
    }
    faces.resize(2 * (nt - 1) * (np - 1), 3);
    int fline = 0;
    for (int i = 0; i < nt - 1; i++)
    {
        for (int j = 0; j < np - 1; j++)
        {
            int id0 = np * i + j;
            int id1 = np * (i + 1) + j;
            int id2 = np * (i + 1) + j + 1;
            int id3 = np * i + j + 1;
            faces.row(fline) = Eigen::Vector3i(id0, id1, id2);
            faces.row(fline + 1) = Eigen::Vector3i(id0, id2, id3);
            fline += 2;
        }
    }
    std::string path("/Users/wangb0d/bolun/D/vs/levelset/level-set-curves/data/");
    igl::write_triangle_mesh(path + "sphere_" + std::to_string(radius) + "_" + std::to_string(theta) + "_" +
                                 std::to_string(phi) + "_" + std::to_string(nt) + "_" + std::to_string(np) + ".obj",
                             ver, faces);
    std::cout << "sphere mesh file saved " << std::endl;
}
void cylinder_example(double radius, double height, int nr, int nh)
{
    Eigen::MatrixXd ver;
    Eigen::MatrixXi faces;
    ver.resize(nh * nr, 3);
    faces.resize(2 * (nh - 1) * nr, 3);
    int verline = 0;
    double hitv = height / (nh - 1);
    double ritv = 2 * LSC_PI / nr;
    for (int i = 0; i < nr; i++)
    {
        double angle = ritv * i;
        double x = cos(angle) * radius;
        double y = sin(angle) * radius;
        for (int j = 0; j < nh; j++)
        {
            double z = j * hitv;
            ver.row(verline) << x, y, z;
            verline++;
        }
    }

    int fline = 0;
    for (int i = 0; i < nr; i++)
    {
        if (i < nr - 1)
        {
            for (int j = 0; j < nh - 1; j++)
            {
                int id0 = nh * i + j;
                int id1 = nh * (i + 1) + j;
                int id2 = nh * (i + 1) + j + 1;
                int id3 = nh * i + j + 1;
                faces.row(fline) = Eigen::Vector3i(id0, id1, id2);
                faces.row(fline + 1) = Eigen::Vector3i(id0, id2, id3);
                fline += 2;
            }
        }
        else
        {
            for (int j = 0; j < nh - 1; j++)
            {
                int id0 = nh * i + j;
                int id1 = j;
                int id2 = j + 1;
                int id3 = nh * i + j + 1;
                faces.row(fline) = Eigen::Vector3i(id0, id1, id2);
                faces.row(fline + 1) = Eigen::Vector3i(id0, id2, id3);
                fline += 2;
            }
        }
    }
    std::string path("/Users/wangb0d/bolun/D/vs/levelset/level-set-curves/data/");
    igl::write_triangle_mesh(path + "cylinder_" + std::to_string(radius) + "_" + std::to_string(height) + "_" +
                                 std::to_string(nr) + "_" + std::to_string(nh) + ".obj",
                             ver, faces);
    std::cout << "sphere mesh file saved " << std::endl;
}

void cylinder_open_example(double radius, double height, int nr, int nh)
{
    Eigen::MatrixXd ver;
    Eigen::MatrixXi faces;
    ver.resize(nh * nr, 3);
    faces.resize(2 * (nh - 1) * (nr-1), 3);
    int verline = 0;
    double hitv = height / (nh - 1);
    double ritv =  LSC_PI / (nr-1);// we only draw half a cylinder
    for (int i = 0; i < nr; i++)
    {
        double angle = ritv * i;
        double x = cos(angle) * radius;
        double y = sin(angle) * radius;
        for (int j = 0; j < nh; j++)
        {
            double z = j * hitv;
            ver.row(verline) << x, y, z;
            verline++;
        }
    }

    int fline = 0;
    for (int i = 0; i < nr; i++)
    {
        if (i < nr - 1)
        {
            for (int j = 0; j < nh - 1; j++)
            {
                int id0 = nh * i + j;
                int id1 = nh * (i + 1) + j;
                int id2 = nh * (i + 1) + j + 1;
                int id3 = nh * i + j + 1;
                faces.row(fline) = Eigen::Vector3i(id0, id1, id2);
                faces.row(fline + 1) = Eigen::Vector3i(id0, id2, id3);
                fline += 2;
            }
        }
        // else
        // {
        //     for (int j = 0; j < nh - 1; j++)
        //     {
        //         int id0 = nh * i + j;
        //         int id1 = j;
        //         int id2 = j + 1;
        //         int id3 = nh * i + j + 1;
        //         faces.row(fline) = Eigen::Vector3i(id0, id1, id2);
        //         faces.row(fline + 1) = Eigen::Vector3i(id0, id2, id3);
        //         fline += 2;
        //     }
        // }
    }
    std::string path("/Users/wangb0d/bolun/D/vs/levelset/level-set-curves/data/");
    igl::write_triangle_mesh(path + "oc_" + std::to_string(radius) + "_" + std::to_string(height) + "_" +
                                 std::to_string(nr) + "_" + std::to_string(nh) + ".obj",
                             ver, faces);
    std::cout << "cylinder file saved " << std::endl;
}

#include<igl/file_dialog_open.h>
#include<igl/file_dialog_save.h>
bool save_levelset(const Eigen::VectorXd &ls){
    std::string fname = igl::file_dialog_save();
    std::ofstream file;
    file.open(fname);
    for(int i=0;i<ls.size();i++){
        file<<ls[i]<<std::endl;
    }
    file.close();
    return true;
}
bool read_levelset(Eigen::VectorXd &ls){
    std::string fname = igl::file_dialog_open();
    std::cout<<"reading "<<fname<<std::endl;
    if (fname.length() == 0)
        return false;

    std::ifstream infile;
    std::vector<double> results;

    infile.open(fname);
    if (!infile.is_open())
    {
        std::cout << "Path Wrong!!!!" << std::endl;
        std::cout << "path, " << fname << std::endl;
        return false;
    }

    int l = 0;
    while (infile) // there is input overload classfile
    {
        std::string s;
        if (!getline(infile, s))
            break;

        if (s[0] != '#')
        {
            std::istringstream ss(s);
            std::string record;

            while (ss)
            {
                std::string line;
                if (!getline(ss, line, ','))
                    break;
                try
                {

                    record = line;

                }
                catch (const std::invalid_argument e)
                {
                    std::cout << "NaN found in file " << fname
                              <<  std::endl;
                    e.what();
                }
            }

            double x = std::stod(record);
            results.push_back(x);
        }
    }

    ls.resize(results.size());
    for(int i=0;i<results.size();i++){
        ls[i]=results[i];
    }
    if (!infile.eof())
    {
        std::cerr << "Could not read file " << fname << "\n";
    }
    std::cout<<fname<<" get readed"<<std::endl;
    return true;
}

// get vid1, vid2, vidA and vidB.
// here we do not assume fvalues[vid1]>fvalues[vid2], we need to consider it outside
std::array<int,4> get_vers_around_edge(CGMesh& lsmesh, int edgeid, int& fid1, int &fid2){
    CGMesh::EdgeHandle edge_middle=lsmesh.edge_handle(edgeid);
    CGMesh::HalfedgeHandle he = lsmesh.halfedge_handle(edge_middle, 0);
    int vid1 = lsmesh.to_vertex_handle(he).idx();
    int vid2 = lsmesh.from_vertex_handle(he).idx();
    
    CGMesh::HalfedgeHandle nexthandle=lsmesh.next_halfedge_handle(he);
    int vidA=lsmesh.to_vertex_handle(nexthandle).idx();
    CGMesh::HalfedgeHandle oppohandle=lsmesh.opposite_halfedge_handle(he);
    fid1=lsmesh.face_handle(he).idx();
    fid2=lsmesh.face_handle(oppohandle).idx();
    CGMesh::HalfedgeHandle opnexthandle=lsmesh.next_halfedge_handle(oppohandle);
    int vidB=lsmesh.to_vertex_handle(opnexthandle).idx();
    std::array<int,4> result;
    result[0]=vid1;
    result[1]=vid2;
    result[2]=vidA;
    result[3]=vidB;
    return result;
}

void get_level_set_sample_values(const Eigen::VectorXd &ls, const int nbr, Eigen::VectorXd &values){
    double vmax=ls.maxCoeff();
    double vmin=ls.minCoeff();
    double itv=(vmax-vmin)/(nbr+1);
    values.resize(nbr);
    for(int i=0;i<nbr;i++){
        values[i]=vmin+(i+1)*itv;
    }
    return;
}
void get_level_set_sample_values_even_pace(const Eigen::VectorXd &ls, const int nbr, const double pace, Eigen::VectorXd &values){
    double vmax=ls.maxCoeff();
    double vmin=ls.minCoeff();
    double remain=(vmax-vmin)-(nbr-1)*pace;
    remain/=2; 
    if(remain<0){
        std::cout<<"computing wrong"<<std::endl;
    }
    values.resize(nbr);
    for(int i=0;i<nbr;i++){
        values[i] = remain + vmin + i * pace;
    }
    return;
}


// 
void get_iso_lines(const Eigen::MatrixXd &V,
                   const Eigen::MatrixXi &F, const Eigen::VectorXd &ls, const double value,
                   std::vector<Eigen::Vector3d> &poly0, std::vector<Eigen::Vector3d> &poly1,
                   std::vector<int> &fids)
{
    for (int i = 0; i < F.rows(); i++)
    {
        Eigen::Vector3d E0, E1;
        bool found = find_one_ls_segment_on_triangle(value, F, V,
                                                     ls, i, E0, E1);
        if(found){
            poly0.push_back(E0);
            poly1.push_back(E1);
            fids.push_back(i);
        }
    }
}
bool halfedge_has_value(const CGMesh &lsmesh, const Eigen::MatrixXd &V,
                         const Eigen::VectorXd &ls, const CGMesh::HalfedgeHandle &hd, const double value, Eigen::Vector3d &pt)
{
    int id_f=lsmesh.from_vertex_handle(hd).idx();
    int id_t=lsmesh.to_vertex_handle(hd).idx();
    double value_f=ls[id_f];
    double value_t=ls[id_t];
    double t=get_t_of_value(value, value_f, value_t);
    if(t<0||t>1){
        return false;
    }
    Eigen::Vector3d ver_f=V.row(id_f);
    Eigen::Vector3d ver_t= V.row(id_t);
    pt=get_3d_ver_from_t(t, ver_f, ver_t);
    // std::cout<<"has value "<<value<<", vf "<<value_f<<" vt "<<value_t<<", idf "<<id_f<<", idt "<<id_t<<std::endl;
    return true;
}

// the mesh provides the connectivity, loop is the boundary loop
// left_large indicates if the from ver of each boundary edge is larger value
void get_iso_lines(const CGMesh &lsmesh, const std::vector<CGMesh::HalfedgeHandle>& loop, const Eigen::MatrixXd &V,
                   const Eigen::MatrixXi &F, const Eigen::VectorXd &ls, double value, std::vector<std::vector<Eigen::Vector3d>> &pts,
                   std::vector<bool>& left_large)
{
    pts.clear();
    left_large.clear();
    std::vector<CGMesh::HalfedgeHandle> has_value; // these boundary edges has value
    std::vector<Eigen::Vector3d> start_pts; // these are the computed start/end points
    for(int i=0;i<loop.size();i++){ // find the points on boundary
        
        CGMesh::HalfedgeHandle bhd;
        if (lsmesh.face_handle(loop[i]).idx()<0)
        {
            bhd=loop[i];
        }
        else{
            bhd=lsmesh.opposite_halfedge_handle(loop[i]);
        }
        
        assert(lsmesh.face_handle(bhd).idx()<0);
        Eigen::Vector3d intersection;
        bool found=halfedge_has_value(lsmesh,V,ls, bhd, value,intersection);
        if(!found){
            continue;
        }
        has_value.push_back(bhd);
        start_pts.push_back(intersection);
        int id0=lsmesh.from_vertex_handle(bhd).idx();
        int id1=lsmesh.to_vertex_handle(bhd).idx();
        if(ls[id0]>ls[id1]){
            left_large.push_back(true);
        }
        else{
            left_large.push_back(false);
        }
    }
    int bsize=has_value.size();
    std::cout<<"has values size "<<bsize<<std::endl;
    // std::vector<bool> checked(bsize); // show if this boundary edge is checked
    // for(int i=0;i<bsize;i++){
    //     checked[i]=false;
    // }

    for(int i=0;i<bsize;i++){
        // if(checked[i]){
        //     continue;
        // }
        // checked[i]=true;
        if(left_large[i]==false){ // only count for the shoouting in boundaries
            continue;
        }

        std::vector<Eigen::Vector3d> tmpts; // the vers for each polyline
        CGMesh::HalfedgeHandle thd=has_value[i]; // the start handle
        tmpts.push_back(start_pts[i]);
        while(1){
            CGMesh::HalfedgeHandle ophd=lsmesh.opposite_halfedge_handle(thd);
            int fid = lsmesh.face_handle(ophd).idx();
            if(fid<0){ // if already reached a boundary
                for(int j=0;j<bsize;j++){
                    if(ophd==has_value[j]){
                        // if(checked[j]){
                        //     std::cout<<"TOPOLOGY WRONG"<<std::endl;
                        // }
                        // checked[j]=true;
                        break;
                    }
                }
                pts.push_back(tmpts);
                break;
            }
            int id_f=lsmesh.from_vertex_handle(ophd).idx();
            int id_t=lsmesh.to_vertex_handle(ophd).idx();
            CGMesh::HalfedgeHandle prev=lsmesh.prev_halfedge_handle(ophd);
            CGMesh::HalfedgeHandle next=lsmesh.next_halfedge_handle(ophd);
            int bid=lsmesh.from_vertex_handle(prev).idx();
            if(bid==id_f||bid==id_t){
                std::cout<<"wrong topology"<<std::endl;
            }
            if(ls[bid]==value){// a degenerate case where the value is on a vertex of the mesh
                value+=1e-16;
            }
            Eigen::Vector3d intersect;
            bool found =halfedge_has_value(lsmesh, V, ls, prev,value, intersect);
            if(found){
                tmpts.push_back(intersect);
                thd=prev;
                continue;
            }
            found = halfedge_has_value(lsmesh, V, ls, next, value, intersect);
            if(found){
                tmpts.push_back(intersect);
                thd=next;
                continue;
            }
            std::cout<<"Both two halfedges did not find the correct value, error"<<std::endl;
        }
    }

}
bool two_triangles_connected(const Eigen::MatrixXi& F, const int fid0, const int fid1){
    if(fid0==fid1){
        return true;
    }
    for(int i=0;i<3;i++){
        int vid0=F(fid0,i);
        for(int j=0;j<3;j++){
            int vid1=F(fid1,j);
            if(vid0==vid1){
                return true;
            }
        }
    }
    return false;
}

#include<igl/segment_segment_intersect.h>
// TODO this is quadratic computation, need use tree structure
bool get_polyline_intersection(const Eigen::MatrixXd &V, const Eigen::MatrixXi &F,
                               const std::vector<Eigen::Vector3d> &poly0_e0, const std::vector<Eigen::Vector3d> &poly0_e1,
                               const std::vector<Eigen::Vector3d> &poly1_e0, const std::vector<Eigen::Vector3d> &poly1_e1,
                               const std::vector<int> &fid0, const std::vector<int> &fid1, Eigen::Vector3d &result)
{
    assert(poly0_e0.size() == poly0_e1.size());
    assert(poly1_e0.size() == poly1_e1.size());
    for (int i = 0; i < poly0_e0.size(); i++)
    {
        int f0 = fid0[i];
        Eigen::Vector3d e0 = poly0_e0[i];
        Eigen::Vector3d dir0 = poly0_e1[i] - poly0_e0[i];
        for (int j = 0; j < poly1_e0.size(); j++)
        {
            Eigen::Vector3d e1 = poly1_e0[j];
            Eigen::Vector3d dir1 = poly1_e1[j] - poly1_e0[j];
            int f1 = fid1[j];

            if (!two_triangles_connected(F, f0, f1))
            {
                continue;
            }
            double u, v;
            bool intersect = igl::segment_segment_intersect(e0, dir0, e1, dir1, u, v);
            if (intersect)
            {
                result = e0 + u * dir0;
                return true;
            }
        }
    }
    return false;
}

void extract_web_from_index_mat(const Eigen::MatrixXi& mat, Eigen::MatrixXi& F){
    std::array<int, 4> face;
    std::vector<std::array<int,4>> tface; 
    tface.reserve(mat.rows()*mat.cols());
    for(int i=0;i<mat.rows()-1;i++){
        for(int j=0;j<mat.cols()-1;j++){
            int f0=mat(i,j);
            int f1=mat(i,j+1);
            int f2=mat(i+1,j+1);
            int f3=mat(i+1, j);
            if(f0<0||f1<0||f2<0||f3<0){
                continue;
            }
            face={f0,f1,f2,f3};
            tface.push_back(face);
        }
    }
    F.resize(tface.size(),4);
    for(int i=0;i<tface.size();i++){
        F(i,0)=tface[i][0];
        F(i,1)=tface[i][1];
        F(i,2)=tface[i][2];
        F(i,3)=tface[i][3];

    }
}
void extract_web_mxn(const int rows, const int cols, Eigen::MatrixXi& F){
    std::array<int, 4> face;
    std::vector<std::array<int,4>> tface; 
    tface.reserve(rows*cols);
    for(int i=0;i<rows-1;i++){
        for(int j=0;j<cols-1;j++){
            int f0 = cols * i + j;
            int f1 = cols * (i + 1) + j;
            int f2 = cols * (i + 1) + (j + 1);
            int f3 = cols * i + j + 1;
            face={f0,f1,f2,f3};
            tface.push_back(face);
        }
    }
    F.resize(tface.size(),4);
    for(int i=0;i<tface.size();i++){
        F(i,0)=tface[i][0];
        F(i,1)=tface[i][1];
        F(i,2)=tface[i][2];
        F(i,3)=tface[i][3];

    }
}

// the parameter even_pace means that the 
void extract_levelset_web(const CGMesh &lsmesh, const Eigen::MatrixXd &V,
                          const Eigen::MatrixXi &F, const Eigen::VectorXd &ls0, const Eigen::VectorXd &ls1,
                          const int expect_nbr_ls0, const int expect_nbr_ls1,
                          Eigen::MatrixXd &vers, Eigen::MatrixXi &Faces, bool even_pace=false)
{
    std::vector<std::vector<Eigen::Vector3d>> ivs;                                    // the intersection vertices
    Eigen::MatrixXi gridmat;                                                          // the matrix for vertex
    Eigen::VectorXd lsv0, lsv1;                                                       // the extracted level set values;
    std::vector<std::vector<Eigen::Vector3d>> poly0_e0, poly0_e1, poly1_e0, poly1_e1; // polylines
    std::vector<std::vector<int>> fid0, fid1;                                         // face ids of each polyline segment
    std::vector<Eigen::Vector3d> verlist;
    if(ls0.size()!=ls1.size()){
        std::cout<<"ERROR, Please use the correct level sets"<<std::endl;
    }
    int nbr_ls0, nbr_ls1;
    if(!even_pace){
        nbr_ls0=expect_nbr_ls0;
        nbr_ls1=expect_nbr_ls1;
        get_level_set_sample_values(ls0, nbr_ls0, lsv0);
        get_level_set_sample_values(ls1, nbr_ls1, lsv1);
    }
    else{
        double pace = std::min((ls0.maxCoeff() - ls0.minCoeff()) / (expect_nbr_ls0 + 1), (ls1.maxCoeff() - ls1.minCoeff()) / (expect_nbr_ls1 + 1));
        nbr_ls0 = (ls0.maxCoeff() - ls0.minCoeff()) / pace + 1;
        nbr_ls1 = (ls1.maxCoeff() - ls1.minCoeff()) / pace + 1;
        get_level_set_sample_values_even_pace(ls0, nbr_ls0, pace, lsv0);
        get_level_set_sample_values_even_pace(ls1, nbr_ls1, pace, lsv1);
    }

    ivs.resize(nbr_ls0);
    verlist.reserve(nbr_ls0 * nbr_ls1);
    gridmat = Eigen::MatrixXi::Ones(nbr_ls0, nbr_ls1) * -1; // initially there is no quad patterns
    lsv0.resize(nbr_ls0);
    lsv1.resize(nbr_ls1);
    poly0_e0.resize(nbr_ls0);
    poly0_e1.resize(nbr_ls0);
    poly1_e0.resize(nbr_ls1);
    poly1_e1.resize(nbr_ls1);
    fid0.resize(nbr_ls0);
    fid1.resize(nbr_ls1);
    for (int i = 0; i < nbr_ls0; i++)
    {
        ivs[i].resize(nbr_ls1);
        poly0_e0[i].reserve(nbr_ls1);
        poly0_e1[i].reserve(nbr_ls1);
        fid0[i].reserve(nbr_ls1);
    }
    for (int i = 0; i < nbr_ls1; i++)
    {
        poly1_e0[i].reserve(nbr_ls0);
        poly1_e1[i].reserve(nbr_ls0);
        fid1[i].reserve(nbr_ls0);
    }
    
    // std::cout<<"sp_0 \n"<<lsv0.transpose()<<"\nsp_1\n"<<lsv1.transpose()<<std::endl;
    for (int i = 0; i < nbr_ls0; i++)
    {
        double vl = lsv0[i];
        get_iso_lines(V, F, ls0, vl, poly0_e0[i], poly0_e1[i], fid0[i]);
        // std::cout<<i<<" size "<<poly0_e0[i].size()<<"\n";
    }
    for (int i = 0; i < nbr_ls1; i++)
    {
        double vl = lsv1[i];
        get_iso_lines(V, F, ls1, vl, poly1_e0[i], poly1_e1[i], fid1[i]);
        // std::cout<<i<<" size "<<poly1_e0[i].size()<<"\n";
    }
    int vnbr = 0;
    for (int i = 0; i < nbr_ls0; i++)
    {
        for (int j = 0; j < nbr_ls1; j++)
        {
            Eigen::Vector3d ipoint;
            bool intersect = get_polyline_intersection(V, F, poly0_e0[i], poly0_e1[i], poly1_e0[j], poly1_e1[j], fid0[i], fid1[j], ipoint);
            if (intersect)
            {
                verlist.push_back(ipoint);
                gridmat(i, j) = vnbr;
                vnbr++;
            }
        }
    }
    std::cout<<"extracted ver nbr "<<verlist.size()<<std::endl;
    vers=vec_list_to_matrix(verlist);
    extract_web_from_index_mat(gridmat,Faces);
    if(even_pace){
        std::cout<<"The even pace quad, pace, "<<std::min((ls0.maxCoeff() - ls0.minCoeff()) / (expect_nbr_ls0 + 1), (ls1.maxCoeff() - ls1.minCoeff()) / (expect_nbr_ls1 + 1))
        <<std::endl;
    }
}
double polyline_length(const std::vector<Eigen::Vector3d>& line){
    int size = line.size();
    double total=0;
    for(int i=0;i<size-1;i++){
        Eigen::Vector3d dire=line[i]-line[i+1];
        double dis=dire.norm();
        total+=dis;
    }
    return total;
}
int select_longest_polyline(const std::vector<std::vector<Eigen::Vector3d>>& lines, double & length){
    int size = lines.size();
    assert(size>0);
    double max_length=0;
    int max_id=0;
    for(int i = 0;i<size; i++){
        double length = polyline_length(lines[i]);
        if(length>max_length){
            max_length=length;
            max_id=i;
        }
    }
    length=max_length;
    return max_id;
}
void find_next_pt_on_polyline(const int start_seg, const std::vector<Eigen::Vector3d> &polyline, const double length,
                              const Eigen::Vector3d &pstart, int &seg, Eigen::Vector3d &pt)
{
    int nbr=polyline.size();
    double ocu_dis=0;
    double dis_to_start=length+(polyline[start_seg]-pstart).norm();
    for(int i=start_seg; i<nbr-1;i++){
        double dis=(polyline[i]-polyline[i+1]).norm();
        ocu_dis+= dis;
        if(ocu_dis>dis_to_start){ // the point should between i and i+1
            double diff = ocu_dis-dis_to_start; // the distance the point to i+1
            double t = diff/dis;
            assert(t>=0&&t<=1);
            Eigen::Vector3d p=get_3d_ver_from_t(t, polyline[i+1], polyline[i]);
            pt = p;
            seg=i;
            return;
        }
    }
    std::cout<<"ERROR OUT OF SEGMENT"<<std::endl;
}
// nbr - 1 is the nbr of segments
// length is the length of the polyline
void sample_polyline_and_extend_verlist(const std::vector<Eigen::Vector3d>& polyline, const int nbr, const double length, std::vector<Eigen::Vector3d>& verlist){
    assert(nbr>3);
    double avg = length / (nbr - 1);
    verlist.push_back(polyline[0]);
    int start=0;
    for (int i = 0; i < nbr - 2; i++)
    {
        int seg;
        Eigen::Vector3d pt;
        std::cout<<"finding vers"<<std::endl;
        find_next_pt_on_polyline(start, polyline, avg, verlist.back(), seg, pt);
        std::cout<<"found vers"<<std::endl;
        verlist.push_back(pt);
        start=seg;
    }
    verlist.push_back(polyline.back());
}

void extract_Origami_web(const CGMesh &lsmesh, const Eigen::MatrixXd &V,const std::vector<CGMesh::HalfedgeHandle>& loop,
                         const Eigen::MatrixXi &F, const Eigen::VectorXd &ls,
                         const int expect_nbr_ls, const int expect_nbr_dis,
                         Eigen::MatrixXd &vers, Eigen::MatrixXi &Faces)
{
    Eigen::VectorXd lsv0;
    int nbr_ls0;

    nbr_ls0 = expect_nbr_ls;
    get_level_set_sample_values(ls, nbr_ls0, lsv0);
    std::cout<<"sampled"<<std::endl;
    std::vector<Eigen::Vector3d> verlist;
    verlist.reserve(nbr_ls0*expect_nbr_dis);
    for(int i=0;i<nbr_ls0;i++){
        std::vector<std::vector<Eigen::Vector3d>> polylines;
        double value = lsv0[i];
        std::vector<bool> left_large;
        std::cout<<"iso..."<<std::endl;
        get_iso_lines(lsmesh, loop, V, F, ls, value, polylines, left_large);
        std::cout<<"iso got, size "<<polylines.size()<<std::endl;
        double length ;
        int longest = select_longest_polyline(polylines, length);
        std::cout<<"longest got, size "<<polylines[longest].size()<<" length "<<length<<std::endl;
        // for(int j=0;j<polylines[longest].size();j++){
        //     std::cout<<"v "<<polylines[longest][j].transpose()<<std::endl;
        // }
        // exit(0);
        sample_polyline_and_extend_verlist(polylines[longest], expect_nbr_dis, length, verlist);
        std::cout<<"vers found"<<std::endl;
    }
    vers=vec_list_to_matrix(verlist);
    extract_web_mxn(nbr_ls0, expect_nbr_dis, Faces);

}

void lsTools::show_binormals(const Eigen::VectorXd &func, Eigen::MatrixXd &E0, Eigen::MatrixXd &E1, Eigen::MatrixXd& binormals,  double ratio){
    int vnbr = V.rows();
    analysis_pseudo_geodesic_on_vertices(func, anas[0]);
    binormals=Eigen::MatrixXd::Zero(vnbr, 3);
    E0.resize(vnbr, 3);
    E1.resize(vnbr, 3);
    int ninner = anas[0].LocalActInner.size();
	for (int i = 0; i < ninner; i++)
	{
		if (anas[0].LocalActInner[i] == false) {
			std::cout << "singularity" << std::endl;
			continue;
		}
		int vm = IVids[i];
		CGMesh::HalfedgeHandle inhd = anas[0].heh0[i], outhd = anas[0].heh1[i];
        int v1 = lsmesh.from_vertex_handle(inhd).idx();
        int v2 = lsmesh.to_vertex_handle(inhd).idx();
        int v3 = lsmesh.from_vertex_handle(outhd).idx();
        int v4 = lsmesh.to_vertex_handle(outhd).idx();

        double t1 = anas[0].t1s[i];
        double t2 = anas[0].t2s[i];

        Eigen::Vector3d ver0 = V.row(v1) + (V.row(v2) - V.row(v1)) * t1;
        Eigen::Vector3d ver1 = V.row(vm);
        Eigen::Vector3d ver2 = V.row(v3) + (V.row(v4) - V.row(v3)) * t2;

        Eigen::Vector3d bi = (ver1 - ver0).cross(ver2 - ver1);
        binormals.row(vm) = bi.normalized();
    }
    E0 = V - binormals * ratio;
    E1 = V + binormals * ratio;
}
#include <igl/point_mesh_squared_distance.h>
#include <fstream>
bool write_quad_mesh_with_binormal(const std::string &fname, const Eigen::MatrixXd &Vt, const Eigen::MatrixXi &Ft, const Eigen::MatrixXd &bi,
                                   const Eigen::MatrixXd &Vq, const Eigen::MatrixXi &Fq)
{
    Eigen::MatrixXd C;
    Eigen::VectorXi I;
    Eigen::VectorXd D;
    igl::point_mesh_squared_distance(Vq, Vt, Ft, D, I, C);
    int nq = Vq.rows();
    Eigen::MatrixXd B;
    B.resize(nq, 3);
    for (int i = 0; i < nq; i++)
    {
        int fid = I(i);
        int v0 = Ft(fid, 0);
        int v1 = Ft(fid, 1);
        int v2 = Ft(fid, 2);
        Eigen::Vector3d dir = bi.row(v0) + bi.row(v1) + bi.row(v2);
        dir = dir.normalized();
        B.row(i) = dir;
    }
    std::ofstream file;
    file.open(fname);
    for (int i = 0; i < nq; i++)
    {
        file << "v " << Vq(i, 0) << " " << Vq(i, 1) << " " << Vq(i, 2) << " " << B(i, 0) << " " << B(i, 1) << " " << B(i, 2) << std::endl;
    }
    for (int i = 0; i < Fq.rows(); i++)
    {
        file << "f " << Fq(i, 0) + 1 << " " << Fq(i, 1) + 1 << " " << Fq(i, 2) + 1 << " " << Fq(i, 3) + 1 << std::endl;
    }
    file.close();
    return true;
}