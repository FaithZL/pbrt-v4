
/*
    pbrt source code is Copyright(c) 1998-2016
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

// core/interpolation.cpp*
#include "interpolation.h"

#include "mathutil.h"
#include <glog/logging.h>

using gtl::ArraySlice;
using gtl::MutableArraySlice;

namespace pbrt {

// Spline Interpolation Definitions
Float CatmullRom(ArraySlice<Float> nodes, ArraySlice<Float> values, Float x) {
    CHECK_EQ(nodes.size(), values.size());
    if (!(x >= nodes.front() && x <= nodes.back())) return 0;
    int idx = GetIntArrayerval(nodes.size(), [&](int i) { return nodes[i] <= x; });
    Float x0 = nodes[idx], x1 = nodes[idx + 1];
    Float f0 = values[idx], f1 = values[idx + 1];
    Float width = x1 - x0;
    Float d0, d1;
    if (idx > 0)
        d0 = width * (f1 - values[idx - 1]) / (x1 - nodes[idx - 1]);
    else
        d0 = f1 - f0;

    if (idx + 2 < nodes.size())
        d1 = width * (values[idx + 2] - f0) / (nodes[idx + 2] - x0);
    else
        d1 = f1 - f0;

    Float t = (x - x0) / (x1 - x0), t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * f0 + (-2 * t3 + 3 * t2) * f1 +
           (t3 - 2 * t2 + t) * d0 + (t3 - t2) * d1;
}

bool CatmullRomWeights(ArraySlice<Float> nodes, Float x, int *offset,
                       MutableArraySlice<Float> weights) {
    CHECK_GE(weights.size(), 4);
    // Return _false_ if _x_ is out of bounds
    if (!(x >= nodes.front() && x <= nodes.back())) return false;

    // Search for the interval _idx_ containing _x_
    int idx = GetIntArrayerval(nodes.size(), [&](int i) { return nodes[i] <= x; });
    *offset = idx - 1;
    Float x0 = nodes[idx], x1 = nodes[idx + 1];

    // Compute the $t$ parameter and powers
    Float t = (x - x0) / (x1 - x0), t2 = t * t, t3 = t2 * t;

    // Compute initial node weights $w_1$ and $w_2$
    weights[1] = 2 * t3 - 3 * t2 + 1;
    weights[2] = -2 * t3 + 3 * t2;

    // Compute first node weight $w_0$
    if (idx > 0) {
        Float w0 = (t3 - 2 * t2 + t) * (x1 - x0) / (x1 - nodes[idx - 1]);
        weights[0] = -w0;
        weights[2] += w0;
    } else {
        Float w0 = t3 - 2 * t2 + t;
        weights[0] = 0;
        weights[1] -= w0;
        weights[2] += w0;
    }

    // Compute last node weight $w_3$
    if (idx + 2 < nodes.size()) {
        Float w3 = (t3 - t2) * (x1 - x0) / (nodes[idx + 2] - x0);
        weights[1] -= w3;
        weights[3] = w3;
    } else {
        Float w3 = t3 - t2;
        weights[1] -= w3;
        weights[2] += w3;
        weights[3] = 0;
    }
    return true;
}

Float SampleCatmullRom(ArraySlice<Float> x, ArraySlice<Float> f,
                       ArraySlice<Float> F, Float u, Float *fval, Float *pdf) {
    CHECK_EQ(x.size(), f.size());
    CHECK_EQ(f.size(), F.size());

    // Map _u_ to a spline interval by inverting _F_
    u *= F.back();
    int i = GetIntArrayerval(F.size(), [&](int i) { return F[i] <= u; });

    // Look up $x_i$ and function values of spline segment _i_
    Float x0 = x[i], x1 = x[i + 1];
    Float f0 = f[i], f1 = f[i + 1];
    Float width = x1 - x0;

    // Approximate derivatives using finite differences
    Float d0, d1;
    if (i > 0)
        d0 = width * (f1 - f[i - 1]) / (x1 - x[i - 1]);
    else
        d0 = f1 - f0;
    if (i + 2 < x.size())
        d1 = width * (f[i + 2] - f0) / (x[i + 2] - x0);
    else
        d1 = f1 - f0;

    // Re-scale _u_ for continous spline sampling step
    u = (u - F[i]) / width;

    // Invert definite integral over spline segment and return solution

    // Set initial guess for $t$ by importance sampling a linear interpolant
    Float t;
    if (f0 != f1)
        t = (f0 - SafeSqrt(f0 * f0 + 2 * u * (f1 - f0))) /
            (f0 - f1);
    else
        t = u / f0;
    Float a = 0, b = 1, Fhat, fhat;
    while (true) {
        // Fall back to a bisection step when _t_ is out of bounds
        if (!(t > a && t < b)) t = 0.5f * (a + b);

        // Evaluate target function and its derivative in Horner form
        Fhat = t * (f0 +
                    t * (.5f * d0 +
                         t * ((1.f / 3.f) * (-2 * d0 - d1) + f1 - f0 +
                              t * (.25f * (d0 + d1) + .5f * (f0 - f1)))));
        fhat = f0 +
               t * (d0 +
                    t * (-2 * d0 - d1 + 3 * (f1 - f0) +
                         t * (d0 + d1 + 2 * (f0 - f1))));

        // Stop the iteration if converged
        if (std::abs(Fhat - u) < 1e-6f || b - a < 1e-6f) break;

        // Update bisection bounds using updated _t_
        if (Fhat - u < 0)
            a = t;
        else
            b = t;

        // Perform a Newton step
        t -= (Fhat - u) / fhat;
    }

    // Return the sample position and function value
    if (fval) *fval = fhat;
    if (pdf) *pdf = fhat / F.back();
    return x0 + width * t;
}

Float SampleCatmullRom2D(ArraySlice<Float> nodes1, ArraySlice<Float> nodes2,
                         ArraySlice<Float> values, ArraySlice<Float> cdf,
                         Float alpha, Float u, Float *fval, Float *pdf) {
    // Determine offset and coefficients for the _alpha_ parameter
    int offset;
    Float weights[4];
    if (!CatmullRomWeights(nodes1, alpha, &offset, weights)) return 0;

    // Define a lambda function to interpolate table entries
    auto interpolate = [&](ArraySlice<Float> array, int idx) {
        Float value = 0;
        for (int i = 0; i < 4; ++i)
            if (weights[i] != 0)
                value += array[(offset + i) * nodes2.size() + idx] * weights[i];
        return value;
    };

    // Map _u_ to a spline interval by inverting the interpolated _cdf_
    Float maximum = interpolate(cdf, nodes2.size() - 1);
    u *= maximum;
    int idx = GetIntArrayerval(nodes2.size(),
                           [&](int i) { return interpolate(cdf, i) <= u; });

    // Look up node positions and interpolated function values
    Float f0 = interpolate(values, idx), f1 = interpolate(values, idx + 1);
    Float x0 = nodes2[idx], x1 = nodes2[idx + 1];
    Float width = x1 - x0;
    Float d0, d1;

    // Re-scale _u_ using the interpolated _cdf_
    u = (u - interpolate(cdf, idx)) / width;

    // Approximate derivatives using finite differences of the interpolant
    if (idx > 0)
        d0 = width * (f1 - interpolate(values, idx - 1)) /
             (x1 - nodes2[idx - 1]);
    else
        d0 = f1 - f0;
    if (idx + 2 < nodes2.size())
        d1 = width * (interpolate(values, idx + 2) - f0) /
             (nodes2[idx + 2] - x0);
    else
        d1 = f1 - f0;

    // Invert definite integral over spline segment and return solution

    // Set initial guess for $t$ by importance sampling a linear interpolant
    Float t;
    if (f0 != f1)
        t = (f0 - SafeSqrt(f0 * f0 + 2 * u * (f1 - f0))) /
            (f0 - f1);
    else
        t = u / f0;
    Float a = 0, b = 1, Fhat, fhat;
    while (true) {
        // Fall back to a bisection step when _t_ is out of bounds
        if (!(t >= a && t <= b)) t = 0.5f * (a + b);

        // Evaluate target function and its derivative in Horner form
        Fhat = t * (f0 +
                    t * (.5f * d0 +
                         t * ((1.f / 3.f) * (-2 * d0 - d1) + f1 - f0 +
                              t * (.25f * (d0 + d1) + .5f * (f0 - f1)))));
        fhat = f0 +
               t * (d0 +
                    t * (-2 * d0 - d1 + 3 * (f1 - f0) +
                         t * (d0 + d1 + 2 * (f0 - f1))));

        // Stop the iteration if converged
        if (std::abs(Fhat - u) < 1e-6f || b - a < 1e-6f) break;

        // Update bisection bounds using updated _t_
        if (Fhat - u < 0)
            a = t;
        else
            b = t;

        // Perform a Newton step
        t -= (Fhat - u) / fhat;
    }

    // Return the sample position and function value
    if (fval) *fval = fhat;
    if (pdf) *pdf = fhat / maximum;
    return x0 + width * t;
}

Float IntegrateCatmullRom(ArraySlice<Float> x, ArraySlice<Float> values,
                          MutableArraySlice<Float> cdf) {
    CHECK_EQ(x.size(), values.size());
    Float sum = 0;
    cdf[0] = 0;
    for (int i = 0; i < x.size() - 1; ++i) {
        // Look up $x_i$ and function values of spline segment _i_
        Float x0 = x[i], x1 = x[i + 1];
        Float f0 = values[i], f1 = values[i + 1];
        Float width = x1 - x0;

        // Approximate derivatives using finite differences
        Float d0, d1;
        if (i > 0)
            d0 = width * (f1 - values[i - 1]) / (x1 - x[i - 1]);
        else
            d0 = f1 - f0;
        if (i + 2 < x.size())
            d1 = width * (values[i + 2] - f0) / (x[i + 2] - x0);
        else
            d1 = f1 - f0;

        // Keep a running sum and build a cumulative distribution function
        sum += ((d0 - d1) * (1.f / 12.f) + (f0 + f1) * .5f) * width;
        cdf[i + 1] = sum;
    }
    return sum;
}

Float InvertCatmullRom(ArraySlice<Float> x, ArraySlice<Float> values, Float u) {
    // Stop when _u_ is out of bounds
    if (!(u > values.front()))
        return x.front();
    else if (!(u < values.back()))
        return x.back();

    // Map _u_ to a spline interval by inverting _values_
    int i = GetIntArrayerval(values.size(), [&](int i) { return values[i] <= u; });

    // Look up $x_i$ and function values of spline segment _i_
    Float x0 = x[i], x1 = x[i + 1];
    Float f0 = values[i], f1 = values[i + 1];
    Float width = x1 - x0;

    // Approximate derivatives using finite differences
    Float d0, d1;
    if (i > 0)
        d0 = width * (f1 - values[i - 1]) / (x1 - x[i - 1]);
    else
        d0 = f1 - f0;
    if (i + 2 < x.size())
        d1 = width * (values[i + 2] - f0) / (x[i + 2] - x0);
    else
        d1 = f1 - f0;

    // Invert the spline interpolant using Newton-Bisection
    Float a = 0, b = 1, t = .5f;
    Float Fhat, fhat;
    while (true) {
        // Fall back to a bisection step when _t_ is out of bounds
        if (!(t > a && t < b)) t = 0.5f * (a + b);

        // Compute powers of _t_
        Float t2 = t * t, t3 = t2 * t;

        // Set _Fhat_ using Equation (8.27)
        Fhat = (2 * t3 - 3 * t2 + 1) * f0 + (-2 * t3 + 3 * t2) * f1 +
               (t3 - 2 * t2 + t) * d0 + (t3 - t2) * d1;

        // Set _fhat_ using Equation (not present)
        fhat = (6 * t2 - 6 * t) * f0 + (-6 * t2 + 6 * t) * f1 +
               (3 * t2 - 4 * t + 1) * d0 + (3 * t2 - 2 * t) * d1;

        // Stop the iteration if converged
        if (std::abs(Fhat - u) < 1e-6f || b - a < 1e-6f) break;

        // Update bisection bounds using updated _t_
        if (Fhat - u < 0)
            a = t;
        else
            b = t;

        // Perform a Newton step
        t -= (Fhat - u) / fhat;
    }
    return x0 + t * width;
}

// Fourier Interpolation Definitions
Float Fourier(ArraySlice<Float> a, double cosPhi) {
    double value = 0.0;
    // Initialize cosine iterates
    double cosKMinusOnePhi = cosPhi;
    double cosKPhi = 1;
    for (int k = 0; k < a.size(); ++k) {
        // Add the current summand and update the cosine iterates
        value += a[k] * cosKPhi;
        double cosKPlusOnePhi = 2 * cosPhi * cosKPhi - cosKMinusOnePhi;
        cosKMinusOnePhi = cosKPhi;
        cosKPhi = cosKPlusOnePhi;
    }
    return value;
}

Float SampleFourier(ArraySlice<Float> ak, ArraySlice<Float> recip, Float u,
                    Float *pdf, Float *phiPtr) {
    // Pick a side and declare bisection variables
    bool flip = (u >= 0.5);
    if (flip)
        u = 1 - 2 * (u - .5f);
    else
        u *= 2;
    double a = 0, b = Pi, phi = 0.5 * Pi;
    double F, f;
    while (true) {
        // Evaluate $F(\phi)$ and its derivative $f(\phi)$

        // Initialize sine and cosine iterates
        double cosPhi = std::cos(phi);
        double sinPhi = SafeSqrt(1 - cosPhi * cosPhi);
        double cosPhiPrev = cosPhi, cosPhiCur = 1;
        double sinPhiPrev = -sinPhi, sinPhiCur = 0;

        // Initialize _F_ and _f_ with the first series term
        F = ak[0] * phi;
        f = ak[0];
        for (int k = 1; k < ak.size(); ++k) {
            // Compute next sine and cosine iterates
            double sinPhiNext = 2 * cosPhi * sinPhiCur - sinPhiPrev;
            double cosPhiNext = 2 * cosPhi * cosPhiCur - cosPhiPrev;
            sinPhiPrev = sinPhiCur;
            sinPhiCur = sinPhiNext;
            cosPhiPrev = cosPhiCur;
            cosPhiCur = cosPhiNext;

            // Add the next series term to _F_ and _f_
            F += ak[k] * recip[k] * sinPhiNext;
            f += ak[k] * cosPhiNext;
        }
        F -= u * ak[0] * Pi;

        // Update bisection bounds using updated $\phi$
        if (F > 0)
            b = phi;
        else
            a = phi;

        // Stop the Fourier bisection iteration if converged
        if (std::abs(F) < 1e-6f || b - a < 1e-6f) break;

        // Perform a Newton step given $f(\phi)$ and $F(\phi)$
        phi -= F / f;

        // Fall back to a bisection step when $\phi$ is out of bounds
        if (!(phi > a && phi < b)) phi = 0.5f * (a + b);
    }
    // Potentially flip $\phi$ and return the result
    if (flip) phi = 2 * Pi - phi;
    *pdf = (Float)(Inv2Pi * f / ak[0]);
    *phiPtr = (Float)phi;
    return f;
}

}  // namespace pbrt
