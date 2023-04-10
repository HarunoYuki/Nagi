#pragma once

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#define Error(err) {\
		printf("Error: %s\n", std::string(err).c_str());\
		exit(1);}