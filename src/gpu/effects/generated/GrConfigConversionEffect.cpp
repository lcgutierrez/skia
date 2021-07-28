/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrConfigConversionEffect.fp; do not modify.
 **************************************************************************************************/
#include "GrConfigConversionEffect.h"

#include "src/core/SkUtils.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLConfigConversionEffect : public GrGLSLFragmentProcessor {
public:
    GrGLSLConfigConversionEffect() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrConfigConversionEffect& _outer = args.fFp.cast<GrConfigConversionEffect>();
        (void)_outer;
        auto pmConversion = _outer.pmConversion;
        (void)pmConversion;

        fragBuilder->forceHighPrecision();
        SkString _sample0 = this->invokeChild(0, args);
        fragBuilder->codeAppendf(
                R"SkSL(half4 color = floor(%s * 255.0 + 0.5) / 255.0;
@switch (%d) {
    case 0:
        color.xyz = floor((color.xyz * color.w) * 255.0 + 0.5) / 255.0;
        break;
    case 1:
        color.xyz = color.w <= 0.0 ? half3(0.0) : floor((color.xyz / color.w) * 255.0 + 0.5) / 255.0;
        break;
}
return color;
)SkSL",
                _sample0.c_str(), (int)_outer.pmConversion);
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {}
};
std::unique_ptr<GrGLSLFragmentProcessor> GrConfigConversionEffect::onMakeProgramImpl() const {
    return std::make_unique<GrGLSLConfigConversionEffect>();
}
void GrConfigConversionEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                     GrProcessorKeyBuilder* b) const {
    b->add32((uint32_t)pmConversion);
}
bool GrConfigConversionEffect::onIsEqual(const GrFragmentProcessor& other) const {
    const GrConfigConversionEffect& that = other.cast<GrConfigConversionEffect>();
    (void)that;
    if (pmConversion != that.pmConversion) return false;
    return true;
}
GrConfigConversionEffect::GrConfigConversionEffect(const GrConfigConversionEffect& src)
        : INHERITED(kGrConfigConversionEffect_ClassID, src.optimizationFlags())
        , pmConversion(src.pmConversion) {
    this->cloneAndRegisterAllChildProcessors(src);
}
std::unique_ptr<GrFragmentProcessor> GrConfigConversionEffect::clone() const {
    return std::make_unique<GrConfigConversionEffect>(*this);
}
#if GR_TEST_UTILS
SkString GrConfigConversionEffect::onDumpInfo() const {
    return SkStringPrintf("(pmConversion=%d)", (int)pmConversion);
}
#endif
GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrConfigConversionEffect);
#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrConfigConversionEffect::TestCreate(
        GrProcessorTestData* data) {
    PMConversion pmConv = static_cast<PMConversion>(
            data->fRandom->nextULessThan((int)PMConversion::kPMConversionCnt));
    return std::unique_ptr<GrFragmentProcessor>(
            new GrConfigConversionEffect(GrProcessorUnitTest::MakeChildFP(data), pmConv));
}
#endif

bool GrConfigConversionEffect::TestForPreservingPMConversions(GrDirectContext* dContext) {
    static constexpr int kSize = 256;
    SkAutoTMalloc<uint32_t> data(kSize * kSize * 3);
    uint32_t* srcData = data.get();

    // Fill with every possible premultiplied A, color channel value. There will be 256-y
    // duplicate values in row y. We set r, g, and b to the same value since they are handled
    // identically.
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            uint8_t* color = reinterpret_cast<uint8_t*>(&srcData[kSize * y + x]);
            color[3] = y;
            color[2] = std::min(x, y);
            color[1] = std::min(x, y);
            color[0] = std::min(x, y);
        }
    }

    const SkImageInfo pmII =
            SkImageInfo::Make(kSize, kSize, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    const SkImageInfo upmII = pmII.makeAlphaType(kUnpremul_SkAlphaType);

    auto readSFC = GrSurfaceFillContext::Make(dContext, upmII, SkBackingFit::kExact);
    auto tempSFC = GrSurfaceFillContext::Make(dContext, pmII, SkBackingFit::kExact);
    if (!readSFC || !tempSFC) {
        return false;
    }

    // This function is only ever called if we are in a GrDirectContext since we are
    // calling read pixels here. Thus the pixel data will be uploaded immediately and we don't
    // need to keep the pixel data alive in the proxy. Therefore the ReleaseProc is nullptr.
    SkBitmap bitmap;
    bitmap.installPixels(pmII, srcData, 4 * kSize);
    bitmap.setImmutable();

    GrBitmapTextureMaker maker(dContext, bitmap, GrImageTexGenPolicy::kNew_Uncached_Budgeted);
    auto dataView = maker.view(GrMipmapped::kNo);
    if (!dataView) {
        return false;
    }

    uint32_t* firstRead = data.get() + kSize * kSize;
    uint32_t* secondRead = data.get() + 2 * kSize * kSize;
    std::fill_n(firstRead, kSize * kSize, 0);
    std::fill_n(secondRead, kSize * kSize, 0);

    GrPixmap firstReadPM(upmII, firstRead, kSize * sizeof(uint32_t));
    GrPixmap secondReadPM(upmII, secondRead, kSize * sizeof(uint32_t));

    // We do a PM->UPM draw from dataTex to readTex and read the data. Then we do a UPM->PM draw
    // from readTex to tempTex followed by a PM->UPM draw to readTex and finally read the data.
    // We then verify that two reads produced the same values.

    auto fp1 = GrConfigConversionEffect::Make(
            GrTextureEffect::Make(std::move(dataView), bitmap.alphaType()),
            PMConversion::kToUnpremul);
    readSFC->fillRectWithFP(SkIRect::MakeWH(kSize, kSize), std::move(fp1));
    if (!readSFC->readPixels(dContext, firstReadPM, {0, 0})) {
        return false;
    }

    auto fp2 = GrConfigConversionEffect::Make(
            GrTextureEffect::Make(readSFC->readSurfaceView(), readSFC->colorInfo().alphaType()),
            PMConversion::kToPremul);
    tempSFC->fillRectWithFP(SkIRect::MakeWH(kSize, kSize), std::move(fp2));

    auto fp3 = GrConfigConversionEffect::Make(
            GrTextureEffect::Make(tempSFC->readSurfaceView(), tempSFC->colorInfo().alphaType()),
            PMConversion::kToUnpremul);
    readSFC->fillRectWithFP(SkIRect::MakeWH(kSize, kSize), std::move(fp3));

    if (!readSFC->readPixels(dContext, secondReadPM, {0, 0})) {
        return false;
    }

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x <= y; ++x) {
            if (firstRead[kSize * y + x] != secondRead[kSize * y + x]) {
                return false;
            }
        }
    }

    return true;
}