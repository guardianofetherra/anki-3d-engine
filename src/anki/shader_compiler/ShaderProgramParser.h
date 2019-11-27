// Copyright (C) 2009-2019, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/shader_compiler/Common.h>
#include <anki/util/StringList.h>
#include <anki/util/WeakArray.h>
#include <anki/util/DynamicArray.h>
#include <anki/util/BitSet.h>
#include <anki/gr/utils/Functions.h>

namespace anki
{

// Forward
class ShaderProgramParser;
class ShaderProgramParserVariant;

/// @addtogroup shader_compiler
/// @{

/// @memberof ShaderProgramParser
class ShaderProgramParserMutator
{
	friend ShaderProgramParser;

public:
	using ValueType = I32;

	ShaderProgramParserMutator(GenericMemoryPoolAllocator<U8> alloc)
		: m_name(alloc)
		, m_values(alloc)
	{
	}

	CString getName() const
	{
		return m_name;
	}

	ConstWeakArray<I32> getValues() const
	{
		return m_values;
	}

	Bool isInstanceCount() const
	{
		return m_instanceCount;
	}

private:
	StringAuto m_name;
	DynamicArrayAuto<I32> m_values;
	Bool m_instanceCount = false;
};

/// @memberof ShaderProgramParser
class ShaderProgramParserInput
{
	friend ShaderProgramParser;
	friend ShaderProgramParserVariant;

public:
	ShaderProgramParserInput(GenericMemoryPoolAllocator<U8> alloc)
		: m_name(alloc)
	{
	}

	CString getName() const
	{
		return m_name.toCString();
	}

	ShaderVariableDataType getDataType() const
	{
		return m_dataType;
	}

	Bool isInstanced() const
	{
		return m_instanced;
	}

	/// @param constantId It's the vulkan spec const index.
	Bool isConstant(U32* constantId = nullptr) const
	{
		if(constantId)
		{
			*constantId = m_specConstId;
		}
		return m_specConstId != MAX_U32;
	}

	Bool isTexture() const
	{
		return m_dataType >= ShaderVariableDataType::TEXTURE_FIRST
			   && m_dataType <= ShaderVariableDataType::TEXTURE_LAST;
	}

	Bool isSampler() const
	{
		return m_dataType == ShaderVariableDataType::SAMPLER;
	}

	Bool inUbo() const
	{
		return !isConstant() && !isTexture() && !isSampler();
	}

private:
	StringAuto m_name;
	U32 m_idx = MAX_U32; ///< Index inside an array.
	U32 m_specConstId = MAX_U32;
	Bool m_instanced = false;
	ShaderVariableDataType m_dataType = ShaderVariableDataType::NONE;
};

/// @memberof ShaderProgramParser
class ShaderProgramParserVariant
{
	friend class ShaderProgramParser;

public:
	~ShaderProgramParserVariant()
	{
		for(String& s : m_sources)
		{
			s.destroy(m_alloc);
		}
		m_blockInfos.destroy(m_alloc);
		m_bindings.destroy(m_alloc);
	}

	CString getSource(ShaderType type) const
	{
		return m_sources[type];
	}

	Bool isInputActive(const ShaderProgramParserInput& in) const
	{
		return m_activeInputVarsMask.get(in.m_idx);
	}

	const ShaderVariableBlockInfo& getBlockInfo(const ShaderProgramParserInput& in) const
	{
		ANKI_ASSERT(in.inUbo() && isInputActive(in));
		return m_blockInfos[in.m_idx];
	}

private:
	GenericMemoryPoolAllocator<U8> m_alloc;
	Array<String, U(ShaderType::COUNT)> m_sources;
	DynamicArray<ShaderVariableBlockInfo> m_blockInfos;
	DynamicArray<I16> m_bindings;
	U32 m_uniBlockSize = 0;
	Bool m_usesPushConstants = false;
	BitSet<MAX_SHADER_PROGRAM_INPUT_VARIABLES> m_activeInputVarsMask = {false};
};

/// @memberof ShaderProgramParser
class ShaderProgramParserMutatorState
{
public:
	const ShaderProgramParserMutator* m_mutator = nullptr;
	ShaderProgramParserMutator::ValueType m_value = MAX_I32;
};

/// This is a special preprocessor that run before the usual preprocessor. Its purpose is to add some meta information
/// in the shader programs.
///
/// It supports the following expressions:
/// #include {<> | ""}
/// #pragma once
/// #pragma anki mutator [instanced] NAME VALUE0 [VALUE1 [VALUE2] ...]
/// #pragma anki input [const | instanced] TYPE NAME
/// #pragma anki start {vert | tessc | tesse | geom | frag | comp}
/// #pragma anki end
/// #pragma anki descriptor_set <number>
///
/// Only the "anki input" should be in an ifdef-like guard.
class ShaderProgramParser : public NonCopyable
{
public:
	ShaderProgramParser(CString fname,
		ShaderProgramFilesystemInterface* fsystem,
		GenericMemoryPoolAllocator<U8> alloc,
		U32 pushConstantsSize,
		U32 backendMinor,
		U32 backendMajor,
		GpuVendor gpuVendor)
		: m_alloc(alloc)
		, m_fname(alloc, fname)
		, m_fsystem(fsystem)
		, m_pushConstSize(pushConstantsSize)
		, m_backendMinor(backendMinor)
		, m_backendMajor(backendMajor)
		, m_gpuVendor(gpuVendor)
	{
	}

