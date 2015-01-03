add_definitions(-DREF_HARD_LINKED -DC_ONLY)

set(refsoft_CPP_FILES
  r_aclip.c
  r_alias.c
  r_bsp.c
  r_draw.c
  r_edge.c
  r_image.c
  r_light.c
  r_main.c
  r_misc.c
  r_model.c
  r_part.c
  r_poly.c
  r_polyse.c
  r_rast.c
  r_scan.c
  r_sprite.c
  r_surf.c)

set(refsoft_H_FILES
  anorms.h
  r_local.h)

add_library(q2refsoft ${refsoft_CPP_FILES} ${refsoft_H_FILES})
