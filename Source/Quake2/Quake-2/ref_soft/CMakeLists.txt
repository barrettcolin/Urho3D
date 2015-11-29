add_definitions(-DREF_HARD_LINKED)

if(UNIX)
  enable_language(ASM)
  add_definitions(-DELF)

  set(refsoft_ASM_FILES
    ../linux/qasm.h
    ../linux/d_polysa.S
    ../linux/r_aclipa.S
    ../linux/r_draw16.S
    ../linux/r_drawa.S
    ../linux/r_edgea.S
    ../linux/r_scana.S
    ../linux/r_spr8.S
    ../linux/r_surf8.S
    ../linux/r_varsa.S
    ../linux/sys_dosa.S
  )
else()
  enable_language(ASM_MASM)
  set(refsoft_ASM_FILES
    d_if.inc
    qasm.inc
    r_polysa.asm
    r_aclipa.asm
    r_draw16.asm
    r_drawa.asm
    r_edgea.asm
    r_scana.asm
    r_spr8.asm
    r_surf8.asm
    r_varsa.asm
  )
  set_source_files_properties(${refsoft_ASM_FILES} PROPERTIES COMPILE_FLAGS /safeseh)
endif()

set(refsoft_C_FILES
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

add_library(q2refsoft ${refsoft_C_FILES} ${refsoft_ASM_FILES} ${refsoft_H_FILES})

