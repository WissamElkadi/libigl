// This file is part of libigl, a simple c++ geometry processing library.
// 
// Copyright (C) 2015 Alec Jacobson <alecjacobson@gmail.com>
// 
// This Source Code Form is subject to the terms of the Mozilla Public License 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef IGL_AABB_H
#define IGL_AABB_H

#include "Hit.h"
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <vector>
namespace igl
{
  // Implementation of semi-general purpose axis-aligned bounding box hierarchy.
  // The mesh (V,Ele) is stored and managed by the caller and each routine here
  // simply takes it as references (it better not change between calls).
  //
  // It's a little annoying that the Dimension is a template parameter and not
  // picked up at run time from V. This leads to duplicated code for 2d/3d (up to
  // dim).
  template <typename DerivedV, int DIM>
    class AABB 
    {
public:
      typedef typename DerivedV::Scalar Scalar;
      typedef Eigen::Matrix<Scalar,1,DIM> RowVectorDIMS;
      typedef Eigen::Matrix<Scalar,DIM,1> VectorDIMS;
      typedef Eigen::Matrix<Scalar,Eigen::Dynamic,DIM> MatrixXDIMS;
      // Shared pointers are slower...
      AABB * m_left;
      AABB * m_right;
      Eigen::AlignedBox<Scalar,DIM> m_box;
      // -1 non-leaf
      int m_primitive;
      //Scalar m_max_sqr_d;
      //int m_depth;
      AABB():
        m_left(NULL), m_right(NULL),
        m_box(), m_primitive(-1)
        //m_max_sqr_d(std::numeric_limits<double>::infinity()),
        //m_depth(0)
    {}
      // http://stackoverflow.com/a/3279550/148668
      AABB(const AABB& other):
        m_left(other.m_left ? new AABB(*other.m_left) : NULL),
        m_right(other.m_right ? new AABB(*other.m_right) : NULL),
        m_box(other.m_box),
        m_primitive(other.m_primitive)
        //m_max_sqr_d(other.m_max_sqr_d),
        //m_depth(std::max(
        //   m_left ? m_left->m_depth + 1 : 0,
        //   m_right ? m_right->m_depth + 1 : 0))
        {
        }
      // copy-swap idiom
      friend void swap(AABB& first, AABB& second)
      {
        // Enable ADL
        using std::swap;
        swap(first.m_left,second.m_left);
        swap(first.m_right,second.m_right);
        swap(first.m_box,second.m_box);
        swap(first.m_primitive,second.m_primitive);
        //swap(first.m_max_sqr_d,second.m_max_sqr_d);
        //swap(first.m_depth,second.m_depth);
      }
      // Pass-by-value (aka copy)
      AABB& operator=(AABB other)
      {
        swap(*this,other);
        return *this;
      }
      AABB(AABB&& other):
        // initialize via default constructor
        AABB() 
      {
        swap(*this,other);
      }
      // Seems like there should have been an elegant solution to this using
      // the copy-swap idiom above:
      inline void deinit()
      {
        m_primitive = -1;
        m_box = Eigen::AlignedBox<Scalar,DIM>();
        delete m_left;
        m_left = NULL;
        delete m_right;
        m_right = NULL;
      }
      ~AABB()
      {
        deinit();
      }
      // Build an Axis-Aligned Bounding Box tree for a given mesh and given
      // serialization of a previous AABB tree.
      //
      // Inputs:
      //   V  #V by dim list of mesh vertex positions. 
      //   Ele  #Ele by dim+1 list of mesh indices into #V. 
      //   bb_mins  max_tree by dim list of bounding box min corner positions
      //   bb_maxs  max_tree by dim list of bounding box max corner positions
      //   elements  max_tree list of element or (not leaf id) indices into Ele
      //   i  recursive call index {0}
      template <typename Derivedbb_mins, typename Derivedbb_maxs>
        inline void init(
            const Eigen::PlainObjectBase<DerivedV> & V,
            const Eigen::MatrixXi & Ele, 
            const Eigen::PlainObjectBase<Derivedbb_mins> & bb_mins,
            const Eigen::PlainObjectBase<Derivedbb_maxs> & bb_maxs,
            const Eigen::VectorXi & elements,
            const int i = 0);
      // Wrapper for root with empty serialization
      inline void init(
          const Eigen::PlainObjectBase<DerivedV> & V,
          const Eigen::MatrixXi & Ele);
      // Build an Axis-Aligned Bounding Box tree for a given mesh.
      //
      // Inputs:
      //   V  #V by dim list of mesh vertex positions. 
      //   Ele  #Ele by dim+1 list of mesh indices into #V. 
      //   SI  #Ele by dim list revealing for each coordinate where Ele's
      //     barycenters would be sorted: SI(e,d) = i --> the dth coordinate of
      //     the barycenter of the eth element would be placed at position i in a
      //     sorted list.
      //   I  #I list of indices into Ele of elements to include (for recursive
      //     calls)
      // 
      inline void init(
          const Eigen::PlainObjectBase<DerivedV> & V,
          const Eigen::MatrixXi & Ele, 
          const Eigen::MatrixXi & SI,
          const Eigen::VectorXi & I);
      // Return whether at leaf node
      inline bool is_leaf() const;
      // Find the indices of elements containing given point: this makes sense
      // when Ele is a co-dimension 0 simplex (tets in 3D, triangles in 2D).
      //
      // Inputs:
      //   V  #V by dim list of mesh vertex positions. **Should be same as used to
      //     construct mesh.**
      //   Ele  #Ele by dim+1 list of mesh indices into #V. **Should be same as used to
      //     construct mesh.**
      //   q  dim row-vector query position
      //   first  whether to only return first element containing q
      // Returns:
      //   list of indices of elements containing q
      template <typename Derivedq>
      inline std::vector<int> find(
          const Eigen::PlainObjectBase<DerivedV> & V,
          const Eigen::MatrixXi & Ele, 
          const Eigen::PlainObjectBase<Derivedq> & q,
          const bool first=false) const;

