#include "App.h"
#include "AssImpModelLoaderService.h"

int main()
{
	// Bootstrappin'
	
	std::unique_ptr<IModelLoaderService> modelLoaderService = std::make_unique<AssimpModelLoaderService>();

	AppOptions options;
	options.ShaderDir = R"(../Bin/)";
	options.AssetsDir = R"(../Source/Assets/)";
	
	App app{ options, std::move(modelLoaderService) };
	
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
