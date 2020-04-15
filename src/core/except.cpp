
#include "except.h"

namespace rtplivelib {

namespace core {

uninitialized_error::uninitialized_error(const char *struct_name):
	std::runtime_error (struct_name)
{
}

uninitialized_error::uninitialized_error(const std::string &struct_name):
	std::runtime_error (struct_name)
{
	
}

func_not_implemented_error::func_not_implemented_error(const char *struct_name):
	std::runtime_error (struct_name)
{
	
}

func_not_implemented_error::func_not_implemented_error(const std::string &struct_name):
	std::runtime_error (struct_name)
{
	
}



} // namespace core

} // namespace rtplivelib
