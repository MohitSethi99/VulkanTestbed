#pragma once

#ifdef _WIN32
	#ifndef NOMINMAX
		// See github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-c4003-the-min-the-max-and-the-macro
		#define NOMINMAX
	#endif
#endif

#include <sstream>

#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <map>

#include "Core/Log.h"
#include "Core/Assert.h"
