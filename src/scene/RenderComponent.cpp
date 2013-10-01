#include "anki/scene/RenderingComponent.h"
#include "anki/scene/SceneNode.h"
#include "anki/resource/TextureResource.h"
#include "anki/gl/ShaderProgram.h"
#include "anki/core/Logger.h"

namespace anki {

//==============================================================================
// Misc                                                                        =
//==============================================================================

/// Create a new RenderingComponentVariable given a MaterialVariable
struct CreateNewRenderingComponentVariableVisitor
{
	const MaterialVariable* mvar = nullptr;
	PropertyMap* pmap = nullptr;
	RenderingComponent::RenderingComponentVariables* vars = nullptr;

	template<typename TMaterialVariableTemplate>
	void visit(const TMaterialVariableTemplate&) const
	{
		typedef typename TMaterialVariableTemplate::Type Type;

		RenderingComponentVariableTemplate<Type>* rvar =
			new RenderingComponentVariableTemplate<Type>(mvar);

		vars->push_back(rvar);
	}
};

/// The names of the buildins
static Array<const char*, BMV_COUNT - 1> buildinNames = {{
	"modelViewProjectionMat",
	"modelViewMat",
	"viewProjectionMat",
	"normalMat",
	"billboardMvpMatrix",
	"blurring",
	"msDepthMap"}};

//==============================================================================
// RenderingComponentVariable                                                  =
//==============================================================================

//==============================================================================
RenderingComponentVariable::RenderingComponentVariable(
	const MaterialVariable* mvar_)
	: mvar(mvar_)
{
	ANKI_ASSERT(mvar);

	// Set buildin id
	const std::string& name = getName();

	buildinId = BMV_NO_BUILDIN;
	for(U i = 0; i < buildinNames.getSize(); i++)
	{
		if(name == buildinNames[i])
		{
			buildinId = (BuildinMaterialVariableId)(i + 1);
			break;
		}
	}

	// Sanity checks
	if(!mvar->hasValues() && buildinId == BMV_NO_BUILDIN)
	{
		ANKI_LOGW("Material variable no buildin and not initialized: %s",
			name.c_str());
	}
}

//==============================================================================
RenderingComponentVariable::~RenderingComponentVariable()
{}

//==============================================================================
// RenderingComponent                                                          =
//==============================================================================

//==============================================================================
RenderingComponent::RenderingComponent(SceneNode* node)
	: vars(node->getSceneAllocator())
{}

//==============================================================================
RenderingComponent::~RenderingComponent()
{
	for(RenderingComponentVariable* var : vars)
	{
		delete var;
	}
}

//==============================================================================
void RenderingComponent::init(PropertyMap& pmap)
{
	const Material& mtl = getRenderingComponentMaterial();

	// Create the material variables using a visitor
	CreateNewRenderingComponentVariableVisitor vis;
	vis.pmap = &pmap;
	vis.vars = &vars;

	vars.reserve(mtl.getVariables().size());

	for(const MaterialVariable* mv : mtl.getVariables())
	{
		vis.mvar = mv;
		mv->acceptVisitor(vis);
	}

	// FUTURE if the material is simple (only viewprojection matrix and samlers)
	// then use a common UBO. It will save the copying to the UBO and the 
	// binding

	// Init the UBO
	const ShaderProgramUniformBlock* block = mtl.getCommonUniformBlock();

	if(block)
	{
		ubo.create(block->getSize(), nullptr, GlObject::DOUBLE_OBJECT);
	}

	// Instancing sanity checks
	U32 instancesCount = getRenderingComponentInstancesCount();
	const MaterialVariable* mv =
		mtl.findVariableByName("instancingModelViewProjectionMatrices");

	if(mv && mv->getAShaderProgramUniformVariable().getSize() < instancesCount)
	{
		throw ANKI_EXCEPTION("The renderable needs more instances that the "
			"shader program can handle");
	}
}

}  // end namespace anki