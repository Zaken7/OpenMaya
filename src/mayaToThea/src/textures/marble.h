#ifndef mtth_marble
#define mtth_marble

#include <maya/MPxNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>


class Marble :public MPxNode
{
public:
Marble();
virtual         ~Marble();

virtual MStatus compute( const MPlug&, MDataBlock& );
virtual void    postConstructor();

static void *  creator();
static MStatus initialize();

// Id tag for use with binary file format
static MTypeId id;

private:

static MObject  outColor;

//---------------------------- automatically created attributes start ------------------------------------
	static    MObject high;
	static    MObject low;
	static    MObject octaves;
	static    MObject omega;
	static    MObject variation;
//---------------------------- automatically created attributes end ------------------------------------

};

#endif
