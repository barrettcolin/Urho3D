set(client_CPP_FILES
  cl_cin.c
  cl_ents.c
  cl_fx.c
  cl_input.c
  cl_inv.c
  cl_main.c
  cl_newfx.c
  cl_parse.c
  cl_pred.c
  cl_scrn.c
  cl_tent.c
  cl_view.c
  console.c
  keys.c
  menu.c
  qmenu.c
  snd_dma.c
  snd_mem.c
  snd_mix.c
  #x86.c
  )

set(client_H_FILES
  anorms.h
  cdaudio.h
  client.h
  console.h
  input.h
  keys.h
  qmenu.h
  ref.h
  screen.h
  snd_loc.h
  sound.h
  vid.h)

add_library(q2client ${client_CPP_FILES} ${client_H_FILES})
