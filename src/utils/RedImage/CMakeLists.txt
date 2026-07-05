project(RedImage)

set(REDIMAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(REDIMAGE_ISPC_DIR "${REDIMAGE_ROOT}/ispc_texcomp")
set(REDIMAGE_NVTT_DIR "${REDIMAGE_ROOT}/nvtt")
set(REDIMAGE_TOOL_DIR "${REDIMAGE_ROOT}/RedImageTool")

if(NOT WIN32)
    message(FATAL_ERROR "RedImage source build is currently supported only on Windows.")
endif()

set(REDIMAGE_ISPC_EXECUTABLE "${REDIMAGE_ROOT}/ISPC/win/ispc.exe")
if(NOT EXISTS "${REDIMAGE_ISPC_EXECUTABLE}")
    message(FATAL_ERROR "RedImage ISPC compiler not found: ${REDIMAGE_ISPC_EXECUTABLE}")
endif()

set(REDIMAGE_ISPC_ARCH_ARG)
if(CMAKE_VS_PLATFORM_NAME MATCHES "Win32")
    set(REDIMAGE_ISPC_ARCH_ARG --arch=x86)
endif()

function(redimage_add_ispc_kernel out_var kernel_name)
    set(kernel_source "${REDIMAGE_ISPC_DIR}/${kernel_name}.ispc")
    set(kernel_output_dir "${CMAKE_CURRENT_BINARY_DIR}/ispc/${CMAKE_CFG_INTDIR}")
    set(kernel_header_dir "${CMAKE_CURRENT_BINARY_DIR}/ispc_headers")

    set(kernel_outputs
        "${kernel_output_dir}/${kernel_name}.obj"
        "${kernel_output_dir}/${kernel_name}_sse2.obj"
        "${kernel_output_dir}/${kernel_name}_sse4.obj"
        "${kernel_output_dir}/${kernel_name}_avx.obj"
        "${kernel_output_dir}/${kernel_name}_avx2.obj"
    )

    add_custom_command(
        OUTPUT ${kernel_outputs}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${kernel_output_dir}" "${kernel_header_dir}"
        COMMAND "${REDIMAGE_ISPC_EXECUTABLE}" -O2 "${kernel_source}"
                -o "${kernel_output_dir}/${kernel_name}.obj"
                -h "${kernel_header_dir}/${kernel_name}_ispc.h"
                ${REDIMAGE_ISPC_ARCH_ARG}
                --target=sse2,sse4,avx,avx2
                --opt=fast-math
        DEPENDS "${kernel_source}" "${REDIMAGE_ISPC_EXECUTABLE}"
        VERBATIM
    )

    set_source_files_properties(${kernel_outputs} PROPERTIES
        EXTERNAL_OBJECT TRUE
        GENERATED TRUE
    )

    set(${out_var} ${kernel_outputs} PARENT_SCOPE)
endfunction()

redimage_add_ispc_kernel(REDIMAGE_KERNEL_OBJECTS kernel)
redimage_add_ispc_kernel(REDIMAGE_KERNEL_ASTC_OBJECTS kernel_astc)

add_library(RedImageISPC STATIC
    "${REDIMAGE_ISPC_DIR}/ispc_texcomp.cpp"
    "${REDIMAGE_ISPC_DIR}/ispc_texcomp_astc.cpp"
    ${REDIMAGE_KERNEL_OBJECTS}
    ${REDIMAGE_KERNEL_ASTC_OBJECTS}
)

target_include_directories(RedImageISPC PUBLIC "${REDIMAGE_ISPC_DIR}")
target_compile_definitions(RedImageISPC PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

set(REDIMAGE_NVTT_SOURCES
    "${REDIMAGE_NVTT_DIR}/bc6h/zoh.cpp"
    "${REDIMAGE_NVTT_DIR}/bc6h/zoh_utils.cpp"
    "${REDIMAGE_NVTT_DIR}/bc6h/zohone.cpp"
    "${REDIMAGE_NVTT_DIR}/bc6h/zohtwo.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode0.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode1.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode2.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode3.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode4.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode5.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode6.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_mode7.cpp"
    "${REDIMAGE_NVTT_DIR}/bc7/avpcl_utils.cpp"
    "${REDIMAGE_NVTT_DIR}/cuda/CudaCompressorDXT.cpp"
    "${REDIMAGE_NVTT_DIR}/cuda/CudaUtils.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/Debug.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/FileSystem.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/Memory.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/StrLib.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/TextWriter.cpp"
    "${REDIMAGE_NVTT_DIR}/nvcore/Timer.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/BlockDXT.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/ColorBlock.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/ColorSpace.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/DirectDrawSurface.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/ErrorMetric.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/Filter.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/FloatImage.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/Image.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/ImageIO.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/NormalMap.cpp"
    "${REDIMAGE_NVTT_DIR}/nvimage/Quantize.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Box.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Color.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Fitting.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Gamma.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Half.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Matrix.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/PackedFloat.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Plane.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/SphericalHarmonic.cpp"
    "${REDIMAGE_NVTT_DIR}/nvmath/Vector.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/Event.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/Mutex.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/nvthread.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/ParallelFor.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/Thread.cpp"
    "${REDIMAGE_NVTT_DIR}/nvthread/ThreadPool.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/BlockCompressor.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/ClusterFit.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressionOptions.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorDX10.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorDX11.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorDX9.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorDXT1.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorDXT5_RGBM.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CompressorRGB.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/Context.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/CubeSurface.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/InputOptions.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/nvtt.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/nvtt_wrapper.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/OptimalCompressDXT.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/OutputOptions.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/QuickCompressDXT.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/SingleColorLookup.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/Surface.cpp"
    "${REDIMAGE_NVTT_DIR}/nvtt/TaskDispatcher.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/alpha.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/colourblock.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/colourfit.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/colourset.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/fastclusterfit.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/maths.cpp"
    "${REDIMAGE_NVTT_DIR}/squish/weightedclusterfit.cpp"
)

add_library(RedImageNvtt STATIC ${REDIMAGE_NVTT_SOURCES})

target_include_directories(RedImageNvtt
    PUBLIC
        "${REDIMAGE_ROOT}"
        "${REDIMAGE_NVTT_DIR}"
)
target_compile_definitions(RedImageNvtt PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    NVCORE_EXPORTS
    NVIMAGE_EXPORTS
    NVMATH_EXPORTS
    NVTHREAD_EXPORTS
    NVTT_EXPORTS
)

target_link_libraries(RedImageNvtt PRIVATE RedImageISPC)

set(REDIMAGE_TOOL_SOURCES
    "${REDIMAGE_TOOL_DIR}/RedImage.cpp"
    "${REDIMAGE_TOOL_DIR}/RedImage1.cpp"
    "${REDIMAGE_TOOL_DIR}/RedImage2.cpp"
    "${REDIMAGE_TOOL_DIR}/RedImage3.cpp"
    "${REDIMAGE_TOOL_DIR}/RedImage.PCH.cpp"
    "${REDIMAGE_TOOL_DIR}/RedMemery.cpp"
    "${REDIMAGE_TOOL_DIR}/RedTextureUtils.cpp"
)

add_library(RedImageTool STATIC ${REDIMAGE_TOOL_SOURCES})
add_library(RedImage::RedImage ALIAS RedImageTool)

target_include_directories(RedImageTool
    PUBLIC
        "${REDIMAGE_ROOT}/.."
        "${REDIMAGE_TOOL_DIR}"
    PRIVATE
        "${REDIMAGE_ROOT}"
        "${REDIMAGE_ISPC_DIR}"
        "${REDIMAGE_NVTT_DIR}"
)

target_compile_definitions(RedImageTool PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(RedImageTool PUBLIC RedImageNvtt RedImageISPC)

set_target_properties(RedImageISPC RedImageNvtt RedImageTool PROPERTIES FOLDER "Utils/RedImage")

source_group(TREE "${REDIMAGE_ROOT}" FILES
    ${REDIMAGE_TOOL_SOURCES}
    ${REDIMAGE_NVTT_SOURCES}
    "${REDIMAGE_ISPC_DIR}/ispc_texcomp.cpp"
    "${REDIMAGE_ISPC_DIR}/ispc_texcomp_astc.cpp"
)
