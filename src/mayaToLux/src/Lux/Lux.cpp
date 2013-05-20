#include "Lux.h"
#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <maya/MStringArray.h>

#include "threads/renderQueueWorker.h"
#include "utilities/tools.h"

#include "../mtlu_common/mtlu_renderGlobals.h"
#include "../mtlu_common/mtlu_mayaScene.h"

#include "utilities/logging.h"
static Logging logger;

bool LuxRenderer::isRendering;


// create a new instance with proper destruction
Instance CreateInstance(const std::string name) {
    return Instance(CreateLuxInstance(name.c_str()), DestroyLuxInstance);
}

// create a new paramset with proper destruction
ParamSet CreateParamSet() {
    return ParamSet(CreateLuxParamSet(), DestroyLuxParamSet);
}

LuxRenderer::LuxRenderer()
{}

LuxRenderer::~LuxRenderer()
{}

void LuxRenderer::initializeRenderer()
{}

void LuxRenderer::updateShape(MayaObject *obj)
{}

void LuxRenderer::updateTransform(MayaObject *obj)
{}

void LuxRenderer::IPRUpdateEntities()
{}

void LuxRenderer::reinitializeIPRRendering()
{}

void LuxRenderer::abortRendering()
{
	this->lux->abort();
}

void LuxRenderer::getMeshPoints(MPointArray& pointArray)
{
	for( int i = 0; i < 500; i++)
	{
		MPoint p;
		float radius = 20.0f * (float)rand()/(float)RAND_MAX;
		MVector v;
		v.x = ((float)rand()/(float)RAND_MAX - 0.5f) * 2.0;
		v.y = ((float)rand()/(float)RAND_MAX - 0.5f) * 2.0;
		v.z = ((float)rand()/(float)RAND_MAX - 0.5f) * 2.0;
		v.normalize();
		p.x =  v.x * radius;
		p.y =  v.y * radius;
		p.z =  v.z * radius;

		pointArray.append(p);
	}
}


void LuxRenderer::getFramebufferThread( void *pointer)
{
	LuxRenderer *luxRenderer = (LuxRenderer*)pointer;
	
	//Context::UpdateFramebuffer

	int width = luxRenderer->mtlu_renderGlobals->imgWidth;
	int height = luxRenderer->mtlu_renderGlobals->imgHeight;
	EventQueue::Event e;

	while( isRendering )
	{
		//logger.debug("Update framebuffer...");

		luxRenderer->lux->updateFramebuffer();

		const unsigned char *fb = luxRenderer->lux->framebuffer();
		const float *ffb = luxRenderer->lux->floatFramebuffer();
		const float *fa = luxRenderer->lux->alphaBuffer();
		e.data = NULL;
		size_t numPixels = width * height;
		RV_PIXEL* pixels = new RV_PIXEL[numPixels];
		unsigned int mayaPixel = 0;

		if( fb != NULL )
		{
			for( unsigned int y = height-1; y > 0; y--)
			{
				for( unsigned int x = 0; x < width; x++)
				{
					unsigned int index = y * width + x;
					pixels[mayaPixel].r = fb[index * 3];
					pixels[mayaPixel].g = fb[index * 3 + 1];
					pixels[mayaPixel].b = fb[index * 3 + 2];
					pixels[mayaPixel].a = 0.0;
					if( fa )
						pixels[mayaPixel].a = fa[index] * 255.0f;
					mayaPixel++;
				}
			}
		}else{
			logger.debug("framebuffer == NULL");
		}
		
		e.tile_xmin = e.tile_ymin = 0;
		e.tile_xmax = width - 1;
		e.tile_ymax = height - 1;
		e.data = pixels;
		e.type = EventQueue::Event::FRAMEUPDATE;
		theRenderEventQueue()->push(e);

		boost::this_thread::sleep(boost::posix_time::milliseconds(luxRenderer->mtlu_renderGlobals->uiupdateinterval * 1000));

	}

	logger.debug("Update framebuffer thread done.");

}

void LuxRenderer::defineSampling()
{
	const char *samplerNames[] = {"random", "lowdiscrepancy", "metropolis"};
	MStringArray samplers(samplerNames, 3);
	ParamSet samplerParams = CreateParamSet();
	samplerParams->AddBool("noiseaware", &this->mtlu_renderGlobals->noiseaware);

	if( (samplers[this->mtlu_renderGlobals->sampler] == "random") || (samplers[this->mtlu_renderGlobals->sampler] == "lowdiscrepancy"))
	{
		samplerParams->AddInt("pixelsamples", &this->mtlu_renderGlobals->numSamples);
		const char *pixelSamplerNames[] = {"hilbert", "linear", "vegas", "lowdiscrepancy", "tile", "random"};
		samplerParams->AddString("pixelsampler", &pixelSamplerNames[this->mtlu_renderGlobals->pixelSampler]);
	}
	if( (samplers[this->mtlu_renderGlobals->sampler] == "metropolis"))
	{
		samplerParams->AddInt("maxconsecrejects", &this->mtlu_renderGlobals->maxconsecrejects);
		samplerParams->AddFloat("largemutationprob", &this->mtlu_renderGlobals->largemutationprob);
		samplerParams->AddFloat("mutationrange", &this->mtlu_renderGlobals->mutationRange);
		samplerParams->AddBool("usevariance", &this->mtlu_renderGlobals->usevariance);
		samplerParams->AddBool("usecooldown", &this->mtlu_renderGlobals->usecooldown);
	}

	this->lux->sampler(samplerNames[this->mtlu_renderGlobals->sampler], boost::get_pointer(samplerParams));
}

