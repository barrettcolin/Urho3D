# Define target name
set (TARGET_NAME Quake2_RefSoft)

include_directories (../Quake-2)

define_source_files ()

# Setup target with resource copying
setup_main_executable ()

target_link_libraries (Quake2_RefSoft Refresh_Soft q2refsoft q2client q2common q2game q2server q2platform)
