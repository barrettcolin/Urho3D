# Define target name
set (TARGET_NAME Quake2_RefSoftC)

include_directories (../Quake-2)

define_source_files (GLOB_CPP_PATTERNS ../Q2App/*.cpp GLOB_H_PATTERNS ../Q2App/*.h)

# Setup target with resource copying
setup_main_executable ()

target_link_libraries (Quake2_RefSoftC Refresh_Soft q2refsoft_reference q2client q2common q2game q2server q2platform)