	~ShaderProgramParser()
	{
	}

	/// Parse the file and its includes.
	ANKI_USE_RESULT Error parse();

	/// Get the source (and a few more things) given a list of mutators.
	ANKI_USE_RESULT Error generateVariant(
		ConstWeakArray<ShaderProgramParserMutatorState> mutatorStates, ShaderProgramParserVariant& variant) const;

	ConstWeakArray<ShaderProgramParserMutator> getMutators() const
	{
		return m_mutators;
	}

	ConstWeakArray<ShaderProgramParserInput> getInputs() const
	{
		return m_inputs;
	}

	ShaderTypeBit getShaderStages() const
	{
		return m_shaderTypes;
	}

	U32 getDescritproSet() const
	{
		return m_set;
	}

private:
	using Mutator = ShaderProgramParserMutator;
	using Input = ShaderProgramParserInput;

	static const U32 MAX_INCLUDE_DEPTH = 8;

	GenericMemoryPoolAllocator<U8> m_alloc;
	StringAuto m_fname;
	ShaderProgramFilesystemInterface* m_fsystem = nullptr;

	StringListAuto m_codeLines = {m_alloc}; ///< The code.
	StringListAuto m_globalsLines = {m_alloc};
	StringListAuto m_uboStructLines = {m_alloc};
	StringAuto m_codeSource = {m_alloc};
	StringAuto m_globalsSource = {m_alloc};
	StringAuto m_uboSource = {m_alloc};

	DynamicArrayAuto<Mutator> m_mutators = {m_alloc};
	DynamicArrayAuto<Input> m_inputs = {m_alloc};

	ShaderTypeBit m_shaderTypes = ShaderTypeBit::NONE;
	Bool m_insideShader = false;
	U32 m_set = 0;
	U32 m_instancedMutatorIdx = MAX_U32;
	U32 m_specConstIdx = 0;
	const U32 m_pushConstSize = 0;
	const U32 m_backendMinor = 1;
	const U32 m_backendMajor = 1;
	const GpuVendor m_gpuVendor = GpuVendor::AMD;
	Bool m_foundAtLeastOneInstancedInput = false;

	ANKI_USE_RESULT Error parseFile(CString fname, U32 depth);
	ANKI_USE_RESULT Error parseLine(CString line, CString fname, Bool& foundPragmaOnce, U32 depth);
	ANKI_USE_RESULT Error parseInclude(
		const StringAuto* begin, const StringAuto* end, CString line, CString fname, U32 depth);
	ANKI_USE_RESULT Error parsePragmaMutator(
		const StringAuto* begin, const StringAuto* end, CString line, CString fname);
	ANKI_USE_RESULT Error parsePragmaInput(const StringAuto* begin, const StringAuto* end, CString line, CString fname);
	ANKI_USE_RESULT Error parsePragmaStart(const StringAuto* begin, const StringAuto* end, CString line, CString fname);
	ANKI_USE_RESULT Error parsePragmaEnd(const StringAuto* begin, const StringAuto* end, CString line, CString fname);
	ANKI_USE_RESULT Error parsePragmaDescriptorSet(
		const StringAuto* begin, const StringAuto* end, CString line, CString fname);

	ANKI_USE_RESULT Error findActiveInputVars(CString source, BitSet<MAX_SHADER_PROGRAM_INPUT_VARIABLES>& active) const;
	void tokenizeLine(CString line, DynamicArrayAuto<StringAuto>& tokens) const;

	static Bool tokenIsComment(CString token)
	{
		return token.getLength() >= 2 && token[0] == '/' && (token[1] == '/' || token[1] == '*');
	}
};
/// @}

} // end namespace anki
