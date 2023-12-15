# About this repository

Demo code to show how to use the AMR software library
[p4est](https://github.com/cburstedde/p4est) to create a
mesh from a `tiff` image. The motivation is that the image
corresponds to a certain material composed of various phases.
In addition, the image defines a fine uniform mesh in order to
depict all these phases and the goal is to coarsen that mesh
in the regions where the phases do not change and keep it
fine between phase transitions / boundaries.

## Compilation

### Prerequisites

- [p4est](https://github.com/cburstedde/p4est)
- [libTIFF](http://www.libtiff.org/)

A [make file](./Makefile.template) template has been provided
to facilitate the compilation, just set the `P4EST_LIBRARY_HOME`
defined in that file and rename it to `Makefile`. 

## Usage

Successful compilation yields the  executable `mesh_from_tiff` which takes
as argument the path to the desired `tiff` image to build the mesh from.

### Example

![](./test_example.png)