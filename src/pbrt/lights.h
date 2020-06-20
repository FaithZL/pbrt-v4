// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_LIGHTS_POINT_H
#define PBRT_LIGHTS_POINT_H

// lights/point.h*
#include <pbrt/pbrt.h>

#include <pbrt/base/light.h>
#include <pbrt/base/medium.h>
#include <pbrt/interaction.h>
#include <pbrt/shapes.h>
#include <pbrt/util/image.h>
#include <pbrt/util/log.h>
#include <pbrt/util/pstd.h>
#include <pbrt/util/sampling.h>
#include <pbrt/util/spectrum.h>
#include <pbrt/util/transform.h>
#include <pbrt/util/vecmath.h>

#include <memory>

namespace pbrt {

std::string ToString(LightType type);

PBRT_CPU_GPU
inline bool IsDeltaLight(LightType type) {
    return (type == LightType::DeltaPosition || type == LightType::DeltaDirection);
}

class LightLiSample {
  public:
    LightLiSample() = default;
    PBRT_CPU_GPU
    LightLiSample(LightHandle light, const SampledSpectrum &L, const Vector3f &wi,
                  Float pdf, const Interaction &pRef, const Interaction &pLight)
        : L(L), wi(wi), pdf(pdf), light(light), pRef(pRef), pLight(pLight) {}

    SampledSpectrum L;
    Vector3f wi;
    Float pdf;
    LightHandle light;
    Interaction pRef, pLight;
};

class LightLeSample {
  public:
    LightLeSample() = default;
    PBRT_CPU_GPU
    LightLeSample(const SampledSpectrum &L, const Ray &ray, Float pdfPos, Float pdfDir)
        : L(L), ray(ray), pdfPos(pdfPos), pdfDir(pdfDir) {}
    PBRT_CPU_GPU
    LightLeSample(const SampledSpectrum &L, const Ray &ray, const Interaction &intr,
                  Float pdfPos, Float pdfDir)
        : L(L), ray(ray), intr(intr), pdfPos(pdfPos), pdfDir(pdfDir) {
        CHECK(this->intr->n != Normal3f(0, 0, 0));
    }

    PBRT_CPU_GPU
    Float AbsCosTheta(const Vector3f &w) const { return intr ? AbsDot(w, intr->n) : 1; }

    SampledSpectrum L;
    Ray ray;
    pstd::optional<Interaction> intr;
    Float pdfPos, pdfDir;
};

struct LightBounds {
    LightBounds() = default;
    // theta_o: angular spread of surface normals around w
    // theta_e: given a visible normal, what is the spread of directions it
    // emits over.
    LightBounds(const Bounds3f &b, const Vector3f &w, Float phi, Float theta_o,
                Float theta_e, bool twoSided)
        : b(b),
          w(Normalize(w)),
          phi(phi),
          theta_o(theta_o),
          theta_e(theta_e),
          cosTheta_o(std::cos(theta_o)),
          cosTheta_e(std::cos(theta_e)),
          twoSided(twoSided) {}
    LightBounds(const Point3f &p, const Vector3f &w, Float phi, Float theta_o,
                Float theta_e, bool twoSided)
        : b(p, p),
          w(Normalize(w)),
          phi(phi),
          theta_o(theta_o),
          theta_e(theta_e),
          cosTheta_o(std::cos(theta_o)),
          cosTheta_e(std::cos(theta_e)),
          twoSided(twoSided) {}

