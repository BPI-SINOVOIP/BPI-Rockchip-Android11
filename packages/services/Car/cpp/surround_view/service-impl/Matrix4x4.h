/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SURROUND_VIEW_SERVICE_IMPL_MATRIX4X4_H_
#define SURROUND_VIEW_SERVICE_IMPL_MATRIX4X4_H_

#include <array>
#include <cassert>
#include <cmath>
#include <iosfwd>

template <class VType>
class Matrix4x4 {
private:
    VType m[4][4];

public:
    typedef Matrix4x4<VType> Self;
    typedef VType BaseType;
    typedef std::array<VType, 4> MVector;

    // Initialize the matrix to 0
    Matrix4x4() {
        m[0][3] = m[0][2] = m[0][1] = m[0][0] = VType();
        m[1][3] = m[1][2] = m[1][1] = m[1][0] = VType();
        m[2][3] = m[2][2] = m[2][1] = m[2][0] = VType();
        m[3][3] = m[3][2] = m[3][1] = m[3][0] = VType();
    }

    // Explicitly set every element on construction
    Matrix4x4(const VType& m00, const VType& m01, const VType& m02, const VType& m03,
              const VType& m10, const VType& m11, const VType& m12, const VType& m13,
              const VType& m20, const VType& m21, const VType& m22, const VType& m23,
              const VType& m30, const VType& m31, const VType& m32, const VType& m33) {
        m[0][0] = m00;
        m[0][1] = m01;
        m[0][2] = m02;
        m[0][3] = m03;

        m[1][0] = m10;
        m[1][1] = m11;
        m[1][2] = m12;
        m[1][3] = m13;

        m[2][0] = m20;
        m[2][1] = m21;
        m[2][2] = m22;
        m[2][3] = m23;

        m[3][0] = m30;
        m[3][1] = m31;
        m[3][2] = m32;
        m[3][3] = m33;
    }

    // Casting constructor
    template <class VType2>
    static Matrix4x4 cast(const Matrix4x4<VType2>& mb) {
        return Matrix4x4(static_cast<VType>(mb(0, 0)), static_cast<VType>(mb(0, 1)),
                         static_cast<VType>(mb(0, 2)), static_cast<VType>(mb(0, 3)),
                         static_cast<VType>(mb(1, 0)), static_cast<VType>(mb(1, 1)),
                         static_cast<VType>(mb(1, 2)), static_cast<VType>(mb(1, 3)),
                         static_cast<VType>(mb(2, 0)), static_cast<VType>(mb(2, 1)),
                         static_cast<VType>(mb(2, 2)), static_cast<VType>(mb(2, 3)),
                         static_cast<VType>(mb(3, 0)), static_cast<VType>(mb(3, 1)),
                         static_cast<VType>(mb(3, 2)), static_cast<VType>(mb(3, 3)));
    }

    // Change the value of all the coefficients of the matrix
    inline Matrix4x4& set(const VType& m00, const VType& m01, const VType& m02, const VType& m03,
                          const VType& m10, const VType& m11, const VType& m12, const VType& m13,
                          const VType& m20, const VType& m21, const VType& m22, const VType& m23,
                          const VType& m30, const VType& m31, const VType& m32, const VType& m33) {
        m[0][0] = m00;
        m[0][1] = m01;
        m[0][2] = m02;
        m[0][3] = m03;

        m[1][0] = m10;
        m[1][1] = m11;
        m[1][2] = m12;
        m[1][3] = m13;

        m[2][0] = m20;
        m[2][1] = m21;
        m[2][2] = m22;
        m[2][3] = m23;

        m[3][0] = m30;
        m[3][1] = m31;
        m[3][2] = m32;
        m[3][3] = m33;
        return (*this);
    }

