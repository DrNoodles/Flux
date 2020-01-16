#include "App.h"
#include "AssImpModelLoaderService.h"

int main()
{
	// Bootstrap and go
	
	try
	{
		std::unique_ptr<IModelLoaderService> modelLoaderService = std::make_unique<AssimpModelLoaderService>();

		AppOptions options;
		options.ShaderDir = R"(../Bin/)";
		options.AssetsDir = R"(../Source/Assets/)";

		#ifdef DEBUG
			options.EnabledVulkanValidationLayers = true;
		#else
			options.EnabledVulkanValidationLayers = false;
		#endif

		
		App app{ options, std::move(modelLoaderService) };
		app.Run();
	}
	catch (const std::exception & e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
