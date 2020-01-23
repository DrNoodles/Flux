#include "App.h"

int main()
{
	try
	{
		// Config options
		AppOptions options;
		options.ShaderDir = R"(../Bin/)";
		options.DataDir = R"(../Data/)";
		options.AssetsDir = R"(../Data/Assets/)";

		#ifdef DEBUG
			options.EnabledVulkanValidationLayers = true;
		#else
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
