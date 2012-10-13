#ifndef MTAP_APPLESEED_H
#define MTAP_APPLESEED_H

#include "renderer/api/bsdf.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/material.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/image/image.h"
#include "foundation/image/tile.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/utility/containers/specializedarrays.h"
#include "foundation/utility/log/consolelogtarget.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/searchpaths.h"

// Standard headers.
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include <maya/MString.h>
#include <maya/MFnDependencyNode.h>

#include "mtap_tileCallback.h"
#include "mtap_mayaObject.h"

// shaderdefs

#define PHYSICAL_SURFACE_SHADER 0x00106EF4
#define AO_SHADER				0x00106EF5
#define WIREFRAME_SHADER		0x00106EF6
#define AOVOXEL_SHADER			0x00106EF7
#define FASTSSS_SHADER			0x00106EF8
#define CONST_SHADER			0x00106EF9
#define DIAGNOSTIC_SHADER		0x00106EFB
#define SMOKE_SHADER			0x00106EFC

class mtap_MayaScene;
class mtap_RenderGlobals;

namespace asf = foundation;
namespace asr = renderer;

namespace AppleRender
{
class AppleseedRenderer
{
public:
	AppleseedRenderer();
	~AppleseedRenderer();
	mtap_MayaScene *mtap_scene;
	mtap_RenderGlobals *renderGlobals;
	void writeXML();
	void defineProject();
	void defineConfig();
	void defineOutput();
	//void checkEntity(asr::con
	//void checkBSDF(asr::BSDFContainer& container, MString name);
	//void checkEDF(asr::BSDFContainer& container, MString name);
	//void checkColor(asr::BSDFContainer& container, MString name);
	void defineColor(MString& name, MColor& color, asr::Assembly *assembly = NULL, float intensity = 1.0f);
	void addDefaultMaterial();
	void defineDefaultMaterial();
	void addDefaultMaterial(asr::Assembly *assembly, asf::StringArray& materialNames);
	void defineObjectMaterial(mtap_RenderGlobals *renderGlobals, mtap_MayaObject *obj, asf::StringArray& materialNames);
	void defineTexture(MFnDependencyNode& shader, MString& attributeName, MString& textureDefinition);
	void defineDefaultLight();
	void defineLights(std::vector<MayaObject *>&);
	void defineLights();
	void defineScene(mtap_RenderGlobals *renderGlobals, std::vector<MayaObject *>&, std::vector<MayaObject *>&, std::vector<MayaObject *>&, std::vector<MayaObject *>&);
	bool initializeRenderer(mtap_RenderGlobals *renderGlobals, std::vector<MayaObject *>& objectList, std::vector<MayaObject *>& lightList, std::vector<MayaObject *>& camList);
	// geometry
	void defineGeometry(mtap_RenderGlobals *renderGlobals,  std::vector<MayaObject *>&);
	void defineGeometry();
	void definePreRender();
	void defineObject(mtap_MayaObject *obj);
	void defineMesh(mtap_MayaObject *obj);
	void defineMeshDeformStep(mtap_MayaObject *obj);
	void defineNurbsSurface(mtap_MayaObject *obj);
	void defineParticle(mtap_MayaObject *obj);
	void defineFluid(mtap_MayaObject *obj);

	void defineCamera(std::vector<MayaObject *>& cameraList, mtap_RenderGlobals *renderGlobals);
	void defineEnvironment(mtap_RenderGlobals *renderGlobals);
	asf::auto_release_ptr<asr::MeshObject> createMesh(MObject& meshObject);
	asr::MeshObject *createMeshPtr(MObject& meshObject);
	void render(mtap_RenderGlobals *renderGlobals);

private:
	asf::auto_release_ptr<asr::Project> project;
	asf::auto_release_ptr<asr::Assembly> masterAssembly;
	asf::auto_release_ptr<asr::Camera> camera;
	asf::auto_release_ptr<asr::Scene> scene;
	asf::auto_release_ptr<mtap_ITileCallbackFactory> tileCallbackFac;
	void MMatrixToAMatrix(MMatrix&, asf::Matrix4d&);
};

void eventListener();

} // namespace AppleRender

#endif