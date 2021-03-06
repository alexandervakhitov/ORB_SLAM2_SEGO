//
// Created by alexander on 14.11.17.
//
#include <iostream>
#include <fstream>

#include "sego.h"
#include <Eigen/Dense>
#include <opencv2/core/mat.hpp>
#include <opencv/cv.hpp>
#include <opencv/cxeigen.hpp>

#include "quadric_solver.h"

void RowReducePivot(Eigen::Matrix<double,6,13>* A)
{

//    std::cout << " hello " << std::endl;
    int i = 0, j = 0;

    const int nrows = A->rows();
    const int ncols = A->cols();

    while (i < nrows && j < ncols)
    {
        double mv = -1;
        int mi = -1;
        for (int ii = i; ii < nrows; ii++)
        {
            double test_val = fabs((*A)(ii, j));
            if (test_val > mv)
            {
                mv = test_val;
                mi = ii;
            }
        }

        if (mv < 1e-6)
        {
            for (int ii = i; ii < nrows; ii++)
            {
                (*A)(ii, j) = 0;
            }
            j += 1;
        } else {
            (*A).row(mi).swap((*A).row(i));
            (*A).row(i) = (*A).row(i) / (*A)(i, j);
            for (int ii = 0; ii < nrows; ii++)
            {
                if (ii == i)
                {
                    continue;
                }
                (*A).block(ii, j, 1, ncols-j) = (*A).block(ii, j, 1, ncols-j) - (*A).block(i, j, 1, ncols-j) * (*A)(ii, j);
            }
            i += 1;
            j += 1;
        }

//        std::cout << "---" << std::endl;
//        for (int xi = 0; xi < nrows; xi++)
//        {
//            for (int yi = 0; yi < ncols; yi++)
//            {
//                std::cout << (*A)(xi, yi) <<" ";
//            }
//            std::cout << std::endl;
//        }
//        std::cout << std::endl;
    }
}

void RowReduce2(Eigen::Matrix<double, 6, 13>* A)
{

    std::ofstream rr_log("/home/alexander/rr_log");
    const int nrows = 6; // number of rows
    const int ncols = 13; // number of columns

    int i = 0, j = 0;

    while (i < nrows && j < ncols) {

        double mv = -1;
        int mi = -1;
        for (int ii = i; ii < nrows; ii++)
        {
            double test_val = fabs((*A)(ii, j));
            if (test_val > mv)
            {
                mv = test_val;
                mi = ii;
            }
        }
        if (mv < 1e-15)
        {
            rr_log <<"zeroing" << std::endl;
            for (int ii = i; ii < nrows; ii++)
            {
                (*A)(ii, j) = 0;
            }
            j += 1;
        } else {
            Eigen::MatrixXd A_block = (*A).block(0,j,nrows, ncols-j);
            rr_log << " ---" << i << " " << j << std::endl;
            for (int xi = 0; xi < nrows; xi++)
            {
                for (int yi = 0; yi < ncols; yi++)
                {
                    rr_log << (*A)(xi, yi) <<" ";
                }
                rr_log << std::endl;
            }

            A_block.row(i).swap(A_block.row(mi));
//            Eigen::Matrix<double, 1, 13> row_temp((*A).block<1,13>(i, 0));
//            (*A).block<1,13>(i, 0) = Eigen::Matrix<double, 1, 13>((*A).block<1,13>(mi, 0));
//            (*A).block<1,13>(mi, 0) = row_temp;
            double d = (*A)(i,j);
            Eigen::MatrixXd Ai(A_block.row(i) / d);
            A_block = A_block - (*A).col(j)*Ai;
            A_block.row(i) = Ai;
            (*A).block(0,j,nrows, ncols-j) = A_block;
            i += 1;
            j += 1;
        }
        rr_log << " ---" << i << " " << j << std::endl;
        for (int xi = 0; xi < nrows; xi++)
        {
            for (int yi = 0; yi < ncols; yi++)
            {
                rr_log << (*A)(xi, yi) <<" ";
            }
            rr_log << std::endl;
        }

    }
}

void RowReduce(Eigen::Matrix<double, 6, 13>* A)
{
    const int nrows = 6; // number of rows
    const int ncols = 13; // number of columns

    int lead = 0;

    while (lead < nrows) {
        float d, m;

        for (int r = 0; r < nrows; r++) { // for each row ...
            /* calculate divisor and multiplier */
            d = (*A)(lead, lead);
            m = (*A)(r, lead) / (*A)(lead, lead);

            for (int c = 0; c < ncols; c++) { // for each column ...
                if (r == lead)
                    (*A)(r, c) /= d;               // make pivot = 1
                else
                    (*A)(r, c) -= (*A)(lead, c) * m;  // make other = 0
            }
        }
        lead++;
    }
}

void build_cam_from_quat(const Eigen::VectorXd& q, Eigen::Matrix3d* R)
{
    *R << q(0)*q(0) + q(1)*q(1) - q(2)*q(2) -q(3)*q(3), 2*q(1)*q(2)-2*q(0)*q(3), 2*q(1)*q(3)+2*q(0)*q(2),
            2*q(1)*q(2)+2*q(0)*q(3), q(0)*q(0)-q(1)*q(1)+q(2)*q(2)-q(3)*q(3), 2*q(2)*q(3)-2*q(0)*q(1),
            2*q(1)*q(3)-2*q(0)*q(2), 2*q(2)*q(3)+2*q(0)*q(1), q(0)*q(0)-q(1)*q(1)-q(2)*q(2)+q(3)*q(3);
    *R = *R / R->determinant();
}

void GenerateCrossProductMatrix(const Eigen::Vector3d& v, Eigen::Matrix3d* M_p)
{
    Eigen::Matrix3d M;
    M.setZero();
    M(0, 1) = -v(2);
    M(0, 2) = v(1);
    M(1, 0) = v(2);
    M(1, 2) = -v(0);
    M(2, 0) = -v(1);
    M(2, 1) = v(0);
    *M_p = M;
}

