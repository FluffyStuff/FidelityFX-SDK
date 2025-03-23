project(Fluid)

add_library(fluid_native STATIC IMPORTED)
set_target_properties(fluid_native PROPERTIES
					IMPORTED_CONFIGURATIONS "DebugDX12;DebugVK;RelWithDebInfoDX12;RelWithDebInfoVK;ReleaseDX12;ReleaseVK"
					IMPORTED_LOCATION_DEBUGDX12 "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}d.lib"
					IMPORTED_LOCATION_DEBUGVK "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}d.lib"
					IMPORTED_LOCATION_RELEASEDX12 "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}.lib"
					IMPORTED_LOCATION_RELEASEVK "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}.lib"
					IMPORTED_LOCATION_RELWITHDEBINFODX12 "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}drel.lib"
					IMPORTED_LOCATION_RELWITHDEBINFOVK "${SDK_ROOT}/bin/ffx_sdk/fluid_${CMAKE_GENERATOR_PLATFORM}drel.lib")

set(fluid_src
	${CMAKE_CURRENT_SOURCE_DIR}/fluidframework.h
	${CMAKE_CURRENT_SOURCE_DIR}/fluidframework.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/fluidrendermodule.h
	${CMAKE_CURRENT_SOURCE_DIR}/fluidrendermodule.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/fluiddebugrendermodule.h
	${CMAKE_CURRENT_SOURCE_DIR}/fluiddebugrendermodule.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/fluidparticlesystem.h
	${CMAKE_CURRENT_SOURCE_DIR}/fluidparticlesystem.cpp)

set(fluid_shaders
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/fluid.hlsli
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/sdf.hlsl
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/sdfdebug.hlsl
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/tiling.hlsl)
set_source_files_properties(${fluid_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${fluid_shaders}" ${SHADER_OUTPUT})

set(config_file ${CMAKE_CURRENT_SOURCE_DIR}/config/fluidconfig.json)
copyCommand("${config_file}" ${CONFIG_OUTPUT})

add_executable(${PROJECT_NAME} WIN32 ${default_icon_src} ${default_entry_point} ${fluid_src} ${config_file} ${fluid_shaders})

set(EXE_OUT_NAME ${PROJECT_NAME}_)

target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Framework RenderModules d3dcompiler backend_interface)
set_target_properties(${PROJECT_NAME} PROPERTIES
						OUTPUT_NAME_DEBUGDX12 "${EXE_OUT_NAME}DX12D"
						OUTPUT_NAME_DEBUGVK "${EXE_OUT_NAME}VKD"
						OUTPUT_NAME_RELEASEDX12 "${EXE_OUT_NAME}DX12"
						OUTPUT_NAME_RELEASEVK "${EXE_OUT_NAME}VK"
						OUTPUT_NAME_RELWITHDEBINFODX12 "${EXE_OUT_NAME}drelDX12"
						OUTPUT_NAME_RELWITHDEBINFOVK "${EXE_OUT_NAME}drelVK"
						VS_DEBUGGER_WORKING_DIRECTORY "${BIN_OUTPUT}")

target_compile_definitions(${PROJECT_NAME} PRIVATE SampleName=L"Fluid"
											FrameworkIncludePath="../fluid/fluidframework.h"
											ConfigFileName=L"configs/fluidconfig.json"
											ConfigSampleName="Fluid"
											RenderModuleName=Fluid
											)

addManifest(${PROJECT_NAME})

source_group(""							FILES ${default_entry_point})
source_group("Icon"						FILES ${default_icon_src})
source_group("Config"					FILES ${config_file})
source_group("RenderModule"				FILES ${fluid_src})
source_group("RenderModule\\Shaders"	FILES ${fluid_shaders})

set(START_PROJECT Fluid PARENT_SCOPE)