    // Matrix addition
    inline Matrix4x4& operator+=(const Matrix4x4& addFrom) {
        m[0][0] += addFrom.m[0][0];
        m[0][1] += addFrom.m[0][1];
        m[0][2] += addFrom.m[0][2];
        m[0][3] += addFrom.m[0][3];

        m[1][0] += addFrom.m[1][0];
        m[1][1] += addFrom.m[1][1];
        m[1][2] += addFrom.m[1][2];
        m[1][3] += addFrom.m[1][3];

        m[2][0] += addFrom.m[2][0];
        m[2][1] += addFrom.m[2][1];
        m[2][2] += addFrom.m[2][2];
        m[2][3] += addFrom.m[2][3];

        m[3][0] += addFrom.m[3][0];
        m[3][1] += addFrom.m[3][1];
        m[3][2] += addFrom.m[3][2];
        m[3][3] += addFrom.m[3][3];
        return (*this);
    }

    // Matrix subtration
    inline Matrix4x4& operator-=(const Matrix4x4& subFrom) {
        m[0][0] -= subFrom.m[0][0];
        m[0][1] -= subFrom.m[0][1];
        m[0][2] -= subFrom.m[0][2];
        m[0][3] -= subFrom.m[0][3];

        m[1][0] -= subFrom.m[1][0];
        m[1][1] -= subFrom.m[1][1];
        m[1][2] -= subFrom.m[1][2];
        m[1][3] -= subFrom.m[1][3];

        m[2][0] -= subFrom.m[2][0];
        m[2][1] -= subFrom.m[2][1];
        m[2][2] -= subFrom.m[2][2];
        m[2][3] -= subFrom.m[2][3];

        m[3][0] -= subFrom.m[3][0];
        m[3][1] -= subFrom.m[3][1];
        m[3][2] -= subFrom.m[3][2];
        m[3][3] -= subFrom.m[3][3];
        return (*this);
    }

    // Matrix multiplication by a scalar
    inline Matrix4x4& operator*=(const VType& k) {
        m[0][0] *= k;
        m[0][1] *= k;
        m[0][2] *= k;
        m[0][3] *= k;

        m[1][0] *= k;
        m[1][1] *= k;
        m[1][2] *= k;
        m[1][3] *= k;

        m[2][0] *= k;
        m[2][1] *= k;
        m[2][2] *= k;
        m[2][3] *= k;

        m[3][0] *= k;
        m[3][1] *= k;
        m[3][2] *= k;
        m[3][3] *= k;
        return (*this);
    }

    // Matrix addition
    inline Matrix4x4 operator+(const Matrix4x4& mb) const { return Matrix4x4(*this) += mb; }

    // Matrix subtraction
    inline Matrix4x4 operator-(const Matrix4x4& mb) const { return Matrix4x4(*this) -= mb; }

    // Change the sign of all the coefficients in the matrix
    friend inline Matrix4x4 operator-(const Matrix4x4& vb) {
        return Matrix4x4(-vb.m[0][0], -vb.m[0][1], -vb.m[0][2], -vb.m[0][3], -vb.m[1][0],
                         -vb.m[1][1], -vb.m[1][2], -vb.m[1][3], -vb.m[2][0], -vb.m[2][1],
                         -vb.m[2][2], -vb.m[2][3], -vb.m[3][0], -vb.m[3][1], -vb.m[3][2],
                         -vb.m[3][3]);
    }

    // Matrix multiplication by a scalar
    inline Matrix4x4 operator*(const VType& k) const { return Matrix4x4(*this) *= k; }

    // Multiplication by a scaler
    friend inline Matrix4x4 operator*(const VType& k, const Matrix4x4& mb) {
        return Matrix4x4(mb) * k;
    }

