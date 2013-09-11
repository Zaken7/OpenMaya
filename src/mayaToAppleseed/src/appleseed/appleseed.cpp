#include "appleseed.h"
#include "mtap_tileCallback.h"

#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/texture.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/edf.h"

#include "renderer/modeling/environmentedf/sphericalcoordinates.h"

#include "maya/MFnDependencyNode.h"
#include "maya/MFnMesh.h"
#include "maya/MItMeshPolygon.h"
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnCamera.h>
#include <maya/MGlobal.h>
#include <maya/MRenderView.h>
#include <maya/MFileIO.h>
#include <maya/MItDag.h>
#include <maya/MFnTransform.h>

#include "utilities/tools.h"
#include "utilities/attrTools.h"
#include "threads/threads.h"
#include "threads/queue.h"
#include "utilities/pystring.h"
#include "../mtap_common/mtap_mayaScene.h"
#include "threads/renderQueueWorker.h"

#include <stdio.h>
#include <iostream>

#include "utilities/logging.h"

static Logging logger;

static int tileCount = 0;
static int tileCountTotal = 0;

using namespace AppleRender;

std::vector<asr::Entity *> definedEntities;

static AppleseedRenderer *appleRenderer = NULL;

AppleseedRenderer::AppleseedRenderer()
{
	this->mtap_scene = NULL;
	this->renderGlobals = NULL;
	definedEntities.clear();
	appleRenderer = this;
    this->project = asf::auto_release_ptr<asr::Project>(asr::ProjectFactory::create("mtap_project"));
	hasAOVs = false;
}

AppleseedRenderer::~AppleseedRenderer()
{
	logger.debug("Releasing scene");
	this->scene.reset();
	logger.debug("Releasing project");
	this->project.reset();
	logger.debug("Releasing done.");
	appleRenderer = NULL;
}

void AppleseedRenderer::defineProject()
{
	//TODO: the logging procedures of applesse causes a crash, no idea why.
	//asf::auto_release_ptr<asf::LogTargetBase> log_target(asf::create_console_log_target(stdout));
	//asr::global_logger().add_target(log_target.get());
	//asr::global_logger().add_target(log_target.release());

	//RENDERER_LOG_INFO("Testlog");

	//fprintf(stdout, "-------------DassnTest---------: %d:",123);
	//fflush(stdout);

	MString outputPath = this->renderGlobals->basePath + "/" + this->renderGlobals->imageName + ".appleseed";
	this->project->set_path(outputPath.asChar());
	this->scene = asr::SceneFactory::create();
	this->scenePtr = this->scene.get();
	this->project->set_scene(this->scene);

	definedEntities.clear();
}

void AppleseedRenderer::definePreRender()
{
	this->defineProject();
	this->defineConfig();
	clearShaderAssemblyAssignments();
}

void AppleseedRenderer::writeXML()
{
	logger.debug("AppleseedRenderer::writeXML");
	MString outputPath = this->renderGlobals->basePath + "/" + this->renderGlobals->imageName + "." + (int)this->renderGlobals->currentFrame + ".appleseed";

	asr::ProjectFileWriter::write(this->project.ref(), outputPath.asChar());
}


void AppleseedRenderer::defineOutput()
{
	logger.debug("AppleseedRenderer::defineOutput");
    // Create a frame and bind it to the project.
	MString res = MString("") + renderGlobals->imgWidth + " " + renderGlobals->imgHeight;
	MString colorSpaceString = renderGlobals->colorSpaceString;
	MString tileSize =  MString("") + renderGlobals->tilesize + " " + renderGlobals->tilesize;
	
    this->project->set_frame( 
        asr::FrameFactory::create(
            "beauty",
            asr::ParamArray()
                .insert("camera",this->scenePtr->get_camera()->get_name())
				.insert("resolution", res.asChar())
				.insert("tile_size", tileSize.asChar())
				.insert("color_space", colorSpaceString.asChar())));

}


