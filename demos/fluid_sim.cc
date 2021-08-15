#include <magma.h>

#include <vector>

int main(int argc, char** argv)
{
    magma::log::initLogging();

    VulkanGlobalContext ctx = {};
    initVulkanGlobalContext(
        {"VK_LAYER_KHRONOS_validation"},
        {VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
        &ctx
    );

    destroyGlobalContext(&ctx);
    
    return 0;
}