set(server_CPP_FILES
  sv_ccmds.c
  sv_ents.c
  sv_game.c
  sv_init.c
  sv_main.c
  sv_send.c
  sv_user.c
  sv_world.c)

set(server_H_FILES
  server.h)

add_library(q2server ${server_CPP_FILES} ${server_H_FILES})
