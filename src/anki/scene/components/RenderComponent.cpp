// Copyright (C) 2009-2019, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/scene/components/RenderComponent.h>
#include <anki/scene/SceneNode.h>
#include <anki/resource/TextureResource.h>
#include <anki/resource/ResourceManager.h>
#include <anki/util/Logger.h>

namespace anki
{

MaterialRenderComponent::MaterialRenderComponent(SceneNode* node, MaterialResourcePtr mtl)
	: m_node(node)
	, m_mtl(mtl)
{
	ANKI_ASSERT(node);

	// Create the material variables
	m_vars.create(m_node->getAllocator(), m_mtl->getVariables().getSize());
	U count = 0;
	for(const MaterialVariable& mv : m_mtl->getVariables())
	{
		m_vars[count++].m_mvar = &mv;
	}

	RenderComponentFlag flags =
		(mtl->isForwardShading()) ? RenderComponentFlag::FORWARD_SHADING : RenderComponentFlag::NONE;
	flags |= (mtl->castsShadow()) ? RenderComponentFlag::CASTS_SHADOW : RenderComponentFlag::NONE;
	setFlags(flags);
}

MaterialRenderComponent::~MaterialRenderComponent()
{
	m_vars.destroy(m_node->getAllocator());
}

void MaterialRenderComponent::allocateAndSetupUniforms(U32 set,
	const RenderQueueDrawContext& ctx,
	ConstWeakArray<Mat4> transforms,
	ConstWeakArray<Mat4> prevTransforms,
	StagingGpuMemoryManager& alloc) const
{
	ANKI_ASSERT(transforms.getSize() <= MAX_INSTANCES);
	ANKI_ASSERT(prevTransforms.getSize() == transforms.getSize());

	const MaterialVariant& variant = m_mtl->getOrCreateVariant(ctx.m_key);
	const ShaderProgramResourceVariant& progVariant = variant.getShaderProgramResourceVariant();

	// Allocate uniform memory
	U8* uniforms;
	void* uniformsBegin;
	const void* uniformsEnd;
	Array<Vec4, 256 / sizeof(Vec4)> pushConsts; // Use Vec4 so it will be aligned
	if(!progVariant.usePushConstants())
	{
		StagingGpuMemoryToken token;
		uniforms =
			static_cast<U8*>(alloc.allocateFrame(variant.getUniformBlockSize(), StagingGpuMemoryType::UNIFORM, token));

		ctx.m_commandBuffer->bindUniformBuffer(set, 0, token.m_buffer, token.m_offset, token.m_range);
	}
	else
	{
		uniforms = reinterpret_cast<U8*>(&pushConsts[0]);
	}

	uniformsBegin = uniforms;
	uniformsEnd = uniforms + variant.getUniformBlockSize();

	// Iterate variables
	for(auto it = m_vars.getBegin(); it != m_vars.getEnd(); ++it)
	{
		const MaterialRenderComponentVariable& var = *it;
		const MaterialVariable& mvar = var.getMaterialVariable();
		const ShaderProgramResourceInputVariable& progvar = mvar.getShaderProgramResourceInputVariable();

		if(!variant.variableActive(mvar))
		{
			continue;
		}

		switch(progvar.getShaderVariableDataType())
		{
		case ShaderVariableDataType::FLOAT:
		{
			F32 val = mvar.getValue<F32>();
			progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
			break;
		}
		case ShaderVariableDataType::VEC2:
		{
			Vec2 val = mvar.getValue<Vec2>();
			progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
			break;
		}
		case ShaderVariableDataType::VEC3:
		{
			switch(mvar.getBuiltin())
			{
			case BuiltinMaterialVariableId::NONE:
			{
				Vec3 val = mvar.getValue<Vec3>();
				progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::CAMERA_POSITION:
			{
				Vec3 val = ctx.m_cameraTransform.getTranslationPart().xyz();
				progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
				break;
			}
			default:
				ANKI_ASSERT(0);
			}

			break;
		}
		case ShaderVariableDataType::VEC4:
		{
			Vec4 val = mvar.getValue<Vec4>();
			progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
			break;
		}
		case ShaderVariableDataType::MAT3:
		{
			switch(mvar.getBuiltin())
			{
			case BuiltinMaterialVariableId::NONE:
			{
				Mat3 val = mvar.getValue<Mat3>();
				progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::NORMAL_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() > 0);

				DynamicArrayAuto<Mat3> normMats(m_node->getFrameAllocator());
				normMats.create(transforms.getSize());

				for(U i = 0; i < transforms.getSize(); i++)
				{
					Mat4 mv = ctx.m_viewMatrix * transforms[i];
					normMats[i] = mv.getRotationPart();
					normMats[i].reorthogonalize();
				}

				progVariant.writeShaderBlockMemory(
					progvar, &normMats[0], U32(transforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::ROTATION_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() > 0);

				DynamicArrayAuto<Mat3> rots(m_node->getFrameAllocator());
				rots.create(transforms.getSize());

				for(U i = 0; i < transforms.getSize(); i++)
				{
					rots[i] = transforms[i].getRotationPart();
				}

				progVariant.writeShaderBlockMemory(
					progvar, &rots[0], U32(transforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::CAMERA_ROTATION_MATRIX:
			{
				Mat3 rot = ctx.m_cameraTransform.getRotationPart();
				progVariant.writeShaderBlockMemory(progvar, &rot, 1, uniformsBegin, uniformsEnd);
				break;
			}
			default:
				ANKI_ASSERT(0);
			}

			break;
		}
		case ShaderVariableDataType::MAT4:
		{
			switch(mvar.getBuiltin())
			{
			case BuiltinMaterialVariableId::NONE:
			{
				Mat4 val = mvar.getValue<Mat4>();
				progVariant.writeShaderBlockMemory(progvar, &val, 1, uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::MODEL_VIEW_PROJECTION_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() > 0);

				DynamicArrayAuto<Mat4> mvp(m_node->getFrameAllocator());
				mvp.create(transforms.getSize());

				for(U i = 0; i < transforms.getSize(); i++)
				{
					mvp[i] = ctx.m_viewProjectionMatrix * transforms[i];
				}

				progVariant.writeShaderBlockMemory(
					progvar, &mvp[0], U32(transforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::PREVIOUS_MODEL_VIEW_PROJECTION_MATRIX:
			{
				ANKI_ASSERT(prevTransforms.getSize() > 0);

				DynamicArrayAuto<Mat4> mvp(m_node->getFrameAllocator());
				mvp.create(prevTransforms.getSize());

				for(U i = 0; i < prevTransforms.getSize(); i++)
				{
					mvp[i] = ctx.m_previousViewProjectionMatrix * prevTransforms[i];
				}

				progVariant.writeShaderBlockMemory(
					progvar, &mvp[0], U32(prevTransforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::MODEL_VIEW_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() > 0);

				DynamicArrayAuto<Mat4> mv(m_node->getFrameAllocator());
				mv.create(transforms.getSize());

				for(U i = 0; i < transforms.getSize(); i++)
				{
					mv[i] = ctx.m_viewMatrix * transforms[i];
				}

				progVariant.writeShaderBlockMemory(
					progvar, &mv[0], U32(transforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::MODEL_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() > 0);

				progVariant.writeShaderBlockMemory(
					progvar, &transforms[0], U32(transforms.getSize()), uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::VIEW_PROJECTION_MATRIX:
			{
				ANKI_ASSERT(transforms.getSize() == 0 && "Cannot have transform");
				progVariant.writeShaderBlockMemory(progvar, &ctx.m_viewProjectionMatrix, 1, uniformsBegin, uniformsEnd);
				break;
			}
			case BuiltinMaterialVariableId::VIEW_MATRIX:
			{
				progVariant.writeShaderBlockMemory(progvar, &ctx.m_viewMatrix, 1, uniformsBegin, uniformsEnd);
				break;
			}
			default:
				ANKI_ASSERT(0);
			}

			break;
		}
		case ShaderVariableDataType::TEXTURE_2D:
		case ShaderVariableDataType::TEXTURE_2D_ARRAY:
		case ShaderVariableDataType::TEXTURE_3D:
		case ShaderVariableDataType::TEXTURE_CUBE:
		{
			ctx.m_commandBuffer->bindTexture(set,
				progVariant.getBinding(progvar),
				mvar.getValue<TextureResourcePtr>()->getGrTextureView(),
				TextureUsageBit::SAMPLED_FRAGMENT);
			break;
		}
		case ShaderVariableDataType::SAMPLER:
		{
			switch(mvar.getBuiltin())
			{
			case BuiltinMaterialVariableId::GLOBAL_SAMPLER:
				ctx.m_commandBuffer->bindSampler(set, progVariant.getBinding(progvar), ctx.m_sampler);
				break;
			default:
				ANKI_ASSERT(0);
			}

			break;
		}
		default:
			ANKI_ASSERT(0);
		} // end switch
	}

	if(progVariant.usePushConstants())
	{
		ctx.m_commandBuffer->setPushConstants(uniformsBegin, variant.getUniformBlockSize());
	}
}

} // end namespace anki
