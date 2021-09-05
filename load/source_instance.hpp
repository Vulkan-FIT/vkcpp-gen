    
	Instance() {}

    Instance(const vk20::LibraryLoader &lib, const vk::InstanceCreateInfo& createInfo) {
        init(lib, createInfo);
    }
    
    void init(const vk20::LibraryLoader &lib, const vk::InstanceCreateInfo& createInfo) {
        m_vkGetInstanceProcAddr = lib.vkGetInstanceProcAddr;
        m_vkCreateInstance = getProcAddr<PFN_vkCreateInstance>("vkCreateInstance");
        createInstance(createInfo);       
        loadTable();        
    }
    
    template<typename PhysicalDeviceAllocator = std::allocator<vk::PhysicalDevice>>
    typename vk::ResultValueType<std::vector<vk::PhysicalDevice, PhysicalDeviceAllocator>>::type
    enumeratePhysicalDevices() const
    {
        std::vector<vk::PhysicalDevice, PhysicalDeviceAllocator> physicalDevices;
        uint32_t                                                 physicalDeviceCount;
        vk::Result                                               result;
        do
        {
          result = static_cast<vk::Result>( m_vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr) );
          if ( ( result == vk::Result::eSuccess ) && physicalDeviceCount )
          {
            physicalDevices.resize( physicalDeviceCount );
            result = static_cast<vk::Result>( m_vkEnumeratePhysicalDevices(
             _instance, &physicalDeviceCount, reinterpret_cast<VkPhysicalDevice *>( physicalDevices.data() ) ) );
            VULKAN_HPP_ASSERT( physicalDeviceCount <= physicalDevices.size() );
          }
        } while ( result == vk::Result::eIncomplete );
        if ( ( result == vk::Result::eSuccess ) && ( physicalDeviceCount < physicalDevices.size() ) )
        {
          physicalDevices.resize( physicalDeviceCount );
        }
        return createResultValue(
          result, physicalDevices, VULKAN_HPP_NAMESPACE_STRING "::Instance::enumeratePhysicalDevices" );
      }
    
private:
    void createInstance(const vk::InstanceCreateInfo& createInfo) {        
        if (m_vkCreateInstance(reinterpret_cast<const VkInstanceCreateInfo*>(&createInfo), nullptr, &_instance) != VK_SUCCESS) {
            throw std::runtime_error("Cant create Instacne");
        }        
    }
