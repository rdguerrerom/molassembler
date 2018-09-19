#ifndef INCLUDE_MOLASSEMBLER_DISTANCE_GEOMETRY_METRIC_MATRIX_H
#define INCLUDE_MOLASSEMBLER_DISTANCE_GEOMETRY_METRIC_MATRIX_H

#include <Eigen/Core>

#include "molassembler/DistanceGeometry/DistanceGeometry.h"

/*! @file
 *
 * @brief Metric matrix class for DG semantics
 *
 * In the Distance Geometry algorithm, a metric matrix is generated from the
 * distance bounds.
 *
 * The metric matrix then offers the functionality to embed itself into four
 * spatial coordinates. The enclosed closely mirror the algorithms described
 * in rough outline in:
 *
 * - Blaney, J. M., & Dixon, J. S. (2007). Distance Geometry in Molecular
 *   Modeling. Reviews in Computational Chemistry, 5, 299–335.
 *   https://doi.org/10.1002/9780470125823.ch6
 *
 * and in more detail in
 *
 * - Crippen, G. M., & Havel, T. F. (1988). Distance geometry and molecular
 *   conformation (Vol. 74). Taunton: Research Studies Press.
 *   (PDF available as download!)
 */

namespace molassembler {

namespace DistanceGeometry {

class MetricMatrix {
public:
/* Constructors */
  MetricMatrix() = delete;
  explicit MetricMatrix(Eigen::MatrixXd distanceMatrix);

/* Information */
  //! Allow const ref access to underlying matrix
  const Eigen::MatrixXd& access() const;

  /*!
   * Embeds itself into 4D space, returning a dynamically sized Matrix where
   * every column vector is the coordinates of a particle.
   *
   * @note For Molecules of size 20 and lower, employs full diagonalization. If
   * larger, attempts to calculate only the required eigenpairs. If that fails,
   * falls back on full diagonalization.
   */
  Eigen::MatrixXd embed() const;

  /*! Implements embedding employing full diagonalization
   *
   * Uses Eigen's SelfAdjointEigenSolver to fully diagonalize the matrix,
   * calculating all eigenpairs. Then selects the necessary from the full set.
   *
   * @note Faster for roughly N < 20
   */
  Eigen::MatrixXd embedWithFullDiagonalization() const;

  /*! Implements embedding calculating only the needed eigenpairs
   *
   * Uses Spectra's SymEigsSolver that uses Arnoldi iterations under the hood
   * to calculate only the required eigenpairs for embedding.
   *
   * @note Faster from roughly N >= 20 on
   */
  Eigen::MatrixXd embedWithNeededEigenpairs() const;

/* Operators */
  bool operator == (const MetricMatrix& other) const;

private:
/* Underlying matrix representation */
  Eigen::MatrixXd _matrix;

  void _constructFromTemporary(Eigen::MatrixXd&& distances);
};

} // namespace DistanceGeometry

} // namespace molassembler

#endif