    // baseline: 38s in importance
    // acos hack: 34s
    // theta_u 0 if far away: 23s (!)
    PBRT_CPU_GPU
    Float Importance(const Interaction &intr) const {
        Point3f pc = (b.pMin + b.pMax) / 2;
        Float d2 = DistanceSquared(intr.p(), pc);
        // Don't let d2 get too small if p is inside the bounds.
        d2 = std::max(d2, Length(b.Diagonal()) / 2);

        Vector3f wi = Normalize(intr.p() - pc);

#if 0
        Float cosTheta = Dot(w, wi);
        Float theta = SafeACos(cosTheta);
        if (twoSided && theta > Pi / 2) {
            theta = std::max<Float>(0, Pi - theta);
            cosTheta = std::abs(cosTheta);
        }
        Float sinTheta = SafeSqrt(1 - cosTheta * cosTheta);

        Float cosTheta_u = BoundSubtendedDirections(b, intr.p()).cosTheta;
        Float theta_u = SafeACos(cosTheta_u);

        Float thetap = std::max<Float>(0, theta - theta_o - theta_u);

        if (thetap >= theta_e)
            return 0;

        Float imp = phi * std::cos(thetap) / d2;
        CHECK_GE(imp, -1e-3);

        if (intr.n != Normal3f(0,0,0)) {
            Float cosTheta_i = AbsDot(wi, intr.n);
            Float theta_i = SafeACos(cosTheta_i);
            Float thetap_i = std::max<Float>(theta_i - theta_u, 0);
            imp *= std::cos(thetap_i);
        }
#else
        Float cosTheta = Dot(w, wi);
        if (twoSided)
            cosTheta = std::abs(cosTheta);
#if 0
        else if (cosTheta < 0 && cosTheta_o == 1) {
            // Catch the case where the point is outside the bounds and definitely
            // not in the emitted cone even though the conservative theta_u test
            // make suggest it could be.
            // Doesn't seem to make much difference in practice.
            Point3f p = intr.p();
            if ((p.x < b.pMin.x || p.x > b.pMax.x) &&
                (p.y < b.pMin.y || p.y > b.pMax.y) &&
                (p.z < b.pMin.z || p.z > b.pMax.z))
                return 0;
        }
#endif

        // FIXME? unstable when cosTheta \approx 1
        Float sinTheta = SafeSqrt(1 - cosTheta * cosTheta);

        // cos(max(0, a-b))
        auto cosSubClamped = [](Float sinThetaA, Float cosThetaA, Float sinThetaB,
                                Float cosThetaB) -> Float {
            if (cosThetaA > cosThetaB)
                // Handle the max(0, ...)
                return 1;
            return cosThetaA * cosThetaB + sinThetaA * sinThetaB;
        };
        // sin(max(0, a-b))
        auto sinSubClamped = [](Float sinThetaA, Float cosThetaA, Float sinThetaB,
                                Float cosThetaB) -> Float {
            if (cosThetaA > cosThetaB)
                // Handle the max(0, ...)
                return 0;
            return sinThetaA * cosThetaB - cosThetaA * sinThetaB;
        };

        Float cosTheta_u = BoundSubtendedDirections(b, intr.p()).cosTheta;
        Float sinTheta_u = SafeSqrt(1 - cosTheta_u * cosTheta_u);

        // Open issue: for a tri light that's axis aligned, we'd like to have
        // very low to zero importance for points in its plane. This doesn't
        // quite work out due to subtracting out the bounds' subtended angle.

        // cos(theta_p). Compute in two steps
        Float cosTheta_x = cosSubClamped(
            sinTheta, cosTheta, SafeSqrt(1 - cosTheta_o * cosTheta_o), cosTheta_o);
        Float sinTheta_x = sinSubClamped(
            sinTheta, cosTheta, SafeSqrt(1 - cosTheta_o * cosTheta_o), cosTheta_o);
        Float cosTheta_p = cosSubClamped(sinTheta_x, cosTheta_x, sinTheta_u, cosTheta_u);

        if (cosTheta_p <= cosTheta_e)
            return 0;

        Float imp = phi * cosTheta_p / d2;
        DCHECK_GE(imp, -1e-3);

        if (intr.n != Normal3f(0, 0, 0)) {
            // cos(thetap_i) = cos(max(0, theta_i - theta_u))
            // cos (a-b) = cos a cos b + sin a sin b
            Float cosTheta_i = AbsDot(wi, intr.n);
            Float sinTheta_i = SafeSqrt(1 - cosTheta_i * cosTheta_i);
            Float cosThetap_i =
                cosSubClamped(sinTheta_i, cosTheta_i, sinTheta_u, cosTheta_u);
            imp *= cosThetap_i;
        }
#endif

        return std::max<Float>(imp, 0);
    }