bool TFromRUsingTrifocalLines(const cv::Mat& projs, const cv::Mat& lprojs, const cv::Mat& vis_p, const cv::Mat& vis_l,
                              const Eigen::Matrix3d& R, Eigen::Vector3d* T)
{
    int f_ind = 0;
    int fti = 0;
    Eigen::Matrix<double, 9, 3> A_full;
    Eigen::Matrix<double, 9, 1> B_full;
    while (f_ind < 3)
    {
        int ip = 2;
        if (vis_l.at<uchar>(fti, 2) == 0 || vis_l.at<uchar>(fti, 3) == 0)
        {
            ip = 1;
        }

        int c3 = -1;
        Eigen::Vector3d l1, l2;
        if (ip == 1) {
            if (vis_l.at<uchar>(fti, 2) == 0) {
                c3 = 3;
            } else {
                c3 = 2;
            }
            l1 = FromVec3d(lprojs.at<cv::Vec3d>(fti, 0));
            l2 = FromVec3d(lprojs.at<cv::Vec3d>(fti, 1));
        } else {
            if (vis_l.at<uchar>(fti, 0) == 0) {
                c3 = 1;
            } else {
                c3 = 0;
            }
            l1 = FromVec3d(lprojs.at<cv::Vec3d>(fti, 2));
            l2 = FromVec3d(lprojs.at<cv::Vec3d>(fti, 3));
        }
        Eigen::Vector3d l3 = FromVec3d(lprojs.at<cv::Vec3d>(fti, c3));
        Eigen::Matrix3d A = l2 * l3.transpose();
        Eigen::Vector3d B;
        if (ip == 1)
        {
            B = l2(0)*R.transpose()*l3;
        } else {
            B = l2(0)*R*l3;
        }
        if (c3 == 3 || c3 == 1)
        {
            B = B - l2*l3(0);
        }
        Eigen::Matrix3d L1x;
        GenerateCrossProductMatrix(l1, &L1x);
        A = L1x*A;
        B = L1x*B;
        if (ip == 2)
        {
            A = -A*R.transpose();
        }
        A_full.block<3, 3>(3*fti, 0) = A;
        B_full.segment<3>(3*fti) = B;
        fti = fti+1;
        f_ind = f_ind+1;
    }
    Eigen::FullPivLU<Eigen::Matrix<double, 9, 3>> lu_decomp(A_full);
    if (lu_decomp.rank() == 2)
    {
        return false;
    }

    *T = A_full.colPivHouseholderQr().solve(B_full);
    return true;
}
void polycoeffs(double x[3][3], double y[3][3], double z[3], double* b, double m1[3][3], double m2[3][3], double m3[3][3], double m4[3][3], double*m5p)
{
//    double m1[3][3];
//    double m2[3][3];
//    double m3[3][3];
//    double m4[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            m1[i][ j] = 0.0;
            m2[i][ j] = 0.0;
            m3[i][ j] = 0.0;
            m4[i][ j] = 0.0;
        }
    }
    m1[0][2] = (1)*x[0][0]*x[2][2]+(1)*x[0][1]*x[1][2]+(-1)*x[0][2]*x[2][0]+(-1)*x[2][1]*x[2][2];
    m4[0][2] = (1)*y[0][0]*z[2]+(1)*y[0][1]*z[1]+(-1)*y[2][0]*z[0]+(-1)*y[2][1]*z[2];
    m2[0][2] = (1)*y[2][2]*x[0][0]+(1)*y[1][2]*x[0][1]+(-1)*y[2][0]*x[0][2]+(1)*y[0][1]*x[1][2]+(-1)*y[0][2]*x[2][0]+(-1)*y[2][2]*x[2][1]+(1)*y[0][0]*x[2][2]+(-1)*y[2][1]*x[2][2];
    m3[0][2] = (1)*x[0][0]*z[2]+(1)*x[0][1]*z[1]+(-1)*x[2][0]*z[0]+(-1)*x[2][1]*z[2]+(1)*y[0][0]*y[2][2]+(1)*y[0][1]*y[1][2]+(-1)*y[0][2]*y[2][0]+(-1)*y[2][1]*y[2][2];
    m1[0][1] = (1)*x[0][0]*x[2][1]+(1)*x[0][1]*x[1][1]+(-1)*x[0][1]*x[2][0]+(-1)*x[2][1]*x[2][1]+(1)*x[0][2];
    m2[0][1] = (1)*y[2][1]*x[0][0]+(1)*y[1][1]*x[0][1]+(-1)*y[2][0]*x[0][1]+(1)*y[0][1]*x[1][1]+(-1)*y[0][1]*x[2][0]+(1)*y[0][0]*x[2][1]+(-2)*y[2][1]*x[2][1]+(1)*y[0][2];
    m3[0][1] = (1)*y[0][0]*y[2][1]+(1)*y[0][1]*y[1][1]+(-1)*y[0][1]*y[2][0]+(-1)*y[2][1]*y[2][1]+(1)*z[0];
    m1[0][0] = (1)*x[0][1]*x[1][0]+(-1)*x[2][0]*x[2][1]+(-1)*x[2][2];
    m2[0][0] = (1)*y[1][0]*x[0][1]+(1)*y[0][1]*x[1][0]+(-1)*y[2][1]*x[2][0]+(-1)*y[2][0]*x[2][1]+(-1)*y[2][2];
    m3[0][0] = (1)*y[0][1]*y[1][0]+(-1)*y[2][0]*y[2][1]+(-1)*z[2];
    m1[1][2] = (-1)*x[0][2]*x[1][0]+(-1)*x[1][1]*x[2][2]+(1)*x[1][2]*x[2][1]+(1)*x[2][0]*x[2][2];
    m4[1][2] = (-1)*y[1][0]*z[0]+(-1)*y[1][1]*z[2]+(1)*y[2][0]*z[2]+(1)*y[2][1]*z[1];
    m2[1][2] = (-1)*y[1][0]*x[0][2]+(-1)*y[0][2]*x[1][0]+(-1)*y[2][2]*x[1][1]+(1)*y[2][1]*x[1][2]+(1)*y[2][2]*x[2][0]+(1)*y[1][2]*x[2][1]+(-1)*y[1][1]*x[2][2]+(1)*y[2][0]*x[2][2];
    m3[1][2] = (-1)*x[1][0]*z[0]+(-1)*x[1][1]*z[2]+(1)*x[2][0]*z[2]+(1)*x[2][1]*z[1]+(-1)*y[0][2]*y[1][0]+(-1)*y[1][1]*y[2][2]+(1)*y[1][2]*y[2][1]+(1)*y[2][0]*y[2][2];
    m1[1][1] = (-1)*x[0][1]*x[1][0]+(1)*x[2][0]*x[2][1]+(1)*x[2][2];
    m2[1][1] = (-1)*y[1][0]*x[0][1]+(-1)*y[0][1]*x[1][0]+(1)*y[2][1]*x[2][0]+(1)*y[2][0]*x[2][1]+(1)*y[2][2];
    m3[1][1] = (-1)*y[0][1]*y[1][0]+(1)*y[2][0]*y[2][1]+(1)*z[2];
    m1[1][0] = (-1)*x[0][0]*x[1][0]+(1)*x[1][0]*x[2][1]+(-1)*x[1][1]*x[2][0]+(1)*x[2][0]*x[2][0]+(-1)*x[1][2];
    m2[1][0] = (-1)*y[1][0]*x[0][0]+(-1)*y[0][0]*x[1][0]+(1)*y[2][1]*x[1][0]+(-1)*y[2][0]*x[1][1]+(-1)*y[1][1]*x[2][0]+(2)*y[2][0]*x[2][0]+(1)*y[1][0]*x[2][1]+(-1)*y[1][2];
    m3[1][0] = (-1)*y[0][0]*y[1][0]+(1)*y[1][0]*y[2][1]+(-1)*y[1][1]*y[2][0]+(1)*y[2][0]*y[2][0]+(-1)*z[1];
    m1[2][2] = (-1)*x[0][0]*x[0][2]*x[1][0]+(-1)*x[0][0]*x[1][1]*x[2][2]+(-1)*x[0][1]*x[1][0]*x[2][2]+(-1)*x[0][1]*x[1][1]*x[1][2]+(1)*x[0][2]*x[2][0]*x[2][0]+(1)*x[1][2]*x[2][1]*x[2][1]+(2)*x[2][0]*x[2][1]*x[2][2]+(-1)*x[0][2]*x[1][2]+(1)*x[2][2]*x[2][2];
    double m5 = (-1)*y[0][0]*y[1][0]*z[0]+(-1)*y[0][0]*y[1][1]*z[2]+(-1)*y[0][1]*y[1][0]*z[2]+(-1)*y[0][1]*y[1][1]*z[1]+(1)*y[2][0]*y[2][0]*z[0]+(2)*y[2][0]*y[2][1]*z[2]+(1)*y[2][1]*y[2][1]*z[1]+(-1)*z[0]*z[1]+(1)*z[2]*z[2];
    *m5p = m5;
    m4[2][2] = (-1)*y[1][0]*x[0][0]*z[0]+(-1)*y[1][1]*x[0][0]*z[2]+(-1)*y[1][0]*x[0][1]*z[2]+(-1)*y[1][1]*x[0][1]*z[1]+(-1)*y[0][0]*x[1][0]*z[0]+(-1)*y[0][1]*x[1][0]*z[2]+(-1)*y[0][0]*x[1][1]*z[2]+(-1)*y[0][1]*x[1][1]*z[1]+(2)*y[2][0]*x[2][0]*z[0]+(2)*y[2][1]*x[2][0]*z[2]+(2)*y[2][0]*x[2][1]*z[2]+(2)*y[2][1]*x[2][1]*z[1]+(-1)*y[0][0]*y[0][2]*y[1][0]+(-1)*y[0][0]*y[1][1]*y[2][2]+(-1)*y[0][1]*y[1][0]*y[2][2]+(-1)*y[0][1]*y[1][1]*y[1][2]+(1)*y[0][2]*y[2][0]*y[2][0]+(1)*y[1][2]*y[2][1]*y[2][1]+(2)*y[2][0]*y[2][1]*y[2][2]+(-1)*y[0][2]*z[1]+(-1)*y[1][2]*z[0]+(2)*y[2][2]*z[2];
    m2[2][2] = (-1)*y[1][0]*x[0][0]*x[0][2]+(-1)*y[0][2]*x[0][0]*x[1][0]+(-1)*y[2][2]*x[0][0]*x[1][1]+(-1)*y[1][1]*x[0][0]*x[2][2]+(-1)*y[2][2]*x[0][1]*x[1][0]+(-1)*y[1][2]*x[0][1]*x[1][1]+(-1)*y[1][1]*x[0][1]*x[1][2]+(-1)*y[1][0]*x[0][1]*x[2][2]+(-1)*y[0][0]*x[0][2]*x[1][0]+(2)*y[2][0]*x[0][2]*x[2][0]+(-1)*y[0][1]*x[1][0]*x[2][2]+(-1)*y[0][1]*x[1][1]*x[1][2]+(-1)*y[0][0]*x[1][1]*x[2][2]+(2)*y[2][1]*x[1][2]*x[2][1]+(1)*y[0][2]*x[2][0]*x[2][0]+(2)*y[2][2]*x[2][0]*x[2][1]+(2)*y[2][1]*x[2][0]*x[2][2]+(1)*y[1][2]*x[2][1]*x[2][1]+(2)*y[2][0]*x[2][1]*x[2][2]+(-1)*y[1][2]*x[0][2]+(-1)*y[0][2]*x[1][2]+(2)*y[2][2]*x[2][2];
    m3[2][2] = (-1)*x[0][0]*x[1][0]*z[0]+(-1)*x[0][0]*x[1][1]*z[2]+(-1)*y[0][2]*y[1][0]*x[0][0]+(-1)*y[1][1]*y[2][2]*x[0][0]+(-1)*x[0][1]*x[1][0]*z[2]+(-1)*x[0][1]*x[1][1]*z[1]+(-1)*y[1][0]*y[2][2]*x[0][1]+(-1)*y[1][1]*y[1][2]*x[0][1]+(-1)*y[0][0]*y[1][0]*x[0][2]+(1)*y[2][0]*y[2][0]*x[0][2]+(-1)*y[0][0]*y[0][2]*x[1][0]+(-1)*y[0][1]*y[2][2]*x[1][0]+(-1)*y[0][0]*y[2][2]*x[1][1]+(-1)*y[0][1]*y[1][2]*x[1][1]+(-1)*y[0][1]*y[1][1]*x[1][2]+(1)*y[2][1]*y[2][1]*x[1][2]+(1)*x[2][0]*x[2][0]*z[0]+(2)*x[2][0]*x[2][1]*z[2]+(2)*y[0][2]*y[2][0]*x[2][0]+(2)*y[2][1]*y[2][2]*x[2][0]+(1)*x[2][1]*x[2][1]*z[1]+(2)*y[1][2]*y[2][1]*x[2][1]+(2)*y[2][0]*y[2][2]*x[2][1]+(-1)*y[0][0]*y[1][1]*x[2][2]+(-1)*y[0][1]*y[1][0]*x[2][2]+(2)*y[2][0]*y[2][1]*x[2][2]+(-1)*x[0][2]*z[1]+(-1)*x[1][2]*z[0]+(2)*x[2][2]*z[2]+(-1)*y[0][2]*y[1][2]+(1)*y[2][2]*y[2][2];
    m1[2][1] = (-1)*x[0][0]*x[0][1]*x[1][0]+(-1)*x[0][0]*x[1][1]*x[2][1]+(-1)*x[0][1]*x[1][0]*x[2][1]+(-1)*x[0][1]*x[1][1]*x[1][1]+(1)*x[0][1]*x[2][0]*x[2][0]+(1)*x[1][1]*x[2][1]*x[2][1]+(2)*x[2][0]*x[2][1]*x[2][1]+(-1)*x[0][1]*x[1][2]+(-1)*x[0][2]*x[1][1]+(2)*x[2][1]*x[2][2];
    m4[2][1] = (-1)*y[0][0]*y[0][1]*y[1][0]+(-1)*y[0][0]*y[1][1]*y[2][1]+(-1)*y[0][1]*y[1][0]*y[2][1]+(-1)*y[0][1]*y[1][1]*y[1][1]+(1)*y[0][1]*y[2][0]*y[2][0]+(1)*y[1][1]*y[2][1]*y[2][1]+(2)*y[2][0]*y[2][1]*y[2][1]+(-1)*y[0][1]*z[1]+(-1)*y[1][1]*z[0]+(2)*y[2][1]*z[2];
    m2[2][1] = (-1)*y[1][0]*x[0][0]*x[0][1]+(-1)*y[0][1]*x[0][0]*x[1][0]+(-1)*y[2][1]*x[0][0]*x[1][1]+(-1)*y[1][1]*x[0][0]*x[2][1]+(-1)*y[0][0]*x[0][1]*x[1][0]+(-1)*y[2][1]*x[0][1]*x[1][0]+(-2)*y[1][1]*x[0][1]*x[1][1]+(2)*y[2][0]*x[0][1]*x[2][0]+(-1)*y[1][0]*x[0][1]*x[2][1]+(-1)*y[0][1]*x[1][0]*x[2][1]+(-1)*y[0][1]*x[1][1]*x[1][1]+(-1)*y[0][0]*x[1][1]*x[2][1]+(2)*y[2][1]*x[1][1]*x[2][1]+(1)*y[0][1]*x[2][0]*x[2][0]+(4)*y[2][1]*x[2][0]*x[2][1]+(1)*y[1][1]*x[2][1]*x[2][1]+(2)*y[2][0]*x[2][1]*x[2][1]+(-1)*y[1][2]*x[0][1]+(-1)*y[1][1]*x[0][2]+(-1)*y[0][2]*x[1][1]+(-1)*y[0][1]*x[1][2]+(2)*y[2][2]*x[2][1]+(2)*y[2][1]*x[2][2];
    m3[2][1] = (-1)*y[0][1]*y[1][0]*x[0][0]+(-1)*y[1][1]*y[2][1]*x[0][0]+(-1)*y[0][0]*y[1][0]*x[0][1]+(-1)*y[1][0]*y[2][1]*x[0][1]+(-1)*y[1][1]*y[1][1]*x[0][1]+(1)*y[2][0]*y[2][0]*x[0][1]+(-1)*y[0][0]*y[0][1]*x[1][0]+(-1)*y[0][1]*y[2][1]*x[1][0]+(-1)*y[0][0]*y[2][1]*x[1][1]+(-2)*y[0][1]*y[1][1]*x[1][1]+(1)*y[2][1]*y[2][1]*x[1][1]+(2)*y[0][1]*y[2][0]*x[2][0]+(2)*y[2][1]*y[2][1]*x[2][0]+(-1)*y[0][0]*y[1][1]*x[2][1]+(-1)*y[0][1]*y[1][0]*x[2][1]+(2)*y[1][1]*y[2][1]*x[2][1]+(4)*y[2][0]*y[2][1]*x[2][1]+(-1)*x[0][1]*z[1]+(-1)*x[1][1]*z[0]+(2)*x[2][1]*z[2]+(-1)*y[0][1]*y[1][2]+(-1)*y[0][2]*y[1][1]+(2)*y[2][1]*y[2][2];
    m1[2][0] = (-1)*x[0][0]*x[0][0]*x[1][0]+(-1)*x[0][0]*x[1][1]*x[2][0]+(1)*x[0][0]*x[2][0]*x[2][0]+(-1)*x[0][1]*x[1][0]*x[1][1]+(-1)*x[0][1]*x[1][0]*x[2][0]+(1)*x[1][0]*x[2][1]*x[2][1]+(2)*x[2][0]*x[2][0]*x[2][1]+(-1)*x[0][0]*x[1][2]+(-1)*x[0][2]*x[1][0]+(2)*x[2][0]*x[2][2];
    m4[2][0] = (-1)*y[0][0]*y[0][0]*y[1][0]+(-1)*y[0][0]*y[1][1]*y[2][0]+(1)*y[0][0]*y[2][0]*y[2][0]+(-1)*y[0][1]*y[1][0]*y[1][1]+(-1)*y[0][1]*y[1][0]*y[2][0]+(1)*y[1][0]*y[2][1]*y[2][1]+(2)*y[2][0]*y[2][0]*y[2][1]+(-1)*y[0][0]*z[1]+(-1)*y[1][0]*z[0]+(2)*y[2][0]*z[2];
    m2[2][0] = (-1)*y[1][0]*x[0][0]*x[0][0]+(-2)*y[0][0]*x[0][0]*x[1][0]+(-1)*y[2][0]*x[0][0]*x[1][1]+(-1)*y[1][1]*x[0][0]*x[2][0]+(2)*y[2][0]*x[0][0]*x[2][0]+(-1)*y[1][1]*x[0][1]*x[1][0]+(-1)*y[2][0]*x[0][1]*x[1][0]+(-1)*y[1][0]*x[0][1]*x[1][1]+(-1)*y[1][0]*x[0][1]*x[2][0]+(-1)*y[0][1]*x[1][0]*x[1][1]+(-1)*y[0][1]*x[1][0]*x[2][0]+(2)*y[2][1]*x[1][0]*x[2][1]+(-1)*y[0][0]*x[1][1]*x[2][0]+(1)*y[0][0]*x[2][0]*x[2][0]+(2)*y[2][1]*x[2][0]*x[2][0]+(4)*y[2][0]*x[2][0]*x[2][1]+(1)*y[1][0]*x[2][1]*x[2][1]+(-1)*y[1][2]*x[0][0]+(-1)*y[1][0]*x[0][2]+(-1)*y[0][2]*x[1][0]+(-1)*y[0][0]*x[1][2]+(2)*y[2][2]*x[2][0]+(2)*y[2][0]*x[2][2];
    m3[2][0] = (-2)*y[0][0]*y[1][0]*x[0][0]+(-1)*y[1][1]*y[2][0]*x[0][0]+(1)*y[2][0]*y[2][0]*x[0][0]+(-1)*y[1][0]*y[1][1]*x[0][1]+(-1)*y[1][0]*y[2][0]*x[0][1]+(-1)*y[0][0]*y[0][0]*x[1][0]+(-1)*y[0][1]*y[1][1]*x[1][0]+(-1)*y[0][1]*y[2][0]*x[1][0]+(1)*y[2][1]*y[2][1]*x[1][0]+(-1)*y[0][0]*y[2][0]*x[1][1]+(-1)*y[0][1]*y[1][0]*x[1][1]+(-1)*y[0][0]*y[1][1]*x[2][0]+(2)*y[0][0]*y[2][0]*x[2][0]+(-1)*y[0][1]*y[1][0]*x[2][0]+(4)*y[2][0]*y[2][1]*x[2][0]+(2)*y[1][0]*y[2][1]*x[2][1]+(2)*y[2][0]*y[2][0]*x[2][1]+(-1)*x[0][0]*z[1]+(-1)*x[1][0]*z[0]+(2)*x[2][0]*z[2]+(-1)*y[0][0]*y[1][2]+(-1)*y[0][2]*y[1][0]+(2)*y[2][0]*y[2][2];
    b[0] = (1)*m1[0][0]*m1[1][1]*m1[2][2]+(-1)*m1[0][0]*m1[1][2]*m1[2][1]+(-1)*m1[0][1]*m1[1][0]*m1[2][2]+(1)*m1[0][1]*m1[1][2]*m1[2][0]+(1)*m1[0][2]*m1[1][0]*m1[2][1]+(-1)*m1[0][2]*m1[1][1]*m1[2][0];
    b[8] = (1)*m3[0][0]*m3[1][1]*m5+(-1)*m3[0][1]*m3[1][0]*m5+(-1)*m3[0][0]*m4[1][2]*m4[2][1]+(1)*m3[0][1]*m4[1][2]*m4[2][0]+(1)*m3[1][0]*m4[0][2]*m4[2][1]+(-1)*m3[1][1]*m4[0][2]*m4[2][0];
    b[7] = (1)*m2[0][0]*m3[1][1]*m5+(-1)*m2[0][1]*m3[1][0]*m5+(-1)*m2[1][0]*m3[0][1]*m5+(1)*m2[1][1]*m3[0][0]*m5+(-1)*m2[0][0]*m4[1][2]*m4[2][1]+(1)*m2[0][1]*m4[1][2]*m4[2][0]+(1)*m2[1][0]*m4[0][2]*m4[2][1]+(-1)*m2[1][1]*m4[0][2]*m4[2][0]+(1)*m3[0][0]*m3[1][1]*m4[2][2]+(-1)*m3[0][0]*m3[1][2]*m4[2][1]+(-1)*m3[0][0]*m3[2][1]*m4[1][2]+(-1)*m3[0][1]*m3[1][0]*m4[2][2]+(1)*m3[0][1]*m3[1][2]*m4[2][0]+(1)*m3[0][1]*m3[2][0]*m4[1][2]+(1)*m3[0][2]*m3[1][0]*m4[2][1]+(-1)*m3[0][2]*m3[1][1]*m4[2][0]+(1)*m3[1][0]*m3[2][1]*m4[0][2]+(-1)*m3[1][1]*m3[2][0]*m4[0][2];
    b[6] = (1)*m1[0][0]*m3[1][1]*m5+(-1)*m1[0][1]*m3[1][0]*m5+(-1)*m1[1][0]*m3[0][1]*m5+(1)*m1[1][1]*m3[0][0]*m5+(1)*m2[0][0]*m2[1][1]*m5+(-1)*m2[0][1]*m2[1][0]*m5+(-1)*m1[0][0]*m4[1][2]*m4[2][1]+(1)*m1[0][1]*m4[1][2]*m4[2][0]+(1)*m1[1][0]*m4[0][2]*m4[2][1]+(-1)*m1[1][1]*m4[0][2]*m4[2][0]+(1)*m2[0][0]*m3[1][1]*m4[2][2]+(-1)*m2[0][0]*m3[1][2]*m4[2][1]+(-1)*m2[0][0]*m3[2][1]*m4[1][2]+(-1)*m2[0][1]*m3[1][0]*m4[2][2]+(1)*m2[0][1]*m3[1][2]*m4[2][0]+(1)*m2[0][1]*m3[2][0]*m4[1][2]+(1)*m2[0][2]*m3[1][0]*m4[2][1]+(-1)*m2[0][2]*m3[1][1]*m4[2][0]+(-1)*m2[1][0]*m3[0][1]*m4[2][2]+(1)*m2[1][0]*m3[0][2]*m4[2][1]+(1)*m2[1][0]*m3[2][1]*m4[0][2]+(1)*m2[1][1]*m3[0][0]*m4[2][2]+(-1)*m2[1][1]*m3[0][2]*m4[2][0]+(-1)*m2[1][1]*m3[2][0]*m4[0][2]+(-1)*m2[1][2]*m3[0][0]*m4[2][1]+(1)*m2[1][2]*m3[0][1]*m4[2][0]+(1)*m2[2][0]*m3[0][1]*m4[1][2]+(-1)*m2[2][0]*m3[1][1]*m4[0][2]+(-1)*m2[2][1]*m3[0][0]*m4[1][2]+(1)*m2[2][1]*m3[1][0]*m4[0][2]+(1)*m3[0][0]*m3[1][1]*m3[2][2]+(-1)*m3[0][0]*m3[1][2]*m3[2][1]+(-1)*m3[0][1]*m3[1][0]*m3[2][2]+(1)*m3[0][1]*m3[1][2]*m3[2][0]+(1)*m3[0][2]*m3[1][0]*m3[2][1]+(-1)*m3[0][2]*m3[1][1]*m3[2][0];
    b[5] = (1)*m1[0][1]*m3[1][2]*m4[2][0]+(1)*m1[0][1]*m3[2][0]*m4[1][2]+(-1)*m1[0][2]*m3[1][1]*m4[2][0]+(-1)*m1[1][1]*m3[0][2]*m4[2][0]+(-1)*m1[1][1]*m3[2][0]*m4[0][2]+(1)*m1[1][2]*m3[0][1]*m4[2][0]+(1)*m1[2][0]*m3[0][1]*m4[1][2]+(-1)*m1[2][0]*m3[1][1]*m4[0][2]+(1)*m2[0][1]*m2[1][2]*m4[2][0]+(1)*m2[0][1]*m2[2][0]*m4[1][2]+(1)*m2[0][1]*m3[1][2]*m3[2][0]+(-1)*m2[0][2]*m2[1][1]*m4[2][0]+(-1)*m2[0][2]*m3[1][1]*m3[2][0]+(-1)*m2[1][1]*m2[2][0]*m4[0][2]+(-1)*m2[1][1]*m3[0][2]*m3[2][0]+(1)*m2[1][2]*m3[0][1]*m3[2][0]+(1)*m2[2][0]*m3[0][1]*m3[1][2]+(-1)*m2[2][0]*m3[0][2]*m3[1][1]+(-1)*m1[1][0]*m3[0][1]*m4[2][2]+(1)*m1[1][0]*m3[0][2]*m4[2][1]+(1)*m1[1][0]*m3[2][1]*m4[0][2]+(1)*m1[2][1]*m3[1][0]*m4[0][2]+(-1)*m2[0][1]*m2[1][0]*m4[2][2]+(-1)*m2[0][1]*m3[1][0]*m3[2][2]+(1)*m2[0][2]*m2[1][0]*m4[2][1]+(1)*m2[0][2]*m3[1][0]*m3[2][1]+(1)*m2[1][0]*m2[2][1]*m4[0][2]+(-1)*m2[1][0]*m3[0][1]*m3[2][2]+(1)*m2[1][0]*m3[0][2]*m3[2][1]+(1)*m2[2][1]*m3[0][2]*m3[1][0]+(-1)*m2[2][2]*m3[0][1]*m3[1][0]+(-1)*m1[0][1]*m2[1][0]*m5+(-1)*m1[1][0]*m2[0][1]*m5+(-1)*m1[0][1]*m3[1][0]*m4[2][2]+(1)*m1[0][2]*m3[1][0]*m4[2][1]+(1)*m1[0][0]*m2[1][1]*m5+(1)*m1[1][1]*m2[0][0]*m5+(1)*m1[0][0]*m3[1][1]*m4[2][2]+(-1)*m1[0][0]*m3[1][2]*m4[2][1]+(-1)*m1[0][0]*m3[2][1]*m4[1][2]+(1)*m1[1][1]*m3[0][0]*m4[2][2]+(-1)*m1[1][2]*m3[0][0]*m4[2][1]+(-1)*m1[2][1]*m3[0][0]*m4[1][2]+(1)*m2[0][0]*m2[1][1]*m4[2][2]+(-1)*m2[0][0]*m2[1][2]*m4[2][1]+(-1)*m2[0][0]*m2[2][1]*m4[1][2]+(1)*m2[0][0]*m3[1][1]*m3[2][2]+(-1)*m2[0][0]*m3[1][2]*m3[2][1]+(1)*m2[1][1]*m3[0][0]*m3[2][2]+(-1)*m2[1][2]*m3[0][0]*m3[2][1]+(-1)*m2[2][1]*m3[0][0]*m3[1][2]+(1)*m2[2][2]*m3[0][0]*m3[1][1];
    b[4] = (-1)*m2[1][1]*m2[2][0]*m3[0][2]+(1)*m2[0][1]*m2[2][0]*m3[1][2]+(-1)*m2[0][2]*m2[1][1]*m3[2][0]+(1)*m2[1][2]*m2[2][0]*m3[0][1]+(-1)*m1[1][1]*m3[0][2]*m3[2][0]+(-1)*m2[0][2]*m2[2][0]*m3[1][1]+(1)*m2[0][1]*m2[1][2]*m3[2][0]+(-1)*m1[2][0]*m2[1][1]*m4[0][2]+(1)*m1[2][0]*m2[0][1]*m4[1][2]+(-1)*m1[2][0]*m3[0][2]*m3[1][1]+(1)*m1[2][0]*m3[0][1]*m3[1][2]+(1)*m1[1][2]*m3[0][1]*m3[2][0]+(1)*m1[1][2]*m2[0][1]*m4[2][0]+(1)*m1[0][1]*m2[1][2]*m4[2][0]+(1)*m1[0][1]*m2[2][0]*m4[1][2]+(1)*m1[0][1]*m3[1][2]*m3[2][0]+(-1)*m1[0][2]*m2[1][1]*m4[2][0]+(-1)*m1[0][2]*m3[1][1]*m3[2][0]+(-1)*m1[1][1]*m2[0][2]*m4[2][0]+(-1)*m1[1][1]*m2[2][0]*m4[0][2]+(-1)*m1[0][1]*m1[1][0]*m5+(-1)*m1[0][1]*m2[1][0]*m4[2][2]+(-1)*m1[0][1]*m3[1][0]*m3[2][2]+(1)*m1[0][2]*m2[1][0]*m4[2][1]+(1)*m1[0][2]*m3[1][0]*m3[2][1]+(-1)*m1[1][0]*m2[0][1]*m4[2][2]+(1)*m1[1][0]*m2[0][2]*m4[2][1]+(1)*m1[1][0]*m2[2][1]*m4[0][2]+(-1)*m1[1][0]*m3[0][1]*m3[2][2]+(1)*m1[1][0]*m3[0][2]*m3[2][1]+(1)*m1[2][1]*m2[1][0]*m4[0][2]+(1)*m1[2][1]*m3[0][2]*m3[1][0]+(-1)*m1[2][2]*m3[0][1]*m3[1][0]+(-1)*m2[0][1]*m2[1][0]*m3[2][2]+(-1)*m2[0][1]*m2[2][2]*m3[1][0]+(1)*m2[0][2]*m2[1][0]*m3[2][1]+(1)*m2[0][2]*m2[2][1]*m3[1][0]+(1)*m2[1][0]*m2[2][1]*m3[0][2]+(-1)*m2[1][0]*m2[2][2]*m3[0][1]+(1)*m1[0][0]*m1[1][1]*m5+(1)*m1[0][0]*m2[1][1]*m4[2][2]+(-1)*m1[0][0]*m2[1][2]*m4[2][1]+(-1)*m1[0][0]*m2[2][1]*m4[1][2]+(1)*m1[0][0]*m3[1][1]*m3[2][2]+(-1)*m1[0][0]*m3[1][2]*m3[2][1]+(1)*m1[1][1]*m2[0][0]*m4[2][2]+(1)*m1[1][1]*m3[0][0]*m3[2][2]+(-1)*m1[1][2]*m2[0][0]*m4[2][1]+(-1)*m1[1][2]*m3[0][0]*m3[2][1]+(-1)*m1[2][1]*m2[0][0]*m4[1][2]+(-1)*m1[2][1]*m3[0][0]*m3[1][2]+(1)*m1[2][2]*m3[0][0]*m3[1][1]+(1)*m2[0][0]*m2[1][1]*m3[2][2]+(-1)*m2[0][0]*m2[1][2]*m3[2][1]+(-1)*m2[0][0]*m2[2][1]*m3[1][2]+(1)*m2[0][0]*m2[2][2]*m3[1][1]+(1)*m2[1][1]*m2[2][2]*m3[0][0]+(-1)*m2[1][2]*m2[2][1]*m3[0][0];
    b[3] = (-1)*m1[0][2]*m2[1][1]*m3[2][0]+(-1)*m1[0][2]*m1[1][1]*m4[2][0]+(1)*m1[0][1]*m2[1][2]*m3[2][0]+(-1)*m1[1][1]*m2[0][2]*m3[2][0]+(1)*m1[0][1]*m1[2][0]*m4[1][2]+(1)*m1[0][1]*m1[1][2]*m4[2][0]+(-1)*m1[1][1]*m1[2][0]*m4[0][2]+(1)*m1[2][0]*m2[0][1]*m3[1][2]+(1)*m1[2][0]*m2[1][2]*m3[0][1]+(-1)*m1[2][0]*m2[1][1]*m3[0][2]+(-1)*m1[2][0]*m2[0][2]*m3[1][1]+(-1)*m1[1][1]*m2[2][0]*m3[0][2]+(-1)*m2[0][2]*m2[1][1]*m2[2][0]+(-1)*m1[0][2]*m2[2][0]*m3[1][1]+(1)*m2[0][1]*m2[1][2]*m2[2][0]+(1)*m1[0][1]*m2[2][0]*m3[1][2]+(1)*m1[1][2]*m2[0][1]*m3[2][0]+(1)*m1[1][2]*m2[2][0]*m3[0][1]+(-1)*m1[0][1]*m1[1][0]*m4[2][2]+(-1)*m1[0][1]*m2[1][0]*m3[2][2]+(-1)*m1[0][1]*m2[2][2]*m3[1][0]+(1)*m1[0][2]*m1[1][0]*m4[2][1]+(1)*m1[0][2]*m2[1][0]*m3[2][1]+(1)*m1[0][2]*m2[2][1]*m3[1][0]+(1)*m1[1][0]*m1[2][1]*m4[0][2]+(-1)*m1[1][0]*m2[0][1]*m3[2][2]+(1)*m1[1][0]*m2[0][2]*m3[2][1]+(1)*m1[1][0]*m2[2][1]*m3[0][2]+(-1)*m1[1][0]*m2[2][2]*m3[0][1]+(1)*m1[2][1]*m2[0][2]*m3[1][0]+(1)*m1[2][1]*m2[1][0]*m3[0][2]+(-1)*m1[2][2]*m2[0][1]*m3[1][0]+(-1)*m1[2][2]*m2[1][0]*m3[0][1]+(-1)*m2[0][1]*m2[1][0]*m2[2][2]+(1)*m2[0][2]*m2[1][0]*m2[2][1]+(1)*m1[0][0]*m2[1][1]*m3[2][2]+(-1)*m1[0][0]*m2[1][2]*m3[2][1]+(-1)*m1[0][0]*m2[2][1]*m3[1][2]+(1)*m1[0][0]*m2[2][2]*m3[1][1]+(1)*m1[1][1]*m2[0][0]*m3[2][2]+(1)*m1[1][1]*m2[2][2]*m3[0][0]+(-1)*m1[1][2]*m2[0][0]*m3[2][1]+(-1)*m1[1][2]*m2[2][1]*m3[0][0]+(-1)*m1[2][1]*m2[0][0]*m3[1][2]+(-1)*m1[2][1]*m2[1][2]*m3[0][0]+(1)*m1[2][2]*m2[0][0]*m3[1][1]+(1)*m1[2][2]*m2[1][1]*m3[0][0]+(1)*m2[0][0]*m2[1][1]*m2[2][2]+(-1)*m2[0][0]*m2[1][2]*m2[2][1]+(1)*m1[0][0]*m1[1][1]*m4[2][2]+(-1)*m1[0][0]*m1[1][2]*m4[2][1]+(-1)*m1[0][0]*m1[2][1]*m4[1][2];
    b[1] = (1)*m1[0][0]*m1[1][1]*m2[2][2]+(-1)*m1[0][0]*m1[1][2]*m2[2][1]+(-1)*m1[0][0]*m1[2][1]*m2[1][2]+(1)*m1[0][0]*m1[2][2]*m2[1][1]+(-1)*m1[0][1]*m1[1][0]*m2[2][2]+(1)*m1[0][1]*m1[1][2]*m2[2][0]+(1)*m1[0][1]*m1[2][0]*m2[1][2]+(-1)*m1[0][1]*m1[2][2]*m2[1][0]+(1)*m1[0][2]*m1[1][0]*m2[2][1]+(-1)*m1[0][2]*m1[1][1]*m2[2][0]+(-1)*m1[0][2]*m1[2][0]*m2[1][1]+(1)*m1[0][2]*m1[2][1]*m2[1][0]+(1)*m1[1][0]*m1[2][1]*m2[0][2]+(-1)*m1[1][0]*m1[2][2]*m2[0][1]+(-1)*m1[1][1]*m1[2][0]*m2[0][2]+(1)*m1[1][1]*m1[2][2]*m2[0][0]+(1)*m1[1][2]*m1[2][0]*m2[0][1]+(-1)*m1[1][2]*m1[2][1]*m2[0][0];
    b[2] = (1)*m1[0][0]*m1[1][1]*m3[2][2]+(-1)*m1[0][0]*m1[1][2]*m3[2][1]+(-1)*m1[0][0]*m1[2][1]*m3[1][2]+(1)*m1[0][0]*m1[2][2]*m3[1][1]+(1)*m1[0][0]*m2[1][1]*m2[2][2]+(-1)*m1[0][0]*m2[1][2]*m2[2][1]+(-1)*m1[0][1]*m1[1][0]*m3[2][2]+(1)*m1[0][1]*m1[1][2]*m3[2][0]+(1)*m1[0][1]*m1[2][0]*m3[1][2]+(-1)*m1[0][1]*m1[2][2]*m3[1][0]+(-1)*m1[0][1]*m2[1][0]*m2[2][2]+(1)*m1[0][1]*m2[1][2]*m2[2][0]+(1)*m1[0][2]*m1[1][0]*m3[2][1]+(-1)*m1[0][2]*m1[1][1]*m3[2][0]+(-1)*m1[0][2]*m1[2][0]*m3[1][1]+(1)*m1[0][2]*m1[2][1]*m3[1][0]+(1)*m1[0][2]*m2[1][0]*m2[2][1]+(-1)*m1[0][2]*m2[1][1]*m2[2][0]+(1)*m1[1][0]*m1[2][1]*m3[0][2]+(-1)*m1[1][0]*m1[2][2]*m3[0][1]+(-1)*m1[1][0]*m2[0][1]*m2[2][2]+(1)*m1[1][0]*m2[0][2]*m2[2][1]+(-1)*m1[1][1]*m1[2][0]*m3[0][2]+(1)*m1[1][1]*m1[2][2]*m3[0][0]+(1)*m1[1][1]*m2[0][0]*m2[2][2]+(-1)*m1[1][1]*m2[0][2]*m2[2][0]+(1)*m1[1][2]*m1[2][0]*m3[0][1]+(-1)*m1[1][2]*m1[2][1]*m3[0][0]+(-1)*m1[1][2]*m2[0][0]*m2[2][1]+(1)*m1[1][2]*m2[0][1]*m2[2][0]+(1)*m1[2][0]*m2[0][1]*m2[1][2]+(-1)*m1[2][0]*m2[0][2]*m2[1][1]+(-1)*m1[2][1]*m2[0][0]*m2[1][2]+(1)*m1[2][1]*m2[0][2]*m2[1][0]+(1)*m1[2][2]*m2[0][0]*m2[1][1]+(-1)*m1[2][2]*m2[0][1]*m2[1][0];
}

