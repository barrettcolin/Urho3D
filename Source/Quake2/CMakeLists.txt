# Set project name
project (Urho3D-Quake2)

# Find Urho3D library
find_package (Urho3D REQUIRED)
include_directories (${URHO3D_INCLUDE_DIRS})

add_subdirectory(Quake-2)
add_subdirectory(Q2App)