void AppleseedRenderer::updateEntitiesCaller()
{
	logger.debug("AppleseedRenderer::updateEntities");
	if(appleRenderer != NULL)
		appleRenderer->updateEntities();
}



bool AppleseedRenderer::initializeRenderer(mtap_RenderGlobals *renderGlobals,  std::vector<MayaObject *>& objectList, std::vector<MayaObject *>& lightList, std::vector<MayaObject *>& camList)
{
	return true;
}

void AppleseedRenderer::defineMasterAssembly()
{
	this->masterAssembly = this->scenePtr->assemblies().get_by_name("world");
}

void AppleseedRenderer::defineScene(mtap_RenderGlobals *renderGlobals, std::vector<MayaObject *>& objectList, std::vector<MayaObject *>& lightList, std::vector<MayaObject *>& camList, std::vector<MayaObject *>& instancerList)
{
	logger.debug("AppleseedRenderer::defineScene");

	// camera has to be defined here because the next command defineOutput will need a camera
	this->updateCamera(true);
	this->defineOutput();
	this->defineEnvironment(renderGlobals);
}


void AppleseedRenderer::addDeformStep(mtap_MayaObject *obj, asr::Assembly *assembly)
{
	MString geoName = obj->fullNiceName;
	asr::Object *geoObject = assembly->objects().get_by_name(geoName.asChar());

	MStatus stat = MStatus::kSuccess;
	MFnMesh meshFn(obj->mobject, &stat);
	CHECK_MSTATUS(stat);
	if( !stat )
		return;
	MItMeshPolygon faceIt(obj->mobject, &stat);
	CHECK_MSTATUS(stat);
	if( !stat )
		return;

	MPointArray points;
	stat = meshFn.getPoints(points);
	CHECK_MSTATUS(stat);
    MFloatVectorArray normals;
    meshFn.getNormals( normals, MSpace::kWorld );

	logger.debug(MString("Defining deform step for mesh object ") + obj->fullNiceName);
	
	asr::MeshObject *meshShape = (asr::MeshObject *)geoObject;
	MString meshName = meshShape->get_name();
	int numMbSegments = (int)meshShape->get_motion_segment_count();
	logger.debug(MString("Mesh name ") + meshName + " has " + numMbSegments + " motion segments");

	// now adding one motion segment
	size_t mbSegmentIndex = meshShape->get_motion_segment_count() + 1;
	meshShape->set_motion_segment_count(mbSegmentIndex);

	for( uint vtxId = 0; vtxId < points.length(); vtxId++)
	{
		asr::GVector3 vtx((float)points[vtxId].x, (float)points[vtxId].y, (float)points[vtxId].z);
		meshShape->set_vertex_pose(vtxId, mbSegmentIndex - 1, vtx);
	}
}

//
//	Puts Object (only mesh at the moment) into this assembly.
//