void FillRMat(const Eigen::Vector3d& X, const Eigen::Matrix3i& inds, Eigen::Matrix<double, 3, 9>* C)
{
    *C = Eigen::Matrix<double, 3, 9>::Zero();
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            (*C)(i, inds(j, i)) = X(j);
        }
    }
}

double cond(const Eigen::MatrixXd& M)
{
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(M);
    double cond = svd.singularValues()(0)
                  / svd.singularValues()(svd.singularValues().size()-1);
    return cond;
}

int rank(const Eigen::Matrix3d& A)
{
    Eigen::FullPivLU<Eigen::Matrix3d> lu_decomp(A);
    return lu_decomp.rank();
}


void FillSubMat(const Eigen::MatrixXd& M, Eigen::Matrix3d* M_sub, int c0, int c1, int c2)
{
    M_sub->col(0) = M.col(c0);
    M_sub->col(1) = M.col(c1);
    M_sub->col(2) = M.col(c2);
}

void FillCols(const Eigen::Matrix<double, 3, 10>& M, const std::vector<int>& col_ids, const std::vector<int>& col_ids_new, Eigen::Matrix<double, 3, 10>* M_new)
{
    for (int i = 0; i < col_ids.size(); i++)
    {
        M_new->col(col_ids_new[i]) = M.col(col_ids[i]);
    }
}


