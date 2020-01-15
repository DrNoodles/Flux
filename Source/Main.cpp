#include "VulkanTutorial.h"
#include "AssImpMeshLoader.h"

int main()
{
	std::unique_ptr<IModelLoaderService> modelLoaderService = std::make_unique<AssImpMeshLoader>();
	VulkanTutorial app(std::move(modelLoaderService));
	
	app.ShaderDir = R"(../Bin/)";
	app.AssetsDir = R"(../Source/Assets/)";
	
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
