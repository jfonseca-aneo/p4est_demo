#include <iostream>

#ifdef P4_TO_P8
#include <p8est.h>
#include <p8est_extended.h>
#include <p8est_vtk.h>
#else
#include <p4est.h>
#include <p4est_extended.h>
#include <p4est_vtk.h>
#endif

#include <stdlib.h>
#include <tiffio.h>
#include <vector>

#include <TiffHolder.hpp>

#define INACTIVE_CELL -1

typedef struct quad_data {
  uint8_t rgb;
} quad_data_t;

static void get_pixel_neighbours(TiffHolder *holder, int i, int j, int k,
                                 int *neighbours) {

  const auto &img = holder->imageVectors[k];

  for (auto f = 0; f < P4EST_FACES; f++)
    neighbours[f] = INACTIVE_CELL;

  if (i - 1 >= 0)
    neighbours[0] =
        static_cast<int>(TIFFGetR(img[j * holder->width + i - 1]));

  if (i + 1 < holder->width)
    neighbours[1] =
        static_cast<int>(TIFFGetR(img[j * holder->width + i + 1]));

  if (j - 1 >= 0)
    neighbours[2] =
        static_cast<int>(TIFFGetR(img[(j - 1) * holder->width + i]));

  if (j + 1 < holder->height)
    neighbours[3] =
        static_cast<int>(TIFFGetR(img[(j + 1) * holder->width + i]));

#ifdef P4_TO_P8
  if (k - 1 >= 0) {
    const auto &img_bottom = holder->imageVectors[k - 1];
    neighbours[4] =
        static_cast<int>(TIFFGetR(img_bottom[j * holder->width + i]));
  }

  if (k + 1 < holder->layers) {
    const auto &img_up = holder->imageVectors[k + 1];
    neighbours[5] =
        static_cast<int>(TIFFGetR(img_up[j * holder->width + i]));
  }
#endif
}

static void init_data(p4est_t *forest, p4est_topidx_t which_tree,
                      p4est_quadrant_t *quadrant) {

  double v[] = {0, 0, 0};

  auto input_tiff = static_cast<TiffHolder *>(forest->user_pointer);

  auto factor = pow(2., quadrant->level);
  p4est_qcoord_to_vertex(forest->connectivity, which_tree, quadrant->x,
                         quadrant->y,
#ifdef P4_TO_P8
                         quadrant->z,
#endif
                         v);

  auto qdata = static_cast<quad_data_t *>(quadrant->p.user_data);

  const auto &img = input_tiff->imageVectors[factor * v[2]];

  qdata->rgb = static_cast<uint8_t>(
      TIFFGetR(img[factor * v[1] * input_tiff->width + factor * v[0]]));
}

static int coarsen_func(p4est_t *forest, p4est_topidx_t which_tree,
                        p4est_quadrant_t *quadrants[]) {

  double v[] = {0, 0, 0};
  auto input_tiff = static_cast<TiffHolder *>(forest->user_pointer);

  /* First quad data */
  auto first_quad = quadrants[0];
  auto qd = static_cast<quad_data_t *>(first_quad->p.user_data);
  auto bb = qd->rgb;

  /* If one of the quads of the family has a different pixel
   * we do not mark the family for coarsening */
  for (int r = 1; r < P4EST_CHILDREN; r++) {
    auto q = quadrants[r];
    auto qd = static_cast<quad_data_t *>(q->p.user_data);
    auto bb_tmp = qd->rgb;
    if (bb != bb_tmp && bb_tmp != INACTIVE_CELL) {
      return 0;
    }
  }

  /* If one of the quads of the family has a different pixel
   * face neighbor we do not mark the family for coarsening */

  int neigh[P4EST_FACES];
  if (input_tiff->initial_level == first_quad->level) {
    for (int r = 0; r < P4EST_CHILDREN; r++) {
      auto q = quadrants[r];
      auto qd = static_cast<quad_data_t *>(q->p.user_data);
      p4est_qcoord_to_vertex(forest->connectivity, which_tree, q->x, q->y,
#ifdef P4_TO_P8
                             q->z,
#endif
                             v);

      auto factor = pow(2., q->level);
      auto ii = factor * v[0];
      auto jj = factor * v[1];
      auto kk = factor * v[2];
      get_pixel_neighbours(input_tiff, ii, jj, kk, neigh);

      for (int t = 0; t < P4EST_FACES; t++) {
        if (qd->rgb != neigh[t] && neigh[t] != INACTIVE_CELL &&
            qd->rgb != INACTIVE_CELL)
          return 0;
      }
    }
  }

  return 1;
}

