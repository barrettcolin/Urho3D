add_definitions(-DREF_HARD_LINKED -DC_ONLY)

set(refsoft_C_FILES
  ../ref_soft/r_aclip.c
  ../ref_soft/r_alias.c
  ../ref_soft/r_bsp.c
  ../ref_soft/r_draw.c
  ../ref_soft/r_edge.c
  ../ref_soft/r_image.c
  ../ref_soft/r_light.c
  ../ref_soft/r_main.c
  ../ref_soft/r_misc.c
  ../ref_soft/r_model.c
  ../ref_soft/r_part.c
  ../ref_soft/r_poly.c
  ../ref_soft/r_polyse.c
  ../ref_soft/r_rast.c
  ../ref_soft/r_scan.c
  ../ref_soft/r_sprite.c
  ../ref_soft/r_surf.c)

set(refsoft_H_FILES
  ../ref_soft/anorms.h
  ../ref_soft/r_local.h)

add_library(q2refsoft_reference ${refsoft_C_FILES} ${refsoft_H_FILES})