    // Matrix multiplication
    friend Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
        return Matrix4x4::fromCols(a * b.col(0), a * b.col(1), a * b.col(2), a * b.col(3));
    }

    // Multiplication of a matrix by a vector
    friend MVector operator*(const Matrix4x4& a, const MVector& b) {
        return MVector{dotProd(a.row(0), b), dotProd(a.row(1), b), dotProd(a.row(2), b),
                       dotProd(a.row(3), b)};
    }

    // Return the trace of the matrix
    inline VType trace() const { return m[0][0] + m[1][1] + m[2][2] + m[3][3]; }

    // Return a pointer to the data array for interface with other libraries
    // like opencv
    VType* data() { return reinterpret_cast<VType*>(m); }
    const VType* data() const { return reinterpret_cast<const VType*>(m); }

    // Return matrix element (i,j) with 0<=i<=3 0<=j<=3
    inline VType& operator()(const int i, const int j) {
        assert(i >= 0);
        assert(i < 4);
        assert(j >= 0);
        assert(j < 4);
        return m[i][j];
    }

    inline VType operator()(const int i, const int j) const {
        assert(i >= 0);
        assert(i < 4);
        assert(j >= 0);
        assert(j < 4);
        return m[i][j];
    }

    // Return matrix element (i/4,i%4) with 0<=i<=15 (access concatenated rows).
    inline VType& operator[](const int i) {
        assert(i >= 0);
        assert(i < 16);
        return reinterpret_cast<VType*>(m)[i];
    }
    inline VType operator[](const int i) const {
        assert(i >= 0);
        assert(i < 16);
        return reinterpret_cast<const VType*>(m)[i];
    }

    // Return the transposed matrix
    inline Matrix4x4 transpose() const {
        return Matrix4x4(m[0][0], m[1][0], m[2][0], m[3][0], m[0][1], m[1][1], m[2][1], m[3][1],
                         m[0][2], m[1][2], m[2][2], m[3][2], m[0][3], m[1][3], m[2][3], m[3][3]);
    }

    // Returns the transpose of the matrix of the cofactors.
    // (Useful for inversion for example.)
    inline Matrix4x4 comatrixTransposed() const {
        const auto cof = [this](unsigned int row, unsigned int col) {
            unsigned int r0 = (row + 1) % 4;
            unsigned int r1 = (row + 2) % 4;
            unsigned int r2 = (row + 3) % 4;
            unsigned int c0 = (col + 1) % 4;
            unsigned int c1 = (col + 2) % 4;
            unsigned int c2 = (col + 3) % 4;

            VType minor = m[r0][c0] * (m[r1][c1] * m[r2][c2] - m[r2][c1] * m[r1][c2]) -
                    m[r1][c0] * (m[r0][c1] * m[r2][c2] - m[r2][c1] * m[r0][c2]) +
                    m[r2][c0] * (m[r0][c1] * m[r1][c2] - m[r1][c1] * m[r0][c2]);
            return (row + col) & 1 ? -minor : minor;
        };

        // Transpose
        return Matrix4x4(cof(0, 0), cof(1, 0), cof(2, 0), cof(3, 0), cof(0, 1), cof(1, 1),
                         cof(2, 1), cof(3, 1), cof(0, 2), cof(1, 2), cof(2, 2), cof(3, 2),
                         cof(0, 3), cof(1, 3), cof(2, 3), cof(3, 3));
    }

    // Return dot production of two the vectors
    static inline VType dotProd(const MVector& lhs, const MVector& rhs) {
        return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2] + lhs[3] * rhs[3];
    }

    // Return the 4D vector at row i
    inline MVector row(const int i) const {
        assert(i >= 0);
        assert(i < 4);
        return MVector{m[i][0], m[i][1], m[i][2], m[i][3]};
    }

    // Return the 4D vector at col i
    inline MVector col(const int i) const {
        assert(i >= 0);
        assert(i < 4);
        return MVector{m[0][i], m[1][i], m[2][i], m[3][i]};
    }

    // Create a matrix from 4 row vectors
    static inline Matrix4x4 fromRows(const MVector& v1, const MVector& v2, const MVector& v3,
                                     const MVector& v4) {
        return Matrix4x4(v1[0], v1[1], v1[2], v1[3], v2[0], v2[1], v2[2], v2[3], v3[0], v3[1],
                         v3[2], v3[3], v4[0], v4[1], v4[2], v4[3]);
    }

    // Create a matrix from 3 column vectors
    static inline Matrix4x4 fromCols(const MVector& v1, const MVector& v2, const MVector& v3,
                                     const MVector& v4) {
        return Matrix4x4(v1[0], v2[0], v3[0], v4[0], v1[1], v2[1], v3[1], v4[1], v1[2], v2[2],
                         v3[2], v4[2], v1[3], v2[3], v3[3], v4[3]);
    }

    // Set the vector in row i to be v1
    void setRow(int i, const MVector& v1) {
        assert(i >= 0);
        assert(i < 4);
        m[i][0] = v1[0];
        m[i][1] = v1[1];
        m[i][2] = v1[2];
        m[i][3] = v1[3];
    }

    // Set the vector in column i to be v1
    void setCol(int i, const MVector& v1) {
        assert(i >= 0);
        assert(i < 4);
        m[0][i] = v1[0];
        m[1][i] = v1[1];
        m[2][i] = v1[2];
        m[3][i] = v1[3];
    }

    // Return the identity matrix
    static inline Matrix4x4 identity() {
        return Matrix4x4(VType(1), VType(), VType(), VType(), VType(), VType(1), VType(), VType(),
                         VType(), VType(), VType(1), VType(), VType(), VType(), VType(), VType(1));
    }

    // Return a matrix full of zeros
    static inline Matrix4x4 zero() { return Matrix4x4(); }

    // Return a diagonal matrix with the coefficients in v
    static inline Matrix4x4 diagonal(const MVector& v) {
        return Matrix4x4(v[0], VType(), VType(), VType(),  //
                         VType(), v[1], VType(), VType(),  //
                         VType(), VType(), v[2], VType(),  //
                         VType(), VType(), VType(), v[3]);
    }

    // Return the matrix vvT
    static Matrix4x4 sym4(const MVector& v) {
        return Matrix4x4(v[0] * v[0], v[0] * v[1], v[0] * v[2], v[0] * v[3], v[1] * v[0],
                         v[1] * v[1], v[1] * v[2], v[1] * v[3], v[2] * v[0], v[2] * v[1],
                         v[2] * v[2], v[2] * v[3], v[3] * v[0], v[3] * v[1], v[3] * v[2],
                         v[3] * v[3]);
    }

    // Return the Frobenius norm of the matrix: sqrt(sum(aij^2))
    VType frobeniusNorm() const {
        VType sum = VType();
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                sum += m[i][j] * m[i][j];
            }
        }
        return std::sqrt(sum);
    }

    // Return true is one of the elements of the matrix is NaN
    bool isNaN() const {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (isnan(m[i][j])) {
                    return true;
                }
            }
        }
        return false;
    }

    friend bool operator==(const Matrix4x4& a, const Matrix4x4& b) {
        return a.m[0][0] == b.m[0][0] && a.m[0][1] == b.m[0][1] && a.m[0][2] == b.m[0][2] &&
                a.m[0][3] == b.m[0][3] && a.m[1][0] == b.m[1][0] && a.m[1][1] == b.m[1][1] &&
                a.m[1][2] == b.m[1][2] && a.m[1][3] == b.m[1][3] && a.m[2][0] == b.m[2][0] &&
                a.m[2][1] == b.m[2][1] && a.m[2][2] == b.m[2][2] && a.m[2][3] == b.m[2][3] &&
                a.m[3][0] == b.m[3][0] && a.m[3][1] == b.m[3][1] && a.m[3][2] == b.m[3][2] &&
                a.m[3][3] == b.m[3][3];
    }

    friend bool operator!=(const Matrix4x4& a, const Matrix4x4& b) { return !(a == b); }
};

typedef Matrix4x4<int> Matrix4x4I;
typedef Matrix4x4<float> Matrix4x4F;
typedef Matrix4x4<double> Matrix4x4D;

#endif  // #ifndef SURROUND_VIEW_SERVICE_IMPL_MATRIX4X4_H_
