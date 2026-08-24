#ifndef TiffHolder_HPP_
#define TiffHolder_HPP_

#include <iostream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <tiffio.h>
#include <vector>

class TiffHolder {
private:
  TIFF *tiff_to_track = nullptr;

public:
  uint32_t layers = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<std::vector<uint32_t>> imageVectors;
  std::string filePath;

  int initial_level = 0;

  TiffHolder(std::string _filePath) {

    filePath = _filePath;
    const char *pathAsCString = _filePath.c_str();

    tiff_to_track = TIFFOpen(pathAsCString, "r");

    if (!tiff_to_track) {
      throw std::runtime_error("Could not open TIFF file: " + filePath);
    }

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

  ~TiffHolder(){};

  void set_initial_level(int level) { initial_level = level; }

  void printTiffInfo() {
    std::cout << "Image dimension information:\n";
    std::cout << "Dim X: " << width << "\n";
    std::cout << "Dim Y: " << height << "\n";
    std::cout << "Dim Z: " << layers << "\n";
  }
};

#endif