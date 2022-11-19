﻿#pragma once
#include <lsc/MeshProcessing.h>
#include <lsc/igl_tool.h>
#include <lsc/basic.h>

// Efunc represent a elementary value, which is the linear combination of
// some function values on their corresponding vertices, of this vertex.
// i.e. Efunc[0]*f_0+Efunc[1]*f_1+...+Efunc[n]*f_n.
typedef Eigen::SparseVector<double> Efunc;
typedef Eigen::SparseVector<int> SpVeci;
// typedef Eigen::SparseMatrix<double> spMat;
typedef Eigen::Triplet<double> Trip;
#define SCALAR_ZERO 1e-8
#define MERGE_VERTEX_RATIO 0.01
#define LSC_PI 3.14159265
#define ANGLE_TOLERANCE 2.
#define QUADRANT_TOLERANCE 0.04 // over 2 degree tolerance

class EnergyPrepare{
    public:
    EnergyPrepare(){};
    double weight_gravity;  
    double weight_lap; 
    double weight_bnd;
    double weight_pg; 
    double weight_strip_width;
    bool solve_pseudo_geodesic;
    bool solve_strip_width_on_traced;
    bool enable_inner_vers_fixed;
    bool enable_functional_angles;
    bool enable_boundary_angles;
    double target_angle;
    double start_angle;
    double target_min_angle;
    double target_max_angle;
    double max_step_length;
};
class MeshEnergyPrepare{
    public:
    MeshEnergyPrepare(){};
    double weight_Mesh_smoothness;
    double weight_Mesh_pesudo_geodesic;
    double weight_Mesh_edgelength;
    double Mesh_opt_max_step_length;
    double target_angle;
};
class TracingPrepare{
    public:
    TracingPrepare(){};
    int every_n_edges;
    int which_boundary_segment;
    int debug_id_tracing;
    double target_angle;
    double start_angle;
    double threadshold_angel_degree;

};
class LSAnalizer {
public:
    LSAnalizer() {};
    Eigen::VectorXd LocalActInner;
    std::vector<CGMesh::HalfedgeHandle> heh0; 
    std::vector<CGMesh::HalfedgeHandle> heh1;
    std::vector<double> t1s;
    std::vector<double> t2s;
};
class NeighbourInfo
{
public:
    NeighbourInfo()
    {
        is_vertex = 0;
        round = 0;
    };
    bool is_vertex = false;
    Eigen::Vector3d pnorm;
    CGMesh::VertexHandle center_handle;
    std::vector<CGMesh::HalfedgeHandle> edges;
    int round = 0;
};
// The basic tool of LSC. Please initialize it with a mesh
class lsTools
{
private:
    MeshProcessing MP;
    
    Eigen::MatrixXd norm_f;               // normal per face
    Eigen::MatrixXd norm_v;               // normal perf vertex
    Eigen::MatrixXd angF;                 // angel per vertex of each F. nx3, n is the number of faces
    Eigen::VectorXd areaF;                // the area of each face.
    Eigen::VectorXd areaPF;               // the area of each face in parametric domain;
    std::vector<Eigen::Matrix3d> Rotate;  // the 90 degree rotation matrices for each face.
    std::vector<Eigen::Matrix3d> RotateV; // the 90 degree rotation matrices on vertices
    // the rotated 3 half edges for each face, in the plane of this face
    // the order of the 3 directions are: v0-v2, v1-v0, v2-v1
    std::vector<std::array<Eigen::Vector3d, 3>> Erotate;
    // 2d rotated half edges in parametric domain
    std::vector<std::array<Eigen::Vector2d, 3>> Erotate2d;
    std::array<spMat, 3> gradVF;                  // gradient of function in each face
    std::array<spMat, 3> gradV;                   // gradient of function in each vertex
    std::array<std::array<spMat, 3>, 3> HessianV; // Hessian (2 order deriavate) of function in each vertex
    std::array<Eigen::MatrixXd, 2> Deriv1;        // the 1 order derivates for each vertex; // TODO
    std::array<Eigen::MatrixXd, 4> Deriv2;        // the 2 order derivates for each vertex; // TODO
    std::vector<double> II_L;                     // second fundamental form: L // TODO
    std::vector<double> II_M;                     // second fundamental form: M // TODO
    std::vector<double> II_N;                     // second fundamental form: N // TODO
    
    
    std::vector<int> refids;                      // output the ids of current dealing points. just for debug purpose
    // spMat Dlpsqr;                       // the derivates of ||laplacian F||^2 for all the vertices.
    spMat QcH; // curved harmonic energy matrix
    spMat Dlps;       // the derivates of laplacian F for all the vertices.
    spMat mass;       // mass(i,i) is the area of the voronoi cell of vertex vi