void LuxRenderer::defineRenderer()
{
	const char *rendererNames[] = {"sampler", "hybrid", "sppm"};
	ParamSet rendererParams = CreateParamSet();
	this->lux->renderer(rendererNames[this->mtlu_renderGlobals->renderer], boost::get_pointer(rendererParams));
}

void LuxRenderer::definePixelFilter()
{
	const char *filterNames[] = {"box", "triangle", "gaussian", "mitchell", "sinc"};
	MStringArray filters(filterNames, 5);
	ParamSet filterParams = CreateParamSet();
	logger.debug(MString("Define pixel filter: ") + filters[this->mtlu_renderGlobals->pixelfilter]);
	filterParams->AddFloat("xwidth", &this->mtlu_renderGlobals->filterWidth);
	filterParams->AddFloat("ywidth", &this->mtlu_renderGlobals->filterHeight);
	logger.debug(MString("Define pixel filter: ") + filters[this->mtlu_renderGlobals->pixelfilter] + " width: " + this->mtlu_renderGlobals->filterWidth);
	if( (filters[this->mtlu_renderGlobals->pixelfilter] == "gaussian"))
	{
		filterParams->AddFloat("alpha", &this->mtlu_renderGlobals->filterAlpha);
	}
	if( (filters[this->mtlu_renderGlobals->pixelfilter] == "mitchell"))
	{
		filterParams->AddFloat("B", &this->mtlu_renderGlobals->B);
		filterParams->AddFloat("C", &this->mtlu_renderGlobals->C);
		filterParams->AddBool("supersample", &this->mtlu_renderGlobals->mSupersample);
	}
	if( (filters[this->mtlu_renderGlobals->pixelfilter] == "sinc"))
	{
		filterParams->AddFloat("tau", &this->mtlu_renderGlobals->sincTau);
	}
	this->lux->pixelFilter(filterNames[this->mtlu_renderGlobals->pixelfilter], boost::get_pointer(filterParams));
}

void LuxRenderer::defineSurfaceIntegrator()
{
	const char *integratorNames[] = {"bidirectional", "path", "exphotonmap", "directlighting", "igi", "distributedpath", "sppm"}; 
	MStringArray integrators(integratorNames, 7);
	ParamSet iParams = CreateParamSet();
	
	if( integrators[this->mtlu_renderGlobals->surfaceIntegrator] != "sppm")
	{
		const char *lStrategies[]= {"one","all","auto","importance","powerimp","allpowerimp","logpowerimp"};
		iParams->AddString("lightstrategy", &lStrategies[this->mtlu_renderGlobals->lightStrategy]);
		iParams->AddInt("shadowraycount", &this->mtlu_renderGlobals->shadowraycount);
	}

	// Bidirectional - hybrid or sampler only
	if(this->mtlu_renderGlobals->renderer < 2)
	{
		if( integrators[this->mtlu_renderGlobals->surfaceIntegrator] == "bidirectional")
		{
			const char *lStrategies[]= {"one","all","auto","importance","powerimp","allpowerimp","logpowerimp"};
			const char *ls = lStrategies[this->mtlu_renderGlobals->lightStrategy];
			// hybrid requires "one"
			if( this->mtlu_renderGlobals->renderer == 1 )
				ls = lStrategies[0];
			iParams->AddString("lightpathstrategy", &ls);
			iParams->AddInt("eyedepth", &this->mtlu_renderGlobals->eyedepth);
			iParams->AddFloat("lightrrthreshold",&this->mtlu_renderGlobals->lightrrthreshold);
			iParams->AddFloat("eyerrthreshold",&this->mtlu_renderGlobals->eyerrthreshold);
		}
		if( integrators[this->mtlu_renderGlobals->surfaceIntegrator] == "path")
		{
			iParams->AddInt("maxdepth", &this->mtlu_renderGlobals->pathMaxdepth);
			iParams->AddBool("includeenvironment", &this->mtlu_renderGlobals->includeenvironment);
			iParams->AddBool("directlightsampling", &this->mtlu_renderGlobals->directlightsampling);
			const char *rrstrategys[] = {"none", "probability", "efficiency"};
			const char *rrstrategy = rrstrategys[this->mtlu_renderGlobals->rrstrategy];
			iParams->AddString("rrstrategy", &rrstrategy);	
			iParams->AddFloat("rrcontinueprob", &this->mtlu_renderGlobals->rrcontinueprob);
		}
	}

	// direct - sampler only
	if(this->mtlu_renderGlobals->renderer == 0)
	{
		if( integrators[this->mtlu_renderGlobals->surfaceIntegrator] == "direct")
		{
			iParams->AddInt("maxdepth", &this->mtlu_renderGlobals->pathMaxdepth);
			const char *rrstrategys[] = {"none", "probability", "efficiency"};
			const char *rrstrategy = rrstrategys[this->mtlu_renderGlobals->rrstrategy];
			iParams->AddString("rrstrategy", &rrstrategy);						
		}
	}

	// sppm only
	if(this->mtlu_renderGlobals->renderer == 2)
	{
		if( integrators[this->mtlu_renderGlobals->surfaceIntegrator] == "sppm")
		{
			const char *photonsamplers[]= {"halton", "amc"};
			//const char *photonsampler= photonsamplers[this->mtlu_renderGlobals->photonSampler];
			//photonsampler
			//lookupaccel
			//parallelhashgridspare
			//pixelsampler
			//maxeyedepth
			//maxphotondepth
			//photonperpass
			//startradius
			//alpha
			//includeenvironment
			//directlightsampling
			//useproba
			//wavelengthstratification
			//debug
			//storeglossy
		}
	}

	this->lux->surfaceIntegrator(integratorNames[this->mtlu_renderGlobals->surfaceIntegrator], boost::get_pointer(iParams));
}


