# Define target name
set (TARGET_NAME Q2App)

include_directories (../Quake-2)

define_source_files ()

# Setup target with resource copying
setup_main_executable ()

target_link_libraries (Q2App q2client q2common q2game q2refsoft q2server q2platform)
