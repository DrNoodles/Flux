#include "VulkanTutorial.h"

int main()
{
	VulkanTutorial app;
	app.ShaderDir = R"(../Bin/)";
	
	try
	{
		app.Run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
