#include "App/App.h"

int main()
{
	try
	{
		// Config options
		AppOptions options;
		
		options.ShaderDir = R"(../Bin/)";
		options.DataDir = R"(../Data/)";
		options.AssetsDir = R"(../Data/Assets/)";
		options.ModelsDir = R"(../Data/Assets/Models/)";
		options.IblDir = R"(../Data/Assets/IBL/)";
		options.VSync = true;

		#ifdef DEBUG
			options.LoadDemoScene = false;
			options.EnabledVulkanValidationLayers = true;
		#else
			options.LoadDemoScene = true;
			options.EnabledVulkanValidationLayers = false;
		#endif

		// Run it
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
