#include <string>
#include <fstream>

//////// header log.h ///////////
namespace logging
{
	extern const std::string path;
	extern std::ofstream out;
	void flush();
}