    Point3f Centroid() const { return (b.pMin + b.pMax) / 2; }

    std::string ToString() const;

    Bounds3f b;  // TODO: rename to |bounds|?
    Vector3f w;
    Float phi = 0;
    Float theta_o = 0, theta_e = 0;
    Float cosTheta_o = 1, cosTheta_e = 1;
    bool twoSided = false;
};

LightBounds Union(const LightBounds &a, const LightBounds &b);

class LightBase {
  public:
    LightBase(LightType flags, const AnimatedTransform &worldFromLight,
              const MediumInterface &mediumInterface);

    PBRT_CPU_GPU
    LightType Type() const { return type; }

    PBRT_CPU_GPU
    SampledSpectrum L(const Interaction &intr, const Vector3f &w,
                      const SampledWavelengths &lambda) const {
        return SampledSpectrum(0.f);
    }

    PBRT_CPU_GPU
    SampledSpectrum Le(const Ray &ray, const SampledWavelengths &lambda) const {
        return SampledSpectrum(0.f);
    }

  protected:
    std::string BaseToString() const;

    // Light Protected Data
    LightType type;
    MediumInterface mediumInterface;

    AnimatedTransform worldFromLight;
};

// PointLight Declarations
class PointLight : public LightBase {
  public:
    // PointLight Public Methods
    PointLight(const AnimatedTransform &worldFromLight,
               const MediumInterface &mediumInterface, SpectrumHandle I, Allocator alloc)
        : LightBase(LightType::DeltaPosition, worldFromLight, mediumInterface), I(I) {}

    static PointLight *Create(const AnimatedTransform &worldFromLight,
                              MediumHandle medium, const ParameterDictionary &parameters,
                              const RGBColorSpace *colorSpace, const FileLoc *loc,
                              Allocator alloc);

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const {
        Point3f p = worldFromLight(Point3f(0, 0, 0), ref.time);
        Vector3f wi = Normalize(p - ref.p());
        return LightLiSample(this, I.Sample(lambda) / DistanceSquared(p, ref.p()), wi, 1,
                             ref, Interaction(p, ref.time, &mediumInterface));
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;
    void Preprocess(const Bounds3f &sceneBounds) {}

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const {
        return 0;
    }

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    LightBounds Bounds() const;

    std::string ToString() const;

  private:
    // PointLight Private Data
    SpectrumHandle I;
};

// DistantLight Declarations
class DistantLight : public LightBase {
  public:
    // DistantLight Public Methods
    DistantLight(const AnimatedTransform &worldFromLight, SpectrumHandle L,
                 Allocator alloc);

