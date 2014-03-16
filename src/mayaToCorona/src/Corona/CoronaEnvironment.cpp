#include "Corona.h"
#include "../mtco_common/mtco_renderGlobals.h"
#include "../mtco_common/mtco_mayaScene.h"
#include "../mtco_common/mtco_mayaObject.h"
#include "utilities/logging.h"
#include "threads/renderQueueWorker.h"
#include "utilities/tools.h"
#include "utilities/attrTools.h"

#include "CoronaMap.h"

static Logging logger;

void CoronaRenderer::defineEnvironment()
{
	Corona::Rgb bgRgb(this->mtco_renderGlobals->bgColor.r,this->mtco_renderGlobals->bgColor.g,this->mtco_renderGlobals->bgColor.b);
	MString texName;
	MObject fileTextureObject;
	if( getConnectedFileTexturePath(MString("bgColor"), MString("coronaGlobals"), texName, fileTextureObject) )
	{
		Corona::String fileName = texName.asChar();
		logger.debug(MString("Found bg texture: ") + texName);
		if( !textureFileSupported(texName))
		{
			logger.error(MString("Sorry, textures of this type are not supported: ") + texName);
			return;
		}
		MapLoader loader;
		//mtco_MapLoader loader;
		//MFnDependencyNode ftn(fileTextureObject);
		//MColor gain, offset;
		//getColor("colorGain", ftn, gain);
		//getColor("colorOffset", ftn, offset);
		//loader.colorGain = gain;
		//loader.colorOffset = offset;
		Corona::Abstract::Map *texmap = loader.loadBitmap(fileName);
		if(texmap == NULL) 
		{
			logger.error(MString("Unable to read bg file: ") + texName);
		}else{
			this->context.scene->setBackground(Corona::ColorOrMap(bgRgb, texmap));
		}
	}else{
		this->context.scene->setBackground(Corona::ColorOrMap(bgRgb));
	}
}


