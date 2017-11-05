
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

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_SHAPES_CURVE_H
#define PBRT_SHAPES_CURVE_H

// shapes/curve.h*
#include <pbrt/core/shape.h>
#include <absl/types/span.h>

namespace pbrt {

// CurveType Declarations
enum class CurveType { Flat, Cylinder, Ribbon };

// CurveCommon Declarations
struct CurveCommon {
    CurveCommon(absl::Span<const Point3f> c, Float w0, Float w1, CurveType type,
                absl::Span<const Normal3f> norm,
                std::shared_ptr<const Transform> ObjectToWorld,
                std::shared_ptr<const Transform> WorldToObject,
                bool reverseOrientation,
                const std::shared_ptr<const ParamSet> &attributes);

    const CurveType type;
    Point3f cpObj[4];
    Float width[2];
    Normal3f n[2];
    Float normalAngle, invSinNormalAngle;
    std::shared_ptr<const Transform> ObjectToWorld, WorldToObject;
    const bool reverseOrientation, transformSwapsHandedness;
    std::shared_ptr<const ParamSet> attributes;

};

// Curve Declarations
class Curve : public Shape {
  public:
    // Curve Public Methods
    Curve(const std::shared_ptr<CurveCommon> &common, Float uMin, Float uMax)
        : common(common), uMin(uMin), uMax(uMax) {}
    Bounds3f WorldBound() const;
    bool Intersect(const Ray &ray, Float *tHit, SurfaceInteraction *isect) const;
    Float Area() const;
    Interaction Sample(const Point2f &u, Float *pdf) const;

    bool ReverseOrientation() const { return common->reverseOrientation; }
    bool TransformSwapsHandedness() const {
        return common->transformSwapsHandedness;
    }
    const ParamSet *GetAttributes() const {
        return common->attributes.get();
    }

  private:
    // Curve Private Methods
    bool recursiveIntersect(const Ray &r, Float *tHit,
                            SurfaceInteraction *isect, const Point3f cp[4],
                            const Transform &rayToObject, Float u0, Float u1,
                            int depth) const;

    // Curve Private Data
    const std::shared_ptr<CurveCommon> common;
    const Float uMin, uMax;
};

std::vector<std::shared_ptr<Shape>> CreateCurveShape(
    std::shared_ptr<const Transform> ObjectToWorld,
    std::shared_ptr<const Transform> WorldToObject, bool reverseOrientation,
    const ParamSet &params, const std::shared_ptr<const ParamSet> &attributes);

}  // namespace pbrt

#endif  // PBRT_SHAPES_CURVE_H