    static DistantLight *Create(const AnimatedTransform &worldFromLight,
                                const ParameterDictionary &parameters,
                                const RGBColorSpace *colorSpace, const FileLoc *loc,
                                Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {
        sceneBounds.BoundingSphere(&sceneCenter, &sceneRadius);
    }
    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const {
        Vector3f wi = Normalize(worldFromLight(Vector3f(0, 0, 1), ref.time));
        Point3f pOutside = ref.p() + wi * (2 * sceneRadius);
        return LightLiSample(this, Lemit.Sample(lambda), wi, 1, ref,
                             Interaction(pOutside, ref.time, &mediumInterface));
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const {
        return 0;
    }

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    pstd::optional<LightBounds> Bounds() const { return {}; }

    std::string ToString() const;

  private:
    // DistantLight Private Data
    SpectrumHandle Lemit;
    Point3f sceneCenter;
    Float sceneRadius;
};

// ProjectionLight Declarations
class ProjectionLight : public LightBase {
  public:
    // ProjectionLight Public Methods
    ProjectionLight(const AnimatedTransform &worldFromLight,
                    const MediumInterface &medium, Image image,
                    const RGBColorSpace *colorSpace, Float scale, Float fov,
                    Allocator alloc);

    static ProjectionLight *Create(const AnimatedTransform &worldFromLight,
                                   MediumHandle medium,
                                   const ParameterDictionary &parameters,
                                   const FileLoc *loc, Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {}

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const;
    PBRT_CPU_GPU
    SampledSpectrum Projection(const Vector3f &w, const SampledWavelengths &lambda) const;

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    LightBounds Bounds() const;

    std::string ToString() const;

  private:
    // ProjectionLight Private Data
    Image image;
    const RGBColorSpace *imageColorSpace;
    Float scale;
    Transform ScreenFromLight, LightFromScreen;
    Float hither;
    Bounds2f screenBounds;
    Float cosTotalWidth;
    Float A;
    PiecewiseConstant2D distrib;
};

// GoniometricLight Declarations
class GoniometricLight : public LightBase {
  public:
    // GoniometricLight Public Methods
    GoniometricLight(const AnimatedTransform &worldFromLight,
                     const MediumInterface &mediumInterface, SpectrumHandle I,
                     Image image, const RGBColorSpace *imageColorSpace, Allocator alloc);

    static GoniometricLight *Create(const AnimatedTransform &worldFromLight,
                                    MediumHandle medium,
                                    const ParameterDictionary &parameters,
                                    const RGBColorSpace *colorSpace, const FileLoc *loc,
                                    Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {}

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const;
    PBRT_CPU_GPU
    SampledSpectrum Scale(Vector3f wl, const SampledWavelengths &lambda) const {
        Float theta = SphericalTheta(wl), phi = SphericalPhi(wl);
        Point2f st(phi * Inv2Pi, theta * InvPi);
        return I.Sample(lambda) * image.LookupNearestChannel(st, 0);
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    LightBounds Bounds() const;

    std::string ToString() const;

  private:
    // GoniometricLight Private Data
    SpectrumHandle I;
    Image image;
    const RGBColorSpace *imageColorSpace;
    WrapMode2D wrapMode;
    PiecewiseConstant2D distrib;
};

// DiffuseAreaLight Declarations
class DiffuseAreaLight : public LightBase {
  public:
    // DiffuseAreaLight Public Methods
    DiffuseAreaLight(const AnimatedTransform &worldFromLight,
                     const MediumInterface &mediumInterface, SpectrumHandle Le,
                     Float scale, const ShapeHandle shape, pstd::optional<Image> image,
                     const RGBColorSpace *imageColorSpace, bool twoSided,
                     Allocator alloc);

    static DiffuseAreaLight *Create(const AnimatedTransform &worldFromLight,
                                    MediumHandle medium,
                                    const ParameterDictionary &parameters,
                                    const RGBColorSpace *colorSpace, const FileLoc *loc,
                                    Allocator alloc, const ShapeHandle shape);

    void Preprocess(const Bounds3f &sceneBounds) {}

    PBRT_CPU_GPU
    SampledSpectrum L(const Interaction &intr, const Vector3f &w,
                      const SampledWavelengths &lambda) const {
        if (!twoSided && Dot(intr.n, w) < 0)
            return SampledSpectrum(0.f);

        if (image) {
            RGB rgb;
            for (int c = 0; c < 3; ++c)
                rgb[c] = image->BilerpChannel(intr.uv, c);
            return scale * RGBSpectrum(*imageColorSpace, rgb).Sample(lambda);
        } else
            return scale * Lemit.Sample(lambda);
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const {
        pstd::optional<ShapeSample> ss;
        Float pdf;

        if (worldFromLight.IsAnimated()) {
            // Will need to discuss this thoroughly in the book.
            // TODO: is there some nice abstraction we could add or
            // some way to make this cleaner?
            //
            // The issue is that animated shapes are given an identity
            // transform and for them world to shape is handled via
            // AnimatedPrimitive. This is important for performance, since it
            // means we can build a BVH over collections shapes with the
            // animated transformations and in turn, only interpolate the
            // matrix and transform the ray to object space once, rather than
            // once per tested shape.
            //
            // However, this means that adjustments need to be made to calls to
            // Shape::Sample and Shape::Pdf, which expect the given reference
            // point to be in world space. However, as far as animated Shapes
            // are concerned, world space == their object space, since they
            // were given the identity for their transform. So we need to
            // transform the reference point to the shape's object space,
            // sample, and then transform the returned interaction back to
            // world space. Yuck.
            Interaction refLight = worldFromLight.ApplyInverse(ref);
            ss = shape.Sample(refLight, u);
            if (!ss)
                return {};
            ss->intr = worldFromLight(ss->intr);
        } else {
            ss = shape.Sample(ref, u);
            if (!ss)
                return {};
        }
        ss->intr.time = ref.time;
        ss->intr.mediumInterface = &mediumInterface;
        DCHECK(!std::isnan(ss->pdf));

        if (ss->pdf == 0 || LengthSquared(ss->intr.p() - ref.p()) == 0)
            return {};

        Vector3f wi = Normalize(ss->intr.p() - ref.p());
        SampledSpectrum Le = L(ss->intr, -wi, lambda);
        if (!Le)
            return {};

        return LightLiSample(this, Le, wi, ss->pdf, ref, ss->intr);
    }

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &ref, const Vector3f &wi,
                 LightSamplingMode mode) const {
        if (worldFromLight.IsAnimated())
            return shape.PDF(worldFromLight.ApplyInverse(ref), wi);
        else
            return shape.PDF(ref, wi);
    }

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const;

    LightBounds Bounds() const;

    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for area lights");
    }

    std::string ToString() const;

  protected:
    // DiffuseAreaLight Protected Data
    SpectrumHandle Lemit;
    Float scale;
    ShapeHandle shape;
    bool twoSided;
    Float area;
    const RGBColorSpace *imageColorSpace;
    pstd::optional<Image> image;
};

// UniformInfiniteLight Declarations
class UniformInfiniteLight : public LightBase {
  public:
    // UniformInfiniteLight Public Methods
    UniformInfiniteLight(const AnimatedTransform &worldFromLight, SpectrumHandle Lemit,
                         Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {
        sceneBounds.BoundingSphere(&sceneCenter, &sceneRadius);
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    SampledSpectrum Le(const Ray &ray, const SampledWavelengths &lambda) const;
    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const;
    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    pstd::optional<LightBounds> Bounds() const { return {}; }

    std::string ToString() const;

  private:
    // UniformInfiniteLight Private Data
    SpectrumHandle Lemit;
    Point3f sceneCenter;
    Float sceneRadius;
};

class ImageInfiniteLight : public LightBase {
  public:
    // ImageInfiniteLight Public Methods
    ImageInfiniteLight(const AnimatedTransform &worldFromLight, Image image,
                       const RGBColorSpace *imageColorSpace, Float scale,
                       const std::string &imageFile, Allocator alloc);
    void Preprocess(const Bounds3f &sceneBounds) {
        sceneBounds.BoundingSphere(&sceneCenter, &sceneRadius);
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    SampledSpectrum Le(const Ray &ray, const SampledWavelengths &lambda) const {
        Vector3f wl = Normalize(worldFromLight.ApplyInverse(ray.d, ray.time));
        Point2f st = EquiAreaSphereToSquare(wl);
        return scale * lookupSpectrum(st, lambda);
    }

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const {
        // Find $(u,v)$ sample coordinates in infinite light texture
        Float mapPDF;
        Point2f uv = (mode == LightSamplingMode::WithMIS)
                         ? compensatedDistribution.Sample(u, &mapPDF)
                         : distribution.Sample(u, &mapPDF);
        if (mapPDF == 0)
            return {};

        // Convert infinite light sample point to direction
        Vector3f wl = EquiAreaSquareToSphere(uv);
        Vector3f wi = worldFromLight(wl, ref.time);

        // Compute PDF for sampled infinite light direction
        Float pdf = mapPDF / (4 * Pi);

        // Return radiance value for infinite light direction
        SampledSpectrum L = lookupSpectrum(uv, lambda);

        L *= scale;

        return LightLiSample(this, L, wi, pdf, ref,
                             Interaction(ref.p() + wi * (2 * sceneRadius), ref.time,
                                         &mediumInterface));
    }

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    pstd::optional<LightBounds> Bounds() const { return {}; }

    std::string ToString() const;

  private:
    PBRT_CPU_GPU
    SampledSpectrum lookupSpectrum(Point2f st, const SampledWavelengths &lambda) const {
        RGB rgb;
        for (int c = 0; c < 3; ++c)
            rgb[c] = image.LookupNearestChannel(st, c, wrapMode);
        return RGBSpectrum(*imageColorSpace, rgb).Sample(lambda);
    }

    // ImageInfiniteLight Private Data
    std::string imageFile;
    Image image;
    const RGBColorSpace *imageColorSpace;
    Float scale;
    WrapMode2D wrapMode;
    Point3f sceneCenter;
    Float sceneRadius;
    PiecewiseConstant2D distribution, compensatedDistribution;
};

class PortalImageInfiniteLight : public LightBase {
  public:
    PortalImageInfiniteLight(const AnimatedTransform &worldFromLight, Image image,
                             const RGBColorSpace *imageColorSpace, Float scale,
                             const std::string &imageFile, std::vector<Point3f> portal,
                             Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {
        sceneBounds.BoundingSphere(&sceneCenter, &sceneRadius);
    }

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    SampledSpectrum Le(const Ray &ray, const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const;

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    pstd::optional<LightBounds> Bounds() const { return {}; }

    std::string ToString() const;

  private:
    PBRT_CPU_GPU
    SampledSpectrum ImageLookup(const Point2f &st,
                                const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    Vector3f ImageToWorld(const Point2f &st, Float *duv_dw = nullptr) const {
        Float alpha = -Pi / 2 + st.x * Pi, beta = -Pi / 2 + st.y * Pi;
        Float x = std::tan(alpha), y = std::tan(beta);
        DCHECK(!std::isinf(x) && !std::isinf(y));
        Vector3f w = Normalize(Vector3f(x, y, -1));

        if (w.z == 0)
            w.z = 1e-5;
        if (duv_dw)
            *duv_dw = Pi * Pi * std::abs((1 - w.y * w.y) * (1 - w.x * w.x) / w.z);
        return portalFrame.FromLocal(w);
    }

    PBRT_CPU_GPU
    Point2f WorldToImage(const Vector3f &wWorld, Float *duv_dw = nullptr) const {
        Vector3f w = portalFrame.ToLocal(wWorld);
        if (w.z == 0)
            w.z = 1e-5;
        if (duv_dw)
            *duv_dw = Pi * Pi * std::abs((1 - w.y * w.y) * (1 - w.x * w.x) / w.z);

        Float alpha = std::atan(w.x / -w.z), beta = std::atan(w.y / -w.z);
        DCHECK(!std::isnan(alpha + beta));
        return Point2f(Clamp((alpha + Pi / 2) / Pi, 0, 1),
                       Clamp((beta + Pi / 2) / Pi, 0, 1));
    }

    PBRT_CPU_GPU
    Bounds2f ImageBounds(const Point3f &p) const {
        Point2f p0 = WorldToImage(Normalize(portal[0] - p));
        Point2f p1 = WorldToImage(Normalize(portal[2] - p));
        return Bounds2f(p0, p1);
    }

    PBRT_CPU_GPU
    Float Area() const {
        return Length(portal[1] - portal[0]) * Length(portal[3] - portal[0]);
    }

    std::string imageFile;
    Image image;
    const RGBColorSpace *imageColorSpace;
    Float scale;
    Frame portalFrame;
    pstd::array<Point3f, 4> portal;
    SATPiecewiseConstant2D distribution;
    Point3f sceneCenter;
    Float sceneRadius;
};

// SpotLight Declarations
class SpotLight : public LightBase {
  public:
    // SpotLight Public Methods
    SpotLight(const AnimatedTransform &worldFromLight, const MediumInterface &m,
              SpectrumHandle I, Float totalWidth, Float falloffStart, Allocator alloc);

    static SpotLight *Create(const AnimatedTransform &worldFromLight, MediumHandle medium,
                             const ParameterDictionary &parameters,
                             const RGBColorSpace *colorSpace, const FileLoc *loc,
                             Allocator alloc);

    void Preprocess(const Bounds3f &sceneBounds) {}

    PBRT_CPU_GPU
    pstd::optional<LightLiSample> Sample_Li(const Interaction &ref, const Point2f &u,
                                            const SampledWavelengths &lambda,
                                            LightSamplingMode mode) const;
    PBRT_CPU_GPU
    Float Falloff(const Vector3f &w) const;

    SampledSpectrum Phi(const SampledWavelengths &lambda) const;

    PBRT_CPU_GPU
    Float Pdf_Li(const Interaction &, const Vector3f &, LightSamplingMode mode) const;

    PBRT_CPU_GPU
    pstd::optional<LightLeSample> Sample_Le(const Point2f &u1, const Point2f &u2,
                                            const SampledWavelengths &lambda,
                                            Float time) const;
    PBRT_CPU_GPU
    void Pdf_Le(const Ray &, Float *pdfPos, Float *pdfDir) const;

    PBRT_CPU_GPU
    void Pdf_Le(const Interaction &, Vector3f &w, Float *pdfPos, Float *pdfDir) const {
        LOG_FATAL("Shouldn't be called for non-area lights");
    }

    LightBounds Bounds() const;

    std::string ToString() const;

  private:
    // SpotLight Private Data
    SpectrumHandle I;
    Float cosFalloffStart, cosFalloffEnd;
};

inline pstd::optional<LightLiSample> LightHandle::Sample_Li(
    const Interaction &ref, const Point2f &u, const SampledWavelengths &lambda,
    LightSamplingMode mode) const {
    auto sample = [&](auto ptr) { return ptr->Sample_Li(ref, u, lambda, mode); };
    return Apply<pstd::optional<LightLiSample>>(sample);
}

inline Float LightHandle::Pdf_Li(const Interaction &ref, const Vector3f &wi,
                                 LightSamplingMode mode) const {
    auto pdf = [&](auto ptr) { return ptr->Pdf_Li(ref, wi, mode); };
    return Apply<Float>(pdf);
}

inline SampledSpectrum LightHandle::L(const Interaction &intr, const Vector3f &w,
                                      const SampledWavelengths &lambda) const {
    CHECK(Type() == LightType::Area);
    auto l = [&](auto ptr) { return ptr->L(intr, w, lambda); };
    return Apply<SampledSpectrum>(l);
}

inline SampledSpectrum LightHandle::Le(const Ray &ray,
                                       const SampledWavelengths &lambda) const {
    auto le = [&](auto ptr) { return ptr->Le(ray, lambda); };
    return Apply<SampledSpectrum>(le);
}

inline LightType LightHandle::Type() const {
    auto t = [&](auto ptr) { return ptr->Type(); };
    return Apply<LightType>(t);
}

}  // namespace pbrt

#endif  // PBRT_LIGHTS_SPOT_H
