#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <set>
#include <fstream>
#include <direct.h>

constexpr uint32_t WIDTH = 1080;
constexpr uint32_t HEIGHT = 720;

// How many frames you let the CPU start rendering before the GPU is done
// MAX_FRAMES_IN_FLIGHT: How many frames the CPU can work on at once
// Swap Chain: How many images are available to render into
// They are not directly tied, but you need to consider things like: MAX_FRAMES_IN_FLIGHT = min(MAX_FRAMES_IN_FLIGHT, SC.size())
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#define RESOURCE(filepath) "..\\..\\src\\" filepath

#define DEBUG
#ifdef DEBUG
constexpr bool enableValidationLayers = true;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#else
constexpr bool enableValidationLayers = false;
#endif

class HelloTraingleApp {

    // GLFW
    GLFWwindow* window;


    // Vulkan
    VkInstance instance; // Connection between Vulkan and the main program
    VkDevice device; // After selecting a Physical Device, create a logical device to interface with it
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkQueue graphicsQueue; // Handle to interact with device graphics queue;
    VkSurfaceKHR surface; // Handle to interact with window
    VkQueue presentQueue; // Handle to interact with window surface queue;

    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VkViewport viewport;
    VkRect2D scissor; // Cut viewport filter >:/

    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        this->window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle - Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        /*
            Clarifications:

            Vulkan uses sType because:
                - C lacks built-in runtime type information.
                - It enables fast, safe, extensible handling of struct chains (pNext).
                - It helps with forward/backward compatibility and debugging.

            The general pattern that object creation function parameters in Vulkan follow is:
                - Pointer to struct with creation info
                - Pointer to custom allocator callbacks, always nullptr in this tutorial
                - Pointer to the variable that stores the handle to the new object

            Nearly all Vulkan functions return a value of type VkResult that is either VK_SUCCESS or an error code

            The Vulkan API is designed around the idea of minimal driver overhead and one of the manifestations of that goal is that
            there is very limited error checking in the API by default
            However, that doesn't mean that these checks can't be added to the API. Vulkan introduces an elegant system for this known as validation layers. Validation layers are optional components that hook into Vulkan function calls to apply additional operations. Common operations in validation layers are:

                Checking the values of parameters against the specification to detect misuse
                Tracking creation and destruction of objects to find resource leaks
                Checking thread safety by tracking the threads that calls originate from
                Logging every call and its parameters to the standard output
                Tracing Vulkan calls for profiling and replaying
            
            OBS: Validation layers won't work on the functions: vkCreateInstance and vkDestroyInstance.
                 So you need to handle that separately
            
        */
        std::cout << "\n Vulkan Header Version: " << VK_HEADER_VERSION << std::endl;
        std::cout << " Vulkan API Version: " << VK_API_VERSION_VARIANT(VK_HEADER_VERSION_COMPLETE)
            << "." << VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE)
            << "." << VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE)
            << std::endl;

        // Technically optional, but it may provide some useful information to the driver
        // in order to optimize our specific application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);  // Your app's version
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);       // Your engine's version
        appInfo.apiVersion = VK_API_VERSION_1_3;                // Latest official Vulkan version

        // Not optional, tells the Vulkan driver which global extensions and validation layers we want to use.
        // Global here means that they apply to the entire program and not a specific device
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;

        // Vulkan is a platform agnostic API, which means that you need an extension to interface with the window system
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        instanceInfo.enabledExtensionCount = glfwExtensionCount;
        instanceInfo.ppEnabledExtensionNames = glfwExtensions;
        instanceInfo.enabledLayerCount = 0;

        #ifdef DEBUG
            // Check the availability of validation layers
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            bool layersAvailable = true;
            for (const char* layerName : validationLayers) {
                bool layerFound = false;
                for(const auto& layerProperties : availableLayers)
                    if (strcmp(layerName, layerProperties.layerName) == 0) { layerFound = true; break; }
                if (!layerFound) std::cerr << "Requested validation layer: " << layerName << " is not availible\n";
                layersAvailable &= layerFound;
            }

            if (layersAvailable) {
                instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                instanceInfo.ppEnabledLayerNames = validationLayers.data();
            }
        #endif

        if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
            std::cerr << "VkInstance Creation Error\n";
            return;
        }

        /*
            Window Surface
            Since Vulkan is a platform agnostic API, it can not interface directly with the window system on its own.
            To establish the connection between Vulkan and the window system to present results to the screen,
            we need to use the WSI (Window System Integration) extensions.
            The surface in our program will be backed by the window that we've already opened with GLFW,
            which extensions have already been requested through the line: createInfo.ppEnabledExtensionNames = glfwExtensions;
        */

        if (glfwCreateWindowSurface(this->instance, this->window, nullptr, &this->surface) != VK_SUCCESS) {
            std::cerr << "VkSurfaceKHR Creation Error\n";
            return;
        }

        // Once the instance has been created, need to pick a physical device to run on (GPU)
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (!deviceCount) { std::cerr << "Failed to find GPUs with Vulkan support\n"; return; }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];

        std::cout << "\n Available Devices:\n";
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures2 deviceFeatures2;
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

        for (const auto& device : devices) {
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            deviceFeatures2 = {};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &dynamicRenderingFeatures;
            vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);
            std::cout << "\n - " << deviceProperties.deviceName;

            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                // Dedicated Hardware
                std::cout << " (Discrete)";
                physicalDevice = device;
            }

            if (dynamicRenderingFeatures.dynamicRendering) {
                std::cout << " (Core Dyn. Rendering)";
            }

            // Check if device extensions (like swap chain) are available
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(this->deviceExtensions.begin(), this->deviceExtensions.end());
            for (const auto& extension : availableExtensions) requiredExtensions.erase(extension.extensionName);

            if(requiredExtensions.empty()) std::cout << " (Extensions Available)";

            uint32_t apiVersion = deviceProperties.apiVersion;
            uint32_t major = VK_VERSION_MAJOR(apiVersion);
            uint32_t minor = VK_VERSION_MINOR(apiVersion);
            uint32_t patch = VK_VERSION_PATCH(apiVersion);

            std::cout << " (" << major << "." << minor << "." << patch << ")";

            std::cout << std::endl;
        }
        std::cout << std::endl;

        // Check supported Queue Families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        uint32_t graphicsQueueFamilyIndex = 0, presentQueueFamilyIndex = 0, computeQueueFamilyIndex = 0;
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamilyIndex = i;
                std::cout << " Queue family " << i << " supports graphics operations\n";
            };
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                computeQueueFamilyIndex = i;
                std::cout << " Queue family " << i << " supports compute operations\n";
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, this->surface, &presentSupport);
            if (presentSupport) {
                presentQueueFamilyIndex = i;
                std::cout << " Queue family " << i << " supports window surface\n";
            }
        }

        
        // Create queues of any family
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> queueFamilyIndicies = { graphicsQueueFamilyIndex, presentQueueFamilyIndex, computeQueueFamilyIndex };
        for (uint32_t queueFamilyIndex : queueFamilyIndicies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            float queuePriority = 1.0f;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Enable Dynamic Rendering extension on device
        dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.features = {};
        deviceFeatures2.pNext = &dynamicRenderingFeatures;

        //VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamicRenderingFeatures;
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.queueCreateInfoCount = queueCreateInfos.size();

        deviceInfo.pEnabledFeatures = nullptr;
        deviceInfo.pNext = &deviceFeatures2;
        deviceInfo.enabledExtensionCount = this->deviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = this->deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &this->device) != VK_SUCCESS) {
            std::cerr << "VkDevice Creation Error\n";
            return;
        }

        vkGetDeviceQueue(this->device, graphicsQueueFamilyIndex, 0, &this->graphicsQueue); // 0 because we created only 1 queue of this family
        vkGetDeviceQueue(this->device, presentQueueFamilyIndex, 0, &this->presentQueue);

        /*
            SWAP CHAIN
            Vulkan does not have the concept of a "default framebuffer", hence it requires an infrastructure
            that will own the buffers we will render to before we visualize them on the screen.
            This infrastructure is known as the swap chain and must be created explicitly in Vulkan.
            The swap chain is essentially a queue of images that are waiting to be presented to the screen.
            Our application will acquire such an image to draw to it, and then return it to the queue.
            How exactly the queue works and the conditions for presenting an image from the queue depend
            on how the swap chain is set up, but the general purpose of the swap chain is to synchronize the presentation
            of images with the refresh rate of the screen.
            
            Just checking if a swap chain is available is not sufficient, because it may not actually be compatible with our window surface
            There are basically three kinds of properties we need to check:

                - Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
                - Surface formats (pixel format, color space)
                - Available presentation modes

            In order to determine the settings for the best possible swap chain, there are three types to choose:
                - Surface format (color depth)
                - Presentation mode (conditions for "swapping" images to the screen)
                - Swap extent (resolution of images in swap chain)

            The presentation mode is arguably the most important setting for the swap chain,
            because it represents the actual conditions for showing images to the screen.
            There are four possible modes available in Vulkan:

                - VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
                - VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
                - VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
                - VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.

            Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available

            The swap extent is the resolution of the swap chain images
            and it's almost always exactly equal to the resolution of the window that we're drawing to in pixels
        */

        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, this->surface, &capabilities);
        
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->surface, &formatCount, nullptr);

        if (formatCount) {
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, this->surface, &formatCount, formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->surface, &presentModeCount, nullptr);

        if (presentModeCount) {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, this->surface, &presentModeCount, presentModes.data());
        }

        if (!formatCount || !presentModeCount) {
            std::cerr << "Chosen physical device swap chain doesn't support current window surface or is not adequate\n";
            return;
        }

        VkPresentModeKHR presentMode = presentModes[0];
        this->surfaceFormat = formats[0];
        this->extent = capabilities.currentExtent;

        for (const auto& sFormat : formats)
            if(sFormat.format == VK_FORMAT_B8G8R8A8_SRGB && sFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                this->surfaceFormat = sFormat;
                break;
            }
       
        for (const auto& pMode : presentModes)
            if (pMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = pMode;
                break;
            }

        if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            this->extent = actualExtent;
        }

        uint32_t imageCount = MAX_FRAMES_IN_FLIGHT; // How many images to have in the swap chain
        //(capabilities.maxImageCount > 0 && capabilities.minImageCount + 1 > capabilities.maxImageCount) ?
        //capabilities.maxImageCount : capabilities.minImageCount + 1; 

        VkSwapchainCreateInfoKHR swapChainInfo{};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.surface = this->surface;
        swapChainInfo.minImageCount = imageCount;
        swapChainInfo.imageFormat = this->surfaceFormat.format;
        swapChainInfo.imageColorSpace = this->surfaceFormat.colorSpace;
        swapChainInfo.imageExtent = this->extent;
        swapChainInfo.imageArrayLayers = 1; // The imageArrayLayers specifies the amount of layers each image consists of. This is always 1 unless you are developing a stereoscopic 3D application
        // The imageUsage bit field specifies what kind of operations we'll use the images in the swap chain for.
        // Because we're going to render directly to them, it means that they're used as color attachment.
        // It is also possible that you'll render images to a separate image first to perform operations like post-processing.
        // In that case you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use a memory operation to transfer the rendered image to a swap chain image.
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainInfo.preTransform = capabilities.currentTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.presentMode = presentMode;
        swapChainInfo.clipped = VK_TRUE;
        swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
            swapChainInfo.queueFamilyIndexCount = sizeof(queueFamilyIndices)/sizeof(uint32_t);
            swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapChainInfo.queueFamilyIndexCount = 0; // Optional
            swapChainInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        if (vkCreateSwapchainKHR(this->device, &swapChainInfo, nullptr, &this->swapChain) != VK_SUCCESS) {
            std::cerr << "VkSwapchainKHR Creation Error\n";
            return;
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        /*
            To use any VkImage, including those in the swap chain,
            in the render pipeline we have to create a VkImageView object.
            An image view is quite literally a view into an image.
            It describes how to access the image and which part of the image to access,
            for example if it should be treated as a 2D texture depth texture without any mipmapping levels.
        */

        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo imageViewInfo{};
            imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewInfo.image = swapChainImages[i];
            imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewInfo.format = swapChainInfo.imageFormat;
            imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewInfo.subresourceRange.baseMipLevel = 0;
            imageViewInfo.subresourceRange.levelCount = 1;
            imageViewInfo.subresourceRange.baseArrayLayer = 0;
            imageViewInfo.subresourceRange.layerCount = 1;
            
            if(vkCreateImageView(this->device, &imageViewInfo, nullptr, &this->swapChainImageViews[i]) != VK_SUCCESS) {
                std::cerr << "VkImageView Creation Error\n";
                return;
            }

            /*
                An image view is sufficient to start using an image as a texture, 
                but it's not quite ready to be used as a render target just yet. 
                That requires one more step of indirection, known as a framebuffer
            */

            // Graphics Pipeline
            /*
                The graphics pipeline in Vulkan is almost completely immutable,
                so you must recreate the pipeline from scratch if you want to change shaders,
                bind different framebuffers or change the blend function.
                The disadvantage is that you'll have to create a number of pipelines
                that represent all of the different combinations of states you want to use in your rendering operations.
                However, because all of the operations you'll be doing in the pipeline are known in advance,
                the driver can optimize for it much better.
                
                While most of the pipeline state needs to be baked into the pipeline state,
                a limited amount of the state can actually be changed without recreating the pipeline at draw time.
                Examples are the size of the viewport, line width and blend constants.

                NOTE: In Vulakn 1.3 Dynamic State pipelines are almost fully covered, 
                      (VK_EXT_extended_dynamic_state1, VK_EXT_extended_dynamic_state2, VK_EXT_extended_dynamic_state3)
                      that means that the concept of a immutable pipeline is not accurate anymore.
                      But we'll follow the old way just to get the full vulkan experience
                        
            */

            // Gotta change cmake.txt
            // char buffer[FILENAME_MAX];
            // _getcwd(buffer, FILENAME_MAX);
            // std::cout << "Current working directory: " << buffer << std::endl;
            
            // Shaders
            system(RESOURCE("Shaders\\runtime_compile.bat"));

            std::ifstream vsFile(RESOURCE("Shaders\\vert.spv"), std::ios::ate | std::ios::binary);
            if(!vsFile.is_open()) { std::cerr << "Failed to read vertex shader file\n"; return; }
            std::ifstream fsFile(RESOURCE("Shaders\\frag.spv"), std::ios::ate | std::ios::binary);
            if (!fsFile.is_open()) { std::cerr << "Failed to load fragment shader file\n"; return; }
            
            size_t vsFileSize = static_cast<size_t>(vsFile.tellg()), fsFileSize = static_cast<size_t>(fsFile.tellg());
            std::vector<char> vsBuffer(vsFileSize);
            std::vector<char> fsBuffer(fsFileSize);

            vsFile.seekg(0);
            vsFile.read(vsBuffer.data(), vsFileSize);
            fsFile.seekg(0);
            fsFile.read(fsBuffer.data(), fsFileSize);

            vsFile.close();
            fsFile.close();

            // Before we can pass the code to the pipeline, we have to wrap it in a VkShaderModule object
            VkShaderModule vsShaderModule;
            VkShaderModuleCreateInfo vsShaderModuleInfo{};
            vsShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vsShaderModuleInfo.codeSize = vsBuffer.size();
            vsShaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vsBuffer.data());
            if (vkCreateShaderModule(this->device, &vsShaderModuleInfo, nullptr, &vsShaderModule) != VK_SUCCESS) { std::cerr << "Failed to create VkShaderModule (vertex)\n"; return; }

            VkShaderModule fsShaderModule;
            VkShaderModuleCreateInfo fsShaderModuleInfo{};
            fsShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            fsShaderModuleInfo.codeSize = fsBuffer.size();
            fsShaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fsBuffer.data());
            if (vkCreateShaderModule(this->device, &fsShaderModuleInfo, nullptr, &fsShaderModule) != VK_SUCCESS) { std::cerr << "Failed to create VkShaderModule (fragment)\n"; return; }

            // To actually use the shaders we'll need to assign them to a specific pipeline stage through VkPipelineShaderStageCreateInfo structures as part of the actual pipeline creation process.
            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vsShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fsShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

            // Dynamic State
            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
            dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicStateInfo.pDynamicStates = dynamicStates.data();

            // Vertex Input Layout
            /*
                The VkPipelineVertexInputStateCreateInfo structure describes the format of the vertex data that will be passed to the vertex shader.
                It describes this in roughly two ways:

                    Bindings: spacing between data and whether the data is per-vertex or per-instance (see instancing)
                    Attribute descriptions: type of the attributes passed to the vertex shader, which binding to load them from and at which offset

                Because we're hard coding the vertex data directly in the vertex shader,
                we'll fill in this structure to specify that there is no vertex data to load for now.
            */

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;

            // The VkPipelineInputAssemblyStateCreateInfo struct describes two things:
            // what kind of geometry will be drawn from the vertices and if primitive restart should be enabled (aka. index buffer/EBO)

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
            inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

            this->viewport.x = 0.0f;
            this->viewport.y = 0.0f;
            this->viewport.width = (float)this->extent.width;
            this->viewport.height = (float)this->extent.height;
            this->viewport.minDepth = 0.0f;
            this->viewport.maxDepth = 1.0f;

            this->scissor.offset = { 0, 0 };
            this->scissor.extent = this->extent;

            VkPipelineViewportStateCreateInfo viewportStateInfo{};
            viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportStateInfo.viewportCount = 1;
            viewportStateInfo.scissorCount = 1;

            /*
                The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and
                turns it into fragments to be colored by the fragment shader. It also performs depth testing,
                face culling and the scissor test, and it can be configured to output fragments that
                fill entire polygons or just the edges (wireframe rendering).
                All this is configured using the VkPipelineRasterizationStateCreateInfo structure.
            */

            VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
            rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizerInfo.depthClampEnable = VK_FALSE;
            rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // Do not discard geometry
            rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizerInfo.lineWidth = 1.0f;
            rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterizerInfo.depthBiasEnable = VK_FALSE;
            rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
            rasterizerInfo.depthBiasClamp = 0.0f; // Optional
            rasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional

            /*
                Multisampling
                The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing.
                It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel
            */

            VkPipelineMultisampleStateCreateInfo multisamplingInfo{}; // Disabled for now
            multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisamplingInfo.sampleShadingEnable = VK_FALSE;
            multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisamplingInfo.minSampleShading = 1.0f; // Optional
            multisamplingInfo.pSampleMask = nullptr; // Optional
            multisamplingInfo.alphaToCoverageEnable = VK_FALSE; // Optional
            multisamplingInfo.alphaToOneEnable = VK_FALSE; // Optional

            /*
                Color blending
                After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
                This transformation is known as color blending and there are two ways to do it:

                    Mix the old and new value to produce a final color
                    Combine the old and new value using a bitwise operation

                There are two types of structs to configure color blending.
                VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer
                VkPipelineColorBlendStateCreateInfo contains the global color blending settings


                There's this line in the fragment shader file: "layout(location = 0) out vec4 outColor;"

                The "0" there means the first framebuffer attachment. But you can have multiple of these lines,
                numbered from 0 to 7, I think (8 in total). So in the same shader you could render to multiple of these attachments,
                for example for Deferred Rendering. "Deferred" means putting it off to a later time,
                sometime later during the frame lifetime before presenting the finished image to the screen.
                Multiple framebuffer attachments would make up what's commonly known as a "G-buffer" ("g" for geometric)
                where you render the normals, frag pos, material id, albedo, specular, roughness, metallic, velocity buffer... whatever you want,
                all within the same shader, each to their own framebuffer attachment. Then read from each of these (as textures)
                later on towards the end of the frame lifetime where you combine them (using a different shader) to produce the final pixel on the screen,
                to save on (potentially) expensive lighting calculations. Deferred Rendering is often used because with Forward Rendering
                you'd be throwing away a lot of (potentially) expensive lighting calculations after they are discarded by
                the rasterizer stage using the depth buffer (fragments behind other fragments), so you'd "defer" these calculations to later where they will only run once,
                for just the visible fragments. LearnOpenGL.com has a nice chapter on it.
            */

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE; // VK_TRUE
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // VK_BLEND_FACTOR_SRC_ALPHA
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo colorBlendingInfo{}; // Array of structures for all of the framebuffers
            colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendingInfo.logicOpEnable = VK_FALSE;
            colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY;
            colorBlendingInfo.attachmentCount = 1;
            colorBlendingInfo.pAttachments = &colorBlendAttachment;
            colorBlendingInfo.blendConstants[0] = 0.0f;
            colorBlendingInfo.blendConstants[1] = 0.0f;
            colorBlendingInfo.blendConstants[2] = 0.0f;
            colorBlendingInfo.blendConstants[3] = 0.0f;

            /*
                Pipeline layout
                You can use uniform values in shaders. These uniform values need to be specified during pipeline creation by creating a VkPipelineLayout object.
                Even though we won't be using them until a future chapter, we are still required to create an empty pipeline layout
            */

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 0;
            pipelineLayoutInfo.pSetLayouts = nullptr;
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout) != VK_SUCCESS) { 
                std::cerr << "Failed to create VkCreatePipelineLayout\n";
                return;
            }

            // Dynamic Rendering VS Renderpasses
            // Framebuffers and Renderpasses: Classic way do deal with rendering, bad nowadays, only good for mobile
            // Dynamic Rendering is the way
            /*
                For Bloom (or any postprocessing):

                Bloom typically involves multiple render passes:
                    Render the scene to a high dynamic range (HDR) image
                    Extract bright areas => blur => combine back
                In traditional Vulkan:
                    Each pass would need a VkFramebuffer
                    Each framebuffer is tied to a VkRenderPass, image view, size, etc.
                With Dynamic Rendering
                    Create render targets as VkImage + VkImageView
                    Specify which image view to use as a color attachment in VkRenderingAttachmentInfo
                    Begin/End rendering blocks per pass

                There is no framebuffer — you "build" the framebuffer at runtime.
            */

            VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
            pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            pipelineRenderingInfo.colorAttachmentCount = 1;
            pipelineRenderingInfo.pColorAttachmentFormats = &this->surfaceFormat.format;

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.pNext = &pipelineRenderingInfo; // this is essential for dynamic rendering!
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
            pipelineInfo.pViewportState = &viewportStateInfo;
            pipelineInfo.pRasterizationState = &rasterizerInfo;
            pipelineInfo.pMultisampleState = &multisamplingInfo;
            pipelineInfo.pDepthStencilState = nullptr;
            pipelineInfo.pColorBlendState = &colorBlendingInfo;
            pipelineInfo.pDynamicState = &dynamicStateInfo;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = VK_NULL_HANDLE;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = -1;

            if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->graphicsPipeline) != VK_SUCCESS) {
                std::cerr << "Failed to create VkCreatePipeline\n";
                return;
            }

            vkDestroyShaderModule(device, vsShaderModule, nullptr);
            vkDestroyShaderModule(device, fsShaderModule, nullptr);

            // Rendering

            /*
                Command Buffers
                Commands in Vulkan, like drawing operations and memory transfers, are not executed directly using function calls.
                You have to record all of the operations you want to perform in command buffer objects.
                The advantage of this is that when we are ready to tell the Vulkan what we want to do,
                all of the commands are submitted together and Vulkan can more efficiently process the commands
                since all of them are available together. In addition, this allows command recording to
                happen in multiple threads if so desired.

                We have to create a command pool before we can create command buffers.
                Command pools manage the memory that is used to store the buffers
                and command buffers are allocated from them

                Flags:
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
                
                Command buffers are executed by submitting them on one of the device queues,
                like the graphics and presentation queues we retrieved.
                Each command pool can only allocate command buffers that are submitted on a single type of queue.
                We're going to record commands for drawing, which is why we've chosen the graphics queue family
            */


            VkCommandPoolCreateInfo cmdPoolInfo{};
            cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            cmdPoolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

            if (vkCreateCommandPool(this->device, &cmdPoolInfo, nullptr, &this->commandPool) != VK_SUCCESS) {
                std::cerr << "Failed to create VkCreateCommandPool\n";
                return;
            }


            /*
                Command buffer allocation
                Command buffers will be automatically freed when their command pool is destroyed, so we don't need explicit cleanup.
            
                The level parameter specifies if the allocated command buffers are primary or secondary command buffers.

                    VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
                    VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.

                You can imagine that it's helpful to reuse common operations from primary command buffers.
                Since we are only allocating one command buffer, the commandBufferCount parameter is just one
            */

            VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
            cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufferAllocInfo.commandPool = this->commandPool;
            cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdBufferAllocInfo.commandBufferCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;

            if (vkAllocateCommandBuffers(this->device, &cmdBufferAllocInfo, this->commandBuffers) != VK_SUCCESS) {
                std::cerr << "Failed to allocate with VkAllocateCommandBuffers\n";
                return;
            }

            /*
                Synchronization

                A core design philosophy in Vulkan is that synchronization of execution on the GPU is explicit.
                The order of operations is up to us to define using various synchronization primitives
                which tell the driver the order we want things to run in.
                This means that many Vulkan API calls which start executing work on the GPU are asynchronous,
                the functions will return before the operation has finished.

                There are a number of events that we need to order explicitly because they happen on the GPU, such as:

                    Acquire an image from the swap chain
                    Execute commands that draw onto the acquired image
                    Present that image to the screen for presentation, returning it to the swapchain

                Each of these events is set in motion using a single function call, but are all executed asynchronously.
                The function calls will return before the operations are actually finished and the order of execution is also undefined.
                That is unfortunate, because each of the operations depends on the previous one finishing.
                Thus we need to explore which primitives we can use to achieve the desired ordering.
            

                Semaphores

                A semaphore is used to add order between queue operations.
                Queue operations refer to the work we submit to a queue,
                either in a command buffer or from within a function as we will see later.
                Examples of queues are the graphics queue and the presentation queue.
                Semaphores are used both to order work inside the same queue and between different queues.

                There happens to be two kinds of semaphores in Vulkan, binary and timeline.

                A semaphore is either unsignaled or signaled. It begins life as unsignaled.


                Fences

                A fence has a similar purpose, in that it is used to synchronize execution,
                but it is for ordering the execution on the CPU, otherwise known as the host.
                Simply put, if the host needs to know when the GPU has finished something, we use a fence.

                What to choose?

                We have two synchronization primitives to use and conveniently two places to apply synchronization:
                Swapchain operations and waiting for the previous frame to finish.
                We want to use semaphores for swapchain operations because they happen on the GPU,
                thus we don't want to make the host wait around if we can help it.
                For waiting on the previous frame to finish, we want to use fences for the opposite reason,
                because we need the host to wait. This is so we don't draw more than one frame at a time.
                Because we re-record the command buffer every frame,
                we cannot record the next frame's work to the command buffer until the current frame has finished executing,
                as we don't want to overwrite the current contents of the command buffer while the GPU is using it.
        
                ULTIMATE SUM UP:
                    SEMAPHORE: You want the GPU to wait
                    FENCES: You want the CPU to wait
            */

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Workaround to avoid vkWaitForFences block the first frame forever

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, this->imageAvailableSemaphores+i) != VK_SUCCESS ||
                    vkCreateSemaphore(this->device, &semaphoreInfo, nullptr, this->renderFinishedSemaphores+i) != VK_SUCCESS ||
                    vkCreateFence(this->device, &fenceInfo, nullptr, this->inFlightFences+i) != VK_SUCCESS) {
                    std::cerr << "Failed to create a VkSemaphore or VkFence\n";
                    return;
                }
            }
        }


    }

    void mainLoop() {
        /*
            Outline of a frame

            At a high level, rendering a frame in Vulkan consists of a common set of steps:

                Wait for the previous frame to finish
                Acquire an image from the swap chain
                Record a command buffer which draws the scene onto that image
                Submit the recorded command buffer
                Present the swap chain image
        */
    
        uint32_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            vkWaitForFences(device, 1, this->inFlightFences+currentFrame, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, this->inFlightFences+currentFrame);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(this->device, this->swapChain, UINT64_MAX, this->imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

            vkResetCommandBuffer(this->commandBuffers[currentFrame], 0);

            /*
                Command buffer recording
                The flags parameter specifies how we're going to use the command buffer. The following values are available:

                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
                VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
                VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.

                The pInheritanceInfo parameter is only relevant for secondary command buffers.
                It specifies which state to inherit from the calling primary command buffers.

                If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
                It's not possible to append commands to a buffer at a later time.
            */

            VkCommandBufferBeginInfo cmdBufferBeginInfo{};
            cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmdBufferBeginInfo.flags = 0;
            cmdBufferBeginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(this->commandBuffers[currentFrame], &cmdBufferBeginInfo) != VK_SUCCESS) {
                std::cerr << "Failed to record VkBeginCommandBuffer\n";
                return;
            }

            VkImageMemoryBarrier barrier{}; // Transition of Layouts
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = this->swapChainImages[imageIndex];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                this->commandBuffers[currentFrame],
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );


            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = this->swapChainImageViews[imageIndex];
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
            colorAttachment.clearValue = clearColor;

            VkRenderingInfo renderingInfo{};
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            renderingInfo.renderArea.offset = { 0, 0 };
            renderingInfo.renderArea.extent = this->extent;
            renderingInfo.layerCount = 1;
            renderingInfo.colorAttachmentCount = 1;
            renderingInfo.pColorAttachments = &colorAttachment;

            vkCmdBeginRendering(this->commandBuffers[currentFrame], &renderingInfo);

            // Record draw commands here
            vkCmdBindPipeline(this->commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphicsPipeline);

            // bind vertex buffers, descriptor sets, and issue draw calls...

            vkCmdSetViewport(this->commandBuffers[currentFrame], 0, 1, &this->viewport);
            vkCmdSetScissor(this->commandBuffers[currentFrame], 0, 1, &this->scissor);

            vkCmdDraw(this->commandBuffers[currentFrame], 3, 1, 0, 0);

            vkCmdEndRendering(this->commandBuffers[currentFrame]);

            //VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = this->swapChainImages[imageIndex];  // <- The swapchain image used this frame
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(
                this->commandBuffers[currentFrame],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            if (vkEndCommandBuffer(this->commandBuffers[currentFrame]) != VK_SUCCESS) {
                std::cerr << "Failed to record VkEndCommandBuffer\n";
                return;
            }

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { this->imageAvailableSemaphores[currentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = this->commandBuffers+currentFrame;
            VkSemaphore signalSemaphores[] = { this->renderFinishedSemaphores[currentFrame] };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(this->graphicsQueue, 1, &submitInfo, this->inFlightFences[currentFrame]) != VK_SUCCESS) {
                std::cerr << "Failed to submit to the graphics queue on VkQueueSubmit\n";
                return;
            }

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { this->swapChain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;
            presentInfo.pResults = nullptr;

            vkQueuePresentKHR(this->presentQueue, &presentInfo);

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

            glfwPollEvents();
        }

        vkDeviceWaitIdle(this->device); // Ensures proper cleanup
    }

    void cleanup() {
        vkDeviceWaitIdle(this->device); // Ensures proper cleanup

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        for (auto imageView : swapChainImageViews)
            vkDestroyImageView(device, imageView, nullptr);
        
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    
    HelloTraingleApp app;
    app.run();

    return 0;
}
