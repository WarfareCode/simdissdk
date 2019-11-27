/* -*- mode: c++ -*- */
/****************************************************************************
 *****                                                                  *****
 *****                   Classification: UNCLASSIFIED                   *****
 *****                    Classified By:                                *****
 *****                    Declassify On:                                *****
 *****                                                                  *****
 ****************************************************************************
 *
 *
 * Developed by: Naval Research Laboratory, Tactical Electronic Warfare Div.
 *               EW Modeling & Simulation, Code 5773
 *               4555 Overlook Ave.
 *               Washington, D.C. 20375-5339
 *
 * License for source code at https://simdis.nrl.navy.mil/License.aspx
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */
#include "osgEarth/VirtualProgram"
#include "osgEarth/ShaderLoader"
#include "simNotify/Notify.h"
#include "simVis/osgEarthVersion.h"
#include "simVis/Shaders.h"
#include "simVis/BathymetryGenerator.h"

// ModifyTileBoundingBoxCallback was added post-2.10, requiring osgEarth 0fb5647
#if SDK_OSGEARTH_MIN_VERSION_REQUIRED(1, 10, 0)
#define HAS_MODIFYTILEBOUNDINGBOXCALLBACK 1
#else
/* #undef HAS_MODIFYTILEBOUNDINGBOXCALLBACK */
#endif

namespace simVis {

static const char* SEA_LEVEL_UNIFORM = "simVis_BathymetryGenerator_seaLevel";
static const char* OFFSET_UNIFORM = "simVis_BathymetryGenerator_offset";

/**
 * Callback assigned to the terrain engine that is responsible for adjusting the bounding
 * box of the tiles based on the current bathymetry offset.  The bounding box needs to be
 * expanded in order to prevent a problem where tiles that SHOULD be drawn are NOT drawn
 * because they would otherwise be outside the current viewing frustum in their normal,
 * non-adjusted position at altitude 0.f.
 *
 * The ModifyTileBoundingBoxCallback was added as part of osgEarth 0fb5647 and is only
 * available in the newest versions of osgEarth.
 */
class BathymetryGenerator::AlterTileBBoxCB
#ifdef HAS_MODIFYTILEBOUNDINGBOXCALLBACK
  : public osgEarth::TerrainEngineNode::ModifyTileBoundingBoxCallback
#else
  : public osg::Referenced
#endif
{
public:
  /** Constructor initializes the offset */
  AlterTileBBoxCB()
    : offset_(0.f)
  {
  }

  /** From ModifyTileBoundingBoxCallback, increase bounding box size by the bathymetry offset */
  virtual void modifyBoundingBox(const osgEarth::TileKey& key, osg::BoundingBox& box) const
  {
    box.zMin() += offset_;
  }

  /** Change the offset in meters, expanding tile bounding box */
  void setOffset(float offset)
  {
    offset_ = offset;
  }

protected:
  /** Protect osg::Referenced destructor */
  virtual ~AlterTileBBoxCB()
  {
  }

private:
  float offset_;
};

//////////////////////////////////////////////////////

BathymetryGenerator::BathymetryGenerator()
{
  const float defaultOffset = -75.0f;
  seaLevelUniform_ = new osg::Uniform(SEA_LEVEL_UNIFORM, 0.1f);
  offsetUniform_ = new osg::Uniform(OFFSET_UNIFORM, defaultOffset);
#ifdef HAS_MODIFYTILEBOUNDINGBOXCALLBACK
  alterTileBBoxCB_ = new AlterTileBBoxCB();
  alterTileBBoxCB_->setOffset(defaultOffset);
#endif
}

BathymetryGenerator::~BathymetryGenerator()
{
}

void BathymetryGenerator::onInstall(osgEarth::TerrainEngineNode* engine)
{
  if (engine)
  {
    osg::StateSet* stateSet = engine->getOrCreateStateSet();
    osgEarth::VirtualProgram* vp = osgEarth::VirtualProgram::getOrCreate(stateSet);

    // bring in our local shaders:
    simVis::Shaders shaders;
    shaders.load(vp, shaders.bathymetryGeneratorVertex());

    stateSet->addUniform(seaLevelUniform_.get());
    stateSet->addUniform(offsetUniform_.get());

#ifdef HAS_MODIFYTILEBOUNDINGBOXCALLBACK
    engine->addModifyTileBoundingBoxCallback(alterTileBBoxCB_.get());
#endif
  }
}

void BathymetryGenerator::onUninstall(osgEarth::TerrainEngineNode* engine)
{
  if (engine)
  {
#ifdef HAS_MODIFYTILEBOUNDINGBOXCALLBACK
    engine->removeModifyTileBoundingBoxCallback(alterTileBBoxCB_.get());
#endif

    osg::StateSet* stateSet = engine->getStateSet();
    if (stateSet)
    {
      osgEarth::VirtualProgram* vp = osgEarth::VirtualProgram::get(stateSet);

      // remove shader functions:
      simVis::Shaders shaders;
      shaders.unload(vp, shaders.bathymetryGeneratorVertex());

      // no need to uninstall the terrain SDK; it is harmless

      // remove uniforms.
      stateSet->removeUniform(seaLevelUniform_.get());
      stateSet->removeUniform(offsetUniform_.get());
    }
  }
}

void BathymetryGenerator::setSeaLevelElevation(float value)
{
  seaLevelUniform_->set(value);
}

float BathymetryGenerator::getSeaLevelElevation() const
{
  float value;
  seaLevelUniform_->get(value);
  return value;
}

void BathymetryGenerator::setOffset(float value)
{
  offsetUniform_->set(value);
  alterTileBBoxCB_->setOffset(value);
}

float BathymetryGenerator::getOffset() const
{
  float value;
  offsetUniform_->get(value);
  return value;
}

}
