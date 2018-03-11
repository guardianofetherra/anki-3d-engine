// Copyright (C) 2009-2018, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/resource/MeshLoader.h>
#include <anki/resource/ResourceManager.h>
#include <anki/resource/ResourceFilesystem.h>

namespace anki
{

MeshLoader::MeshLoader(ResourceManager* manager)
	: MeshLoader(manager, manager->getTempAllocator())
{
}

MeshLoader::~MeshLoader()
{
	m_subMeshes.destroy(m_alloc);
}

Error MeshLoader::load(const ResourceFilename& filename)
{
	auto& alloc = m_alloc;

	// Load header
	ANKI_CHECK(m_manager->getFilesystem().openFile(filename, m_file));
	ANKI_CHECK(m_file->read(&m_header, sizeof(m_header)));
	ANKI_CHECK(checkHeader());

	// Read submesh info
	{
		m_subMeshes.create(alloc, m_header.m_subMeshCount);
		ANKI_CHECK(m_file->read(&m_subMeshes[0], m_subMeshes.getSizeInBytes()));

		// Checks
		const U32 indicesPerFace = !!(m_header.m_flags & MeshBinaryFile::Flag::QUAD) ? 4 : 3;
		U idxSum = 0;
		for(U i = 0; i < m_subMeshes.getSize(); i++)
		{
			const MeshBinaryFile::SubMesh& sm = m_subMeshes[0];
			if(sm.m_firstIndex != idxSum || (sm.m_indexCount % indicesPerFace) != 0)
			{
				ANKI_RESOURCE_LOGE("Incorrect sub mesh info");
				return Error::USER_DATA;
			}

			idxSum += sm.m_indexCount;
		}

		if(idxSum != m_header.m_totalIndexCount)
		{
			ANKI_RESOURCE_LOGE("Incorrect sub mesh info");
			return Error::USER_DATA;
		}
	}

	// Read vert buffer info
	{
		U32 vertBufferMask = 0;
		U32 vertBufferCount = 0;
		for(const MeshBinaryFile::VertexAttribute& attrib : m_header.m_vertexAttributes)
		{
			if(attrib.m_format == Format::NONE)
			{
				continue;
			}

			vertBufferCount = max(attrib.m_bufferBinding + 1, vertBufferCount);
			vertBufferMask |= 1 << attrib.m_bufferBinding;
		}

		if(U(__builtin_popcount(vertBufferMask)) != vertBufferCount)
		{
			ANKI_RESOURCE_LOGE("Problem in vertex buffers");
			return Error::USER_DATA;
		}

		m_vertBufferCount = vertBufferCount;
	}

	// Count and check the file size
	{
		U32 totalSize = sizeof(m_header);

		totalSize += sizeof(MeshBinaryFile::SubMesh) * m_header.m_subMeshCount;
		totalSize += getIndexBufferSize();

		for(U i = 0; i < m_vertBufferCount; ++i)
		{
			totalSize += m_header.m_vertexBuffers[i].m_vertexStride * m_header.m_totalVertexCount;
		}

		if(totalSize != m_file->getSize())
		{
			ANKI_RESOURCE_LOGE("Unexpected file size");
			return Error::USER_DATA;
		}
	}

	return Error::NONE;
}

Error MeshLoader::checkFormat(VertexAttributeLocation type, ConstWeakArray<Format> supportedFormats) const
{
	const MeshBinaryFile::VertexAttribute& attrib = m_header.m_vertexAttributes[type];

	// Check format
	Bool found = false;
	for(Format fmt : supportedFormats)
	{
		if(fmt == attrib.m_format)
		{
			found = true;
			break;
		}
	}

	if(!found)
	{
		ANKI_RESOURCE_LOGE(
			"Vertex attribute %u has unsupported format %u", U(type), U(m_header.m_vertexAttributes[type].m_format));
		return Error::USER_DATA;
	}

	// Scale should be 1.0 for now
	if(attrib.m_scale != 0.0)
	{
		ANKI_RESOURCE_LOGE("Vertex attribute %u should have 1.0 scale", U(type));
		return Error::USER_DATA;
	}

	return Error::NONE;
}

Error MeshLoader::checkHeader() const
{
	const MeshBinaryFile::Header& h = m_header;

	// Header
	if(memcmp(&h.m_magic[0], MeshBinaryFile::MAGIC, 8) != 0)
	{
		ANKI_RESOURCE_LOGE("Wrong magic word");
		return Error::USER_DATA;
	}

	// Flags
	if((h.m_flags & MeshBinaryFile::Flag::ALL) != MeshBinaryFile::Flag::ALL)
	{
		ANKI_RESOURCE_LOGE("Wrong header flags");
		return Error::USER_DATA;
	}

	// Attributes
	ANKI_CHECK(checkFormat(
		VertexAttributeLocation::POSITION, Array<Format, 2>{{Format::R16G16B16_SFLOAT, Format::R32G32B32_SFLOAT}}));
	ANKI_CHECK(checkFormat(VertexAttributeLocation::NORMAL, Array<Format, 1>{{Format::A2B10G10R10_SNORM_PACK32}}));
	ANKI_CHECK(checkFormat(VertexAttributeLocation::TANGENT, Array<Format, 1>{{Format::A2B10G10R10_SNORM_PACK32}}));
	ANKI_CHECK(
		checkFormat(VertexAttributeLocation::UV, Array<Format, 2>{{Format::R16G16_UNORM, Format::R16G16_SFLOAT}}));
	ANKI_CHECK(checkFormat(
		VertexAttributeLocation::BONE_INDICES, Array<Format, 2>{{Format::NONE, Format::R16G16B16A16_UINT}}));
	ANKI_CHECK(
		checkFormat(VertexAttributeLocation::BONE_WEIGHTS, Array<Format, 2>{{Format::NONE, Format::R8G8B8A8_UNORM}}));

	// Indices format
	if(h.m_indicesFormat != Format::R16_UINT || h.m_indicesFormat != Format::R32_UINT)
	{
		ANKI_RESOURCE_LOGE("Wrong format for indices");
		return Error::USER_DATA;
	}

	// m_totalIndexCount
	const U indicesPerFace = !!(h.m_flags & MeshBinaryFile::Flag::QUAD) ? 4 : 3;
	if(h.m_totalIndexCount == 0 || (h.m_totalIndexCount % indicesPerFace) != 0)
	{
		ANKI_RESOURCE_LOGE("Wrong index count");
		return Error::USER_DATA;
	}

	// m_totalVertexCount
	if(h.m_totalVertexCount == 0)
	{
		ANKI_RESOURCE_LOGE("Wrong vertex count");
		return Error::USER_DATA;
	}

	// m_subMeshCount
	if(h.m_subMeshCount == 0)
	{
		ANKI_RESOURCE_LOGE("Wrong submesh count");
		return Error::USER_DATA;
	}

	return Error::NONE;
}

Error MeshLoader::storeIndexBuffer(void* ptr, PtrSize size)
{
	ANKI_ASSERT(isLoaded());
	ANKI_ASSERT(size == getIndexBufferSize());
	ANKI_ASSERT(m_loadedChunk == 0);

	if(ptr)
	{
		ANKI_CHECK(m_file->read(ptr, size));
	}
	else
	{
		ANKI_CHECK(m_file->seek(size, ResourceFile::SeekOrigin::CURRENT));
	}

	++m_loadedChunk;
	return Error::NONE;
}

Error MeshLoader::storeVertexBuffer(U32 bufferIdx, void* ptr, PtrSize size)
{
	ANKI_ASSERT(isLoaded());
	ANKI_ASSERT(bufferIdx < m_vertBufferCount);
	ANKI_ASSERT(size == m_header.m_vertexBuffers[bufferIdx].m_vertexStride * m_header.m_totalVertexCount);
	ANKI_ASSERT(m_loadedChunk == bufferIdx + 1);

	if(ptr)
	{
		ANKI_CHECK(m_file->read(ptr, size));
	}
	else
	{
		ANKI_CHECK(m_file->seek(size, ResourceFile::SeekOrigin::CURRENT));
	}

	++m_loadedChunk;
	return Error::NONE;
}

} // end namespace anki
