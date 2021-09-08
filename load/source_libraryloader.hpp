
#ifdef _WIN32
#  define LIBHANDLE HINSTANCE
#else
#  define LIBHANDLE void*
#endif

class LibraryLoader {
protected:

    LIBHANDLE lib;
    uint32_t m_version;
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;    

public:
#ifdef _WIN32
	static constexpr char const* defaultName = "vulkan-1.dll";
#else
	static constexpr char const* defaultName = "libvulkan.so.1";
#endif

	LibraryLoader() {}

    LibraryLoader(const std::string &name) {
        load(name);
    }

    void load(const std::string &name = defaultName) {
		unload();
#ifdef _WIN32
        lib = LoadLibraryA(name.c_str());
#else
	    lib = dlopen(name.c_str(), RTLD_NOW);
#endif
        if (!lib) {
            throw std::runtime_error("Cant load library");
        }

#ifdef _WIN32
        m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
#else
		m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
#endif        
        if (!m_vkGetInstanceProcAddr) {
            throw std::runtime_error("Cant load vkGetInstanceProcAddr");
        }

        PFN_vkEnumerateInstanceVersion m_vkEnumerateInstanceVersion = getProcAddr<PFN_vkEnumerateInstanceVersion>("vkEnumerateInstanceVersion");       

        if (m_vkEnumerateInstanceVersion) {
            m_vkEnumerateInstanceVersion(&m_version);
        }
        else {
            m_version = VK_API_VERSION_1_0;
        }
    }
	
	void unload() {
		if (lib) {
#ifdef _WIN32
			FreeLibrary(lib);
#else
			dlclose(lib);
#endif
			lib = nullptr;
			m_vkGetInstanceProcAddr = nullptr;
        }
	}

	uint32_t version() const {
        return m_version;
    }    

    ~LibraryLoader() {
		unload();
    }
		
    template<typename T>
    inline T getProcAddr(const char *name) {
        return reinterpret_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

    const PFN_vkGetInstanceProcAddr &vkGetInstanceProcAddr {m_vkGetInstanceProcAddr};

};

