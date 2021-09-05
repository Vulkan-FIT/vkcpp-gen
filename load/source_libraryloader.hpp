
class LibraryLoader {

public:

    LibraryLoader(const std::string &name = "vulkan-1.dll") {
        load(name);
    }

    void load(const std::string &name) {
#ifdef _WIN32
        lib = LoadLibraryA(name.c_str());
#endif
        if (!lib) {
            throw std::runtime_error("Cant load library");
        }

        m_vkGetInstanceProcAddr = std::bit_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
        if (!m_vkGetInstanceProcAddr) {
            throw std::runtime_error("Cant load vkGetInstanceProcAddr");
        }

        printAddr(m_vkGetInstanceProcAddr, "vkGetInstanceProcAddr");

        m_vkGetDeviceProcAddr = std::bit_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(lib, "vkGetDeviceProcAddr"));
        if (!m_vkGetDeviceProcAddr) {
            throw std::runtime_error("Cant load vkGetDeviceProcAddr");
        }

        printAddr(m_vkGetDeviceProcAddr, "vkGetDeviceProcAddr");

        m_vkEnumerateInstanceVersion = getProcAddr<PFN_vkEnumerateInstanceVersion>("vkEnumerateInstanceVersion");
        printAddr(m_vkEnumerateInstanceVersion, "vkEnumerateInstanceVersion");

        if (m_vkEnumerateInstanceVersion) {
            m_vkEnumerateInstanceVersion(&m_version);
        }
        else {
            m_version = VK_API_VERSION_1_0;
        }
        //patch version?    m_version & 0xFFFF0000
    }

	uint32_t version() const {
        return m_version;
    }

    void printVersion() {
        std::cout << "API version:" << VK_VERSION_MAJOR(m_version)
                  << "." << VK_VERSION_MINOR(m_version)
                  << "." << VK_VERSION_PATCH(m_version) << std::endl;
    }

    ~LibraryLoader() {
        if (lib) {
#ifdef _WIN32
            FreeLibrary(lib);
#endif
        }
    }

    const PFN_vkGetInstanceProcAddr &vkGetInstanceProcAddr {m_vkGetInstanceProcAddr};
    const PFN_vkGetDeviceProcAddr &vkGetDeviceProcAddr {m_vkGetDeviceProcAddr};

private:

    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr m_vkGetDeviceProcAddr;
    PFN_vkEnumerateInstanceVersion m_vkEnumerateInstanceVersion;

    template<typename T>
    inline T getProcAddr(const char *name) {
        return reinterpret_cast<T>(m_vkGetInstanceProcAddr(nullptr, name));
    }

    void printAddr(auto *f, const char *name) {
        std::cout << name << ": " << (void*)f << std::endl;
    }

    LIBHANDLE lib;
    uint32_t m_version;

};

