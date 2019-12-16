
#include <vulkan/vulkan.h>

#include <iostream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


int main()
{
   glfwInit();

   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

   uint32_t extensionCount = 0;
   vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

   std::cout << extensionCount << " extensions supported" << std::endl;

   glm::mat4 matrix;
   glm::vec4 vec;
   auto test = matrix * vec;

   while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
   }

   glfwDestroyWindow(window);

   glfwTerminate();

   return 0;
}
