#include <Eigen/QR>

#include "factors/feat.h"

using namespace Eigen;
using namespace xform;

#define Tr transpose()

namespace salsa
{

FeatFunctor::FeatFunctor(const Matrix2d& cov, const xform::Xformd& x_b2c, const Vector3d &zetai,
                         const Vector3d &zetaj, int to_idx) :
    to_idx_(to_idx),
    zetai_(zetai),
    zetaj_(zetaj),
    x_b2c_(x_b2c)
{
    Xi_ = cov.inverse().llt().matrixL().transpose();
    //  Pz_.block<1,3>(0,0) = zetaj_.cross(e_x).transpose().normalized();
    //  Pz_.block<1,3>(1,0) = zetaj_.cross(Pz_.block<1,3>(0,0).transpose()).transpose().normalized();
    Pz_.setZero();
    Pz_(0,0) = 1;
    Pz_(1,1) = 1;
}

template <typename T>
bool FeatFunctor::operator ()(const T* _xi, const T* _xj, const T* _rho, T* _res) const
{
    typedef Matrix<T,2,1> Vec2;
    typedef Matrix<T,3,1> Vec3;
    typedef Matrix<T,3,3> Mat3;
    Map<Vec2> res(_res);
    Xform<T> xi(_xi);
    Xform<T> xj(_xj);
    const T& rho(*_rho);
    Vec3 zi = 1.0/rho * zetai_;
    Vec3 zj_hat = x_b2c_.rotp<T>(xj.rotp(xi.transforma(x_b2c_.transforma<T>(zi)) - xj.transforma(x_b2c_.t_)));
    //    Vec3 zj_hat = xi.rotp(e_z);
    zj_hat.normalize();

    //    Mat3 RI2i = xi.q().R();
    //    Vec3 pI2i = xi.t();
    //    Mat3 RI2j = xj.q().R();
    //    Vec3 pI2j = xj.t();
    //    Matrix3d Rb2c = x_b2c_.q().R();
    //    Vector3d pb2c = x_b2c_.t();

    //    Vec3 zj_hat = /*Rb2c **/ /*(RI2j**/(/*RI2i.Tr*(Rb2c.Tr*(zi)+pb2c)+*/pI2i /*- RI2j.Tr*(pb2c)-pI2j*//*)*/);
    //    Vec3 zj_hat = x_b2c_.rotp(xj.rotp(xi.transforma(x_b2c_.transforma(zi)) - xj.transforma(x_b2c_.t_)));
    //    Vec3 zj_hat = Rb2c * (RI2j*(RI2i*(pI2i - (RI2j *pb2c + pI2j))));
    //    std::cout << "Pz " << Pz_ << std::endl;
    //    std::cout << "Rb2c " << Rb2c << std::endl;
    //    std::cout << "RI2j " << RI2j << std::endl;
    //    Vec3 zj_hat = Rb2c * (RI2j*(pI2i));


    //    res = Xi_ * Pz_ * (zj_hat - zetaj_);
    //    Vec3 zj_hat = pI2i;
    //    T norm_zj = zj_hat.norm();
    //    std::cout << "zj " << zj_hat << std::endl;
    res = Xi_ * Pz_ * (zj_hat - zetaj_);
    return true;
}

template bool FeatFunctor::operator ()<double>(const double* _xi, const double* _xj,
const double* _rho, double* _res) const;
typedef ceres::Jet<double, 15> jactype;
template bool FeatFunctor::operator ()<jactype>(const jactype* _xi, const jactype* _xj,
const jactype* _rho, jactype* _res) const;


bool FeatFactor::Evaluate(const double * const *parameters, double *residuals, double **jacobians) const
{
    Map<Vector2d> res(residuals);
    Xformd xi(parameters[0]);
    Xformd xj(parameters[1]);
    const double& rho(*parameters[2]);
    Vector3d zi = 1.0/rho * zetai_;

    Matrix3d RI2i = xi.q().R();
    Vector3d pI2i = xi.t();
    Matrix3d RI2j = xj.q().R();
    Vector3d pI2j = xj.t();
    Matrix3d Rb2c = x_b2c_.q().R();
    Vector3d pb2c = x_b2c_.t();

    Vector3d zj_hat = Rb2c * (RI2j*(RI2i.Tr*(Rb2c.Tr*(zi)+pb2c) + pI2i -pI2j) - pb2c);
    double zj_norm = zj_hat.norm();

    res = Xi_ * Pz_ * (zj_hat/zj_hat.norm() - zetaj_);


    if (jacobians)
    {

        Matrix3d Z = (I_3x3*zj_norm - zj_hat*zj_hat.Tr/zj_norm)/(zj_norm * zj_norm);
        if (jacobians[0])
        {
            Map<Matrix<double, 2, 7, RowMajor>> dres_dxi(jacobians[0]);
            Matrix<double, 3, 4> dqdd;

            quat::Quatd& qi(xi.q());
            dqdd << -qi.x()*2.0,  qi.w()*2.0,  qi.z()*2.0, -qi.y()*2.0,
                    -qi.y()*2.0, -qi.z()*2.0,  qi.w()*2.0,  qi.x()*2.0,
                    -qi.z()*2.0,  qi.y()*2.0, -qi.x()*2.0,  qi.w()*2.0;
            dres_dxi.block<2,3>(0, 0) = Xi_*Pz_*Z*Rb2c*RI2j;
            dres_dxi.block<2,4>(0, 3) = -Xi_*Pz_*Z*Rb2c*RI2j*RI2i.Tr*skew(Rb2c.Tr*zi+pb2c)*dqdd;
        }
        if (jacobians[1])
        {
            Map<Matrix<double, 2, 7, RowMajor>> dres_dxj(jacobians[1]);
            Matrix<double, 3, 4> dqdd;
            quat::Quatd& qj(xj.q());
            dqdd << -qj.x()*2.0,  qj.w()*2.0,  qj.z()*2.0, -qj.y()*2.0,
                    -qj.y()*2.0, -qj.z()*2.0,  qj.w()*2.0,  qj.x()*2.0,
                    -qj.z()*2.0,  qj.y()*2.0, -qj.x()*2.0,  qj.w()*2.0;
            dres_dxj.block<2,3>(0, 0) = -Xi_*Pz_*Z*Rb2c*RI2j;
            dres_dxj.block<2,4>(0, 3) = Xi_*Pz_*Z*Rb2c
                    *skew(RI2j*(RI2i.Tr*(Rb2c.Tr*(zi)+pb2c) + pI2i -pI2j))
                    *dqdd;
        }
        if (jacobians[2])
        {
            Map<Matrix<double, 2, 1>> dres_drho(jacobians[2]);
            dres_drho = -Xi_*Pz_*Z*Rb2c*RI2j*RI2i.Tr*Rb2c.Tr*1.0/rho*zi;
        }
    }

}
}
