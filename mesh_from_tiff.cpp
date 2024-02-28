#include <iostream>
#include <p4est_to_p8est.h>
#include <p8est.h>
#include <p8est_extended.h>
#include <p8est_iterate.h>
#include <p8est_vtk.h>
#include <stdlib.h>
#include <tiffio.h>
#include <vector>

class TiffHolder {
private:
  TIFF *tiff_to_track = nullptr;

public:
  uint32_t layers;
  uint32_t width;
  uint32_t height;
  std::vector<std::vector<uint32_t>> imageVectors;

  TiffHolder(std::string filePath) {

    const char *pathAsCString = filePath.c_str();

    tiff_to_track = TIFFOpen(pathAsCString, "r");

    if (tiff_to_track) {

      // Get image information
      TIFFGetField(tiff_to_track, TIFFTAG_IMAGEWIDTH, &width);
      TIFFGetField(tiff_to_track, TIFFTAG_IMAGELENGTH, &height);
      layers = TIFFNumberOfDirectories(tiff_to_track);

      for (int l = 0; l < layers; l++) {

        TIFFSetDirectory(tiff_to_track, l);

        // Allocate memory for current image layer
        std::vector<uint32_t> raster(width * height);

        // Read the image
        if (TIFFReadRGBAImage(tiff_to_track, width, height, raster.data())) {
          // Add the image data to the vector
          imageVectors.push_back(std::move(raster));
        }
      }

      // Close TIFF file
      TIFFClose(tiff_to_track);
    }
  }

  ~TiffHolder(){};

  void printTiffInfo() {
    std::cout << "Image dimension information:\n";
    std::cout << "Dim X: " << width << "\n";
    std::cout << "Dim Y: " << height << "\n";
    std::cout << "Dim Z: " << layers << "\n";
  }
};

typedef struct quad_data {
  double rgb;
} quad_data_t;

static void init_data(p4est_t *forest, p4est_topidx_t which_tree,
                      p4est_quadrant_t *quadrant) {

  double v[3];

  auto input_tiff = static_cast<TiffHolder *>(forest->user_pointer);

  auto factor = pow(2., quadrant->level);
  p4est_qcoord_to_vertex(forest->connectivity, which_tree, quadrant->x,
                         quadrant->y, quadrant->z, v);
  auto qdata = static_cast<quad_data_t *>(quadrant->p.user_data);

  auto img = input_tiff->imageVectors[factor * v[2]];

  qdata->rgb = static_cast<double>(
      img[factor * v[1] * input_tiff->width + factor * v[0]]);
}

static int coarsen_func(p4est_t *forest, p4est_topidx_t which_tree,
                        p4est_quadrant_t *quadrants[]) {

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
    if (bb != bb_tmp) {
      return 0;
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

  context = p4est_vtk_write_header(context);
  SC_CHECK_ABORT(context != NULL, P4EST_STRING "_vtk: Error writing header");

  context = p4est_vtk_write_cell_dataf(context, 1, /* write tree id */
                                       0,          /* do not write the level */
                                       1,          /* write the rank */
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

  std::string filePath = argv[1];

  TiffHolder *input_tiff = new TiffHolder(filePath);

  input_tiff->printTiffInfo();

  auto width = 416;                 // input_tiff->width;
  auto height = 368;                // input_tiff->height;
  auto layers = input_tiff->layers; // 496

  auto t = my_gcd(width, height);
  auto tt = my_gcd(t, layers);

  auto initial_level = powtwo_div(tt);
  auto g = 1 << initial_level;

  std::cout << "Initial level is: " << g << "\n";

  /* initialize MPI and p4est internals */
  auto mpi_init_return = sc_MPI_Init(&argc, &argv);
  SC_CHECK_MPI(mpi_init_return);

  sc_init(sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_DEFAULT);
  p4est_init(NULL, SC_LP_PRODUCTION);

  /* initialize connectivity structure with a brick that will be refined
  to match the input tiff dimensions */
  auto connectivity =
      p4est_connectivity_new_brick(width / g, height / g, layers / g, 0, 0, 0);

  /* initialize main p4est structure */
  auto forest = p4est_new_ext(sc_MPI_COMM_WORLD, connectivity, 0, initial_level,
                              1, sizeof(quad_data_t), init_data,
                              static_cast<void *>(input_tiff));

  /* write the brick in vtk file for visualization */
  write_mesh_to_file(forest, "mesh_from_tiff_uniform");

  /* Coarsen based on pixel information */
  p4est_coarsen_ext(forest, 1, 0, coarsen_func, NULL, replace_for_coarsening);

  /* Face balance */
  p4est_balance_ext(forest, P4EST_CONNECT_FACE, NULL, replace_for_balance);

  /* Redistribute new quads */
  p4est_partition_ext(forest, 0, NULL);

  /* write the brick in vtk file for visualization */
  write_mesh_to_file(forest, "mesh_from_tiff_adaptive");

  /* finalize the libraries */
  p4est_destroy(forest);
  p4est_connectivity_destroy(connectivity);

  /* Finalize MPI and exit */
  auto mpi_finalize_ret = sc_MPI_Finalize();
  SC_CHECK_MPI(mpi_finalize_ret);

  sc_finalize();

  delete input_tiff;

  return 0;
}
