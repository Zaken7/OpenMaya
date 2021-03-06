#include "CoronaCore/api/Api.h"
#include "OSL/oslexec.h"
#include "../coronaOSL/oslRenderer.h"

class CoronaRenderer;
// basic map class as interface for OSL calls
class OSLMap : public Corona::Abstract::Map 
{
public:
	//CoronaRenderer *coronaRenderer;
	OSL::OSLShadingNetworkRenderer *oslRenderer;
	enum BumpType { NONE = 0, BUMP, NORMALOBJECT, NORMALTANGENT, NORMALWORLD};
	BumpType bumpType;
	float worldScale;
	Corona::Matrix33 envSphereMatrix;
	bool isEnvMap;
	OSL::ShaderGroupRef shaderGroup;
	OSLMap();
	~OSLMap();

	void setShadingGlobals(const Corona::IShadeContext& context, OSL::ShaderGlobals &sg, int x, int y, OSL::Matrix44& Mshad, OSL::Matrix44& Mobj);
    virtual Corona::Rgb evalColor(const Corona::IShadeContext& context, Corona::TextureCache* cache, float& outAlpha);
    virtual float evalMono(const Corona::IShadeContext& context, Corona::TextureCache* cache, float& outAlpha);
    virtual Corona::Dir evalBump(const Corona::IShadeContext&, Corona::TextureCache*);
	virtual Corona::Rgb evalColorBump(const Corona::IShadeContext& context, Corona::TextureCache* cache, float& outAlpha, float u, float v);
	virtual void renderTo(Corona::Bitmap<Corona::Rgb>& output);
	virtual void getChildren(Corona::Stack<Corona::Resource*>&) {}
};
