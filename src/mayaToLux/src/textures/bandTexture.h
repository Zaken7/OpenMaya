#include <maya/MPxNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>


class band :public MPxNode
{
    public:
                    band();
    virtual         ~band();

    virtual MStatus compute( const MPlug&, MDataBlock& );
    virtual void    postConstructor();

    static void *  creator();
    static MStatus initialize();

    // Id tag for use with binary file format
    static MTypeId id;

    private:

	static MObject  outColor;

//---------------------------- automatically created attributes start ------------------------------------
	static    MObject tex;
	static    MObject amount;
	static    MObject offsets;
	static    MObject luxOutFloat;
	static    MObject luxOutColor;
	static    MObject luxOutFresnel;
//---------------------------- automatically created attributes end ------------------------------------

};