      // If number of elements m then total tree size should be 2*h where h is
      // the deepest depth 2^ceil(log(#Ele*2-1))
      inline int subtree_size() const;

      // Serialize this class into 3 arrays (so we can pass it pack to matlab)
      //
      // Outputs:
      //   bb_mins  max_tree by dim list of bounding box min corner positions
      //   bb_maxs  max_tree by dim list of bounding box max corner positions
      //   elements  max_tree list of element or (not leaf id) indices into Ele
      //   i  recursive call index into these arrays {0}
      template <typename Derivedbb_mins, typename Derivedbb_maxs>
        inline void serialize(
            Eigen::PlainObjectBase<Derivedbb_mins> & bb_mins,
            Eigen::PlainObjectBase<Derivedbb_maxs> & bb_maxs,
            Eigen::VectorXi & elements,
            const int i = 0) const;
      // Compute squared distance to a query point
      //
      // Inputs:
      //   V  #V by dim list of vertex positions
      //   Ele  #Ele by dim list of simplex indices
      //   P  3 list of query point coordinates
      //   min_sqr_d  current minimum squared distance (only find distances
      //   less than this)
      // Outputs:
      //   I  #P list of facet indices corresponding to smallest distances
      //   C  #P by 3 list of closest points
      // Returns squared distance
      //
      // Known bugs: currently assumes Elements are triangles regardless of
      // dimension.
      inline Scalar squared_distance(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & p,
        int & i,
        RowVectorDIMS & c) const;
//private:
      inline Scalar squared_distance(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & p,
        const Scalar min_sqr_d,
        int & i,
        RowVectorDIMS & c) const;
      // All hits
      inline bool intersect_ray(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & origin,
        const RowVectorDIMS & dir,
        std::vector<igl::Hit> & hits) const;
      // First hit
      inline bool intersect_ray(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & origin,
        const RowVectorDIMS & dir,
        igl::Hit & hit) const;
//private:
      inline bool intersect_ray(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & origin,
        const RowVectorDIMS & dir,
        const Scalar min_t,
        igl::Hit & hit) const;


public:
      template <
        typename DerivedP, 
        typename DerivedsqrD, 
        typename DerivedI, 
        typename DerivedC>
      inline void squared_distance(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const Eigen::PlainObjectBase<DerivedP> & P,
        Eigen::PlainObjectBase<DerivedsqrD> & sqrD,
        Eigen::PlainObjectBase<DerivedI> & I,
        Eigen::PlainObjectBase<DerivedC> & C) const;

      template < 
        typename Derivedother_V,
        typename DerivedsqrD, 
        typename DerivedI, 
        typename DerivedC>
      inline void squared_distance(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const AABB<Derivedother_V,DIM> & other,
        const Eigen::PlainObjectBase<Derivedother_V> & other_V,
        const Eigen::MatrixXi & other_Ele, 
        Eigen::PlainObjectBase<DerivedsqrD> & sqrD,
        Eigen::PlainObjectBase<DerivedI> & I,
        Eigen::PlainObjectBase<DerivedC> & C) const;
private:
      template < 
        typename Derivedother_V,
        typename DerivedsqrD, 
        typename DerivedI, 
        typename DerivedC>
      inline Scalar squared_distance_helper(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const AABB<Derivedother_V,DIM> * other,
        const Eigen::PlainObjectBase<Derivedother_V> & other_V,
        const Eigen::MatrixXi & other_Ele, 
        const Scalar min_sqr_d,
        Eigen::PlainObjectBase<DerivedsqrD> & sqrD,
        Eigen::PlainObjectBase<DerivedI> & I,
        Eigen::PlainObjectBase<DerivedC> & C) const;
      // Helper function for leaves: works in-place on sqr_d
      inline void leaf_squared_distance(
        const Eigen::PlainObjectBase<DerivedV> & V,
        const Eigen::MatrixXi & Ele, 
        const RowVectorDIMS & p,
        Scalar & sqr_d,
        int & i,
        RowVectorDIMS & c) const;
      inline void set_min(
        const RowVectorDIMS & p,
        const Scalar sqr_d_candidate,
        const int i_candidate,
        const RowVectorDIMS & c_candidate,
        Scalar & sqr_d,
        int & i,
        RowVectorDIMS & c) const;
public:
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}


#ifndef IGL_STATIC_LIBRARY
#  include "AABB.cpp"
#endif

#endif
