// Copyright (C) 2009-2020, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/utils/ClassGpuAllocator.h>
#include <anki/gr/vulkan/Common.h>

namespace anki
{

/// @addtorgoup vulkan
/// @{

/// The handle that is returned from GpuMemoryManager's allocations.
class GpuMemoryHandle
{
	friend class GpuMemoryManager;

public:
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	PtrSize m_offset = MAX_PTR_SIZE;

	operator Bool() const
	{
		return m_memory != VK_NULL_HANDLE && m_offset < MAX_PTR_SIZE && m_memTypeIdx < MAX_U8;
	}

private:
	ClassGpuAllocatorHandle m_classHandle;
	U8 m_memTypeIdx = MAX_U8;
	Bool m_linear = false;
	Bool m_exposesBufferGpuAddress = false;
};

/// Dynamic GPU memory allocator for all types.
class GpuMemoryManager : public NonCopyable
{
public:
	GpuMemoryManager() = default;

	~GpuMemoryManager();

	void init(VkPhysicalDevice pdev, VkDevice dev, GrAllocator<U8> alloc);

	void destroy();

	/// Allocate memory.
	void allocateMemory(U32 memTypeIdx,
		PtrSize size,
		U32 alignment,
		Bool linearResource,
		Bool exposesBufferGpuAddress,
		GpuMemoryHandle& handle);

	/// Free memory.
	void freeMemory(GpuMemoryHandle& handle);

	/// Map memory.
	ANKI_USE_RESULT void* getMappedAddress(GpuMemoryHandle& handle);

	/// Find a suitable memory type.
	U32 findMemoryType(
		U32 resourceMemTypeBits, VkMemoryPropertyFlags preferFlags, VkMemoryPropertyFlags avoidFlags) const;

	/// Get some statistics.
	void getAllocatedMemory(PtrSize& gpuMemory, PtrSize& cpuMemory) const;

private:
	class Memory;
	class Interface;
	class ClassAllocator;

	GrAllocator<U8> m_alloc;
	DynamicArray<Array<Interface, 2>> m_ifaces;
	DynamicArray<Array<ClassAllocator, 3>> m_callocs;
	VkPhysicalDeviceMemoryProperties m_memoryProperties;
};
/// @}

} // end namespace anki
