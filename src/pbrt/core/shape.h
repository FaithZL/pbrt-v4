
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

#ifndef PBRT_CORE_SHAPE_H
#define PBRT_CORE_SHAPE_H

// core/shape.h*
#include <pbrt/core/pbrt.h>

#include <pbrt/util/bounds.h>
#include <pbrt/util/geometry.h>
#include <pbrt/util/transform.h>

#include <memory>

namespace pbrt {

// Shape Declarations
class Shape {
  public:
    // Shape Interface
    Shape();
    virtual ~Shape();
    virtual Bounds3f WorldBound() const = 0;
    virtual bool Intersect(const Ray &ray, Float *tHit,
                           SurfaceInteraction *isect) const = 0;
    virtual bool IntersectP(const Ray &ray) const {
        return Intersect(ray, nullptr, nullptr);
    }
    virtual Float Area() const = 0;
    // Sample a point on the surface of the shape and return the PDF with
    // respect to area on the surface.
    virtual Interaction Sample(const Point2f &u, Float *pdf) const = 0;
    virtual Float Pdf(const Interaction &) const { return 1 / Area(); }

    // Sample a point on the shape given a reference point |ref| and
    // return the PDF with respect to solid angle from |ref|.
    virtual Interaction Sample(const Interaction &ref, const Point2f &u,
                               Float *pdf) const;
    virtual Float Pdf(const Interaction &ref, const Vector3f &wi) const;

    // Returns the solid angle subtended by the shape w.r.t. the reference
    // point p, given in world space. Some shapes compute this value in
    // closed-form, while the default implementation uses Monte Carlo
    // integration; the nSamples parameter determines how many samples are
    // used in this case.
    virtual Float SolidAngle(const Point3f &p, int nSamples = 512) const;

    virtual bool ReverseOrientation() const = 0;
    virtual bool TransformSwapsHandedness() const = 0;
    virtual const ParamSet *GetAttributes() const = 0;
};

class TransformedShape : public Shape {
  public:
    TransformedShape(std::shared_ptr<const Transform> ObjectToWorld,
                     std::shared_ptr<const Transform> WorldToObject,
                     bool reverseOrientation,
                     const std::shared_ptr<const ParamSet> &attributes);

    Bounds3f WorldBound() const;
    virtual Bounds3f ObjectBound() const = 0;

    bool ReverseOrientation() const { return reverseOrientation; }
    bool TransformSwapsHandedness() const { return transformSwapsHandedness; }
    const ParamSet *GetAttributes() const { return attributes.get(); }

  protected:
    // Shape Public Data
    std::shared_ptr<const Transform> ObjectToWorld, WorldToObject;
    const bool reverseOrientation;
    const bool transformSwapsHandedness;
    std::shared_ptr<const ParamSet> attributes;
};

}  // namespace pbrt

#endif  // PBRT_CORE_SHAPE_H