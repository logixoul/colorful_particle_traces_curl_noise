#include "precompiled.h"
#include "my_console.h"

std::streambuf* my_console::original_cout_rdbuf;
stringstream my_console::my_cout;
