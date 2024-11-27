#include "usb_descriptor.h"
#include <algorithm>
namespace USB
{

    bool DeviceDescriptor::addConfiguration(const ConfigurationDescriptor &config)
    {
        // Check for duplicate configuration values
        for (const auto &existing : configurations_)
        {
            if (existing.getConfigurationValue() == config.getConfigurationValue())
            {
                return false; // Configuration value already exists
            }
        }

        configurations_.push_back(config);
        updateDescriptor();
        return true;
    }

    void DeviceDescriptor::updateDescriptor()
    {
        desc_.bNumConfigurations = static_cast<uint8_t>(configurations_.size());
    }

    std::vector<uint8_t> DeviceDescriptor::generateDescriptorBlock() const
    {
        std::vector<uint8_t> block;
        block.reserve(sizeof(tusb_desc_device_t));

        // Copy the device descriptor
        block.insert(block.end(), (uint8_t *)&desc_, (uint8_t *)(&desc_ + 1));

        return block;
    }

} // namespace USB