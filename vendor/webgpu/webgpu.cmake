# This file is part of the "Learn WebGPU for C++" book.
#   https://eliemichel.github.io/LearnWebGPU
# 
# MIT License
# Copyright (c) 2022-2024 Elie Michel and the wgpu-native authors
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

include(FetchContent)

set(WEBGPU_BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, WGPU, WGPU_STATIC, DAWN and DAWN_LOCAL (it does not matter when using emcmake)")
set_property(CACHE WEBGPU_BACKEND PROPERTY STRINGS EMSCRIPTEN WGPU WGPU_STATIC DAWN DAWN_LOCAL)

# Path to a local Google Dawn build (the directory containing DawnConfig.cmake,
# e.g. ~/src/dawn/out/Release). Only used when WEBGPU_BACKEND=DAWN_LOCAL.
set(CGFX_DAWN_DIR "$ENV{HOME}/src/dawn/out/Release"
	CACHE PATH "Path to a local Dawn build (dir containing DawnConfig.cmake)")

# FetchContent's GIT_SHALLOW option is buggy and does not actually do a shallow
# clone. This macro takes care of it.
macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
	FetchContent_Declare(
		"${Name}"

		# This is what it'd look line if GIT_SHALLOW was indeed working:
		#GIT_REPOSITORY "${GitRepository}"
		#GIT_TAG        "${GitTag}"
		#GIT_SHALLOW    ON

		# Manual download mode instead:
		DOWNLOAD_COMMAND
			cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
			git init &&
			git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
			git reset --hard FETCH_HEAD
	)
endmacro()

if (NOT TARGET webgpu)
	string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)

	if (EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")

		FetchContent_DeclareShallowGit(
			webgpu-backend-emscripten
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        fa0b54d68841fb33188403b07959d403b24511de # emscripten-v3.1.61 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-emscripten)

	elseif (WEBGPU_BACKEND_U STREQUAL "WGPU")

		FetchContent_DeclareShallowGit(
			webgpu-backend-wgpu
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        54a60379a9d792848a2311856375ceef16db150e # wgpu-v0.19.4.1 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-wgpu)

	elseif (WEBGPU_BACKEND_U STREQUAL "WGPU_STATIC")

		FetchContent_DeclareShallowGit(
			webgpu-backend-wgpu-static
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        992fef64da25072ebe3844a73f7103105e7fd133 # wgpu-static-v0.19.4.1 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-wgpu-static)

	elseif (WEBGPU_BACKEND_U STREQUAL "DAWN")

		FetchContent_DeclareShallowGit(
			webgpu-backend-dawn
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        f49f0f3f6784a86a85944600d66f743e0c7eb4a9 # dawn-6536 + fix
		)
		FetchContent_MakeAvailable(webgpu-backend-dawn)

	elseif (WEBGPU_BACKEND_U STREQUAL "DAWN_LOCAL")

		# Use a locally-built Google Dawn (modern webgpu.h) instead of the
		# bundled distribution. We wire it manually rather than via
		# find_package(Dawn): Dawn's generated DawnConfig.cmake points at a
		# DawnTargets.cmake that lives in a different directory in the build
		# tree, so the package is not self-consistent. The monolithic static
		# library and headers are at stable, known paths.
		find_package(Threads REQUIRED)

		# CGFX_DAWN_DIR = <dawn>/out/<Config>; the source tree is two levels up.
		get_filename_component(CGFX_DAWN_SRC_ROOT "${CGFX_DAWN_DIR}/../.." ABSOLUTE)

		set(CGFX_DAWN_LIB "${CGFX_DAWN_DIR}/src/dawn/native/libwebgpu_dawn.a")
		if (NOT EXISTS "${CGFX_DAWN_LIB}")
			message(FATAL_ERROR
				"DAWN_LOCAL: ${CGFX_DAWN_LIB} not found. Set -DCGFX_DAWN_DIR to a "
				"Dawn build output dir (the one containing src/dawn/native/libwebgpu_dawn.a).")
		endif()

		add_library(webgpu INTERFACE)
		target_include_directories(webgpu INTERFACE
			"${CGFX_DAWN_SRC_ROOT}/include"     # webgpu/webgpu.h wrapper
			"${CGFX_DAWN_DIR}/gen/include")     # generated dawn/webgpu.h
		# Dawn is C++; a C executable must also pull in the C++ runtime and the
		# platform deps Dawn needs (threads, dl, rt, math).
		target_link_libraries(webgpu INTERFACE
			"${CGFX_DAWN_LIB}" Threads::Threads ${CMAKE_DL_LIBS} stdc++ m rt)
		# Signals the Dawn code paths; the modern-vs-legacy header split is
		# resolved in C via CGFX_WEBGPU_MODERN (see cgfx_webgpu.h).
		target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_DAWN)

		# Dawn is linked statically here, so there is no runtime library to
		# copy next to the executables. Provide a no-op so callers that invoke
		# target_copy_webgpu_binaries() still work unchanged.
		function(target_copy_webgpu_binaries Target)
		endfunction()

	else()

		message(FATAL_ERROR "Invalid value for WEBGPU_BACKEND: possible values are EMSCRIPTEN, WGPU, WGPU_STATIC, DAWN and DAWN_LOCAL, but '${WEBGPU_BACKEND_U}' was provided.")

	endif()
endif()