void qr_solve_mat(const Eigen::Matrix3d& A, const Eigen::Matrix3d& b, Eigen::Matrix3d* x)
{
    *x = A.colPivHouseholderQr().solve(b);
}


void roots_companion_8(double b[9], std::vector<double>* roots)
{
    Eigen::Matrix<double, 8, 8> C;
    C.setZero();
    for (int i = 0; i < 8; i++)
    {
        if (i > 0)
        {
            C(i, i-1) = 1.0;
        }
        C(i, 7) = -b[i]/b[8];
    }
    Eigen::VectorXcd  eig_vals = C.eigenvalues();
    roots->clear();
    for (int i = 0; i < eig_vals.rows(); i++)
    {
        if (fabs(eig_vals(i).imag()) == 0)
        {
            roots->push_back(eig_vals(i).real());
        }
    }
}

bool solve_3_quadric(const Eigen::Matrix<double, 3, 10>& M, bool is_det_check, std::vector<double>* bs, std::vector<double>* cs, std::vector<double>* ds)
{
    int di = -1;
    Eigen::Matrix<double, 3, 10> M_new(M);
    if (is_det_check)
    {
        Eigen::Matrix<double, 3, 3> M_b, M_d, M_c;
        FillSubMat(M, &M_b, 8, 9, 5);
        FillSubMat(M, &M_d, 8, 7, 3);
        FillSubMat(M, &M_c, 7, 9, 4);
        std::vector<double> dets = {cond(M_b), cond(M_c), cond(M_d)};
        for (int i = 0; i < 3; i++)
        {
            dets[i] = fabs(fabs(dets[i])-1);
//            log_fout << " detsi: " <<i << " " << dets[i] << std::endl;
        }
        di = (int)(std::min_element(dets.begin(), dets.end()) - dets.begin());
        if (di == 2)
        {
            FillCols(M, std::vector<int>{2, 0, 5, 3, 9, 7}, std::vector<int>{0,2,3,5,7,9}, &M_new);
        }
        if (di == 1)
        {
            FillCols(M, std::vector<int>{1, 0, 5, 4, 8, 7}, std::vector<int>{0, 1, 4, 5, 7, 8}, &M_new);
        }
    }

//    for (int i = 0; i < 3; i++)
//    {
//        for (int j = 0; j < 10; j++)
//        {
//            log_fout << M_new(i, j) <<" ";
//        }
//        log_fout << std::endl;
//    }
//    log_fout << " di: " << di << std::endl;
//    log_fout << " det done " << std::endl;
    Eigen::Matrix3d A;
    FillSubMat(M_new, &A, 8, 9, 5);
    if (rank(A) < 3)
    {
//        log_fout << " rank A " << rank(A) << std::endl;
        return false;
    }

    Eigen::Matrix3d M0, M1, M2;
    FillSubMat(M_new, &M0, 1, 2, 6);
    FillSubMat(M_new, &M1, 3, 4, 0);
    M2.setZero();
    M2.col(2) = M_new.col(7);
    Eigen::Matrix3d C0, C1, C2;
    qr_solve_mat(A, M0, &C0);
    qr_solve_mat(A, M1, &C1);
    qr_solve_mat(A, M2, &C2);
    C0 *= -1;
    C1 *= -1;
    C2 *= -1;
    double c0[3][3];
    double c1[3][3];
    double c2[3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            c0[i][j] = C0(i,j);
            c1[i][j] = C1(i,j);
        }
        c2[i] = C2(i, 2);
    }