    // vertex-based pseudo-energy values
    Eigen::VectorXd InnerV; // the vector show if it is a inner ver (1) or not (0).
    std::vector<int> IVids; // the ids of the inner vers.

    std::vector<CGMesh::HalfedgeHandle> Boundary_Edges;
    std::vector<CGMesh::HalfedgeHandle> tracing_start_edges;
    std::vector<double> pseudo_geodesic_angles_per_ver;
    Eigen::VectorXd Glob_lsvars; // global variables for levelset opt

    
    Eigen::MatrixXd norm_e; // normal directions on each edge
    std::vector<std::vector<Eigen::Vector3d>> trace_vers;        // traced vertices
    std::vector<std::vector<CGMesh::HalfedgeHandle>> trace_hehs; // traced half edge handles
    std::vector<Eigen::Vector3d> guideVers;
    std::vector<CGMesh::HalfedgeHandle> guideHehs;
    std::vector<double> assigned_trace_ls; // function values for each traced curve.
    double trace_start_angle_degree; //the angle between the first segment and the given boundary
    spMat  DBdirections; // the derivative of boundary directions from tracing 
    // edge-based pseudo-energy values
    Efunc ActE;// active edges, which means they are not co-planar edges, and not boundary edges.
    std::vector<int> Actid; // active edge ids. the length equals to the number of non-zero elements in ActE
    Eigen::VectorXd BDEvalue; // boundary direction energy value
    void get_mesh_angles();
    void get_mesh_normals_per_face();
    void get_mesh_normals_per_ver();
    // This is to calculate the 90 degree rotation matrices in
    // each triangle face. It can be used to calculate gradients.
    void get_face_rotation_matices();
    void get_rotated_edges_for_each_face();
    void gradient_v2f(std::array<spMat, 3> &output); // calculate gradient in each face from vertex values
    void gradient_f2v(spMat &output);                // calculate gradient in each vertex by averging face values
    void get_function_gradient_vertex();             // get the gradient of f on vertices
    void get_function_hessian_vertex();              // get the hessian matrix of f on vertices
    // get the gradients and hessians on each vertices using the assigned level-set function values
    // CAUTION: assign function values before call this function.
    void get_gradient_hessian_values(const Eigen::VectorXd& func, Eigen::MatrixXd& Vgrad, Eigen::MatrixXd& Fgrad);

    void get_vertex_rotation_matices();

    void get_I_and_II_locally();

    // CAUTION: please call this after you initialize the function values.

    // get the derivatives of direction.dot(edge) of boundary triangles the traced curves start, 
    // where edge is the boundary edge directions from large to small. 
    void get_traced_boundary_triangle_direction_derivatives(); 
    // get the boundary vertices and one ring vertices from them
    void get_bnd_vers_and_handles();
    void get_all_the_edge_normals();// the edge normals and the active edges
    void analysis_pseudo_geodesic_on_vertices(const Eigen::VectorXd& func_values, Eigen::VectorXd& LocalActInner,
        std::vector<CGMesh::HalfedgeHandle>& heh0, std::vector<CGMesh::HalfedgeHandle>& heh1,
        std::vector<double>& t1s, std::vector<double>& t2s);
    void analysis_pseudo_geodesic_on_vertices(const Eigen::VectorXd& func_values, LSAnalizer& analizer);
    void calculate_pseudo_geodesic_opt_expanded_function_values(Eigen::VectorXd& vars, const std::vector<double>& angle_degree,
        const Eigen::VectorXd& LocalActInner, const std::vector<CGMesh::HalfedgeHandle>& heh0, const std::vector<CGMesh::HalfedgeHandle>& heh1,
        const std::vector<double>& t1s, const std::vector<double>& t2s, const bool first_compute, const int aux_start_loc, std::vector<Trip>& tripletes, Eigen::VectorXd& Energy);