static void replace_for_coarsening(p4est_t *p4est, p4est_topidx_t which_tree,
                                   int num_outgoing,
                                   p4est_quadrant_t *outgoing[],
                                   int num_incoming,
                                   p4est_quadrant_t *incoming[]) {

  auto q_in = incoming[0];
  auto qd = static_cast<quad_data_t *>(q_in->p.user_data);

  auto q_out = outgoing[0];
  auto qd_tmp = static_cast<quad_data_t *>(q_out->p.user_data);

  qd->rgb = qd_tmp->rgb;
}

static void replace_for_balance(p4est_t *p4est, p4est_topidx_t which_tree,
                                int num_outgoing, p4est_quadrant_t *outgoing[],
                                int num_incoming,
                                p4est_quadrant_t *incoming[]) {

  quad_data_t *qd, *qd_tmp;
  p4est_quadrant_t *q_in, *q_out;

  q_out = outgoing[0];
  qd_tmp = static_cast<quad_data_t *>(q_out->p.user_data);

  for (int r = 0; r < P4EST_CHILDREN; r++) {
    q_in = incoming[r];
    qd = static_cast<quad_data_t *>(q_in->p.user_data);
    qd->rgb = qd_tmp->rgb;
  }
}

static int powtwo_div(int a) {
  int c = 0;
  while (!(a % 2)) {
    ++c;
    a /= 2;
  }
  return c;
}

static int my_gcd(int a, int b) {
  int c;
  while (a) {
    c = a;
    a = b % a;
    b = c;
  }
  return b;
}

static void write_mesh_to_file(p4est_t *forest, const char *filename) {
  int retval;
  p4est_vtk_context_t *context;

  auto material_data =
      sc_array_new_size(sizeof(double), forest->local_num_quadrants);

  /* Extract and store the material data from this rank into a sc_array
   * to attach it as cell data in the mesh vtk */
  auto k = 0;
  for (auto t = forest->first_local_tree; t <= forest->last_local_tree; t++) {
    auto tree = p4est_tree_array_index(forest->trees, t);
    auto quadrants = &(tree->quadrants);
    auto n_quads = quadrants->elem_count;

    for (auto q = 0; q < n_quads; q++, k++) {
      auto quad = p4est_quadrant_array_index(quadrants, q);
      auto qdata = static_cast<quad_data_t *>(quad->p.user_data);
      auto val = static_cast<double *>(sc_array_index(material_data, k));
      *val = qdata->rgb;
    }
  }

  context = p4est_vtk_context_new(forest, filename);
  p4est_vtk_context_set_geom(context, NULL);

  p4est_vtk_context_set_continuous(context, 1);

  p4est_vtk_context_set_scale(context, 1.0);

  context = p4est_vtk_write_header(context);
  SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk: Error writing header");

  context = p4est_vtk_write_cell_dataf(context, 1, /* do not write tree id */
                                       1,          /* do not write the level */
                                       0,          /* do no write the rank */
                                       0,          /* do not wrap the rank */
                                       1, /* Number of scalars data sets */
                                       0 /* Number of vector data sets */,
                                       "material_id", material_data, context);

  SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk: Error writing cell data");

  retval = p4est_vtk_write_footer(context);
  SC_CHECK_ABORT(!retval, P4EST_STRING "_vtk: Error writing footer");

  sc_array_destroy(material_data);
}

