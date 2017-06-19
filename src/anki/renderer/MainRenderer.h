// Copyright (C) 2009-2017, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/renderer/Common.h>
#include <anki/resource/Forward.h>
#include <anki/core/Timestamp.h>

namespace anki
{

// Forward
class ResourceManager;
class ConfigSet;
class ThreadPool;
class StagingGpuMemoryManager;

/// @addtogroup renderer
/// @{

/// Main onscreen renderer
class MainRenderer
{
public:
	MainRenderer();

	~MainRenderer();

	ANKI_USE_RESULT Error create(ThreadPool* threadpool,
		ResourceManager* resources,
		GrManager* gl,
		StagingGpuMemoryManager* stagingMem,
		AllocAlignedCallback allocCb,
		void* allocCbUserData,
		const ConfigSet& config,
		Timestamp* globTimestamp);

	ANKI_USE_RESULT Error render(RenderQueue& rqueue);

	Dbg& getDbg();

	F32 getAspectRatio() const;

	const Renderer& getOffscreenRenderer() const
	{
		return *m_r;
	}

	Renderer& getOffscreenRenderer()
	{
		return *m_r;
	}

private:
	HeapAllocator<U8> m_alloc;
	StackAllocator<U8> m_frameAlloc;

	UniquePtr<Renderer> m_r;
	Bool8 m_rDrawToDefaultFb = false;

	ShaderProgramResourcePtr m_blitProg;
	ShaderProgramPtr m_blitGrProg;

	FramebufferPtr m_defaultFb;
	U32 m_width = 0; ///< Default FB size.
	U32 m_height = 0; ///< Default FB size.

	F32 m_renderingQuality = 1.0;

	/// Optimize job chain
	CommandBufferInitHints m_cbInitHints;
};
/// @}

} // end namespace anki
