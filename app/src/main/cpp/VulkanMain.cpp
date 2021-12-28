#include <android/log.h>
#include <android_native_app_glue.h>
#include <cassert>
#include <vector>
#include <vulkan_wrapper.h>
#include "VkValidationLayer.h"

// Android log function wrappers
static const char* kTAG = "Vulkan1";

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

// Global variables
VkInstance tutorialInstance;
VkPhysicalDevice tutorialGpu;
VkDevice tutorialDevice;
VkSurfaceKHR tutorialSurface;
static bool initialized_ = false;

bool openVulkan(android_app* app) {
    // Load Android vulkan and retrieve vulkan API function pointers
    if (!InitVulkan()) {
        LOGE("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    LOGE("Hello Sekwon! Welcome to Vulkan world!");

    VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "tutorial01_load_vulkan",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "tutorial",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_MAKE_VERSION(1, 1, 0),
    };

    // prepare necessary extensions: Vulkan on Android need these to function
    // Enable validation and debug layer/extensions, together with other necessary extensions
    LayerAndExtensions layerUtil;
    std::vector<const char *> layers = {"VK_LAYER_KHRONOS_validation"};
    std::vector<const char *> instanceExt, deviceExt;
    instanceExt.push_back("VK_KHR_surface");
    instanceExt.push_back("VK_KHR_android_surface");
    deviceExt.push_back("VK_KHR_swapchain");
    for (auto layerName : layers) {
        // check vulkan sees the layers packed inside this app's APK. layers could also be
        // be pushed to Android with adb on command line, this sample does test that approach
        // in the sense: if you want to use the adb way, you want to use it to enable/disable too
        //               if you want to use the source code way, pack the layer into apk
        //    blending different ways might work, feel free to use and experiment if you prefer.
        if (layerUtil.isLayerSupported(layerName)) {
            LOGE("layer supported %c", layerName);
        } else {
            LOGE("layer not supported %c", layerName);
        }
        assert(layerUtil.isLayerSupported(layerName));
    }
//    for (auto extName : instanceExt) {
//        assert(layerUtil.isExtensionSupported(
//                extName, VK_NULL_HANDLE, nullptr));
//    }
//    for (auto extName : deviceExt) {
//        assert(layerUtil.isExtensionSupported(
//                extName, VK_NULL_HANDLE, nullptr));
//    }


    // Create the Vulkan instance
    VkInstanceCreateInfo instanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(instanceExt.size()),
            .ppEnabledExtensionNames = instanceExt.data(),
    };
    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &tutorialInstance));

    // if we create a surface, we need the surface extension
    VkAndroidSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = app->window};
    CALL_VK(vkCreateAndroidSurfaceKHR(tutorialInstance, &createInfo, nullptr,
                                      &tutorialSurface));

    // Find one GPU to use:
    // On Android, every GPU device is equal -- supporting
    // graphics/compute/present
    // for this sample, we use the very first GPU device found on the system
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(tutorialInstance, &gpuCount, nullptr));
    VkPhysicalDevice tmpGpus[gpuCount];
    CALL_VK(vkEnumeratePhysicalDevices(tutorialInstance, &gpuCount, tmpGpus));
    tutorialGpu = tmpGpus[0];  // Pick up the first GPU Device

    // check for vulkan info on this GPU device
    VkPhysicalDeviceProperties gpuProperties;
    vkGetPhysicalDeviceProperties(tutorialGpu, &gpuProperties);
    LOGI("Vulkan Physical Device Name: %s", gpuProperties.deviceName);
    LOGI("Vulkan Physical Device Info: apiVersion: %x \n\t driverVersion: %x",
         gpuProperties.apiVersion, gpuProperties.driverVersion);
    LOGI("API Version Supported: %d.%d.%d",
         VK_VERSION_MAJOR(gpuProperties.apiVersion),
         VK_VERSION_MINOR(gpuProperties.apiVersion),
         VK_VERSION_PATCH(gpuProperties.apiVersion));

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(tutorialGpu, tutorialSurface,
                                              &surfaceCapabilities);

    LOGI("Vulkan Surface Capabilities:\n");
    LOGI("\timage count: %u - %u\n", surfaceCapabilities.minImageCount,
         surfaceCapabilities.maxImageCount);
    LOGI("\tarray layers: %u\n", surfaceCapabilities.maxImageArrayLayers);
    LOGI("\timage size (now): %dx%d\n", surfaceCapabilities.currentExtent.width,
         surfaceCapabilities.currentExtent.height);
    LOGI("\timage size (extent): %dx%d - %dx%d\n",
         surfaceCapabilities.minImageExtent.width,
         surfaceCapabilities.minImageExtent.height,
         surfaceCapabilities.maxImageExtent.width,
         surfaceCapabilities.maxImageExtent.height);
    LOGI("\tusage: %x\n", surfaceCapabilities.supportedUsageFlags);
    LOGI("\tcurrent transform: %u\n", surfaceCapabilities.currentTransform);
    LOGI("\tallowed transforms: %x\n", surfaceCapabilities.supportedTransforms);
    LOGI("\tcomposite alpha flags: %u\n", surfaceCapabilities.currentTransform);

    // Find a GFX queue family
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(tutorialGpu, &queueFamilyCount, nullptr);
    assert(queueFamilyCount);
    std::vector<VkQueueFamilyProperties>  queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(tutorialGpu, &queueFamilyCount,
                                             queueFamilyProperties.data());

    uint32_t queueFamilyIndex;
    for (queueFamilyIndex=0; queueFamilyIndex < queueFamilyCount;
         queueFamilyIndex++) {
        if (queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            break;
        }
    }
    assert(queueFamilyIndex < queueFamilyCount);

    // Create a logical device from GPU we picked
    float priorities[] = {
            1.0f,
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities,
    };

    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExt.size()),
            .ppEnabledExtensionNames = deviceExt.data(),
            .pEnabledFeatures = nullptr,
    };

    CALL_VK(
            vkCreateDevice(tutorialGpu, &deviceCreateInfo, nullptr, &tutorialDevice));
    initialized_ = true;
    return 0;
}

void closeVulkan(void) {
    vkDestroySurfaceKHR(tutorialInstance, tutorialSurface, nullptr);
    vkDestroyDevice(tutorialDevice, nullptr);
    vkDestroyInstance(tutorialInstance, nullptr);
    initialized_ = false;
}

bool IsVulkanReady() {
    return initialized_;
}