    void calculate_boundary_direction_energy_function_values(const Eigen::MatrixXd& GradFValue,
        const std::vector<CGMesh::HalfedgeHandle>& edges, const Eigen::VectorXd& func, Eigen::VectorXd& lens, Eigen::VectorXd& energy);
    void assemble_solver_laplacian_part(spMat &H, Efunc &B);
    void assemble_solver_biharmonic_smoothing(const Eigen::VectorXd& func, spMat &H, Eigen::VectorXd &B); //Biharmonic smoothing (natural curved Hessian boundary)
    void assemble_solver_boundary_condition_part(const Eigen::VectorXd& func, spMat& H, Eigen::VectorXd& B, Eigen::VectorXd& bcfvalue);
    void assemble_solver_pesudo_geodesic_energy_part_vertex_based(Eigen::VectorXd& vars, const std::vector<double>& angle_degree, Eigen::VectorXd& LocalActInner,
        std::vector<CGMesh::HalfedgeHandle>& heh0, std::vector<CGMesh::HalfedgeHandle>& heh1,
        std::vector<double>& t1s, std::vector<double>& t2s, const bool first_compute, const int aux_start_loc, spMat& H, Eigen::VectorXd& B, Eigen::VectorXd& energy);
    void assemble_solver_strip_width_part(const Eigen::MatrixXd& GradValue,  spMat& H, Eigen::VectorXd& B);
    void assemble_solver_fixed_boundary_direction_part(const Eigen::MatrixXd& GradFValue, const std::vector<CGMesh::HalfedgeHandle>& edges,
        const Eigen::VectorXd& func,
        spMat& H, Eigen::VectorXd& B, Eigen::VectorXd& energy);

    // void assemble_solver_pseudo_geodesic_part(spMat &H, Efunc& B);
    bool find_geodesic_intersection_p1_is_NOT_ver(const Eigen::Vector3d &p0, const Eigen::Vector3d &p1,
                                                  const CGMesh::HalfedgeHandle &edge_middle, const Eigen::Vector3d &pnorm, CGMesh::HalfedgeHandle &edge_out, Eigen::Vector3d &p_end);
    bool find_osculating_plane_intersection_not_geodesic_p1_is_not_ver(const Eigen::Vector3d &p0, const Eigen::Vector3d &p1,
                                                                       const CGMesh::HalfedgeHandle &edge_middle, const Eigen::Vector3d &pnorm, const double angle, std::vector<CGMesh::HalfedgeHandle> &edge_out, std::vector<Eigen::Vector3d> &p_end);
    bool get_pseudo_vertex_and_trace_forward(
        QuadricCalculator &cc,
        const std::vector<Eigen::Vector3d> &curve, std::array<Eigen::Vector3d, 3> &pcurve_local, const double angle_degree,
        const CGMesh::HalfedgeHandle &edge_middle,
        const Eigen::Vector3d &point_in, const Eigen::Vector3d &point_middle,
        const bool calculate_pseudo_vertex, CGMesh::HalfedgeHandle &edge_out,
        Eigen::Vector3d &point_out, bool &generate_pseudo_vertex, Eigen::Vector3d &pseudo_vertex_out);
    bool trace_pseudo_geodesic_forward(
        const NeighbourInfo &ninfo,
        const std::vector<Eigen::Vector3d> &curve, const double angle_degree,
        const CGMesh::HalfedgeHandle &edge_middle,
        const Eigen::Vector3d &point_in, const Eigen::Vector3d &point_middle,
        CGMesh::HalfedgeHandle &edge_out,
        Eigen::Vector3d &point_out);
    bool init_pseudo_geodesic_first_segment(
        const CGMesh::HalfedgeHandle &start_boundary_edge_pre, const double &start_point_para,
        const double start_boundary_angle_degree,
        CGMesh::HalfedgeHandle &intersected_handle,
        Eigen::Vector3d &intersected_point);
    // start_point_para is the parameter t, start_point=from+t*(to-from).
    // start_angle_degree is the angle between the initial direction and the direction from->to
    bool trace_single_pseudo_geodesic_curve_pseudo_vertex_method(const double target_angle,
                                                                 const CGMesh::HalfedgeHandle &start_boundary_edge, const double &start_point_para,
                                                                 const double start_angle_degree,
                                                                 std::vector<Eigen::Vector3d> &curve,
                                                                 std::vector<CGMesh::HalfedgeHandle> &handles);
    bool trace_single_pseudo_geodesic_curve(const double target_angle_degree,
                                            const CGMesh::HalfedgeHandle &start_boundary_edge, const double &start_point_para,
                                            const double start_boundary_angle_degree,
                                            std::vector<Eigen::Vector3d> &curve,
                                            std::vector<CGMesh::HalfedgeHandle> &handles);