//    log_fout << " Ci done " << std::endl;
    double b[9];
    double m1[3][3];
    double m2[3][3];
    double m3[3][3];
    double m4[3][3];
    double m5;

    polycoeffs(c0, c1, c2, b, m1, m2, m3, m4, &m5);

    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> m1_eig(&m1[0][0]);
    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> m2_eig(&m2[0][0]);
    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> m3_eig(&m3[0][0]);
    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> m4_eig(&m4[0][0]);

    Eigen::Matrix3d m5_eig;
    m5_eig.setZero();
    m5_eig(2,2) = m5;
//    for (int i = 0; i < 9; i++)
//    {
//        std::cout << b[i] << std::endl;
//    }
//

    std::vector<double> roots;
    roots_companion_8(b, &roots);
//    std::cout << " root num " << roots.size() << std::endl;
    std::vector<double> cs_init, ds_init;
    for (int i = 0; i < roots.size(); i++)
    {
//        Eigen::Matrix3d C = C0 + roots[i]*C1 + roots[i]*roots[i]*C2;
        double r = roots[i];
        Eigen::Matrix3d C = m1_eig + r*m2_eig + r*r*m3_eig + r*r*r*m4_eig + m5_eig*r*r*r*r;
        Eigen::JacobiSVD<Eigen::Matrix3d> svd(C, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Vector3d ev = svd.matrixV().col(2);
        cs_init.push_back(ev(0)/ev(2));
        ds_init.push_back(ev(1)/ev(2));
    }
    if (is_det_check)
    {
        if (di == 2)
        {
            *ds = roots;
            *bs = ds_init;
            *cs = cs_init;
        }
        if (di == 1)
        {
            *cs = roots;
            *bs = cs_init;
            *ds = ds_init;
        }
    }
    if (!is_det_check || di==0)
    {
        *bs = roots;
        *cs = cs_init;
        *ds = ds_init;
    }

    return true;
}

