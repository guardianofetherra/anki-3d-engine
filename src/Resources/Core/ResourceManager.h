#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <list>
#include <boost/ptr_container/ptr_list.hpp>
#include <string>
#include "Singleton.h"
#include "AsyncLoader.h"
#include "Properties.h"


class Texture;
class ShaderProg;
class Material;
class Mesh;
class Skeleton;
class SkelAnim;
class LightData;
class ParticleEmitterProps;
class Script;
class Model;
class Skin;
class DummyRsrc;


/// Holds information about a resource
template<typename Type>
struct RsrcHook
{
	std::string uuid; ///< Unique identifier
	int referenceCounter;
	Type* resource;
};


/// Responsible of loading and unloading resources
class ResourceManager
{
	public:
		/// Because the current C++ standard does not offer template typedefs
		template<typename Type>
		struct Types
		{
			typedef RsrcHook<Type> Hook;
			typedef std::list<Hook> Container;
			typedef typename Container::iterator Iterator;
			typedef typename Container::const_iterator ConstIterator;
		};

		/// Load a resource
		/// See if its already loaded, if its not:
		/// - Create a instance
		/// - Call load method of the instance
		/// If its loaded:
		/// - Increase the resource counter
		template<typename Type>
		typename Types<Type>::Hook& load(const char* filename);

		/// Unload a resource if no one uses it
		template<typename Type>
		void unload(const typename Types<Type>::Hook& info);

	private:
		/// @name Containers
		/// @{
		Types<Texture>::Container textures;
		Types<ShaderProg>::Container shaderProgs;
		Types<Material>::Container materials;
		Types<Mesh>::Container meshes;
		Types<Skeleton>::Container skeletons;
		Types<SkelAnim>::Container skelAnims;
		Types<LightData>::Container lightProps;
		Types<ParticleEmitterProps>::Container particleEmitterProps;
		Types<Script>::Container scripts;
		Types<Model>::Container models;
		Types<Skin>::Container skins;
		Types<DummyRsrc>::Container dummies;
		/// @}

		/// Request for the AsyncLoader [Base class]
		class LoadingRequestBase
		{
			public:
				enum RequestType
				{
					RT_IMAGE,
					RT_MESH_DATA
				};

				LoadingRequestBase(const char* filename_, RequestType type_): filename(filename_), type(type_) {}

				GETTER_R(std::string, filename, getFilename)
				GETTER_R(RequestType, type, getType)

			private:
				std::string filename;
				RequestType type;
		};

		/// Request for the AsyncLoader
		template<typename Type>
		struct LoadingRequest: public LoadingRequestBase
		{
			public:
				GETTER_RW(Type, object, getObject)

			private:
				Type object; ///< The object to load
		};

		boost::ptr_list<LoadingRequestBase> loadingRequests; ///< Loading requests

		AsyncLoader al; ///< Asynchronous loader

		/// Find a resource using the filename
		template<typename Type>
		typename Types<Type>::Iterator find(const char* filename, typename Types<Type>::Container& container);

		/// Find a resource using the pointer
		template<typename Type>
		typename Types<Type>::Iterator find(const Type* resource, typename Types<Type>::Container& container);

		/// Specialized func
		template<typename Type>
		typename Types<Type>::Container& choseContainer();

		/// Unload a resource if no one uses it. This is the real deal
		template<typename Type>
		void unloadR(const typename Types<Type>::Hook& info);
};


#include "ResourceManager.inl.h"


typedef Singleton<ResourceManager> ResourceManagerSingleton;


#endif
