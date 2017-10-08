# Define target name
set (TARGET_NAME OpenVR)

# Define source files
define_source_files (GLOB_CPP_PATTERNS src/*.cpp src/vrcommon/*.cpp GLOB_H_PATTERNS headers/*.h)

# Define dependency libs
set (INCLUDE_DIRS headers src src/json src/vrcommon)

add_definitions (-DVR_API_NODLL -DVR_API_PUBLIC)

# Check if 32 or 64 bit system.
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PROCESSOR_ARCH "64")
else()
  set(PROCESSOR_ARCH "32")
endif()

# Get platform.
if(WIN32)
    if(PROCESSOR_ARCH MATCHES "64")
        add_definitions(-DWIN64)
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
        add_definitions(-DLINUX -DPOSIX)
        if(PROCESSOR_ARCH MATCHES "64")
            add_definitions(-DLINUX64)
        endif()
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*" OR CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
        add_definitions(-DOSX -DPOSIX)
        if(BUILD_UNIVERSAL)
            set(CMAKE_OSX_ARCHITECTURES "i386;x86_64")
        endif()
        if(BUILD_OSX_I386)
            set(PROCESSOR_ARCH "32")
            set(CMAKE_OSX_ARCHITECTURES "i386")
        endif()
    endif()
endif()

# Setup target
setup_library ()

# Install headers for building the Urho3D library
install_header_files (DIRECTORY headers/ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/openvr FILES_MATCHING PATTERN *.h BUILD_TREE_ONLY)  # Note: the trailing slash is significant
