#include "App.h"
#include "AssImpModelLoaderService.h"



int main()
{
	
	// Bootstrap and go

	
	try
	{
		

		
		AppOptions options;
		options.ShaderDir = R"(../Bin/)";
		options.AssetsDir = R"(../Source/Assets/)";

		#ifdef DEBUG
			options.EnabledVulkanValidationLayers = true;
		#else
			options.EnabledVulkanValidationLayers = false;
		#endif

		
		App app{ options };
		app.Run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