void AppleseedRenderer::putObjectIntoAssembly(asr::Assembly *assembly, mtap_MayaObject *obj, MMatrix matrix)
{

	asf::StringArray material_names;
	// here the mesh per face assignments are read and placed into the obj->perFaceAssignments
	this->defineObjectMaterial(renderGlobals, obj, material_names);

	asf::auto_release_ptr<asr::MeshObject> mesh = this->createMesh(obj);
	MString meshName = mesh->get_name();
	assembly->objects().insert(asf::auto_release_ptr<asr::Object>(mesh));
	asr::Object *meshObject = assembly->objects().get_by_name(meshName.asChar());

	asf::Matrix4d tmatrix = asf::Matrix4d::identity();
	this->MMatrixToAMatrix(matrix, tmatrix);

	asf::StringDictionary matDict = asf::StringDictionary();
	asf::StringDictionary matBackDict = asf::StringDictionary();

	// if no material is attached, use a default material
	if( material_names.size() == 0)
	{
		material_names.push_back("gray_material");
		matDict.insert("default", "gray_material");
		matBackDict.insert("default", "gray_material");
	}else{
		int counterFront = 0;
		int counterBack = 0;
		for( size_t i = 0; i < material_names.size(); i++)
		{	
			if( pystring::endswith(material_names[i], "_back") )
				matBackDict.insert(MString(MString("slot_") + counterBack++).asChar(), material_names[i]);
			else
				matDict.insert(MString(MString("slot_") + counterFront++).asChar(), material_names[i]);
			
		}
	}

	bool doubleSided = true;
	MFnDependencyNode depFn(obj->mobject);
	getBool(MString("doubleSided"), depFn, doubleSided);

	asr::ParamArray meshInstanceParams;

	MString ray_bias_method = "none";
	int id;
	getEnum("mtap_ray_bias_method", depFn, id, ray_bias_method);
	if( ray_bias_method != "none")
	{
		float ray_bias_distance = 0.0f;
		getFloat("mtap_ray_bias_distance", depFn, ray_bias_distance);
		meshInstanceParams.insert("ray_bias_method", ray_bias_method.asChar());
		meshInstanceParams.insert("mtap_ray_bias_distance", ray_bias_distance);
	}

	if(doubleSided)
	{
		if( matBackDict.size() == 0)
			matBackDict = matDict;

		assembly->object_instances().insert(
				asr::ObjectInstanceFactory::create(
				(meshName + "_inst").asChar(),
				meshInstanceParams,
				meshObject->get_name(),
				asf::Transformd::from_local_to_parent(tmatrix),
				matDict,
				matBackDict
				));
	}else
		assembly->object_instances().insert(
				asr::ObjectInstanceFactory::create(
				(meshName + "_inst").asChar(),
				asr::ParamArray(),
				meshObject->get_name(),
				asf::Transformd::from_local_to_parent(tmatrix),
				matDict
				));
}
	

void AppleseedRenderer::render()
{
	logger.debug("AppleseedRenderer::render");

	bool isIpr = this->mtap_scene->renderType == MayaScene::IPR;

	this->tileCallbackFac.reset(new mtap_ITileCallbackFactory());

	masterRenderer = NULL;
	
	mtap_controller.entityUpdateProc = updateEntitiesCaller;

	if( this->mtap_scene->renderType == MayaScene::NORMAL)
	{
		masterRenderer = new asr::MasterRenderer(
			this->project.ref(),
			this->project->configurations().get_by_name("final")->get_inherited_parameters(),
			&mtap_controller,
			this->tileCallbackFac.get());
	}

	if( this->mtap_scene->renderType == MayaScene::IPR)
	{
		masterRenderer = new asr::MasterRenderer(
			this->project.ref(),
			this->project->configurations().get_by_name("interactive")->get_inherited_parameters(),
			&mtap_controller,
			this->tileCallbackFac.get());
	}
	
	
	//log_target.reset(asf::create_console_log_target(stderr));
    //asr::global_logger().add_target(log_target.get());

	m_log_target.reset(asf::create_file_log_target());
	m_log_target->open((this->renderGlobals->basePath + "/applelog.log").asChar() );
	asr::global_logger().add_target(m_log_target.get());

	logger.debug("------------- start rendering ----------------------");

	masterRenderer->render();

	//asr::global_logger().remove_target(log_target.get());
	asr::global_logger().remove_target(m_log_target.get());
	m_log_target->close();

	this->renderGlobals->getImageName();
	logger.debug(MString("Writing image: ") + renderGlobals->imageOutputFile);
	MString imageOutputFile =  renderGlobals->imageOutputFile;
	project->get_frame()->write_main_image(imageOutputFile.asChar());
	project->get_frame()->write_aov_images(imageOutputFile.asChar());
	EventQueue::Event e;
	e.data = NULL;
	e.type = EventQueue::Event::FRAMEDONE;
	theRenderEventQueue()->push(e);
}