int main(int argc, char **argv) {

  if (argc != 2 && argc != 5) {
    std::cout << "Usage: mpirun -np <nprocs> " << argv[0]
              << " <path_to_tiff>  <Nx> <Ny> <Nz> \n"
              << " If no image dimensions are given those read\n"
              << " from the tiff file are used. The idea to specify them \n"
              << " is to tweak the dimensions such that gcd(Nx, Ny, Nz) \n"
              << " yields the biggest power of two as possible. \n";
    return 1;
  }

  std::string filePath = argv[1];

  uint32_t layers;
  uint32_t width;
  uint32_t height;

  TiffHolder input_tiff(filePath);

  if (argc == 5) {
    width = atoi(argv[2]);
    height = atoi(argv[3]);
    layers = atoi(argv[4]);
  } else {
    width = input_tiff.width;
    height = input_tiff.height;
    layers = input_tiff.layers;
  }

  auto t = my_gcd(width, height);
#ifdef P4_TO_P8
  auto tt = my_gcd(t, layers);
  auto initial_level = powtwo_div(tt);
#else
  auto initial_level = powtwo_div(t);
#endif

  auto g = 1 << initial_level;

  input_tiff.set_initial_level(initial_level);

  /* initialize MPI and p4est internals */
  auto mpi_init_return = sc_MPI_Init(&argc, &argv);
  SC_CHECK_MPI(mpi_init_return);

  int rank;
  auto mpi_rank_return = sc_MPI_Comm_rank(sc_MPI_COMM_WORLD, &rank);
  SC_CHECK_MPI(mpi_rank_return);

  if (rank == 0) {
    std::cout << "Initial level is: " << initial_level << "\n";
    input_tiff.printTiffInfo();
  }

  sc_init(sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_DEFAULT);
  p4est_init(NULL, SC_LP_PRODUCTION);

  /* initialize connectivity structure with a brick that will be refined
  to match the input tiff dimensions */
  auto connectivity = p4est_connectivity_new_brick(width / g, height / g,
#ifdef P4_TO_P8
                                                   layers / g, 0,
#endif
                                                   0, 0);

  /* initialize main p4est structure */
  auto forest = p4est_new_ext(sc_MPI_COMM_WORLD, connectivity, 0, initial_level,
                              1, sizeof(quad_data_t), init_data,
                              static_cast<void *>(&input_tiff));

  /* write the brick in vtk file for visualization */
  {
    std::string fullFileName = filePath;
    auto p(fullFileName.find_last_of('.'));
    std::string baseName = fullFileName.substr(0, p);
    baseName.append("_");
    baseName.append(P4EST_STRING);
    baseName.append("_uniform");
    write_mesh_to_file(forest, baseName.c_str());
  }

  /* Coarsen based on pixel information */
  p4est_coarsen_ext(forest, 1,             /* Do recursive coarsening ? */
                    0,                     /* Do callback orphans ? */
                    coarsen_func,          /* callback with coarsen criteria */
                    NULL,                  /* callback to init user data */
                    replace_for_coarsening /* callback to replace incoming quads
                                              based on quads they replace */
  );

  /* Face balance */
  p4est_balance_ext(forest, P4EST_CONNECT_FACE, NULL, replace_for_balance);

  /* Redistribute new quads */
  p4est_partition_ext(forest, 0, NULL);

  /* write the brick in vtk file for visualization */
  {
    std::string fullFileName = filePath;
    auto p(fullFileName.find_last_of('.'));
    std::string baseName = fullFileName.substr(0, p);
    baseName.append("_");
    baseName.append(P4EST_STRING);
    baseName.append("_adaptive");
    write_mesh_to_file(forest, baseName.c_str());
  }

  /* finalize the libraries */
  p4est_destroy(forest);
  p4est_connectivity_destroy(connectivity);

  /* Finalize MPI and exit */
  auto mpi_finalize_ret = sc_MPI_Finalize();
  SC_CHECK_MPI(mpi_finalize_ret);

  sc_finalize();

  return 0;
}