Eigen::Vector3d FromVec3d(const cv::Vec3d& vec)
{
    Eigen::Vector3d v;
    for (int i = 0; i < 3; i++)
    {
        v(i) = vec[i];
    }
    return v;
}

Eigen::Matrix<double, 10, 10> BuildQ()
{
    Eigen::Matrix<double, 10, 10> Q;
    Q.setZero();
    Q.block<1, 4>(0, 6)  << 1,1,-1,-1;
    Q.block<1, 2>(1, 2) << 2,2;
    Q(2, 4) = 2;
    Q(2, 1) = -2;
    Q.block<1, 2>(3, 2) << -2,2;
    Q.block<1, 4>(4, 6)  << 1, -1, 1, -1;
    Q(5, 0) = 2;
    Q(5, 5) = 2;
    Q(6, 4) = 2;
    Q(6, 1) = 2;
    Q(7, 5) = 2;
    Q(7, 0) = -2;
    Q.block<1, 4>(8, 6)  << 1, -1, -1, 1;
    Q.block<1, 4>(9, 6) << 1, 1, 1, 1;
    return Q;
}


bool TriangulateLine(const Eigen::Matrix3d& R1, const Eigen::Vector3d& t1, const Eigen::Matrix3d& R2, const Eigen::Vector3d& t2,
                     const Eigen::Vector3d& leq1, const Eigen::Vector3d& leq2, Eigen::Vector3d* X1, Eigen::Vector3d* X2)
{
    Eigen::Vector3d a = R1.transpose() * leq1;
    double b = t1.transpose() * leq1;
    Eigen::Vector3d c = R2.transpose() * leq2;
    double d = t2.transpose() * leq2;
    Eigen::Matrix<double, 5, 5> A;
    A.setZero();
    A.block<1,3>(0, 0) = a.transpose();
    A.block<1,3>(1, 0) = c.transpose();
    A.block<3,3>(2, 0).setIdentity();
    A.block<3,1>(2, 3) = a;
    A.block<3,1>(2, 4) = c;
    Eigen::FullPivLU<Eigen::Matrix<double, 5, 5>> lu_decomp(A);
    if (lu_decomp.rank() < 5)
    {
        return false;
    }
    Eigen::VectorXd rhs(5);
    rhs.setZero();
    rhs(0) = -b;
    rhs(1) = -d;
    Eigen::VectorXd X = A.colPivHouseholderQr().solve(rhs);
    *X1 = X.segment<3>(0);
    Eigen::Vector3d ra = R1.transpose() * a;
    Eigen::Vector3d rc = R2.transpose() * c;
    Eigen::Vector3d line_dir = ra.cross(rc);
    line_dir = line_dir / line_dir.norm();
    *X2 = (*X1) + line_dir;
    if ((*X1).norm() == 0 || (*X2).norm() == 0)
    {
        return false;
    }
    return true;
}

