
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

#ifndef PBRT_CORE_PARAMSET_H
#define PBRT_CORE_PARAMSET_H

// core/paramset.h*
#include "pbrt.h"

#include "ext/google/array_slice.h"
#include "geometry.h"
#include "spectrum.h"
#include "texture.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace pbrt {

template <typename T>
struct ParamSetItem {
    // ParamSetItem Public Methods
    ParamSetItem(const std::string &name, std::unique_ptr<T[]> values,
                 size_t nValues = 1)
        : name(name), values(std::move(values)), nValues(nValues) {}

    // ParamSetItem Data
    const std::string name;
    std::unique_ptr<T[]> values;
    size_t nValues;
    mutable bool lookedUp = false;
};

// ParamSet Declarations
class ParamSet {
  public:
    // ParamSet Public Methods
    void AddFloat(const std::string &, std::unique_ptr<Float[]> v,
                  int nValues = 1);
    void AddInt(const std::string &, std::unique_ptr<int[]> v, int nValues);
    void AddBool(const std::string &, std::unique_ptr<bool[]> v, int nValues);
    void AddPoint2f(const std::string &, std::unique_ptr<Point2f[]> v,
                    int nValues);
    void AddVector2f(const std::string &, std::unique_ptr<Vector2f[]> v,
                     int nValues);
    void AddPoint3f(const std::string &, std::unique_ptr<Point3f[]> v,
                    int nValues);
    void AddVector3f(const std::string &, std::unique_ptr<Vector3f[]> v,
                     int nValues);
    void AddNormal3f(const std::string &, std::unique_ptr<Normal3f[]> v,
                     int nValues);
    void AddString(const std::string &, std::unique_ptr<std::string[]> v,
                   int nValues);
    void AddTexture(const std::string &, const std::string &);
    void AddSpectrum(const std::string &, std::unique_ptr<Spectrum[]> v,
                     int nValues);

    Float GetOneFloat(const std::string &name, Float def) const;
    int GetOneInt(const std::string &name, int def) const;
    bool GetOneBool(const std::string &name, bool def) const;
    Point2f GetOnePoint2f(const std::string &name, const Point2f &def) const;
    Vector2f GetOneVector2f(const std::string &name, const Vector2f &def) const;
    Point3f GetOnePoint3f(const std::string &name, const Point3f &def) const;
    Vector3f GetOneVector3f(const std::string &name, const Vector3f &def) const;
    Normal3f GetOneNormal3f(const std::string &name, const Normal3f &def) const;
    Spectrum GetOneSpectrum(const std::string &name, const Spectrum &def) const;
    std::string GetOneString(const std::string &name,
                             const std::string &def) const;
    std::string GetOneFilename(const std::string &name,
                               const std::string &def) const;
    std::string FindTexture(const std::string &) const;

    gtl::ArraySlice<Float> GetFloatArray(const std::string &name) const;
    gtl::ArraySlice<int> GetIntArray(const std::string &name) const;
    gtl::ArraySlice<bool> GetBoolArray(const std::string &name) const;
    gtl::ArraySlice<Point2f> GetPoint2fArray(const std::string &name) const;
    gtl::ArraySlice<Vector2f> GetVector2fArray(const std::string &name) const;
    gtl::ArraySlice<Point3f> GetPoint3fArray(const std::string &name) const;
    gtl::ArraySlice<Vector3f> GetVector3fArray(const std::string &name) const;
    gtl::ArraySlice<Normal3f> GetNormal3fArray(const std::string &name) const;
    gtl::ArraySlice<Spectrum> GetSpectrumArray(const std::string &name) const;
    gtl::ArraySlice<std::string> GetStringArray(const std::string &name) const;

    void ReportUnused() const;

    std::string ToString(int indent = 0) const;

  private:
    // ParamSet Private Data
    std::vector<ParamSetItem<bool>> bools;
    std::vector<ParamSetItem<int>> ints;
    std::vector<ParamSetItem<Float>> floats;
    std::vector<ParamSetItem<Point2f>> point2fs;
    std::vector<ParamSetItem<Vector2f>> vector2fs;
    std::vector<ParamSetItem<Point3f>> point3fs;
    std::vector<ParamSetItem<Vector3f>> vector3fs;
    std::vector<ParamSetItem<Normal3f>> normals;
    std::vector<ParamSetItem<Spectrum>> spectra;
    std::vector<ParamSetItem<std::string>> strings;
    std::vector<ParamSetItem<std::string>> textures;
};

// TextureParams Declarations
class TextureParams : public ParamSet {
  public:
    // TextureParams Public Methods
    TextureParams(
        ParamSet params,
        std::map<std::string, std::shared_ptr<Texture<Float>>> &fTex,
        std::map<std::string, std::shared_ptr<Texture<Spectrum>>> &sTex)
        : ParamSet(std::move(params)),
          floatTextures(fTex),
          spectrumTextures(sTex) {}

    std::shared_ptr<Texture<Spectrum>> GetSpectrumTexture(
        const std::string &name, const Spectrum &def) const;
    std::shared_ptr<Texture<Spectrum>> GetSpectrumTextureOrNull(
        const std::string &name) const;
    std::shared_ptr<Texture<Float>> GetFloatTexture(const std::string &name,
                                                    Float def) const;
    std::shared_ptr<Texture<Float>> GetFloatTextureOrNull(
        const std::string &name) const;

  private:
    // TextureParams Private Data
    std::map<std::string, std::shared_ptr<Texture<Float>>> &floatTextures;
    std::map<std::string, std::shared_ptr<Texture<Spectrum>>> &spectrumTextures;
};

}  // namespace pbrt

#endif  // PBRT_CORE_PARAMSET_H
