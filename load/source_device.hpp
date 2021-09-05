
    Device() {}
    
    Device(const vk20::Instance &instance, const vk::PhysicalDevice &physicalDevice, const vk::DeviceCreateInfo& createInfo) {
       init(instance, physicalDevice, createInfo);
    }
    
    void init(const vk20::Instance &instance, const VkPhysicalDevice &physicalDevice, const vk::DeviceCreateInfo& createInfo) {
        m_vkGetDeviceProcAddr = instance.getProcAddr<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");       
        m_vkCreateDevice = instance.getProcAddr<PFN_vkCreateDevice>("vkCreateDevice");        
        createDevice(physicalDevice, createInfo);
        m_vkGetDeviceProcAddr = getProcAddr<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");                
        loadTable();        
    }
    
private:
    
    void createDevice(const VkPhysicalDevice &physicalDevice, const vk::DeviceCreateInfo& createInfo) {                
        if (m_vkCreateDevice(physicalDevice, reinterpret_cast<const VkDeviceCreateInfo*>(&createInfo), nullptr, &_device) != VK_SUCCESS) {
            throw std::runtime_error("Cant create Device");
        }        
    }