    bool get_checking_edges(const std::vector<int> &start_point_ids, const CGMesh::HalfedgeHandle &edge_middle, const Eigen::Vector3d &point_middle,
                            Efunc &edges_checked, Efunc &points_checked, NeighbourInfo &ninfo, std::vector<int> &point_to_check);

    void extract_one_curve(const double value, Eigen::MatrixXd& E0, Eigen::MatrixXd &E1);
    void estimate_strip_width_according_to_tracing();
    
    // Mesh Optimization Part

    Eigen::VectorXd ElStored; // edge length of the original mesh
    spMat MVLap; // mean value laplacian
    spMat Elmat; // the edge length constriant
    double weight_Mesh_smoothness;
    double weight_Mesh_pesudo_geodesic;
    double weight_Mesh_edgelength;
    double Mesh_opt_max_step_length;
    bool Last_Opt_Mesh=false; // the last step was mesh optimization. need to update all the mesh properties
    Eigen::VectorXd Glob_Vars;
    std::array<LSAnalizer, 3> anas;

    void solve_edge_length_matrix(const Eigen::MatrixXd& V, const Eigen::MatrixXi& E, spMat& mat);
    void calculate_mesh_opt_expanded_function_values(const Eigen::VectorXd& Loc_ActInner, Eigen::VectorXd& vars,
        const std::vector<CGMesh::HalfedgeHandle>& heh0, const std::vector<CGMesh::HalfedgeHandle>& heh1,
        const std::vector<double>& t1s, const std::vector<double>& t2s,
        const std::vector<double>& angle_degree,
        const bool first_compute, const int aux_start_loc, std::vector<Trip>& tripletes, Eigen::VectorXd& MTenergy);
    void assemble_solver_mesh_opt_part(const Eigen::VectorXd& Loc_ActInner, Eigen::VectorXd& vars,
        const std::vector<CGMesh::HalfedgeHandle>& heh0, const std::vector<CGMesh::HalfedgeHandle>& heh1,
        const std::vector<double>& t1s, const std::vector<double>& t2s,
        const std::vector<double>& angle_degrees, const bool first_compute, const int aux_start_loc, spMat& JTJ, Eigen::VectorXd& B, Eigen::VectorXd& MTEnergy);
    void assemble_solver_mesh_smoothing(const Eigen::VectorXd &vars, spMat &H, Eigen::VectorXd &B);
    void assemble_solver_mean_value_laplacian(const Eigen::VectorXd& vars, spMat& H, Eigen::VectorXd& B);
    void assemble_solver_mesh_edge_length_part(const Eigen::VectorXd vars, spMat& H, Eigen::VectorXd& B, 
    Eigen::VectorXd& energy);
    void update_mesh_properties();// the mesh properties (normals, laplacian, etc.) get updated before optimization


public:
    // parametrization and find the boundary loop
    lsTools(CGMesh &mesh);
    lsTools(){};
    void init(CGMesh &mesh);
    CGMesh lsmesh;                        // the input mesh
    Eigen::MatrixXd V;
    Eigen::MatrixXi F;
    Eigen::MatrixXi E;
    Eigen::VectorXi bnd;     // boundary loop
    Eigen::MatrixXd paras;   // parameters of the mesh vertices, nx2
    Eigen::VectorXd fvalues; // function values
    double weight_assign_face_value; // weight of the assigned value shown in the solver
    double weight_mass;              // weight of the mass function energy
    double weight_laplacian;              // weight of the laplacian energy
    double weight_boundary;              // weight of the boundary energy
    double weight_pseudo_geodesic_energy;// weight of the pseudo-geodesic energy
    double weight_strip_width;
    // Don't Use Strip Width Condition When Opt Multiple Functions
    double strip_width = 0;       // strip width, defined as h/w, h: level set function value difference. w: distance between two points on the surface
    double max_step_length;
    double step_length;
    
    bool enable_pseudo_geodesic_energy=false; // decide if we include pseudo-geodesic energy
    bool enable_strip_width_energy=false;
    bool enable_inner_vers_fixed=false;// decide if we enables fixing the inner vers and optimize boundary laplacian
    bool enable_functional_angles=false;// decide if we enable functional angle variation
    bool enable_boundary_angles=false;


