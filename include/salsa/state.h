#pragma once
#include <deque>
#include <Eigen/Core>

#include "geometry/xform.h"
#include "factors/feat.h"

using namespace Eigen;
using namespace xform;

namespace salsa
{

class State
{
public:
    double buf_[13];
    int kf;
    int node;
    Xformd x;
    double& t;
    Map<Vector3d> v;
    Map<Vector2d> tau;

    State();
};

class Features
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    int id; // image label
    double t; // time stamp of this image
    std::vector<Vector3d, aligned_allocator<Vector3d>> zetas; // unit vectors to features
    std::vector<double> depths; // feature distances corresponding to feature measurements
    std::vector<int> feat_ids; // feature ids corresonding to pixel measurements

    void reserve(const int& N);
    void clear();
};

typedef std::deque<FeatFunctor, aligned_allocator<FeatFunctor>> FeatDeque;

struct Feat
{
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    int kf0;
    int idx0;
    double rho;
    Vector3d z0;
    FeatDeque funcs;

    Feat(int _idx, int _kf0, const Vector3d& _z0, double _rho);

    void addMeas(int to_idx, const Xformd& x_b2c, const Matrix2d& cov, const Vector3d& zj);
    void moveMeas(int to_idx, const Vector3d& zj);
    bool slideAnchor(int new_from_idx, int new_from_kf, const State* xbuf, const Xformd& x_b2c);
    Vector3d pos(const State* xbuf, const Xformd& x_b2c);
};

}