bool quadric_solver(const cv::Mat& projs, const cv::Mat& lprojs,
                    const cv::Mat& vis_p, const cv::Mat& vis_l, bool is_det_check,
                    std::vector<Eigen::Matrix3d>* Rs, std::vector<Eigen::Vector3d>* ts)
{
//    std::ofstream log_out("/home/alexander/debug_quad_mex");

    Eigen::Matrix3i inds;
    inds << 0, 1, 2,
            3, 4, 5,
            6, 7, 8;

    Eigen::Vector3d t2;
    t2 << 1,0,0;
    cv::Mat t2_cv;
    cv::eigen2cv(t2, t2_cv);

    bool is_3_lines = false;
    if (vis_p.rows == 0)
    {
        is_3_lines = true;
    }

//    std::cout << is_3_lines << std::endl;

    Eigen::Matrix<double, 6, 13> A;
    A.setZero();
    std::vector<int> tis;
    int a_ind = 0;
    for (int pi = 0; pi < vis_p.rows; pi++)
    {
        int ci = 2;
        Eigen::Vector3d b = Eigen::Vector3d::Zero();
        if ((int)vis_p.at<uchar>(pi, ci) == 0)
        {
            ci = 3;
            b = t2;
        }
        cv::Point2d pt1(projs.at<cv::Vec2d>(pi, 0)[0], projs.at<cv::Vec2d>(pi, 0)[1]);
        cv::Point2d pt2(projs.at<cv::Vec2d>(pi, 1)[0], projs.at<cv::Vec2d>(pi, 1)[1]);
//        std::vector<cv::Point3d> pts_3d;
        cv::Mat P0 = cv::Mat::zeros(3, 4, CV_64FC1);
        cv::Mat eye_mat = cv::Mat::eye(3, 3, CV_64FC1);
        eye_mat.copyTo(P0(cv::Rect(0, 0, 3, 3)));
        cv::Mat P1 = cv::Mat::zeros(3, 4, CV_64FC1);
        eye_mat.copyTo(P1(cv::Rect(0, 0, 3, 3)));
        t2_cv.copyTo(P1(cv::Rect(3, 0, 1, 3)));
//        log_out << " before triang " << std::endl;
        cv::Mat pts_3d;
        cv::triangulatePoints(P0, P1, std::vector<cv::Point2d>{pt1}, std::vector<cv::Point2d>{pt2}, pts_3d);
//        log_out << " triang done" << std::endl;
        Eigen::Vector3d X;
//        cv::cv2eigen(pts_3d[0], X);
        X <<  pts_3d.at<double>(0, 0), pts_3d.at<double>(1, 0), pts_3d.at<double>(2,0);
        X = X / pts_3d.at<double>(3, 0);
//        log_out << pts_3d.cols << std::endl;
//        log_out << pts_3d.rows << std::endl;
//        log_out << pt1.x << " " << pt1.y << std::endl;
//        log_out << pt2.x << " " << pt2.y << std::endl;
//        log_out << pts_3d.type() << " " << CV_64FC1 << std::endl;
//        for (int xi = 0; xi < 3; xi++)
//        {
//            log_out << X(xi) << " ";
//        }
//        log_out << std::endl;

        Eigen::Matrix<double,3,9> C;
        FillRMat(X, inds, &C);
        Eigen::Matrix<double, 3, 13> Cfull;
        Cfull.setZero();
        Cfull.block<3,9>(0, 0) = C;
        Cfull.block<3,3>(0, 9).setIdentity();
        Cfull.block<3,1>(0, 12) = b;

//        log_out << " C full: " << std::endl;
//        for (int xi = 0; xi < 3; xi++)
//        {
//            for (int yi = 0; yi < 13; yi++)
//            {
//                log_out << Cfull(xi, yi) << " ";
//            }
//            log_out << std::endl;
//        }

        Eigen::Matrix<double, 2, 13> C2;
        C2.block<1, 13>(0,0) = Cfull.block<1, 13>(0, 0) - Cfull.block<1, 13>(2, 0)*projs.at<cv::Vec2d>(pi, ci)[0];
        C2.block<1, 13>(1,0) = Cfull.block<1, 13>(1, 0) - Cfull.block<1, 13>(2, 0)*projs.at<cv::Vec2d>(pi, ci)[1];
        A.block<2, 13>(2*pi, 0) = C2;
        a_ind = 2*pi+2;
        tis.push_back(2*pi);
        tis.push_back(2*pi+1);
    }

//    log_out << " points done" << std::endl;
//    log_out << " A points: " << std::endl;
//    for (int xi = 0; xi < 6; xi++)
//    {
//        for (int yi = 0; yi < 13; yi++)
//        {
//            log_out << A(xi, yi) << " ";
//        }
//        log_out << std::endl;
//    }

    for (int li = 0; li < vis_l.rows; li++)
    {
        if (is_3_lines)
        {
            Eigen::Matrix3i inds_l;
            Eigen::Vector3d n_cp;
            int ci;
            if ((int)vis_l.at<uchar>(li, 0) + (int)vis_l.at<uchar>(li, 1) == 2)
            {
                inds_l = inds;
                cv::Vec3d l1_cv = lprojs.at<cv::Vec3d>(li, 0);
                cv::Vec3d l2_cv = lprojs.at<cv::Vec3d>(li, 1);
                Eigen::Vector3d l1 = FromVec3d(l1_cv);
                Eigen::Vector3d l2 = FromVec3d(l2_cv);
                n_cp = l1.cross(l2);
                ci = 2;
                if ((int)vis_l.at<uchar>(li, ci) == 0)
                {
                    ci = 3;
                }
            } else {
                inds_l = inds.transpose();
                cv::Vec3d l3_cv = lprojs.at<cv::Vec3d>(li, 2);
                cv::Vec3d l4_cv = lprojs.at<cv::Vec3d>(li, 3);
                Eigen::Vector3d l3 = FromVec3d(l3_cv);
                Eigen::Vector3d l4 = FromVec3d(l4_cv);
                n_cp = l3.cross(l4);
                ci = 0;
                if ((int)vis_l.at<uchar>(li, ci) == 0)
                {
                    ci = 1;
                }
            }
            Eigen::Vector3d leq3 = FromVec3d(lprojs.at<cv::Vec3d>(li, ci));
            Eigen::Matrix<double,3,9> C;
            FillRMat(n_cp, inds_l, &C);

            Eigen::Matrix<double, 1, 13> C_full;
            C_full.setZero();
            C_full.block<1,9>(0, 0) = leq3.transpose() * C;
            A.block<1, 13>(li, 0) = C_full;
        } else {
            int ci1 = 0;
            int ci2 = 1;
            int ci3 = 2;
            Eigen::Matrix3i inds_l = inds;
            Eigen::Vector3d b;
            b.setZero();
            if ((int)vis_l.at<uchar>(li, 2) + (int)vis_l.at<uchar>(li, 3) == 2)
            {
                ci1 = 2;
                ci2 = 3;
                ci3 = 0;
                inds_l = inds.transpose();
                if ((int)vis_l.at<uchar>(li, 1) == 1)
                {
                    ci3 = 1;
                    b = t2;
                }
            } else {
                if ((int)vis_l.at<uchar>(li, 3) == 1)
                {
                    ci3 = 3;
                    b = t2;
                }
            }
            Eigen::Vector3d l1 = FromVec3d(lprojs.at<cv::Vec3d>(li, ci1));
            Eigen::Vector3d l2 = FromVec3d(lprojs.at<cv::Vec3d>(li, ci2));
            Eigen::Vector3d X1, X2;
            bool rv = TriangulateLine(Eigen::Matrix3d::Identity(), Eigen::Vector3d::Zero(), Eigen::Matrix3d::Identity(), t2,
                                 l1, l2, &X1, &X2);
            if (!rv)
            {
                return false;
            }
            Eigen::Matrix<double, 3, 9> C1, C2;
            FillRMat(X1, inds_l, &C1);
            FillRMat(X2, inds_l, &C2);

            Eigen::Vector3d l3 = FromVec3d(lprojs.at<cv::Vec3d>(li, ci3));

            A.block<1, 9>(a_ind+2*li, 0) = l3.transpose() * C1;
            A.block<1, 3>(a_ind+2*li, 9) = l3.transpose();
            A(a_ind+2*li, 12) = l3.transpose() * b;
            A.block<1, 9>(a_ind+2*li+1, 0) = l3.transpose() * C2;
            A.block<1, 3>(a_ind+2*li+1, 9) = l3.transpose();
            A(a_ind+2*li+1, 12) = l3.transpose() * b;
            tis.push_back(2*li);
        }
    }

//    log_out << " A" << std::endl;
//    for (int xi = 0; xi < 6; xi++)
//    {
//        for (int yi = 0; yi < 13; yi++)
//        {
//            log_out << A(xi, yi) << " ";
//        }
//        log_out << std::endl;
//    }

    Eigen::Matrix<double, 3, 10> A_fin;
    A_fin.setZero();
    Eigen::Matrix<double, 3, 10> T;
    if (!is_3_lines)
    {
        Eigen::Matrix<double, 6, 13> At;
        At.block<6,3>(0, 0) = A.block<6,3>(0, 9);
        At.block<6,9>(0, 3) = A.block<6,9>(0, 0);
        At.block<6,1>(0, 12) = A.block<6,1>(0, 12);
        RowReducePivot(&At);

//        log_out << " rref result " << std::endl;
//        for (int xi = 0; xi < 6; xi++)
//        {
//            for (int yi = 0; yi < 13; yi++)
//            {
//                log_out << At(xi, yi) << " ";
//            }
//            log_out << std::endl;
//        }

        T = -At.block<3,10>(0, 3);
        A_fin.block<3,10>(0, 0) = At.block<3, 10>(3, 3);
    } else {
        A_fin.block<3,9>(0,0) = A.block<3, 9>(0,0);
        //no constant terms
    }
//    std::cout << " A_fin" << std::endl;
//    for (int xi = 0; xi < 3; xi++)
//    {
//        for (int yi = 0; yi < 10; yi++)
//        {
//            std::cout << A_fin(xi, yi) << " ";
//        }
//        std::cout << std::endl;
//    }

    Eigen::Matrix<double, 10, 10> Q = BuildQ();

//    log_out << " Q" << std::endl;
//    for (int xi = 0; xi < 10; xi++)
//    {
//        for (int yi = 0; yi < 10; yi++)
//        {
//            log_out << Q(xi, yi) << " ";
//        }
//        log_out << std::endl;
//    }

    A_fin = A_fin * Q;

    std::vector<double> bs, cs, ds;
    if (!solve_3_quadric(A_fin, is_det_check, &bs, &cs, &ds))
    {
//        std::ofstream log_out("/home/alexander/debug_quad_mex");
//        log_out << "3 quad fail" << std::endl;
        return false;
    }

//    std::cout << " solution decoding " << bs.size() << std::endl;
    for (int i = 0; i < bs.size(); i++)
    {
        Eigen::VectorXd q(4);
        q(0) = 1.0;
        q(1) = bs[i];
        q(2) = cs[i];
        q(3) = ds[i];
        q = q / q.norm();
        Eigen::Matrix3d R;
        build_cam_from_quat(q, &R);
        Eigen::Vector3d t;
        if (is_3_lines)
        {
            if (!TFromRUsingTrifocalLines(projs, lprojs, vis_p, vis_l, R, &t))
            {
                continue;
            }
        } else {
            Eigen::Map<Eigen::RowVectorXd> v(R.data(), R.size());
            Eigen::VectorXd rvec(10);
            rvec.segment<9>(0) = v;
            rvec(9) = 1.0;
            t = T * rvec;
        }
        ts->push_back(t);
        Rs->push_back(R);
//        log_out << "b" << bs[i] << std::endl;
//        log_out << "t" << std::endl;
//        for (int xi = 0; xi < 3; xi++)
//        {
//            log_out << t(xi) << " ";
//        }
//        log_out << std::endl;
    }
    return true;
}