void LuxRenderer::render()
{
	logger.debug(MString("Render lux."));
	EventQueue::Event e;

	int width = this->mtlu_renderGlobals->imgWidth;
	int height = this->mtlu_renderGlobals->imgHeight;

	MPointArray pointArray;
	getMeshPoints(pointArray);
	isRendering = false;

	MString outputPath = this->mtlu_renderGlobals->basePath + "/" + this->mtlu_renderGlobals->imageName + "." + (int)this->mtlu_renderGlobals->currentFrame + ".lxs";
	logger.debug(MString("Writing scene file to: ") + outputPath);
	luxFile.open(outputPath.asChar());

    try {

		float fov=30;
		std::string filename = "cpp_api_test";

		lux = CreateInstance("cpp_api_test");

		this->defineCamera();

		this->defineFilm();

		this->defineSampling();

		this->defineSurfaceIntegrator();

		this->defineRenderer();

		this->definePixelFilter();

		lux->worldBegin();
		if( this->mtlu_renderGlobals->exportSceneFile)
			this->luxFile << "WorldBegin\n";

		this->defineShaders();

		this->defineLights();

		this->defineGeometry();

		lux->worldEnd();
		if( this->mtlu_renderGlobals->exportSceneFile)
			this->luxFile << "WorldEnd\n";

		luxFile.close();

		isRendering = true;

		// wait for the WorldEnd thread to start running
		// this isn't terribly reliable, cpp_api should be modified
		boost::this_thread::sleep(boost::posix_time::seconds(1));
		boost::thread(LuxRenderer::getFramebufferThread, this);

		for( int tid = 0; tid < this->mtlu_renderGlobals->threads; tid++)
			lux->addThread();

		lux->wait();

		isRendering = false;
		const unsigned char *fb = lux->framebuffer();
		const float *ffb = lux->floatFramebuffer();
		const float *fa = lux->alphaBuffer();
		e.data = NULL;
		size_t numPixels = width * height;
		RV_PIXEL* pixels = new RV_PIXEL[numPixels];
		unsigned int mayaPixel = 0;

		if( fb != NULL )
		{
			for( unsigned int y = height-1; y > 0; y--)
			{
				for( unsigned int x = 0; x < width; x++)
				{
					unsigned int index = y * width + x;
					pixels[mayaPixel].r = fb[index * 3];
					pixels[mayaPixel].g = fb[index * 3 + 1];
					pixels[mayaPixel].b = fb[index * 3 + 2];
					pixels[mayaPixel].a = 0.0;
					if( fa )
						pixels[mayaPixel].a = fa[index] * 255.0f;
					mayaPixel++;
				}
			}
		}
		
		e.tile_xmin = e.tile_ymin = 0;
		e.tile_xmax = width - 1;
		e.tile_ymax = height - 1;
		e.data = pixels;

		// saveFLM needs extension
		filename = filename + ".flm";
		lux->saveFLM(filename.c_str());

		logger.debug(MString("Render lux."));

		lux->cleanup();

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }

	e.type = EventQueue::Event::FRAMEDONE;
	theRenderEventQueue()->push(e);
}