    double pseudo_geodesic_target_angle_degree; // the target pseudo-geodesic angle
    double pseudo_geodesic_start_angle_degree; // the start angle
    double pseudo_geodesic_target_min_angle_degree; // the target angle for max function value of LS
    double pseudo_geodesic_target_max_angle_degree; // the target angle for min function value of LS
    // bool enable_pseudo_vertex=false;
    Eigen::MatrixXd ver_dbg;
    Eigen::MatrixXd ver_dbg1;
    bool flag_dbg = false;
    int id_dbg;
    Eigen::MatrixXd E0_dbg;
    Eigen::MatrixXd direction_dbg;
    Eigen::MatrixXd pnorm_dbg;
    std::vector<Eigen::Vector3d> pseudo_vers_dbg;
    std::vector<Eigen::Vector3d> pnorm_list_dbg;
    Eigen::MatrixXd visual_pts_dbg;
    Eigen::MatrixXd vBinormal;

    // this function should be calculated first once the class get constructed
    //  1. get face normals;
    //  2. get all the angles;
    //  3. get vertex normals using angles and face normals;
    //  4. get the face rotation matrices.
    //  5. get the rotated edges in each face.
    //  6. get the gradients on vertices
    //  7. get the Hessian of function f on vertices
    //  8. get rotated edges in parametric domain
    //  9. get the I and II fundamental forms
    void initialize_mesh_properties()
    {
        // 1
        get_mesh_normals_per_face();
        // 2
        get_mesh_angles();
        // // 3
        get_mesh_normals_per_ver();
        // 4
        get_face_rotation_matices();

        // 5
        get_rotated_edges_for_each_face();
        // 6
        get_function_gradient_vertex();

        // 7
        get_function_hessian_vertex();

        // 8
        // get_rotated_parameter_edges();
        // 9
        // get_I_and_II_locally();
        get_vertex_rotation_matices();
        get_bnd_vers_and_handles();
        get_all_the_edge_normals();
        
    }
    void prepare_level_set_solving(const EnergyPrepare &Energy_initializer);
    void prepare_mesh_optimization_solving(const MeshEnergyPrepare& initializer);
    void show_level_set(Eigen::VectorXd &val);
    // convert the parameters into a mesh, for visulization purpose
    void convert_paras_as_meshes(CGMesh &output);

    void show_gradients(Eigen::MatrixXd &E0, Eigen::MatrixXd &E1, double ratio);
    void show_gradient_scalar(Eigen::VectorXd &values, int which);
    void show_face_gradients(Eigen::MatrixXd &E0, Eigen::MatrixXd &E1, double ratio);
    void show_current_reference_points(Eigen::MatrixXd &pts);
    void show_1_order_derivate(Eigen::MatrixXd &E0, Eigen::MatrixXd &E1, Eigen::MatrixXd &E2, Eigen::MatrixXd &E3, double ratio);
    void show_vertex_normal(Eigen::MatrixXd &E0, Eigen::MatrixXd &E1, double ratio);
    void show_pseudo_geodesic_curve(std::vector<Eigen::MatrixXd> &E0, std::vector<Eigen::MatrixXd> &E1, Eigen::MatrixXd &vers);
    void show_binormals(Eigen::MatrixXd& E0, Eigen::MatrixXd& E1, double ratio);
    void print_info(const int vid);

    void Trace_One_Guide_Pseudo_Geodesic();
    // after tracing, use this function to get smooth level set
    void Run_Level_Set_Opt();
    void Run_Mesh_Opt();
    void initialize_level_set_accroding_to_parametrization();
    void debug_tool(int id = 0, int id2 = 0, double value = 0);
    void debug_tool_v2(const std::vector<int> &ids, const std::vector<double> values);
    void debug_tool_v3(int id = 0, int id2 = 0, double value = 0);
    void debug_tool_v4(const std::vector<int> &ids, const std::vector<double> values);
    void initialize_level_set_by_tracing(const TracingPrepare& Tracing_initializer);
    void extract_levelset_curves(const int nbr, std::vector<Eigen::MatrixXd> &E0, std::vector<Eigen::MatrixXd> &E1);
    void make_sphere_ls_example(int rowid);
};

// // partial derivate tools, regard level set function as variates.
// class PDtools{
//     PDtools(){};
// };
